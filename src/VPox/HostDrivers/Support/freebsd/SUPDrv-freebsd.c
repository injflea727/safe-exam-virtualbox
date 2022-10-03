/* $Id: SUPDrv-freebsd.c $ */
/** @file
 * VPoxDrv - The VirtualPox Support Driver - FreeBSD specifics.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
/* Deal with conflicts first. */
#include <sys/param.h>
#undef PVM
#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/uio.h>

#include "../SUPDrvInternal.h"
#include <VPox/version.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/spinlock.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <VPox/log.h>
#include <iprt/alloc.h>
#include <iprt/err.h>
#include <iprt/asm.h>

#ifdef VPOX_WITH_HARDENING
# define VPOXDRV_PERM 0600
#else
# define VPOXDRV_PERM 0666
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int VPoxDrvFreeBSDModuleEvent(struct module *pMod, int enmEventType, void *pvArg);
static int VPoxDrvFreeBSDLoad(void);
static int VPoxDrvFreeBSDUnload(void);

static d_open_t     VPoxDrvFreeBSDOpenUsr;
static d_open_t     VPoxDrvFreeBSDOpenSys;
static void         vpoxdrvFreeBSDDtr(void *pvData);
static d_ioctl_t    VPoxDrvFreeBSDIOCtl;
static int          VPoxDrvFreeBSDIOCtlSlow(PSUPDRVSESSION pSession, u_long ulCmd, caddr_t pvData, struct thread *pTd);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Module info structure used by the kernel.
 */
static moduledata_t         g_VPoxDrvFreeBSDModule =
{
    "vpoxdrv",
    VPoxDrvFreeBSDModuleEvent,
    NULL
};

/** Declare the module as a pseudo device. */
DECLARE_MODULE(vpoxdrv,     g_VPoxDrvFreeBSDModule, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(vpoxdrv, 1);

/**
 * The /dev/vpoxdrv character device entry points.
 */
static struct cdevsw        g_VPoxDrvFreeBSDChrDevSwSys =
{
    .d_version =        D_VERSION,
    .d_open =           VPoxDrvFreeBSDOpenSys,
    .d_ioctl =          VPoxDrvFreeBSDIOCtl,
    .d_name =           "vpoxdrv"
};
/** The /dev/vpoxdrv character device. */
static struct cdev         *g_pVPoxDrvFreeBSDChrDevSys;

/**
 * The /dev/vpoxdrvu character device entry points.
 */
static struct cdevsw        g_VPoxDrvFreeBSDChrDevSwUsr =
{
    .d_version =        D_VERSION,
    .d_open =           VPoxDrvFreeBSDOpenUsr,
    .d_ioctl =          VPoxDrvFreeBSDIOCtl,
    .d_name =           "vpoxdrvu"
};
/** The /dev/vpoxdrvu character device. */
static struct cdev         *g_pVPoxDrvFreeBSDChrDevUsr;

/** Reference counter. */
static volatile uint32_t    g_cUsers;

/** The device extention. */
static SUPDRVDEVEXT         g_VPoxDrvFreeBSDDevExt;

/**
 * Module event handler.
 *
 * @param   pMod            The module structure.
 * @param   enmEventType    The event type (modeventtype_t).
 * @param   pvArg           Module argument. NULL.
 *
 * @return  0 on success, errno.h status code on failure.
 */
static int VPoxDrvFreeBSDModuleEvent(struct module *pMod, int enmEventType, void *pvArg)
{
    int rc;
    switch (enmEventType)
    {
        case MOD_LOAD:
            rc = VPoxDrvFreeBSDLoad();
            break;

        case MOD_UNLOAD:
            mtx_unlock(&Giant);
            rc = VPoxDrvFreeBSDUnload();
            mtx_lock(&Giant);
            break;

        case MOD_SHUTDOWN:
        case MOD_QUIESCE:
        default:
            return EOPNOTSUPP;
    }

    if (RT_SUCCESS(rc))
        return 0;
    return RTErrConvertToErrno(rc);
}


static int VPoxDrvFreeBSDLoad(void)
{
    g_cUsers = 0;

    /*
     * Initialize the runtime.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VPoxDrvFreeBSDLoad:\n"));

        /*
         * Initialize the device extension.
         */
        rc = supdrvInitDevExt(&g_VPoxDrvFreeBSDDevExt, sizeof(SUPDRVSESSION));
        if (RT_SUCCESS(rc))
        {
            /*
             * Configure character devices. Add symbolic links for compatibility.
             */
            g_pVPoxDrvFreeBSDChrDevSys = make_dev(&g_VPoxDrvFreeBSDChrDevSwSys, 0, UID_ROOT, GID_WHEEL, VPOXDRV_PERM, "vpoxdrv");
            g_pVPoxDrvFreeBSDChrDevUsr = make_dev(&g_VPoxDrvFreeBSDChrDevSwUsr, 1, UID_ROOT, GID_WHEEL, 0666,         "vpoxdrvu");
            return VINF_SUCCESS;
        }

        printf("vpoxdrv: supdrvInitDevExt failed, rc=%d\n", rc);
        RTR0Term();
    }
    else
        printf("vpoxdrv: RTR0Init failed, rc=%d\n", rc);
    return rc;
}

