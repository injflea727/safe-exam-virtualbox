/* $Id: VPoxGuest-darwin.cpp $ */
/** @file
 * VPoxGuest - Darwin Specifics.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_VGDRV
/*
 * Deal with conflicts first.
 * PVM - BSD mess, that FreeBSD has correct a long time ago.
 * iprt/types.h before sys/param.h - prevents UINT32_C and friends.
 */
#include <iprt/types.h>
#include <sys/param.h>
#undef PVM

#include <IOKit/IOLib.h> /* Assert as function */

#include <VPox/version.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/power.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include <VPox/err.h>
#include <VPox/log.h>

#include <mach/kmod.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include "VPoxGuestInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The system device node name. */
#define DEVICE_NAME_SYS     "vpoxguest"
/** The user device node name. */
#define DEVICE_NAME_USR     "vpoxguestu"


/** @name For debugging/whatever, now permanent.
 * @{  */
#define VPOX_PROC_SELFNAME_LEN              31
#define VPOX_RETRIEVE_CUR_PROC_NAME(a_Name) char a_Name[VPOX_PROC_SELFNAME_LEN + 1]; \
                                            proc_selfname(a_Name, VPOX_PROC_SELFNAME_LEN)
/** @} */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
static kern_return_t    vgdrvDarwinStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    vgdrvDarwinStop(struct kmod_info *pKModInfo, void *pvData);
static int              vgdrvDarwinCharDevRemove(void);

static int              vgdrvDarwinOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int              vgdrvDarwinClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int              vgdrvDarwinIOCtlSlow(PVPOXGUESTSESSION pSession, u_long iCmd, caddr_t pData, struct proc *pProcess);
static int              vgdrvDarwinIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess);

static int              vgdrvDarwinErr2DarwinErr(int rc);

static IOReturn         vgdrvDarwinSleepHandler(void *pvTarget, void *pvRefCon, UInt32 uMessageType, IOService *pProvider, void *pvMessageArgument, vm_size_t argSize);
RT_C_DECLS_END


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The service class for handling the VMMDev PCI device.
 *
 * Instantiated when the module is loaded (and on PCI hotplugging?).
 */
class org_virtualpox_VPoxGuest : public IOService
{
    OSDeclareDefaultStructors(org_virtualpox_VPoxGuest);

private:
    IOPCIDevice                *m_pIOPCIDevice;
    IOMemoryMap                *m_pMap;
    IOFilterInterruptEventSource *m_pInterruptSrc;

    bool setupVmmDevInterrupts(IOService *pProvider);
    bool disableVmmDevInterrupts(void);
    bool isVmmDev(IOPCIDevice *pIOPCIDevice);

protected:
    /** Non-NULL if interrupts are registered.  Probably same as getProvider(). */
    IOService                 *m_pInterruptProvider;

public:
    virtual bool init(OSDictionary *pDictionary = 0);
    virtual void free(void);
    virtual IOService *probe(IOService *pProvider, SInt32 *pi32Score);
    virtual bool start(IOService *pProvider);
    virtual void stop(IOService *pProvider);
    virtual bool terminate(IOOptionBits fOptions);
    static void  vgdrvDarwinIrqHandler(OSObject *pTarget, void *pvRefCon, IOService *pNub, int iSrc);
};

OSDefineMetaClassAndStructors(org_virtualpox_VPoxGuest, IOService);


/**
 * An attempt at getting that clientDied() notification.
 * I don't think it'll work as I cannot figure out where/what creates the correct
 * port right.
 *
 * Instantiated when userland does IOServiceOpen().
 */
class org_virtualpox_VPoxGuestClient : public IOUserClient
{
    OSDeclareDefaultStructors(org_virtualpox_VPoxGuestClient);

private:
    /** Guard against the parent class growing and us using outdated headers. */
    uint8_t m_abSafetyPadding[256];

    PVPOXGUESTSESSION           m_pSession;     /**< The session. */
    task_t                      m_Task;         /**< The client task. */
    org_virtualpox_VPoxGuest   *m_pProvider;    /**< The service provider. */

public:
    virtual bool initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type);
    virtual bool start(IOService *pProvider);
    static  void sessionClose(RTPROCESS Process);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    virtual bool terminate(IOOptionBits fOptions = 0);
    virtual bool finalize(IOOptionBits fOptions);
    virtual void stop(IOService *pProvider);

    RTR0MEMEF_NEW_AND_DELETE_OPERATORS_IOKIT();
};

OSDefineMetaClassAndStructors(org_virtualpox_VPoxGuestClient, IOUserClient);



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Declare the module stuff.
 */
RT_C_DECLS_BEGIN
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VPoxGuest, VPOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain = vgdrvDarwinStart;
DECLHIDDEN(kmod_stop_func_t *)  _antimain = vgdrvDarwinStop;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
RT_C_DECLS_END


/**
 * Device extention & session data association structure.
 */
static VPOXGUESTDEVEXT  g_DevExt;

/**
 * The character device switch table for the driver.
 */
static struct cdevsw    g_DevCW =
{
    /*.d_open     = */ vgdrvDarwinOpen,
    /*.d_close    = */ vgdrvDarwinClose,
    /*.d_read     = */ eno_rdwrt,
    /*.d_write    = */ eno_rdwrt,
    /*.d_ioctl    = */ vgdrvDarwinIOCtl,
    /*.d_stop     = */ eno_stop,
    /*.d_reset    = */ eno_reset,
    /*.d_ttys     = */ NULL,
    /*.d_select   = */ eno_select,
    /*.d_mmap     = */ eno_mmap,
    /*.d_strategy = */ eno_strat,
    /*.d_getc     = */ (void *)(uintptr_t)&enodev, //eno_getc,
    /*.d_putc     = */ (void *)(uintptr_t)&enodev, //eno_putc,
    /*.d_type     = */ 0
};

