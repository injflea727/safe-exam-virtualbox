/* $Id: VPoxNetAdp-linux.c $ */
/** @file
 * VPoxNetAdp - Virtual Network Adapter Driver (Host), Linux Specific Code.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "the-linux-kernel.h"
#include "version-generated.h"
#include "revision-generated.h"
#include "product-generated.h"
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/miscdevice.h>

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV
#include <VPox/log.h>
#include <iprt/errcore.h>
#include <iprt/process.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>

/*
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/alloca.h>
*/

#define VPOXNETADP_OS_SPECFIC 1
#include "../VPoxNetAdpInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VPOXNETADP_LINUX_NAME      "vpoxnet%d"
#define VPOXNETADP_CTL_DEV_NAME    "vpoxnetctl"

#define VPOXNETADP_FROM_IFACE(iface) ((PVPOXNETADP) ifnet_softc(iface))

/** Set netdev MAC address. */
#if RTLNX_VER_MIN(5,17,0)
# define VPOX_DEV_ADDR_SET(dev, addr, len) dev_addr_mod(dev, 0, addr, len)
#else /* < 5.17.0 */
# define VPOX_DEV_ADDR_SET(dev, addr, len) memcpy(dev->dev_addr, addr, len)
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int  __init VPoxNetAdpLinuxInit(void);
static void __exit VPoxNetAdpLinuxUnload(void);

static int VPoxNetAdpLinuxOpen(struct inode *pInode, struct file *pFilp);
static int VPoxNetAdpLinuxClose(struct inode *pInode, struct file *pFilp);
#if RTLNX_VER_MAX(2,6,36)
static int VPoxNetAdpLinuxIOCtl(struct inode *pInode, struct file *pFilp,
                                unsigned int uCmd, unsigned long ulArg);
#else  /* >= 2,6,36 */
static long VPoxNetAdpLinuxIOCtlUnlocked(struct file *pFilp,
                                         unsigned int uCmd, unsigned long ulArg);
#endif /* >= 2,6,36 */

static void vpoxNetAdpEthGetDrvinfo(struct net_device *dev, struct ethtool_drvinfo *info);
#if RTLNX_VER_MIN(4,20,0)
static int vpoxNetAdpEthGetLinkSettings(struct net_device *pNetDev, struct ethtool_link_ksettings *pLinkSettings);
#else  /* < 4,20,0 */
static int vpoxNetAdpEthGetSettings(struct net_device *dev, struct ethtool_cmd *cmd);
#endif /* < 4,20,0 */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
module_init(VPoxNetAdpLinuxInit);
module_exit(VPoxNetAdpLinuxUnload);

MODULE_AUTHOR(VPOX_VENDOR);
MODULE_DESCRIPTION(VPOX_PRODUCT " Network Adapter Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VPOX_VERSION_STRING " r" RT_XSTR(VPOX_SVN_REV) " (" RT_XSTR(INTNETTRUNKIFPORT_VERSION) ")");
#endif

/**
 * The (common) global data.
 */
static struct file_operations gFileOpsVPoxNetAdp =
{
    owner:      THIS_MODULE,
    open:       VPoxNetAdpLinuxOpen,
    release:    VPoxNetAdpLinuxClose,
#if RTLNX_VER_MAX(2,6,36)
    ioctl:      VPoxNetAdpLinuxIOCtl,
#else /* RTLNX_VER_MIN(2,6,36) */
    unlocked_ioctl: VPoxNetAdpLinuxIOCtlUnlocked,
#endif /* RTLNX_VER_MIN(2,6,36) */
};

/** The miscdevice structure. */
static struct miscdevice g_CtlDev =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       VPOXNETADP_CTL_DEV_NAME,
    fops:       &gFileOpsVPoxNetAdp,
# if RTLNX_VER_MAX(2,6,18)
    devfs_name: VPOXNETADP_CTL_DEV_NAME
# endif
};