static int VPoxDrvFreeBSDUnload(void)
{
    Log(("VPoxDrvFreeBSDUnload:\n"));

    if (g_cUsers > 0)
        return VERR_RESOURCE_BUSY;

    /*
     * Reserve what we did in VPoxDrvFreeBSDInit.
     */
    destroy_dev(g_pVPoxDrvFreeBSDChrDevUsr);
    destroy_dev(g_pVPoxDrvFreeBSDChrDevSys);

    supdrvDeleteDevExt(&g_VPoxDrvFreeBSDDevExt);

    RTR0TermForced();

    memset(&g_VPoxDrvFreeBSDDevExt, 0, sizeof(g_VPoxDrvFreeBSDDevExt));
    return VINF_SUCCESS;
}


/**
 *
 * @returns 0 on success, errno on failure.
 *          EBUSY if the device is used by someone else.
 * @param   pDev            The device node.
 * @param   fOpen           The open flags.
 * @param   iDevType        Some device type thing we don't use.
 * @param   pTd             The thread.
 * @param   fUnrestricted   Set if opening /dev/vpoxdrv, clear if /dev/vpoxdrvu.
 */
static int vpoxdrvFreeBSDOpenCommon(struct cdev *pDev, int fOpen, int iDevType, struct thread *pTd, bool fUnrestricted)
{
    PSUPDRVSESSION pSession;
    int rc;

    /*
     * Let's be a bit picky about the flags...
     */
    if (fOpen != (FREAD | FWRITE /*=O_RDWR*/))
    {
        Log(("VPoxDrvFreeBSDOpen: fOpen=%#x expected %#x\n", fOpen, O_RDWR));
        return EINVAL;
    }

    /*
     * Create a new session.
     */
    rc = supdrvCreateSession(&g_VPoxDrvFreeBSDDevExt, true /* fUser */, fUnrestricted, &pSession);
    if (RT_SUCCESS(rc))
    {
        /** @todo get (r)uid and (r)gid.
        pSession->Uid = stuff;
        pSession->Gid = stuff; */
        rc = devfs_set_cdevpriv(pSession, vpoxdrvFreeBSDDtr); Assert(rc == 0);
        Log(("VPoxDrvFreeBSDOpen: pSession=%p\n", pSession));
        ASMAtomicIncU32(&g_cUsers);
        return 0;
    }

    return RTErrConvertToErrno(rc);
}


/** For vpoxdrv. */
static int VPoxDrvFreeBSDOpenSys(struct cdev *pDev, int fOpen, int iDevType, struct thread *pTd)
{
    return vpoxdrvFreeBSDOpenCommon(pDev, fOpen, iDevType, pTd, true);
}


/** For vpoxdrvu. */
static int VPoxDrvFreeBSDOpenUsr(struct cdev *pDev, int fOpen, int iDevType, struct thread *pTd)
{
    return vpoxdrvFreeBSDOpenCommon(pDev, fOpen, iDevType, pTd, false);
}