/** Major device number. */
static int                  g_iMajorDeviceNo = -1;
/** Registered devfs device handle. */
static void                *g_hDevFsDeviceSys = NULL;
/** Registered devfs device handle for the user device. */
static void                *g_hDevFsDeviceUsr = NULL; /**< @todo 4 later */

/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Hash table */
static PVPOXGUESTSESSION    g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(pid)   ((pid) % RT_ELEMENTS(g_apSessionHashTab))
/** The number of open sessions. */
static int32_t volatile     g_cSessions = 0;
/** Makes sure there is only one org_virtualpox_VPoxGuest instance. */
static bool volatile        g_fInstantiated = 0;
/** The notifier handle for the sleep callback handler. */
static IONotifier          *g_pSleepNotifier = NULL;


/**
 * Start the kernel module.
 */
static kern_return_t    vgdrvDarwinStart(struct kmod_info *pKModInfo, void *pvData)
{
    RT_NOREF(pKModInfo, pvData);
#ifdef DEBUG
    printf("vgdrvDarwinStart\n");
#endif
#if 0
    gIOKitDebug |= 0x001 //kIOLogAttach
                |  0x002 //kIOLogProbe
                |  0x004 //kIOLogStart
                |  0x008 //kIOLogRegister
                |  0x010 //kIOLogMatch
                |  0x020 //kIOLogConfig
                ;
#endif

    /*
     * Initialize IPRT.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VPoxGuest: driver loaded\n"));
        return KMOD_RETURN_SUCCESS;
    }

    RTLogBackdoorPrintf("VPoxGuest: RTR0Init failed with rc=%Rrc\n", rc);
    printf("VPoxGuest: RTR0Init failed with rc=%d\n", rc);
    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t vgdrvDarwinStop(struct kmod_info *pKModInfo, void *pvData)
{
    RT_NOREF(pKModInfo, pvData);

    /** @todo we need to check for VPoxSF clients? */

    RTLogBackdoorPrintf("VPoxGuest: calling RTR0TermForced ...\n");
    RTR0TermForced();

    RTLogBackdoorPrintf("VPoxGuest: vgdrvDarwinStop returns.\n");
    printf("VPoxGuest: driver unloaded\n");
    return KMOD_RETURN_SUCCESS;
}


/**
 * Register VPoxGuest char device
 */