# if RTLNX_VER_MIN(2,6,19)
static const struct ethtool_ops gEthToolOpsVPoxNetAdp =
# else
static struct ethtool_ops gEthToolOpsVPoxNetAdp =
# endif
{
    .get_drvinfo        = vpoxNetAdpEthGetDrvinfo,
# if RTLNX_VER_MIN(4,20,0)
    .get_link_ksettings = vpoxNetAdpEthGetLinkSettings,
# else
    .get_settings       = vpoxNetAdpEthGetSettings,
# endif
    .get_link           = ethtool_op_get_link,
};


struct VPoxNetAdpPriv
{
    struct net_device_stats Stats;
};

typedef struct VPoxNetAdpPriv VPOXNETADPPRIV;
typedef VPOXNETADPPRIV *PVPOXNETADPPRIV;

static int vpoxNetAdpLinuxOpen(struct net_device *pNetDev)
{
    netif_start_queue(pNetDev);
    return 0;
}

static int vpoxNetAdpLinuxStop(struct net_device *pNetDev)
{
    netif_stop_queue(pNetDev);
    return 0;
}

static int vpoxNetAdpLinuxXmit(struct sk_buff *pSkb, struct net_device *pNetDev)
{
    PVPOXNETADPPRIV pPriv = netdev_priv(pNetDev);

    /* Update the stats. */
    pPriv->Stats.tx_packets++;
    pPriv->Stats.tx_bytes += pSkb->len;
#if RTLNX_VER_MAX(2,6,31)
    /* Update transmission time stamp. */
    pNetDev->trans_start = jiffies;
#endif
    /* Nothing else to do, just free the sk_buff. */
    dev_kfree_skb(pSkb);
    return 0;
}

static struct net_device_stats *vpoxNetAdpLinuxGetStats(struct net_device *pNetDev)
{
    PVPOXNETADPPRIV pPriv = netdev_priv(pNetDev);
    return &pPriv->Stats;
}


/* ethtool_ops::get_drvinfo */
static void vpoxNetAdpEthGetDrvinfo(struct net_device *pNetDev, struct ethtool_drvinfo *info)
{
    PVPOXNETADPPRIV pPriv = netdev_priv(pNetDev);
    NOREF(pPriv);

    RTStrPrintf(info->driver, sizeof(info->driver),
                "%s", VPOXNETADP_NAME);

    /*
     * Would be nice to include VPOX_SVN_REV, but it's not available
     * here.  Use file's svn revision via svn keyword?
     */
    RTStrPrintf(info->version, sizeof(info->version),
                "%s", VPOX_VERSION_STRING);

    RTStrPrintf(info->fw_version, sizeof(info->fw_version),
                "0x%08X", INTNETTRUNKIFPORT_VERSION);

    RTStrPrintf(info->bus_info, sizeof(info->driver),
                "N/A");
}


# if RTLNX_VER_MIN(4,20,0)
/* ethtool_ops::get_link_ksettings */
static int vpoxNetAdpEthGetLinkSettings(struct net_device *pNetDev, struct ethtool_link_ksettings *pLinkSettings)
{
    /* We just need to set field we care for, the rest is done by ethtool_get_link_ksettings() helper in ethtool. */
    ethtool_link_ksettings_zero_link_mode(pLinkSettings, supported);
    ethtool_link_ksettings_zero_link_mode(pLinkSettings, advertising);
    ethtool_link_ksettings_zero_link_mode(pLinkSettings, lp_advertising);
    pLinkSettings->base.speed       = SPEED_10;
    pLinkSettings->base.duplex      = DUPLEX_FULL;
    pLinkSettings->base.port        = PORT_TP;
    pLinkSettings->base.phy_address = 0;
    pLinkSettings->base.transceiver = XCVR_INTERNAL;
    pLinkSettings->base.autoneg     = AUTONEG_DISABLE;
    return 0;
}
#else /* RTLNX_VER_MAX(4,20,0) */
/* ethtool_ops::get_settings */
static int vpoxNetAdpEthGetSettings(struct net_device *pNetDev, struct ethtool_cmd *cmd)
{
    cmd->supported      = 0;
    cmd->advertising    = 0;
#if RTLNX_VER_MIN(2,6,27)
    ethtool_cmd_speed_set(cmd, SPEED_10);
#else
    cmd->speed          = SPEED_10;
#endif
    cmd->duplex         = DUPLEX_FULL;
    cmd->port           = PORT_TP;
    cmd->phy_address    = 0;
    cmd->transceiver    = XCVR_INTERNAL;
    cmd->autoneg        = AUTONEG_DISABLE;
    cmd->maxtxpkt       = 0;
    cmd->maxrxpkt       = 0;
    return 0;
}
#endif /* RTLNX_VER_MAX(4,20,0) */