/**
 * Close a file device previously opened by VPoxDrvFreeBSDOpen.
 *
 * @returns 0 on success.
 * @param   pvData      The session being closed.
 */
static void vpoxdrvFreeBSDDtr(void *pvData)
{
    PSUPDRVSESSION pSession = pvData;
    Log(("vpoxdrvFreeBSDDtr: pSession=%p\n", pSession));

    /*
     * Close the session.
     */
    supdrvSessionRelease(pSession);
    ASMAtomicDecU32(&g_cUsers);
}


/**
 * I/O control request.
 *
 * @returns depends...
 * @param   pDev        The device.
 * @param   ulCmd       The command.
 * @param   pvData      Pointer to the data.
 * @param   fFile       The file descriptor flags.
 * @param   pTd         The calling thread.
 */
static int VPoxDrvFreeBSDIOCtl(struct cdev *pDev, u_long ulCmd, caddr_t pvData, int fFile, struct thread *pTd)
{
    PSUPDRVSESSION pSession;
    devfs_get_cdevpriv((void **)&pSession);

    /*
     * Deal with the fast ioctl path first.
     */
    AssertCompile((SUP_IOCTL_FAST_DO_FIRST & 0xff) == (SUP_IOCTL_FLAG | 64));
    if (   (uintptr_t)(ulCmd - SUP_IOCTL_FAST_DO_FIRST) < (uintptr_t)32
        && pSession->fUnrestricted)
        return supdrvIOCtlFast(ulCmd - SUP_IOCTL_FAST_DO_FIRST, *(uint32_t *)pvData, &g_VPoxDrvFreeBSDDevExt, pSession);

    return VPoxDrvFreeBSDIOCtlSlow(pSession, ulCmd, pvData, pTd);
}


/**
 * Deal with the 'slow' I/O control requests.
 *
 * @returns 0 on success, appropriate errno on failure.
 * @param   pSession    The session.
 * @param   ulCmd       The command.
 * @param   pvData      The request data.
 * @param   pTd         The calling thread.
 */