static int vgdrvDarwinCharDevInit(void)
{
    int rc = RTSpinlockCreate(&g_Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VPoxGuestDarwin");
    if (RT_SUCCESS(rc))
    {
        /*
         * Registering ourselves as a character device.
         */
        g_iMajorDeviceNo = cdevsw_add(-1, &g_DevCW);
        if (g_iMajorDeviceNo >= 0)
        {
            /** @todo limit /dev/vpoxguest access. */
            g_hDevFsDeviceSys = devfs_make_node(makedev(g_iMajorDeviceNo, 0), DEVFS_CHAR,
                                                UID_ROOT, GID_WHEEL, 0666, DEVICE_NAME_SYS);
            if (g_hDevFsDeviceSys != NULL)
            {
                /*
                 * And a all-user device.
                 */
                g_hDevFsDeviceUsr = devfs_make_node(makedev(g_iMajorDeviceNo, 1), DEVFS_CHAR,
                                                    UID_ROOT, GID_WHEEL, 0666, DEVICE_NAME_USR);
                if (g_hDevFsDeviceUsr != NULL)
                {
                    /*
                     * Register a sleep/wakeup notification callback.
                     */
                    g_pSleepNotifier = registerPrioritySleepWakeInterest(&vgdrvDarwinSleepHandler, &g_DevExt, NULL);
                    if (g_pSleepNotifier != NULL)
                        return KMOD_RETURN_SUCCESS;
                }
            }
        }
        vgdrvDarwinCharDevRemove();
    }
    return KMOD_RETURN_FAILURE;
}


/**
 * Unregister VPoxGuest char devices and associated session spinlock.
 */
static int vgdrvDarwinCharDevRemove(void)
{
    if (g_pSleepNotifier)
    {
        g_pSleepNotifier->remove();
        g_pSleepNotifier = NULL;
    }

    if (g_hDevFsDeviceSys)
    {
        devfs_remove(g_hDevFsDeviceSys);
        g_hDevFsDeviceSys = NULL;
    }

    if (g_hDevFsDeviceUsr)
    {
        devfs_remove(g_hDevFsDeviceUsr);
        g_hDevFsDeviceUsr = NULL;
    }

    if (g_iMajorDeviceNo != -1)
    {
        int rc2 = cdevsw_remove(g_iMajorDeviceNo, &g_DevCW);
        Assert(rc2 == g_iMajorDeviceNo); NOREF(rc2);
        g_iMajorDeviceNo = -1;
    }

    if (g_Spinlock != NIL_RTSPINLOCK)
    {
        int rc2 = RTSpinlockDestroy(g_Spinlock); AssertRC(rc2);
        g_Spinlock = NIL_RTSPINLOCK;
    }

    return KMOD_RETURN_SUCCESS;
}


/**
 * Device open. Called on open /dev/vpoxguest and (later) /dev/vpoxguestu.
 *
 * @param   Dev         The device number.
 * @param   fFlags      ???.
 * @param   fDevType    ???.
 * @param   pProcess    The process issuing this request.
 */
static int vgdrvDarwinOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    RT_NOREF(fFlags,  fDevType);

    /*
     * Only two minor devices numbers are allowed.
     */
    if (minor(Dev) != 0 && minor(Dev) != 1)
        return EACCES;

    /*
     * The process issuing the request must be the current process.
     */
    RTPROCESS Process = RTProcSelf();
    if ((int)Process != proc_pid(pProcess))
        return EIO;

    /*
     * Find the session created by org_virtualpox_VPoxGuestClient, fail
     * if no such session, and mark it as opened. We set the uid & gid
     * here too, since that is more straight forward at this point.
     */
    const bool          fUnrestricted = minor(Dev) == 0;
    int                 rc = VINF_SUCCESS;
    PVPOXGUESTSESSION   pSession = NULL;
    kauth_cred_t        pCred = kauth_cred_proc_ref(pProcess);
    if (pCred)
    {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
        RTUID           Uid = kauth_cred_getruid(pCred);
        RTGID           Gid = kauth_cred_getrgid(pCred);
#else
        RTUID           Uid = pCred->cr_ruid;
        RTGID           Gid = pCred->cr_rgid;
#endif
        unsigned        iHash = SESSION_HASH(Process);
        RTSpinlockAcquire(g_Spinlock);

        pSession = g_apSessionHashTab[iHash];
        while (pSession && pSession->Process != Process)
            pSession = pSession->pNextHash;
        if (pSession)
        {
            if (!pSession->fOpened)
            {
                pSession->fOpened = true;
                pSession->fUserSession = !fUnrestricted;
                pSession->fRequestor = VMMDEV_REQUESTOR_USERMODE | VMMDEV_REQUESTOR_TRUST_NOT_GIVEN;
                if (Uid == 0)
                    pSession->fRequestor |= VMMDEV_REQUESTOR_USR_ROOT;
                else
                    pSession->fRequestor |= VMMDEV_REQUESTOR_USR_USER;
                if (Gid == 0)
                    pSession->fRequestor |= VMMDEV_REQUESTOR_GRP_WHEEL;
                if (!fUnrestricted)
                    pSession->fRequestor |= VMMDEV_REQUESTOR_USER_DEVICE;
                pSession->fRequestor |= VMMDEV_REQUESTOR_CON_DONT_KNOW; /** @todo see if we can figure out console relationship of pProc. */
            }
            else
                rc = VERR_ALREADY_LOADED;
        }
        else
            rc = VERR_GENERAL_FAILURE;

        RTSpinlockRelease(g_Spinlock);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
        kauth_cred_unref(&pCred);
#else  /* 10.4 */
        /* The 10.4u SDK headers and 10.4.11 kernel source have inconsistent definitions
           of kauth_cred_unref(), so use the other (now deprecated) API for releasing it. */
        kauth_cred_rele(pCred);
#endif /* 10.4 */
    }
    else
        rc = VERR_INVALID_PARAMETER;

    Log(("vgdrvDarwinOpen: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, proc_pid(pProcess)));
    return vgdrvDarwinErr2DarwinErr(rc);
}


/**
 * Close device.
 */
static int vgdrvDarwinClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    RT_NOREF(Dev, fFlags, fDevType, pProcess);
    Log(("vgdrvDarwinClose: pid=%d\n", (int)RTProcSelf()));
    Assert(proc_pid(pProcess) == (int)RTProcSelf());

    /*
     * Hand the session closing to org_virtualpox_VPoxGuestClient.
     */
    org_virtualpox_VPoxGuestClient::sessionClose(RTProcSelf());
    return 0;
}


/**
 * Device I/O Control entry point.
 *
 * @returns Darwin for slow IOCtls and VPox status code for the fast ones.
 * @param   Dev         The device number (major+minor).
 * @param   iCmd        The IOCtl command.
 * @param   pData       Pointer to the request data.
 * @param   fFlags      Flag saying we're a character device (like we didn't know already).
 * @param   pProcess    The process issuing this request.
 */
static int vgdrvDarwinIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess)
{
    RT_NOREF(Dev, fFlags);
    const bool          fUnrestricted = minor(Dev) == 0;
    const RTPROCESS     Process = proc_pid(pProcess);
    const unsigned      iHash = SESSION_HASH(Process);
    PVPOXGUESTSESSION   pSession;

    /*
     * Find the session.
     */
    RTSpinlockAcquire(g_Spinlock);
    pSession = g_apSessionHashTab[iHash];
    while (pSession && (pSession->Process != Process || pSession->fUserSession == fUnrestricted || !pSession->fOpened))
        pSession = pSession->pNextHash;

    //if (RT_LIKELY(pSession))
    //    supdrvSessionRetain(pSession);

    RTSpinlockRelease(g_Spinlock);
    if (!pSession)
    {
        Log(("VPoxDrvDarwinIOCtl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d iCmd=%#lx\n",
             (int)Process, iCmd));
        return EINVAL;
    }

    /*
     * Deal with the high-speed IOCtl.
     */
    int rc;
    if (VBGL_IOCTL_IS_FAST(iCmd))
        rc = VGDrvCommonIoCtlFast(iCmd, &g_DevExt, pSession);
    else
        rc = vgdrvDarwinIOCtlSlow(pSession, iCmd, pData, pProcess);

    //supdrvSessionRelease(pSession);
    return rc;
}


/**
 * Worker for VPoxDrvDarwinIOCtl that takes the slow IOCtl functions.
 *
 * @returns Darwin errno.
 *
 * @param pSession  The session.
 * @param iCmd      The IOCtl command.
 * @param pData     Pointer to the request data.
 * @param pProcess  The calling process.
 */