#if RTLNX_VER_MIN(2,6,29)
static const struct net_device_ops vpoxNetAdpNetdevOps = {
    .ndo_open               = vpoxNetAdpLinuxOpen,
    .ndo_stop               = vpoxNetAdpLinuxStop,
    .ndo_start_xmit         = vpoxNetAdpLinuxXmit,
    .ndo_get_stats          = vpoxNetAdpLinuxGetStats
};
#endif

static void vpoxNetAdpNetDevInit(struct net_device *pNetDev)
{
    PVPOXNETADPPRIV pPriv;

    ether_setup(pNetDev);
#if RTLNX_VER_MIN(2,6,29)
    pNetDev->netdev_ops = &vpoxNetAdpNetdevOps;
#else /* RTLNX_VER_MAX(2,6,29) */
    pNetDev->open = vpoxNetAdpLinuxOpen;
    pNetDev->stop = vpoxNetAdpLinuxStop;
    pNetDev->hard_start_xmit = vpoxNetAdpLinuxXmit;
    pNetDev->get_stats = vpoxNetAdpLinuxGetStats;
#endif /* RTLNX_VER_MAX(2,6,29) */
#if RTLNX_VER_MIN(4,10,0)
    pNetDev->max_mtu = 16110;
#endif /* RTLNX_VER_MIN(4,10,0) */

    pNetDev->ethtool_ops = &gEthToolOpsVPoxNetAdp;

    pPriv = netdev_priv(pNetDev);
    memset(pPriv, 0, sizeof(*pPriv));
}


int vpoxNetAdpOsCreate(PVPOXNETADP pThis, PCRTMAC pMACAddress)
{
    int rc = VINF_SUCCESS;
    struct net_device *pNetDev;

    /* No need for private data. */
    pNetDev = alloc_netdev(sizeof(VPOXNETADPPRIV),
                           pThis->szName[0] ? pThis->szName : VPOXNETADP_LINUX_NAME,
#if RTLNX_VER_MIN(3,17,0)
                           NET_NAME_UNKNOWN,
#endif
                           vpoxNetAdpNetDevInit);
    if (pNetDev)
    {
        int err;

        if (pNetDev->dev_addr)
        {
            VPOX_DEV_ADDR_SET(pNetDev, pMACAddress, ETH_ALEN);
            Log2(("vpoxNetAdpOsCreate: pNetDev->dev_addr = %.6Rhxd\n", pNetDev->dev_addr));

            /*
             * We treat presence of VPoxNetFlt filter as our "carrier",
             * see vpoxNetFltSetLinkState().
             *
             * operstates.txt: "On device allocation, networking core
             * sets the flags equivalent to netif_carrier_ok() and
             * !netif_dormant()" - so turn carrier off here.
             */
            netif_carrier_off(pNetDev);

            err = register_netdev(pNetDev);
            if (!err)
            {
                strncpy(pThis->szName, pNetDev->name, sizeof(pThis->szName));
                pThis->szName[sizeof(pThis->szName) - 1] = '\0';
                pThis->u.s.pNetDev = pNetDev;
                Log2(("vpoxNetAdpOsCreate: pThis=%p pThis->szName = %p\n", pThis, pThis->szName));
                return VINF_SUCCESS;
            }
        }
        else
        {
            LogRel(("VPoxNetAdp: failed to set MAC address (dev->dev_addr == NULL)\n"));
            err = EFAULT;
        }
        free_netdev(pNetDev);
        rc = RTErrConvertFromErrno(err);
    }
    return rc;
}