static int VPoxDrvFreeBSDIOCtlSlow(PSUPDRVSESSION pSession, u_long ulCmd, caddr_t pvData, struct thread *pTd)
{
    PSUPREQHDR  pHdr;
    uint32_t    cbReq = IOCPARM_LEN(ulCmd);
    void       *pvUser = NULL;

    /*
     * Buffered request?
     */
    if ((IOC_DIRMASK & ulCmd) == IOC_INOUT)
    {
        pHdr = (PSUPREQHDR)pvData;
        if (RT_UNLIKELY(cbReq < sizeof(*pHdr)))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: cbReq=%#x < %#x; ulCmd=%#lx\n", cbReq, (int)sizeof(*pHdr), ulCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY((pHdr->fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: bad magic fFlags=%#x; ulCmd=%#lx\n", pHdr->fFlags, ulCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY(    RT_MAX(pHdr->cbIn, pHdr->cbOut) != cbReq
                        ||  pHdr->cbIn < sizeof(*pHdr)
                        ||  pHdr->cbOut < sizeof(*pHdr)))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: max(%#x,%#x) != %#x; ulCmd=%#lx\n", pHdr->cbIn, pHdr->cbOut, cbReq, ulCmd));
            return EINVAL;
        }
    }
    /*
     * Big unbuffered request?
     */
    else if ((IOC_DIRMASK & ulCmd) == IOC_VOID && !cbReq)
    {
        /*
         * Read the header, validate it and figure out how much that needs to be buffered.
         */
        SUPREQHDR Hdr;
        pvUser = *(void **)pvData;
        int rc = copyin(pvUser, &Hdr, sizeof(Hdr));
        if (RT_UNLIKELY(rc))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: copyin(%p,Hdr,) -> %#x; ulCmd=%#lx\n", pvUser, rc, ulCmd));
            return rc;
        }
        if (RT_UNLIKELY((Hdr.fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: bad magic fFlags=%#x; ulCmd=%#lx\n", Hdr.fFlags, ulCmd));
            return EINVAL;
        }
        cbReq = RT_MAX(Hdr.cbIn, Hdr.cbOut);
        if (RT_UNLIKELY(    Hdr.cbIn < sizeof(Hdr)
                        ||  Hdr.cbOut < sizeof(Hdr)
                        ||  cbReq > _1M*16))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: max(%#x,%#x); ulCmd=%#lx\n", Hdr.cbIn, Hdr.cbOut, ulCmd));
            return EINVAL;
        }

        /*
         * Allocate buffer and copy in the data.
         */
        pHdr = (PSUPREQHDR)RTMemTmpAlloc(cbReq);
        if (RT_UNLIKELY(!pHdr))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: failed to allocate buffer of %d bytes; ulCmd=%#lx\n", cbReq, ulCmd));
            return ENOMEM;
        }
        rc = copyin(pvUser, pHdr, Hdr.cbIn);
        if (RT_UNLIKELY(rc))
        {
            OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: copyin(%p,%p,%#x) -> %#x; ulCmd=%#lx\n",
                        pvUser, pHdr, Hdr.cbIn, rc, ulCmd));
            RTMemTmpFree(pHdr);
            return rc;
        }
        if (Hdr.cbIn < cbReq)
            RT_BZERO((uint8_t *)pHdr + Hdr.cbIn, cbReq - Hdr.cbIn);
    }
    else
    {
        Log(("VPoxDrvFreeBSDIOCtlSlow: huh? cbReq=%#x ulCmd=%#lx\n", cbReq, ulCmd));
        return EINVAL;
    }

    /*
     * Process the IOCtl.
     */
    int rc = supdrvIOCtl(ulCmd, &g_VPoxDrvFreeBSDDevExt, pSession, pHdr, cbReq);
    if (RT_LIKELY(!rc))
    {
        /*
         * If unbuffered, copy back the result before returning.
         */
        if (pvUser)
        {
            uint32_t cbOut = pHdr->cbOut;
            if (cbOut > cbReq)
            {
                OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: too much output! %#x > %#x; uCmd=%#lx!\n", cbOut, cbReq, ulCmd));
                cbOut = cbReq;
            }
            rc = copyout(pHdr, pvUser, cbOut);
            if (RT_UNLIKELY(rc))
                OSDBGPRINT(("VPoxDrvFreeBSDIOCtlSlow: copyout(%p,%p,%#x) -> %d; uCmd=%#lx!\n", pHdr, pvUser, cbOut, rc, ulCmd));

            Log(("VPoxDrvFreeBSDIOCtlSlow: returns %d / %d ulCmd=%lx\n", 0, pHdr->rc, ulCmd));

            /* cleanup */
            RTMemTmpFree(pHdr);
        }
    }
    else
    {
        /*
         * The request failed, just clean up.
         */
        if (pvUser)
            RTMemTmpFree(pHdr);

        Log(("VPoxDrvFreeBSDIOCtlSlow: ulCmd=%lx pData=%p failed, rc=%d\n", ulCmd, pvData, rc));
        rc = EINVAL;
    }

    return rc;
}


/**
 * The SUPDRV IDC entry point.
 *
 * @returns VPox status code, see supdrvIDC.
 * @param   uReq        The request code.
 * @param   pReq        The request.
 */
int VPOXCALL SUPDrvFreeBSDIDC(uint32_t uReq, PSUPDRVIDCREQHDR pReq)
{
    PSUPDRVSESSION  pSession;

    /*
     * Some quick validations.
     */
    if (RT_UNLIKELY(!VALID_PTR(pReq)))
        return VERR_INVALID_POINTER;

    pSession = pReq->pSession;
    if (pSession)
    {
        if (RT_UNLIKELY(!VALID_PTR(pReq->pSession)))
            return VERR_INVALID_PARAMETER;
        if (RT_UNLIKELY(pSession->pDevExt != &g_VPoxDrvFreeBSDDevExt))
            return VERR_INVALID_PARAMETER;
    }
    else if (RT_UNLIKELY(uReq != SUPDRV_IDC_REQ_CONNECT))
        return VERR_INVALID_PARAMETER;

    /*
     * Do the job.
     */
    return supdrvIDC(uReq, &g_VPoxDrvFreeBSDDevExt, pSession, pReq);
}