static int vgdrvDarwinIOCtlSlow(PVPOXGUESTSESSION pSession, u_long iCmd, caddr_t pData, struct proc *pProcess)
{
    RT_NOREF(pProcess);
    LogFlow(("vgdrvDarwinIOCtlSlow: pSession=%p iCmd=%p pData=%p pProcess=%p\n", pSession, iCmd, pData, pProcess));


    /*
     * Buffered or unbuffered?
     */
    PVBGLREQHDR pHdr;
    user_addr_t pUser = 0;
    void *pvPageBuf = NULL;
    uint32_t cbReq = IOCPARM_LEN(iCmd);
    if ((IOC_DIRMASK & iCmd) == IOC_INOUT)
    {
        pHdr = (PVBGLREQHDR)pData;
        if (RT_UNLIKELY(cbReq < sizeof(*pHdr)))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: cbReq=%#x < %#x; iCmd=%#lx\n", cbReq, (int)sizeof(*pHdr), iCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY(pHdr->uVersion != VBGLREQHDR_VERSION))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: bad uVersion=%#x; iCmd=%#lx\n", pHdr->uVersion, iCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY(   RT_MAX(pHdr->cbIn, pHdr->cbOut) != cbReq
                        || pHdr->cbIn < sizeof(*pHdr)
                        || (pHdr->cbOut < sizeof(*pHdr) && pHdr->cbOut != 0)))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: max(%#x,%#x) != %#x; iCmd=%#lx\n", pHdr->cbIn, pHdr->cbOut, cbReq, iCmd));
            return EINVAL;
        }
    }
    else if ((IOC_DIRMASK & iCmd) == IOC_VOID && !cbReq)
    {
        /*
         * Get the header and figure out how much we're gonna have to read.
         */
        VBGLREQHDR Hdr;
        pUser = (user_addr_t)*(void **)pData;
        int rc = copyin(pUser, &Hdr, sizeof(Hdr));
        if (RT_UNLIKELY(rc))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: copyin(%llx,Hdr,) -> %#x; iCmd=%#lx\n", (unsigned long long)pUser, rc, iCmd));
            return rc;
        }
        if (RT_UNLIKELY(Hdr.uVersion != VBGLREQHDR_VERSION))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: bad uVersion=%#x; iCmd=%#lx\n", Hdr.uVersion, iCmd));
            return EINVAL;
        }
        cbReq = RT_MAX(Hdr.cbIn, Hdr.cbOut);
        if (RT_UNLIKELY(   Hdr.cbIn < sizeof(Hdr)
                        || (Hdr.cbOut < sizeof(Hdr) && Hdr.cbOut != 0)
                        || cbReq > _1M*16))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: max(%#x,%#x); iCmd=%#lx\n", Hdr.cbIn, Hdr.cbOut, iCmd));
            return EINVAL;
        }

        /*
         * Allocate buffer and copy in the data.
         */
        pHdr = (PVBGLREQHDR)RTMemTmpAlloc(cbReq);
        if (!pHdr)
            pvPageBuf = pHdr = (PVBGLREQHDR)IOMallocAligned(RT_ALIGN_Z(cbReq, PAGE_SIZE), 8);
        if (RT_UNLIKELY(!pHdr))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: failed to allocate buffer of %d bytes; iCmd=%#lx\n", cbReq, iCmd));
            return ENOMEM;
        }
        rc = copyin(pUser, pHdr, Hdr.cbIn);
        if (RT_UNLIKELY(rc))
        {
            LogRel(("vgdrvDarwinIOCtlSlow: copyin(%llx,%p,%#x) -> %#x; iCmd=%#lx\n",
                    (unsigned long long)pUser, pHdr, Hdr.cbIn, rc, iCmd));
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, RT_ALIGN_Z(cbReq, PAGE_SIZE));
            else
                RTMemTmpFree(pHdr);
            return rc;
        }
        if (Hdr.cbIn < cbReq)
            RT_BZERO((uint8_t *)pHdr + Hdr.cbIn, cbReq - Hdr.cbIn);
    }
    else
    {
        Log(("vgdrvDarwinIOCtlSlow: huh? cbReq=%#x iCmd=%#lx\n", cbReq, iCmd));
        return EINVAL;
    }

    /*
     * Process the IOCtl.
     */
    int rc = VGDrvCommonIoCtl(iCmd, &g_DevExt, pSession, pHdr, cbReq);
    if (RT_LIKELY(!rc))
    {
        /*
         * If not buffered, copy back the buffer before returning.
         */
        if (pUser)
        {
            uint32_t cbOut = pHdr->cbOut;
            if (cbOut > cbReq)
            {
                LogRel(("vgdrvDarwinIOCtlSlow: too much output! %#x > %#x; uCmd=%#lx!\n", cbOut, cbReq, iCmd));
                cbOut = cbReq;
            }
            rc = copyout(pHdr, pUser, cbOut);
            if (RT_UNLIKELY(rc))
                LogRel(("vgdrvDarwinIOCtlSlow: copyout(%p,%llx,%#x) -> %d; uCmd=%#lx!\n",
                        pHdr, (unsigned long long)pUser, cbOut, rc, iCmd));

            /* cleanup */
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, RT_ALIGN_Z(cbReq, PAGE_SIZE));
            else
                RTMemTmpFree(pHdr);
        }
    }
    else
    {
        /*
         * The request failed, just clean up.
         */
        if (pUser)
        {
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, RT_ALIGN_Z(cbReq, PAGE_SIZE));
            else
                RTMemTmpFree(pHdr);
        }

        Log(("vgdrvDarwinIOCtlSlow: pid=%d iCmd=%lx pData=%p failed, rc=%d\n", proc_pid(pProcess), iCmd, (void *)pData, rc));
        rc = EINVAL;
    }

    Log2(("vgdrvDarwinIOCtlSlow: returns %d\n", rc));
    return rc;
}


/**
 * @note This code is duplicated on other platforms with variations, so please
 *       keep them all up to date when making changes!
 */
