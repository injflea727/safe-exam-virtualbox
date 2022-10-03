/* $Id: VPoxSF.cpp $ */
/** @file
 * VPoxSF - Darwin Shared Folders, KEXT entry points.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_FOLDERS
#include "VPoxSFInternal.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <VPox/version.h>
#include <VPox/log.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static kern_return_t vpoxSfDwnModuleLoad(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t vpoxSfDwnModuleUnload(struct kmod_info *pKModInfo, void *pvData);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The VPoxGuest service if we've managed to connect to it already. */
static IOService               *g_pVPoxGuest = NULL;
/** The shared folder service client structure. */
VBGLSFCLIENT                    g_SfClientDarwin = { UINT32_MAX, NULL };
/** Number of active mounts.  Used for unload prevention. */
uint32_t volatile               g_cVPoxSfMounts = 0;

/** VFS table entry for our file system (for vfs_fsremove). */
static vfstable_t               g_pVPoxSfVfsTableEntry;
/** For vfs_fsentry. */
static struct vnodeopv_desc    *g_apVPoxSfVnodeOpDescList[] =
{
    &g_VPoxSfVnodeOpvDesc,
};
/** VFS registration structure. */
static struct vfs_fsentry       g_VPoxSfFsEntry =
{
    .vfe_vfsops     = &g_VPoxSfVfsOps,
    .vfe_vopcnt     = RT_ELEMENTS(g_apVPoxSfVnodeOpDescList),
    .vfe_opvdescs   = g_apVPoxSfVnodeOpDescList,
    .vfe_fstypenum  = -1,
    .vfe_fsname     = VPOXSF_DARWIN_FS_NAME,
    .vfe_flags      = VFS_TBLTHREADSAFE     /* Required. */
                    | VFS_TBLFSNODELOCK     /* Required. */
                    | VFS_TBLNOTYPENUM      /* No historic file system number. */
                    | VFS_TBL64BITREADY,    /* Can handle 64-bit processes */
    /** @todo add VFS_TBLREADDIR_EXTENDED */
    .vfe_reserv     = { NULL, NULL },
};


/**
 * Declare the module stuff.
 */
RT_C_DECLS_BEGIN
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VPoxSF, VPOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain      = vpoxSfDwnModuleLoad;
DECLHIDDEN(kmod_stop_func_t *)  _antimain      = vpoxSfDwnModuleUnload;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
RT_C_DECLS_END


/**
 * Connect to VPoxGuest and host shared folders service.
 *
 * @returns true if connected, false if not.
 */
bool vpoxSfDwnConnect(void)
{
    /*
     * Grab VPoxGuest - since it's a dependency of this module, it shouldn't be hard.
     */
    if (!g_pVPoxGuest)
    {
        OSDictionary *pServiceMatcher = IOService::serviceMatching("org_virtualpox_VPoxGuest");
        if (pServiceMatcher)
        {
            IOService *pVPoxGuest = IOService::waitForMatchingService(pServiceMatcher, 10 * RT_NS_1SEC);
            if (pVPoxGuest)
                g_pVPoxGuest = pVPoxGuest;
            else
                LogRel(("vpoxSfDwnConnect: IOService::waitForMatchingService failed!!\n"));
        }
        else
            LogRel(("vpoxSfDwnConnect: serviceMatching failed\n"));
    }

    if (g_pVPoxGuest)
    {
        /*
         * Get hold of the shared folders service if we haven't already.
         */
        if (g_SfClientDarwin.handle != NULL)
            return true;

        int rc = VbglR0SfConnect(&g_SfClientDarwin);
        if (RT_SUCCESS(rc))
        {
            rc = VbglR0SfSetUtf8(&g_SfClientDarwin);
            if (RT_SUCCESS(rc))
                return true;

            LogRel(("VPoxSF: VbglR0SfSetUtf8 failed: %Rrc\n", rc));

            VbglR0SfDisconnect(&g_SfClientDarwin);
            g_SfClientDarwin.handle = NULL;
        }
        else
            LogRel(("VPoxSF: VbglR0SfConnect failed: %Rrc\n", rc));
    }

    return false;
}


/**
 * Start the kernel module.
 */
static kern_return_t vpoxSfDwnModuleLoad(struct kmod_info *pKModInfo, void *pvData)
{
    RT_NOREF(pKModInfo, pvData);
#ifdef DEBUG
    printf("vpoxSfDwnModuleLoad\n");
    RTLogBackdoorPrintf("vpoxSfDwnModuleLoad\n");
#endif

    /*
     * Initialize IPRT and the ring-0 guest library.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR0SfInit();
        if (RT_SUCCESS(rc))
        {
            /*
             * Register the file system.
             */
            rc = vfs_fsadd(&g_VPoxSfFsEntry, &g_pVPoxSfVfsTableEntry);
            if (rc == 0)
            {
                /*
                 * Try find VPoxGuest and connect to the shared folders service on the host.
                 */
                /** @todo should we just ignore the error here and retry at mount time?
                 * Technically, VPoxGuest should be available since it's one of our
                 * dependencies... */
                vpoxSfDwnConnect();

                /*
                 * We're done for now.  We'll deal with
                 */
                LogRel(("VPoxSF: loaded\n"));
                return KERN_SUCCESS;
            }

            printf("VPoxSF: vfs_fsadd failed: %d\n", rc);
            RTLogBackdoorPrintf("VPoxSF: vfs_fsadd failed: %d\n", rc);
            VbglR0SfTerm();
        }
        else
        {
            printf("VPoxSF: VbglR0SfInit failed: %d\n", rc);
            RTLogBackdoorPrintf("VPoxSF: VbglR0SfInit failed: %Rrc\n", rc);
        }
        RTR0Term();
    }
    else
    {
        printf("VPoxSF: RTR0Init failed: %d\n", rc);
        RTLogBackdoorPrintf("VPoxSF: RTR0Init failed: %Rrc\n", rc);
    }
    return KERN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t vpoxSfDwnModuleUnload(struct kmod_info *pKModInfo, void *pvData)
{
    RT_NOREF(pKModInfo, pvData);
#ifdef DEBUG
    printf("vpoxSfDwnModuleUnload\n");
    RTLogBackdoorPrintf("vpoxSfDwnModuleUnload\n");
#endif


    /*
     * Are we busy?  If so fail.  Otherwise try deregister the file system.
     */
    if (g_cVPoxSfMounts > 0)
    {
        LogRel(("VPoxSF: Refusing to unload with %u active mounts\n", g_cVPoxSfMounts));
        return KERN_NO_ACCESS;
    }

    if (g_pVPoxSfVfsTableEntry)
    {
        int rc = vfs_fsremove(g_pVPoxSfVfsTableEntry);
        if (rc != 0)
        {
            LogRel(("VPoxSF: vfs_fsremove failed: %d\n", rc));
            return KERN_NO_ACCESS;
        }
    }

    /*
     * Disconnect and terminate libraries we're using.
     */
    if (g_SfClientDarwin.handle != NULL)
    {
        VbglR0SfDisconnect(&g_SfClientDarwin);
        g_SfClientDarwin.handle = NULL;
    }

    if (g_pVPoxGuest)
    {
        g_pVPoxGuest->release();
        g_pVPoxGuest = NULL;
    }

    VbglR0SfTerm();
    RTR0Term();
    return KERN_SUCCESS;
}