void VPOXCALL supdrvOSCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    NOREF(pDevExt);
    NOREF(pSession);
}


void VPOXCALL supdrvOSSessionHashTabInserted(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, void *pvUser)
{
    NOREF(pDevExt); NOREF(pSession); NOREF(pvUser);
}


void VPOXCALL supdrvOSSessionHashTabRemoved(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, void *pvUser)
{
    NOREF(pDevExt); NOREF(pSession); NOREF(pvUser);
}


void VPOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
{
    NOREF(pObj);
    NOREF(pSession);
}


bool VPOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}


bool VPOXCALL  supdrvOSGetForcedAsyncTscMode(PSUPDRVDEVEXT pDevExt)
{
    return false;
}


bool VPOXCALL  supdrvOSAreCpusOfflinedOnSuspend(void)
{
    /** @todo verify this. */
    return false;
}


bool VPOXCALL  supdrvOSAreTscDeltasInSync(void)
{
    return false;
}


int  VPOXCALL   supdrvOSLdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const char *pszFilename)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pszFilename);
    return VERR_NOT_SUPPORTED;
}


int  VPOXCALL   supdrvOSLdrValidatePointer(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, void *pv,
                                           const uint8_t *pbImageBits, const char *pszSymbol)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pv); NOREF(pbImageBits); NOREF(pszSymbol);
    return VERR_NOT_SUPPORTED;
}


int  VPOXCALL   supdrvOSLdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const uint8_t *pbImageBits, PSUPLDRLOAD pReq)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pbImageBits); NOREF(pReq);
    return VERR_NOT_SUPPORTED;
}


void VPOXCALL   supdrvOSLdrUnload(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    NOREF(pDevExt); NOREF(pImage);
}


void VPOXCALL   supdrvOSLdrNotifyOpened(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const char *pszFilename)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pszFilename);
}


void VPOXCALL   supdrvOSLdrNotifyUnloaded(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    NOREF(pDevExt); NOREF(pImage);
}


int  VPOXCALL   supdrvOSLdrQuerySymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage,
                                       const char *pszSymbol, size_t cchSymbol, void **ppvSymbol)
{
    RT_NOREF(pDevExt, pImage, pszSymbol, cchSymbol, ppvSymbol);
    return VERR_WRONG_ORDER;
}


#ifdef SUPDRV_WITH_MSR_PROBER

int VPOXCALL    supdrvOSMsrProberRead(uint32_t uMsr, RTCPUID idCpu, uint64_t *puValue)
{
    NOREF(uMsr); NOREF(idCpu); NOREF(puValue);
    return VERR_NOT_SUPPORTED;
}


int VPOXCALL    supdrvOSMsrProberWrite(uint32_t uMsr, RTCPUID idCpu, uint64_t uValue)
{
    NOREF(uMsr); NOREF(idCpu); NOREF(uValue);
    return VERR_NOT_SUPPORTED;
}


int VPOXCALL    supdrvOSMsrProberModify(RTCPUID idCpu, PSUPMSRPROBER pReq)
{
    NOREF(idCpu); NOREF(pReq);
    return VERR_NOT_SUPPORTED;
}

#endif /* SUPDRV_WITH_MSR_PROBER */


SUPR0DECL(int) SUPR0PrintfV(const char *pszFormat, va_list va)
{
    char szMsg[256];
    RTStrPrintfV(szMsg, sizeof(szMsg), pszFormat, va);
    szMsg[sizeof(szMsg) - 1] = '\0';

    printf("%s", szMsg);
    return 0;
}


SUPR0DECL(uint32_t) SUPR0GetKernelFeatures(void)
{
    return 0;
}


SUPR0DECL(bool) SUPR0FpuBegin(bool fCtxHook)
{
    RT_NOREF(fCtxHook);
    return false;
}


SUPR0DECL(void) SUPR0FpuEnd(bool fCtxHook)
{
    RT_NOREF(fCtxHook);
}