int VPOXCALL VPoxGuestIDC(void *pvSession, uintptr_t uReq, PVBGLREQHDR pReqHdr, size_t cbReq)
{
    /*
     * Simple request validation (common code does the rest).
     */
    int rc;
    if (   RT_VALID_PTR(pReqHdr)
        && cbReq >= sizeof(*pReqHdr))
    {
        /*
         * All requests except the connect one requires a valid session.
         */
        PVPOXGUESTSESSION pSession = (PVPOXGUESTSESSION)pvSession;
        if (pSession)
        {
            if (   RT_VALID_PTR(pSession)
                && pSession->pDevExt == &g_DevExt)
                rc = VGDrvCommonIoCtl(uReq, &g_DevExt, pSession, pReqHdr, cbReq);
            else
                rc = VERR_INVALID_HANDLE;
        }
        else if (uReq == VBGL_IOCTL_IDC_CONNECT)
        {
            rc = VGDrvCommonCreateKernelSession(&g_DevExt, &pSession);
            if (RT_SUCCESS(rc))
            {
                rc = VGDrvCommonIoCtl(uReq, &g_DevExt, pSession, pReqHdr, cbReq);
                if (RT_FAILURE(rc))
                    VGDrvCommonCloseSession(&g_DevExt, pSession);
            }
        }
        else
            rc = VERR_INVALID_HANDLE;
    }
    else
        rc = VERR_INVALID_POINTER;
    return rc;
}


void VGDrvNativeISRMousePollEvent(PVPOXGUESTDEVEXT pDevExt)
{
    NOREF(pDevExt);
}


bool VGDrvNativeProcessOption(PVPOXGUESTDEVEXT pDevExt, const char *pszName, const char *pszValue)
{
    RT_NOREF(pDevExt); RT_NOREF(pszName); RT_NOREF(pszValue);
    return false;
}


/**
 * Callback for blah blah blah.
 *
 * @todo move to IPRT.
 */
static IOReturn vgdrvDarwinSleepHandler(void *pvTarget, void *pvRefCon, UInt32 uMessageType,
                                        IOService *pProvider, void *pvMsgArg, vm_size_t cbMsgArg)
{
    RT_NOREF(pvTarget, pProvider, pvMsgArg, cbMsgArg);
    LogFlow(("VPoxGuest: Got sleep/wake notice. Message type was %x\n", uMessageType));

    if (uMessageType == kIOMessageSystemWillSleep)
        RTPowerSignalEvent(RTPOWEREVENT_SUSPEND);
    else if (uMessageType == kIOMessageSystemHasPoweredOn)
        RTPowerSignalEvent(RTPOWEREVENT_RESUME);

    acknowledgeSleepWakeNotification(pvRefCon);

    return 0;
}


/**
 * Converts an IPRT error code to a darwin error code.
 *
 * @returns corresponding darwin error code.
 * @param   rc      IPRT status code.
 */
static int vgdrvDarwinErr2DarwinErr(int rc)
{
    switch (rc)
    {
        case VINF_SUCCESS:              return 0;
        case VERR_GENERAL_FAILURE:      return EACCES;
        case VERR_INVALID_PARAMETER:    return EINVAL;
        case VERR_INVALID_MAGIC:        return EILSEQ;
        case VERR_INVALID_HANDLE:       return ENXIO;
        case VERR_INVALID_POINTER:      return EFAULT;
        case VERR_LOCK_FAILED:          return ENOLCK;
        case VERR_ALREADY_LOADED:       return EEXIST;
        case VERR_PERMISSION_DENIED:    return EPERM;
        case VERR_VERSION_MISMATCH:     return ENOSYS;
    }

    return EPERM;
}


/*
 *
 * org_virtualpox_VPoxGuest
 *
 * - IOService diff resync -
 * - IOService diff resync -
 * - IOService diff resync -
 *
 */


/**
 * Initialize the object.
 */
bool org_virtualpox_VPoxGuest::init(OSDictionary *pDictionary)
{
    LogFlow(("IOService::init([%p], %p)\n", this, pDictionary));
    if (IOService::init(pDictionary))
    {
        /* init members. */
        return true;
    }
    return false;
}


/**
 * Free the object.
 */
void org_virtualpox_VPoxGuest::free(void)
{
    RTLogBackdoorPrintf("IOService::free([%p])\n", this); /* might go sideways if we use LogFlow() here. weird. */
    IOService::free();
}


/**
 * Check if it's ok to start this service.
 * It's always ok by us, so it's up to IOService to decide really.
 */
IOService *org_virtualpox_VPoxGuest::probe(IOService *pProvider, SInt32 *pi32Score)
{
    LogFlow(("IOService::probe([%p])\n", this));
    IOService *pRet = IOService::probe(pProvider, pi32Score);
    LogFlow(("IOService::probe([%p]) returns %p *pi32Score=%d\n", this, pRet, pi32Score ? *pi32Score : -1));
    return pRet;
}


/**
 * Start this service.
 */