void vpoxNetAdpOsDestroy(PVPOXNETADP pThis)
{
    struct net_device *pNetDev = pThis->u.s.pNetDev;
    AssertPtr(pThis->u.s.pNetDev);

    pThis->u.s.pNetDev = NULL;
    unregister_netdev(pNetDev);
    free_netdev(pNetDev);
}

/**
 * Device open. Called on open /dev/vpoxnetctl
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VPoxNetAdpLinuxOpen(struct inode *pInode, struct file *pFilp)
{
    Log(("VPoxNetAdpLinuxOpen: pid=%d/%d %s\n", RTProcSelf(), current->pid, current->comm));

#ifdef VPOX_WITH_HARDENING
    /*
     * Only root is allowed to access the device, enforce it!
     */
    if (!capable(CAP_SYS_ADMIN))
    {
        Log(("VPoxNetAdpLinuxOpen: admin privileges required!\n"));
        return -EPERM;
    }
#endif

    return 0;
}


/**
 * Close device.
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VPoxNetAdpLinuxClose(struct inode *pInode, struct file *pFilp)
{
    Log(("VPoxNetAdpLinuxClose: pid=%d/%d %s\n",
         RTProcSelf(), current->pid, current->comm));
    pFilp->private_data = NULL;
    return 0;
}

/**
 * Device I/O Control entry point.
 *
 * @param   pFilp       Associated file pointer.
 * @param   uCmd        The function specified to ioctl().
 * @param   ulArg       The argument specified to ioctl().
 */
#if RTLNX_VER_MAX(2,6,36)
static int VPoxNetAdpLinuxIOCtl(struct inode *pInode, struct file *pFilp,
                                unsigned int uCmd, unsigned long ulArg)
#else /* RTLNX_VER_MIN(2,6,36) */
static long VPoxNetAdpLinuxIOCtlUnlocked(struct file *pFilp,
                                         unsigned int uCmd, unsigned long ulArg)