bool org_virtualpox_VPoxGuest::start(IOService *pProvider)
{
    LogFlow(("IOService::start([%p])\n", this));

    /*
     * Low level initialization / device initialization should be performed only once.
     */
    if (ASMAtomicCmpXchgBool(&g_fInstantiated, true, false))
    {
        /*
         * Make sure it's a PCI device.
         */
        m_pIOPCIDevice = OSDynamicCast(IOPCIDevice, pProvider);
        if (m_pIOPCIDevice)
        {
            /*
             * Call parent.
             */
            if (IOService::start(pProvider))
            {
                /*
                 * Is it the VMM device?
                 */
                if (isVmmDev(m_pIOPCIDevice))
                {
                    /*
                     * Enable I/O port and memory regions on the device.
                     */
                    m_pIOPCIDevice->setMemoryEnable(true);
                    m_pIOPCIDevice->setIOEnable(true);

                    /*
                     * Region #0: I/O ports. Mandatory.
                     */
                    IOMemoryDescriptor *pMem = m_pIOPCIDevice->getDeviceMemoryWithIndex(0);
                    if (pMem)
                    {
                        IOPhysicalAddress IOPortBasePhys = pMem->getPhysicalAddress();
                        if ((IOPortBasePhys >> 16) == 0)
                        {
                            RTIOPORT IOPortBase = (RTIOPORT)IOPortBasePhys;
                            void    *pvMMIOBase = NULL;
                            uint32_t cbMMIO     = 0;

                            /*
                             * Region #1: Shared Memory.  Technically optional.
                             */
                            m_pMap = m_pIOPCIDevice->mapDeviceMemoryWithIndex(1);
                            if (m_pMap)
                            {
                                pvMMIOBase = (void *)m_pMap->getVirtualAddress();
                                cbMMIO     = m_pMap->getLength();
                            }

                            /*
                             * Initialize the device extension.
                             */
                            int rc = VGDrvCommonInitDevExt(&g_DevExt, IOPortBase, pvMMIOBase, cbMMIO,
                                                           ARCH_BITS == 64 ? VPOXOSTYPE_MacOS_x64 : VPOXOSTYPE_MacOS, 0);
                            if (RT_SUCCESS(rc))
                            {
                                /*
                                 * Register the device nodes and enable interrupts.
                                 */
                                rc = vgdrvDarwinCharDevInit();
                                if (rc == KMOD_RETURN_SUCCESS)
                                {
                                    if (setupVmmDevInterrupts(pProvider))
                                    {
                                        /*
                                         * Read host configuration.
                                         */
                                        VGDrvCommonProcessOptionsFromHost(&g_DevExt);

                                        /*
                                         * Just register the service and we're done!
                                         */
                                        registerService();

                                        LogRel(("VPoxGuest: IOService started\n"));
                                        return true;
                                    }

                                    LogRel(("VPoxGuest: Failed to set up interrupts\n"));
                                    vgdrvDarwinCharDevRemove();
                                }
                                else
                                    LogRel(("VPoxGuest: Failed to initialize character devices (rc=%#x).\n", rc));

                                VGDrvCommonDeleteDevExt(&g_DevExt);
                            }
                            else
                                LogRel(("VPoxGuest: Failed to initialize common code (rc=%Rrc).\n", rc));

                            if (m_pMap)
                            {
                                m_pMap->release();
                                m_pMap = NULL;
                            }
                        }
                        else
                            LogRel(("VPoxGuest: Bad I/O port address: %#RX64\n", (uint64_t)IOPortBasePhys));
                    }
                    else
                        LogRel(("VPoxGuest: The device missing is the I/O port range (#0).\n"));
                }
                else
                    LogRel(("VPoxGuest: Not the VMMDev (%#x:%#x).\n",
                           m_pIOPCIDevice->configRead16(kIOPCIConfigVendorID), m_pIOPCIDevice->configRead16(kIOPCIConfigDeviceID)));

                IOService::stop(pProvider);
            }
        }
        else
            LogRel(("VPoxGuest: Provider is not an instance of IOPCIDevice.\n"));

        ASMAtomicXchgBool(&g_fInstantiated, false);
    }
    return false;
}


/**
 * Stop this service.
 */
void org_virtualpox_VPoxGuest::stop(IOService *pProvider)
{
#ifdef LOG_ENABLED
    RTLogBackdoorPrintf("org_virtualpox_VPoxGuest::stop([%p], %p)\n", this, pProvider); /* Being cautious here, no Log(). */
#endif
    AssertReturnVoid(ASMAtomicReadBool(&g_fInstantiated));

    /* Low level termination should be performed only once */
    if (!disableVmmDevInterrupts())
        printf("VPoxGuest: unable to unregister interrupt handler\n");

    vgdrvDarwinCharDevRemove();
    VGDrvCommonDeleteDevExt(&g_DevExt);

    if (m_pMap)
    {
        m_pMap->release();
        m_pMap = NULL;
    }

    IOService::stop(pProvider);

    ASMAtomicWriteBool(&g_fInstantiated, false);

    printf("VPoxGuest: IOService stopped\n");
    RTLogBackdoorPrintf("org_virtualpox_VPoxGuest::stop: returning\n"); /* Being cautious here, no Log(). */
}


/**
 * Termination request.
 *
 * @return  true if we're ok with shutting down now, false if we're not.
 * @param   fOptions        Flags.
 */
bool org_virtualpox_VPoxGuest::terminate(IOOptionBits fOptions)
{
#ifdef LOG_ENABLED
    RTLogBackdoorPrintf("org_virtualpox_VPoxGuest::terminate: reference_count=%d g_cSessions=%d (fOptions=%#x)\n",
                        KMOD_INFO_NAME.reference_count, ASMAtomicUoReadS32(&g_cSessions), fOptions); /* Being cautious here, no Log(). */
#endif

    bool fRc;
    if (   KMOD_INFO_NAME.reference_count != 0
        || ASMAtomicUoReadS32(&g_cSessions))
        fRc = false;
    else
        fRc = IOService::terminate(fOptions);

#ifdef LOG_ENABLED
    RTLogBackdoorPrintf("org_virtualpox_SupDrv::terminate: returns %d\n", fRc); /* Being cautious here, no Log(). */
#endif
    return fRc;
}


/**
 * Implementes a IOInterruptHandler, called by provider when an interrupt occurs.
 */
/*static*/ void org_virtualpox_VPoxGuest::vgdrvDarwinIrqHandler(OSObject *pTarget, void *pvRefCon, IOService *pNub, int iSrc)
{
#ifdef LOG_ENABLED
    RTLogBackdoorPrintf("vgdrvDarwinIrqHandler: %p %p %p %d\n", pTarget, pvRefCon, pNub, iSrc);
#endif
    RT_NOREF(pTarget, pvRefCon, pNub, iSrc);

    VGDrvCommonISR(&g_DevExt);
    /* There is in fact no way of indicating that this is our interrupt, other
       than making the device lower it.  So, the return code is ignored. */
}