#endif /* RTLNX_VER_MIN(2,6,36) */
{
    VPOXNETADPREQ Req;
    PVPOXNETADP pAdp;
    int rc;
    char *pszName = NULL;

    Log(("VPoxNetAdpLinuxIOCtl: param len %#x; uCmd=%#x; add=%#x\n", _IOC_SIZE(uCmd), uCmd, VPOXNETADP_CTL_ADD));
    if (RT_UNLIKELY(_IOC_SIZE(uCmd) != sizeof(Req))) /* paranoia */
    {
        Log(("VPoxNetAdpLinuxIOCtl: bad ioctl sizeof(Req)=%#x _IOC_SIZE=%#x; uCmd=%#x.\n", sizeof(Req), _IOC_SIZE(uCmd), uCmd));
        return -EINVAL;
    }

    switch (uCmd)
    {
        case VPOXNETADP_CTL_ADD:
            Log(("VPoxNetAdpLinuxIOCtl: _IOC_DIR(uCmd)=%#x; IOC_OUT=%#x\n", _IOC_DIR(uCmd), IOC_OUT));
            if (RT_UNLIKELY(copy_from_user(&Req, (void *)ulArg, sizeof(Req))))
            {
                Log(("VPoxNetAdpLinuxIOCtl: copy_from_user(,%#lx,) failed; uCmd=%#x.\n", ulArg, uCmd));
                return -EFAULT;
            }
            Log(("VPoxNetAdpLinuxIOCtl: Add %s\n", Req.szName));

            if (Req.szName[0])
            {
                pAdp = vpoxNetAdpFindByName(Req.szName);
                if (pAdp)
                {
                    Log(("VPoxNetAdpLinuxIOCtl: '%s' already exists\n", Req.szName));
                    return -EINVAL;
                }
                pszName = Req.szName;
            }
            rc = vpoxNetAdpCreate(&pAdp, pszName);
            if (RT_FAILURE(rc))
            {
                Log(("VPoxNetAdpLinuxIOCtl: vpoxNetAdpCreate -> %Rrc\n", rc));
                return -(rc == VERR_OUT_OF_RESOURCES ? ENOMEM : EINVAL);
            }

            Assert(strlen(pAdp->szName) < sizeof(Req.szName));
            strncpy(Req.szName, pAdp->szName, sizeof(Req.szName) - 1);
            Req.szName[sizeof(Req.szName) - 1] = '\0';

            if (RT_UNLIKELY(copy_to_user((void *)ulArg, &Req, sizeof(Req))))
            {
                /* this is really bad! */
                /** @todo remove the adapter again? */
                printk(KERN_ERR "VPoxNetAdpLinuxIOCtl: copy_to_user(%#lx,,%#zx); uCmd=%#x!\n", ulArg, sizeof(Req), uCmd);
                return -EFAULT;
            }
            Log(("VPoxNetAdpLinuxIOCtl: Successfully added '%s'\n", Req.szName));
            break;

        case VPOXNETADP_CTL_REMOVE:
            if (RT_UNLIKELY(copy_from_user(&Req, (void *)ulArg, sizeof(Req))))
            {
                Log(("VPoxNetAdpLinuxIOCtl: copy_from_user(,%#lx,) failed; uCmd=%#x.\n", ulArg, uCmd));
                return -EFAULT;
            }
            Log(("VPoxNetAdpLinuxIOCtl: Remove %s\n", Req.szName));

            pAdp = vpoxNetAdpFindByName(Req.szName);
            if (!pAdp)
            {
                Log(("VPoxNetAdpLinuxIOCtl: '%s' not found\n", Req.szName));
                return -EINVAL;
            }

            rc = vpoxNetAdpDestroy(pAdp);
            if (RT_FAILURE(rc))
            {
                Log(("VPoxNetAdpLinuxIOCtl: vpoxNetAdpDestroy('%s') -> %Rrc\n", Req.szName, rc));
                return -EINVAL;
            }
            Log(("VPoxNetAdpLinuxIOCtl: Successfully removed '%s'\n", Req.szName));
            break;

        default:
            printk(KERN_ERR "VPoxNetAdpLinuxIOCtl: unknown command %x.\n", uCmd);
            return -EINVAL;
    }

    return 0;
}

int  vpoxNetAdpOsInit(PVPOXNETADP pThis)
{
    /*
     * Init linux-specific members.
     */
    pThis->u.s.pNetDev = NULL;

    return VINF_SUCCESS;
}



/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init VPoxNetAdpLinuxInit(void)
{
    int rc;
    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VPoxNetAdpLinuxInit\n"));

        rc = vpoxNetAdpInit();
        if (RT_SUCCESS(rc))
        {
            rc = misc_register(&g_CtlDev);
            if (rc)
            {
                printk(KERN_ERR "VPoxNetAdp: Can't register " VPOXNETADP_CTL_DEV_NAME " device! rc=%d\n", rc);
                return rc;
            }
            LogRel(("VPoxNetAdp: Successfully started.\n"));
            return 0;
        }
        else
            LogRel(("VPoxNetAdp: failed to register vpoxnet0 device (rc=%d)\n", rc));
    }
    else
        LogRel(("VPoxNetAdp: failed to initialize IPRT (rc=%d)\n", rc));

    return -RTErrConvertToErrno(rc);
}


/**
 * Unload the module.
 *
 * @todo We have to prevent this if we're busy!
 */
static void __exit VPoxNetAdpLinuxUnload(void)
{
    Log(("VPoxNetAdpLinuxUnload\n"));

    /*
     * Undo the work done during start (in reverse order).
     */

    vpoxNetAdpShutdown();
    /* Remove control device */
    misc_deregister(&g_CtlDev);

    RTR0Term();

    Log(("VPoxNetAdpLinuxUnload - done\n"));
}