/**
 * Sets up and enables interrupts on the device.
 *
 * Interrupts are handled directly, no messing around with workloops.  The
 * rational here is is that the main job of our interrupt handler is waking up
 * other threads currently sitting in HGCM calls, i.e. little more effort than
 * waking up the workloop thread.
 *
 * @returns success indicator.  Failures are fully logged.
 */
bool org_virtualpox_VPoxGuest::setupVmmDevInterrupts(IOService *pProvider)
{
    AssertReturn(pProvider, false);

    if (m_pInterruptProvider != pProvider)
    {
        pProvider->retain();
        if (m_pInterruptProvider)
            m_pInterruptProvider->release();
        m_pInterruptProvider = pProvider;
    }

    IOReturn rc = pProvider->registerInterrupt(0 /*intIndex*/, this, vgdrvDarwinIrqHandler, this);
    if (rc == kIOReturnSuccess)
    {
        rc = pProvider->enableInterrupt(0 /*intIndex*/);
        if (rc == kIOReturnSuccess)
            return true;

        LogRel(("VPoxGuest: Failed to enable interrupt: %#x\n", rc));
        m_pInterruptProvider->unregisterInterrupt(0 /*intIndex*/);
    }
    else
        LogRel(("VPoxGuest: Failed to register interrupt: %#x\n", rc));
    return false;
}


/**
 * Counterpart to setupVmmDevInterrupts().
 */
bool org_virtualpox_VPoxGuest::disableVmmDevInterrupts(void)
{
    if (m_pInterruptProvider)
    {
        IOReturn rc = m_pInterruptProvider->disableInterrupt(0 /*intIndex*/);
        AssertMsg(rc == kIOReturnSuccess, ("%#x\n", rc));
        rc = m_pInterruptProvider->unregisterInterrupt(0 /*intIndex*/);
        AssertMsg(rc == kIOReturnSuccess, ("%#x\n", rc));
        RT_NOREF_PV(rc);

        m_pInterruptProvider->release();
        m_pInterruptProvider = NULL;
    }

    return true;
}


/**
 * Checks if it's the VMM device.
 *
 * @returns true if it is, false if it isn't.
 * @param   pIOPCIDevice    The PCI device we think might be the VMM device.
 */
bool org_virtualpox_VPoxGuest::isVmmDev(IOPCIDevice *pIOPCIDevice)
{
    if (pIOPCIDevice)
    {
        uint16_t idVendor = m_pIOPCIDevice->configRead16(kIOPCIConfigVendorID);
        if (idVendor == VMMDEV_VENDORID)
        {
            uint16_t idDevice = m_pIOPCIDevice->configRead16(kIOPCIConfigDeviceID);
            if (idDevice == VMMDEV_DEVICEID)
                return true;
        }
    }
    return false;
}



/*
 *
 * org_virtualpox_VPoxGuestClient
 *
 */


/**
 * Initializer called when the client opens the service.
 */
bool org_virtualpox_VPoxGuestClient::initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type)
{
    LogFlow(("org_virtualpox_VPoxGuestClient::initWithTask([%p], %#x, %p, %#x) (cur pid=%d proc=%p)\n",
             this, OwningTask, pvSecurityId, u32Type, RTProcSelf(), RTR0ProcHandleSelf()));
    AssertMsg((RTR0PROCESS)OwningTask == RTR0ProcHandleSelf(), ("%p %p\n", OwningTask, RTR0ProcHandleSelf()));

    if (!OwningTask)
        return false;

    if (u32Type != VPOXGUEST_DARWIN_IOSERVICE_COOKIE)
    {
        VPOX_RETRIEVE_CUR_PROC_NAME(szProcName);
        LogRelMax(10, ("org_virtualpox_VPoxGuestClient::initWithTask: Bad cookie %#x (%s)\n", u32Type, szProcName));
        return false;
    }

    if (IOUserClient::initWithTask(OwningTask, pvSecurityId , u32Type))
    {
        /*
         * In theory we have to call task_reference() to make sure that the task is
         * valid during the lifetime of this object. The pointer is only used to check
         * for the context this object is called in though and never dereferenced
         * or passed to anything which might, so we just skip this step.
         */
        m_Task = OwningTask;
        m_pSession = NULL;
        m_pProvider = NULL;
        return true;
    }
    return false;
}


/**
 * Start the client service.
 */
bool org_virtualpox_VPoxGuestClient::start(IOService *pProvider)
{
    LogFlow(("org_virtualpox_VPoxGuestClient::start([%p], %p) (cur pid=%d proc=%p)\n",
             this, pProvider, RTProcSelf(), RTR0ProcHandleSelf() ));
    AssertMsgReturn((RTR0PROCESS)m_Task == RTR0ProcHandleSelf(),
                    ("%p %p\n", m_Task, RTR0ProcHandleSelf()),
                    false);

    if (IOUserClient::start(pProvider))
    {
        m_pProvider = OSDynamicCast(org_virtualpox_VPoxGuest, pProvider);
        if (m_pProvider)
        {
            Assert(!m_pSession);

            /*
             * Create a new session.
             * Note! We complete the requestor stuff in the open method.
             */
            int rc = VGDrvCommonCreateUserSession(&g_DevExt, VMMDEV_REQUESTOR_USERMODE, &m_pSession);
            if (RT_SUCCESS(rc))
            {
                m_pSession->fOpened = false;
                /* The Uid, Gid and fUnrestricted fields are set on open. */

                /*
                 * Insert it into the hash table, checking that there isn't
                 * already one for this process first. (One session per proc!)
                 */
                unsigned iHash = SESSION_HASH(m_pSession->Process);
                RTSpinlockAcquire(g_Spinlock);

                PVPOXGUESTSESSION pCur = g_apSessionHashTab[iHash];
                while (pCur && pCur->Process != m_pSession->Process)
                    pCur = pCur->pNextHash;
                if (!pCur)
                {
                    m_pSession->pNextHash = g_apSessionHashTab[iHash];
                    g_apSessionHashTab[iHash] = m_pSession;
                    m_pSession->pvVPoxGuestClient = this;
                    ASMAtomicIncS32(&g_cSessions);
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_ALREADY_LOADED;

                RTSpinlockRelease(g_Spinlock);
                if (RT_SUCCESS(rc))
                {
                    Log(("org_virtualpox_VPoxGuestClient::start: created session %p for pid %d\n", m_pSession, (int)RTProcSelf()));
                    return true;
                }

                LogFlow(("org_virtualpox_VPoxGuestClient::start: already got a session for this process (%p)\n", pCur));
                VGDrvCommonCloseSession(&g_DevExt, m_pSession);  //supdrvSessionRelease(m_pSession);
            }

            m_pSession = NULL;
            LogFlow(("org_virtualpox_VPoxGuestClient::start: rc=%Rrc from supdrvCreateSession\n", rc));
        }
        else
            LogFlow(("org_virtualpox_VPoxGuestClient::start: %p isn't org_virtualpox_VPoxGuest\n", pProvider));
    }
    return false;
}


/**
 * Common worker for clientClose and VPoxDrvDarwinClose.
 */
/* static */ void org_virtualpox_VPoxGuestClient::sessionClose(RTPROCESS Process)
{
    /*
     * Find the session and remove it from the hash table.
     *
     * Note! Only one session per process. (Both start() and
     * vgdrvDarwinOpen makes sure this is so.)
     */
    const unsigned  iHash = SESSION_HASH(Process);
    RTSpinlockAcquire(g_Spinlock);
    PVPOXGUESTSESSION  pSession = g_apSessionHashTab[iHash];
    if (pSession)
    {
        if (pSession->Process == Process)
        {
            g_apSessionHashTab[iHash] = pSession->pNextHash;
            pSession->pNextHash = NULL;
            ASMAtomicDecS32(&g_cSessions);
        }
        else
        {
            PVPOXGUESTSESSION pPrev = pSession;
            pSession = pSession->pNextHash;
            while (pSession)
            {
                if (pSession->Process == Process)
                {
                    pPrev->pNextHash = pSession->pNextHash;
                    pSession->pNextHash = NULL;
                    ASMAtomicDecS32(&g_cSessions);
                    break;
                }

                /* next */
                pPrev = pSession;
                pSession = pSession->pNextHash;
            }
        }
    }
    RTSpinlockRelease(g_Spinlock);
    if (!pSession)
    {
        Log(("VPoxGuestClient::sessionClose: pSession == NULL, pid=%d; freed already?\n", (int)Process));
        return;
    }

    /*
     * Remove it from the client object.
     */
    org_virtualpox_VPoxGuestClient *pThis = (org_virtualpox_VPoxGuestClient *)pSession->pvVPoxGuestClient;
    pSession->pvVPoxGuestClient = NULL;
    if (pThis)
    {
        Assert(pThis->m_pSession == pSession);
        pThis->m_pSession = NULL;
    }

    /*
     * Close the session.
     */
    VGDrvCommonCloseSession(&g_DevExt, pSession); // supdrvSessionRelease(m_pSession);
}


/**
 * Client exits normally.
 */
IOReturn org_virtualpox_VPoxGuestClient::clientClose(void)
{
    LogFlow(("org_virtualpox_VPoxGuestClient::clientClose([%p]) (cur pid=%d proc=%p)\n", this, RTProcSelf(), RTR0ProcHandleSelf()));
    AssertMsg((RTR0PROCESS)m_Task == RTR0ProcHandleSelf(), ("%p %p\n", m_Task, RTR0ProcHandleSelf()));

    /*
     * Clean up the session if it's still around.
     *
     * We cannot rely 100% on close, and in the case of a dead client
     * we'll end up hanging inside vm_map_remove() if we postpone it.
     */
    if (m_pSession)
    {
        sessionClose(RTProcSelf());
        Assert(!m_pSession);
    }

    m_pProvider = NULL;
    terminate();

    return kIOReturnSuccess;
}


/**
 * The client exits abnormally / forgets to do cleanups. (logging)
 */
IOReturn org_virtualpox_VPoxGuestClient::clientDied(void)
{
    LogFlow(("IOService::clientDied([%p]) m_Task=%p R0Process=%p Process=%d\n", this, m_Task, RTR0ProcHandleSelf(), RTProcSelf()));

    /* IOUserClient::clientDied() calls clientClose, so we'll just do the work there. */
    return IOUserClient::clientDied();
}


/**
 * Terminate the service (initiate the destruction). (logging)
 */
bool org_virtualpox_VPoxGuestClient::terminate(IOOptionBits fOptions)
{
    LogFlow(("IOService::terminate([%p], %#x)\n", this, fOptions));
    return IOUserClient::terminate(fOptions);
}


/**
 * The final stage of the client service destruction. (logging)
 */
bool org_virtualpox_VPoxGuestClient::finalize(IOOptionBits fOptions)
{
    LogFlow(("IOService::finalize([%p], %#x)\n", this, fOptions));
    return IOUserClient::finalize(fOptions);
}


/**
 * Stop the client service. (logging)
 */
void org_virtualpox_VPoxGuestClient::stop(IOService *pProvider)
{
    LogFlow(("IOService::stop([%p])\n", this));
    IOUserClient::stop(pProvider);
}

