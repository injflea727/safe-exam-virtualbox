/* $Id: VPoxPci.c $ */
/** @file
 * VPoxPci - PCI card passthrough support (Host), Common Code.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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

/** @page pg_rawpci     VPoxPci - host PCI support
 *
 * This is a kernel module that works as host proxy between guest and
 * PCI hardware.
 *
 */

#define LOG_GROUP LOG_GROUP_DEV_PCI_RAW
#include <VPox/log.h>
#include <VPox/err.h>
#include <VPox/sup.h>
#include <VPox/version.h>

#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/uuid.h>
#include <iprt/asm.h>
#include <iprt/mem.h>

#include "VPoxPciInternal.h"


#define DEVPORT_2_VPOXRAWPCIINS(pPort) \
    ( (PVPOXRAWPCIINS)((uint8_t *)pPort - RT_OFFSETOF(VPOXRAWPCIINS, DevPort)) )


/**
 * Implements the SUPDRV component factor interface query method.
 *
 * @returns Pointer to an interface. NULL if not supported.
 *
 * @param   pSupDrvFactory      Pointer to the component factory registration structure.
 * @param   pSession            The session - unused.
 * @param   pszInterfaceUuid    The factory interface id.
 */
static DECLCALLBACK(void *) vpoxPciQueryFactoryInterface(PCSUPDRVFACTORY pSupDrvFactory, PSUPDRVSESSION pSession, const char *pszInterfaceUuid)
{
    PVPOXRAWPCIGLOBALS pGlobals = (PVPOXRAWPCIGLOBALS)((uint8_t *)pSupDrvFactory - RT_OFFSETOF(VPOXRAWPCIGLOBALS, SupDrvFactory));

    /*
     * Convert the UUID strings and compare them.
     */
    RTUUID UuidReq;
    int rc = RTUuidFromStr(&UuidReq, pszInterfaceUuid);
    if (RT_SUCCESS(rc))
    {
        if (!RTUuidCompareStr(&UuidReq, RAWPCIFACTORY_UUID_STR))
        {
            ASMAtomicIncS32(&pGlobals->cFactoryRefs);
            return &pGlobals->RawPciFactory;
        }
    }
    else
        Log(("VPoxRawPci: rc=%Rrc, uuid=%s\n", rc, pszInterfaceUuid));

    return NULL;
}
DECLINLINE(int) vpoxPciDevLock(PVPOXRAWPCIINS pThis)
{
#ifdef VPOX_WITH_SHARED_PCI_INTERRUPTS
    RTSpinlockAcquire(pThis->hSpinlock);
    return VINF_SUCCESS;
#else
    int rc = RTSemFastMutexRequest(pThis->hFastMtx);

    AssertRC(rc);
    return rc;
#endif
}

DECLINLINE(void) vpoxPciDevUnlock(PVPOXRAWPCIINS pThis)
{
#ifdef VPOX_WITH_SHARED_PCI_INTERRUPTS
    RTSpinlockRelease(pThis->hSpinlock);
#else
    RTSemFastMutexRelease(pThis->hFastMtx);
#endif
}

DECLINLINE(int) vpoxPciVmLock(PVPOXRAWPCIDRVVM pThis)
{
    int rc = RTSemFastMutexRequest(pThis->hFastMtx);
    AssertRC(rc);
    return rc;
}

DECLINLINE(void) vpoxPciVmUnlock(PVPOXRAWPCIDRVVM pThis)
{
    RTSemFastMutexRelease(pThis->hFastMtx);
}

DECLINLINE(int) vpoxPciGlobalsLock(PVPOXRAWPCIGLOBALS pGlobals)
{
    int rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    AssertRC(rc);
    return rc;
}

DECLINLINE(void) vpoxPciGlobalsUnlock(PVPOXRAWPCIGLOBALS pGlobals)
{
    RTSemFastMutexRelease(pGlobals->hFastMtx);
}

static PVPOXRAWPCIINS vpoxPciFindInstanceLocked(PVPOXRAWPCIGLOBALS pGlobals, uint32_t iHostAddress)
{
    PVPOXRAWPCIINS pCur;
    for (pCur = pGlobals->pInstanceHead; pCur != NULL; pCur = pCur->pNext)
    {
        if (iHostAddress == pCur->HostPciAddress)
            return pCur;
    }
    return NULL;
}

static void vpoxPciUnlinkInstanceLocked(PVPOXRAWPCIGLOBALS pGlobals, PVPOXRAWPCIINS pToUnlink)
{
    if (pGlobals->pInstanceHead == pToUnlink)
        pGlobals->pInstanceHead = pToUnlink->pNext;
    else
    {
        PVPOXRAWPCIINS pCur;
        for (pCur = pGlobals->pInstanceHead; pCur != NULL; pCur = pCur->pNext)
        {
            if (pCur->pNext == pToUnlink)
            {
                pCur->pNext = pToUnlink->pNext;
                break;
            }
        }
    }
    pToUnlink->pNext = NULL;
}


#if 0 /** @todo r=bird: Who the heck is supposed to call this?!?   */
DECLHIDDEN(void) vpoxPciDevCleanup(PVPOXRAWPCIINS pThis)
{
    pThis->DevPort.pfnDeinit(&pThis->DevPort, 0);

    if (pThis->hFastMtx)
    {
        RTSemFastMutexDestroy(pThis->hFastMtx);
        pThis->hFastMtx = NIL_RTSEMFASTMUTEX;
    }

    if (pThis->hSpinlock)
    {
        RTSpinlockDestroy(pThis->hSpinlock);
        pThis->hSpinlock = NIL_RTSPINLOCK;
    }

    vpoxPciGlobalsLock(pThis->pGlobals);
    vpoxPciUnlinkInstanceLocked(pThis->pGlobals, pThis);
    vpoxPciGlobalsUnlock(pThis->pGlobals);
}
#endif


/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnInit}
 */
static DECLCALLBACK(int) vpoxPciDevInit(PRAWPCIDEVPORT pPort, uint32_t fFlags)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevInit(pThis, fFlags);

    vpoxPciDevUnlock(pThis);

    return rc;
}

/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnDeinit}
 */
static DECLCALLBACK(int) vpoxPciDevDeinit(PRAWPCIDEVPORT pPort, uint32_t fFlags)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    if (pThis->IrqHandler.pfnIrqHandler)
    {
        vpoxPciOsDevUnregisterIrqHandler(pThis, pThis->IrqHandler.iHostIrq);
        pThis->IrqHandler.iHostIrq = 0;
        pThis->IrqHandler.pfnIrqHandler = NULL;
    }

    rc = vpoxPciOsDevDeinit(pThis, fFlags);

    vpoxPciDevUnlock(pThis);

    return rc;
}


/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnDestroy}
 */
static DECLCALLBACK(int) vpoxPciDevDestroy(PRAWPCIDEVPORT pPort)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int rc;

    rc = vpoxPciOsDevDestroy(pThis);
    if (rc == VINF_SUCCESS)
    {
        if (pThis->hFastMtx)
        {
            RTSemFastMutexDestroy(pThis->hFastMtx);
            pThis->hFastMtx = NIL_RTSEMFASTMUTEX;
        }

        if (pThis->hSpinlock)
        {
            RTSpinlockDestroy(pThis->hSpinlock);
            pThis->hSpinlock = NIL_RTSPINLOCK;
        }

        vpoxPciGlobalsLock(pThis->pGlobals);
        vpoxPciUnlinkInstanceLocked(pThis->pGlobals, pThis);
        vpoxPciGlobalsUnlock(pThis->pGlobals);

        RTMemFree(pThis);
    }

    return rc;
}
/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnGetRegionInfo}
 */
static DECLCALLBACK(int) vpoxPciDevGetRegionInfo(PRAWPCIDEVPORT pPort,
                                                 int32_t        iRegion,
                                                 RTHCPHYS       *pRegionStart,
                                                 uint64_t       *pu64RegionSize,
                                                 bool           *pfPresent,
                                                 uint32_t        *pfFlags)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevGetRegionInfo(pThis, iRegion,
                                   pRegionStart, pu64RegionSize,
                                   pfPresent, pfFlags);
    vpoxPciDevUnlock(pThis);

    return rc;
}

/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnMapRegion}
 */
static DECLCALLBACK(int) vpoxPciDevMapRegion(PRAWPCIDEVPORT pPort,
                                             int32_t        iRegion,
                                             RTHCPHYS       RegionStart,
                                             uint64_t       u64RegionSize,
                                             int32_t        fFlags,
                                             RTR0PTR        *pRegionBaseR0)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevMapRegion(pThis, iRegion, RegionStart, u64RegionSize, fFlags, pRegionBaseR0);

    vpoxPciDevUnlock(pThis);

    return rc;
}

/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnUnmapRegion}
 */
static DECLCALLBACK(int) vpoxPciDevUnmapRegion(PRAWPCIDEVPORT pPort,
                                               int32_t        iRegion,
                                               RTHCPHYS       RegionStart,
                                               uint64_t       u64RegionSize,
                                               RTR0PTR        RegionBase)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevUnmapRegion(pThis, iRegion, RegionStart, u64RegionSize, RegionBase);

    vpoxPciDevUnlock(pThis);

    return rc;
}

/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnPciCfgRead}
 */
static DECLCALLBACK(int) vpoxPciDevPciCfgRead(PRAWPCIDEVPORT pPort,
                                              uint32_t       Register,
                                              PCIRAWMEMLOC   *pValue)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevPciCfgRead(pThis, Register, pValue);

    vpoxPciDevUnlock(pThis);

    return rc;
}

/**
 * @interface_method_impl{RAWPCIDEVPORT,pfnPciCfgWrite}
 */
static DECLCALLBACK(int) vpoxPciDevPciCfgWrite(PRAWPCIDEVPORT pPort,
                                               uint32_t       Register,
                                               PCIRAWMEMLOC   *pValue)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevPciCfgWrite(pThis, Register, pValue);

    vpoxPciDevUnlock(pThis);

    return rc;
}

static DECLCALLBACK(int) vpoxPciDevRegisterIrqHandler(PRAWPCIDEVPORT  pPort,
                                                      PFNRAWPCIISR    pfnHandler,
                                                      void*           pIrqContext,
                                                      PCIRAWISRHANDLE *phIsr)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;
    int32_t        iHostIrq = 0;

    if (pfnHandler == NULL)
        return VERR_INVALID_PARAMETER;

    vpoxPciDevLock(pThis);

    if (pThis->IrqHandler.pfnIrqHandler)
    {
        rc = VERR_ALREADY_EXISTS;
    }
    else
    {
        rc = vpoxPciOsDevRegisterIrqHandler(pThis, pfnHandler, pIrqContext, &iHostIrq);
        if (RT_SUCCESS(rc))
        {
            *phIsr = 0xcafe0000;
            pThis->IrqHandler.iHostIrq      = iHostIrq;
            pThis->IrqHandler.pfnIrqHandler = pfnHandler;
            pThis->IrqHandler.pIrqContext   = pIrqContext;
        }
    }

    vpoxPciDevUnlock(pThis);

    return rc;
}

static DECLCALLBACK(int) vpoxPciDevUnregisterIrqHandler(PRAWPCIDEVPORT  pPort,
                                                        PCIRAWISRHANDLE hIsr)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    if (hIsr != 0xcafe0000)
        return VERR_INVALID_PARAMETER;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevUnregisterIrqHandler(pThis, pThis->IrqHandler.iHostIrq);
    if (RT_SUCCESS(rc))
    {
        pThis->IrqHandler.pfnIrqHandler = NULL;
        pThis->IrqHandler.pIrqContext   = NULL;
        pThis->IrqHandler.iHostIrq = 0;
    }
    vpoxPciDevUnlock(pThis);

    return rc;
}

static DECLCALLBACK(int) vpoxPciDevPowerStateChange(PRAWPCIDEVPORT    pPort,
                                                    PCIRAWPOWERSTATE  aState,
                                                    uint64_t          *pu64Param)
{
    PVPOXRAWPCIINS pThis = DEVPORT_2_VPOXRAWPCIINS(pPort);
    int            rc;

    vpoxPciDevLock(pThis);

    rc = vpoxPciOsDevPowerStateChange(pThis, aState);

    switch (aState)
    {
        case PCIRAW_POWER_ON:
            /*
             * Let virtual device know about VM caps.
             */
            *pu64Param = VPOX_DRV_VMDATA(pThis)->pPerVmData->fVmCaps;
            break;
        default:
            pu64Param = 0;
            break;
    }


    vpoxPciDevUnlock(pThis);

    return rc;
}

/**
 * Creates a new instance.
 *
 * @returns VPox status code.
 * @param   pGlobals            The globals.
 * @param   u32HostAddress      Host address.
 * @param   fFlags              Flags.
 * @param   pVmCtx              VM context.
 * @param   ppDevPort           Where to store the pointer to our port interface.
 * @param   pfDevFlags          The device flags.
 */
static int vpoxPciNewInstance(PVPOXRAWPCIGLOBALS pGlobals,
                              uint32_t           u32HostAddress,
                              uint32_t           fFlags,
                              PRAWPCIPERVM       pVmCtx,
                              PRAWPCIDEVPORT     *ppDevPort,
                              uint32_t           *pfDevFlags)
{
    int             rc;
    PVPOXRAWPCIINS  pNew = (PVPOXRAWPCIINS)RTMemAllocZ(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    pNew->pGlobals                      = pGlobals;
    pNew->hSpinlock                     = NIL_RTSPINLOCK;
    pNew->cRefs                         = 1;
    pNew->pNext                         = NULL;
    pNew->HostPciAddress                = u32HostAddress;
    pNew->pVmCtx                        = pVmCtx;

    pNew->DevPort.u32Version            = RAWPCIDEVPORT_VERSION;

    pNew->DevPort.pfnInit               = vpoxPciDevInit;
    pNew->DevPort.pfnDeinit             = vpoxPciDevDeinit;
    pNew->DevPort.pfnDestroy            = vpoxPciDevDestroy;
    pNew->DevPort.pfnGetRegionInfo      = vpoxPciDevGetRegionInfo;
    pNew->DevPort.pfnMapRegion          = vpoxPciDevMapRegion;
    pNew->DevPort.pfnUnmapRegion        = vpoxPciDevUnmapRegion;
    pNew->DevPort.pfnPciCfgRead         = vpoxPciDevPciCfgRead;
    pNew->DevPort.pfnPciCfgWrite        = vpoxPciDevPciCfgWrite;
    pNew->DevPort.pfnPciCfgRead         = vpoxPciDevPciCfgRead;
    pNew->DevPort.pfnPciCfgWrite        = vpoxPciDevPciCfgWrite;
    pNew->DevPort.pfnRegisterIrqHandler = vpoxPciDevRegisterIrqHandler;
    pNew->DevPort.pfnUnregisterIrqHandler = vpoxPciDevUnregisterIrqHandler;
    pNew->DevPort.pfnPowerStateChange   = vpoxPciDevPowerStateChange;
    pNew->DevPort.u32VersionEnd         = RAWPCIDEVPORT_VERSION;

    rc = RTSpinlockCreate(&pNew->hSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VPoxPCI");
    if (RT_SUCCESS(rc))
    {
        rc = RTSemFastMutexCreate(&pNew->hFastMtx);
        if (RT_SUCCESS(rc))
        {
            rc = pNew->DevPort.pfnInit(&pNew->DevPort, fFlags);
            if (RT_SUCCESS(rc))
            {
                *ppDevPort = &pNew->DevPort;

                pNew->pNext = pGlobals->pInstanceHead;
                pGlobals->pInstanceHead = pNew;
            }
            else
            {
                RTSemFastMutexDestroy(pNew->hFastMtx);
                RTSpinlockDestroy(pNew->hSpinlock);
                RTMemFree(pNew);
            }
        }
    }

    return rc;
}

/**
 * @interface_method_impl{RAWPCIFACTORY,pfnCreateAndConnect}
 */
static DECLCALLBACK(int) vpoxPciFactoryCreateAndConnect(PRAWPCIFACTORY       pFactory,
                                                        uint32_t             u32HostAddress,
                                                        uint32_t             fFlags,
                                                        PRAWPCIPERVM         pVmCtx,
                                                        PRAWPCIDEVPORT       *ppDevPort,
                                                        uint32_t             *pfDevFlags)
{
    PVPOXRAWPCIGLOBALS pGlobals = (PVPOXRAWPCIGLOBALS)((uint8_t *)pFactory - RT_OFFSETOF(VPOXRAWPCIGLOBALS, RawPciFactory));
    int rc;

    LogFlow(("vpoxPciFactoryCreateAndConnect: PCI=%x fFlags=%#x\n", u32HostAddress, fFlags));
    Assert(pGlobals->cFactoryRefs > 0);
    rc = vpoxPciGlobalsLock(pGlobals);
    AssertRCReturn(rc, rc);

    /* First search if there's no existing instance with same host device
     * address - if so - we cannot continue.
     */
    if (vpoxPciFindInstanceLocked(pGlobals, u32HostAddress) != NULL)
    {
        rc = VERR_RESOURCE_BUSY;
        goto unlock;
    }

    rc = vpoxPciNewInstance(pGlobals, u32HostAddress, fFlags, pVmCtx, ppDevPort, pfDevFlags);

unlock:
    vpoxPciGlobalsUnlock(pGlobals);

    return rc;
}

/**
 * @interface_method_impl{RAWPCIFACTORY,pfnRelease}
 */
static DECLCALLBACK(void) vpoxPciFactoryRelease(PRAWPCIFACTORY pFactory)
{
    PVPOXRAWPCIGLOBALS pGlobals = (PVPOXRAWPCIGLOBALS)((uint8_t *)pFactory - RT_OFFSETOF(VPOXRAWPCIGLOBALS, RawPciFactory));

    int32_t cRefs = ASMAtomicDecS32(&pGlobals->cFactoryRefs);
    Assert(cRefs >= 0); NOREF(cRefs);
    LogFlow(("vpoxPciFactoryRelease: cRefs=%d (new)\n", cRefs));
}

/**
 * @interface_method_impl{RAWPCIFACTORY,pfnInitVm}
 */
static DECLCALLBACK(int)  vpoxPciFactoryInitVm(PRAWPCIFACTORY       pFactory,
                                               PVM                  pVM,
                                               PRAWPCIPERVM         pVmData)
{
    PVPOXRAWPCIDRVVM pThis = (PVPOXRAWPCIDRVVM)RTMemAllocZ(sizeof(VPOXRAWPCIDRVVM));
    int rc;

    if (!pThis)
         return VERR_NO_MEMORY;

    rc = RTSemFastMutexCreate(&pThis->hFastMtx);
    if (RT_SUCCESS(rc))
    {
        rc = vpoxPciOsInitVm(pThis, pVM, pVmData);

        if (RT_SUCCESS(rc))
        {
#ifdef VPOX_WITH_IOMMU
            /* If IOMMU notification routine in pVmData->pfnContigMemInfo
               is set - we have functional IOMMU hardware. */
            if (pVmData->pfnContigMemInfo)
                pVmData->fVmCaps |= PCIRAW_VMFLAGS_HAS_IOMMU;
#endif
            pThis->pPerVmData = pVmData;
            pVmData->pDriverData = pThis;
            return VINF_SUCCESS;
        }

        RTSemFastMutexDestroy(pThis->hFastMtx);
        pThis->hFastMtx = NIL_RTSEMFASTMUTEX;
        RTMemFree(pThis);
    }

    return rc;
}

/**
 * @interface_method_impl{RAWPCIFACTORY,pfnDeinitVm}
 */
static DECLCALLBACK(void)  vpoxPciFactoryDeinitVm(PRAWPCIFACTORY       pFactory,
                                                  PVM                  pVM,
                                                  PRAWPCIPERVM         pVmData)
{
    if (pVmData->pDriverData)
    {
        PVPOXRAWPCIDRVVM pThis = (PVPOXRAWPCIDRVVM)pVmData->pDriverData;

#ifdef VPOX_WITH_IOMMU
        /* If we have IOMMU, need to unmap all guest's physical pages from IOMMU on VM termination. */
#endif

        vpoxPciOsDeinitVm(pThis, pVM);

        if (pThis->hFastMtx)
        {
            RTSemFastMutexDestroy(pThis->hFastMtx);
            pThis->hFastMtx = NIL_RTSEMFASTMUTEX;
        }

        RTMemFree(pThis);
        pVmData->pDriverData = NULL;
    }
}


static bool vpoxPciCanUnload(PVPOXRAWPCIGLOBALS pGlobals)
{
    int rc = vpoxPciGlobalsLock(pGlobals);
    bool fRc = !pGlobals->pInstanceHead
            && pGlobals->cFactoryRefs <= 0;
    vpoxPciGlobalsUnlock(pGlobals);
    AssertRC(rc);
    return fRc;
}


static int vpoxPciInitIdc(PVPOXRAWPCIGLOBALS pGlobals)
{
    int rc;
    Assert(!pGlobals->fIDCOpen);

    /*
     * Establish a connection to SUPDRV and register our component factory.
     */
    rc = SUPR0IdcOpen(&pGlobals->SupDrvIDC, 0 /* iReqVersion = default */, 0 /* iMinVersion = default */, NULL, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = SUPR0IdcComponentRegisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
        if (RT_SUCCESS(rc))
        {
            pGlobals->fIDCOpen = true;
            Log(("VPoxRawPci: pSession=%p\n", SUPR0IdcGetSession(&pGlobals->SupDrvIDC)));
            return rc;
        }

        /* bail out. */
        LogRel(("VPoxRawPci: Failed to register component factory, rc=%Rrc\n", rc));
        SUPR0IdcClose(&pGlobals->SupDrvIDC);
    }

    return rc;
}


/**
 * Try to close the IDC connection to SUPDRV if established.
 *
 * @returns VPox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_WRONG_ORDER if we're busy.
 *
 * @param   pGlobals        Pointer to the globals.
 */
static int vpoxPciDeleteIdc(PVPOXRAWPCIGLOBALS pGlobals)
{
    int rc;

    Assert(pGlobals->hFastMtx != NIL_RTSEMFASTMUTEX);

    /*
     * Check before trying to deregister the factory.
     */
    if (!vpoxPciCanUnload(pGlobals))
        return VERR_WRONG_ORDER;

    if (!pGlobals->fIDCOpen)
        rc = VINF_SUCCESS;
    else
    {
        /*
         * Disconnect from SUPDRV.
         */
        rc = SUPR0IdcComponentDeregisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
        AssertRC(rc);
        SUPR0IdcClose(&pGlobals->SupDrvIDC);
        pGlobals->fIDCOpen = false;
    }

    return rc;
}


/**
 * Initializes the globals.
 *
 * @returns VPox status code.
 * @param   pGlobals        Pointer to the globals.
 */
static int vpoxPciInitGlobals(PVPOXRAWPCIGLOBALS pGlobals)
{
    /*
     * Initialize the common portions of the structure.
     */
    int rc = RTSemFastMutexCreate(&pGlobals->hFastMtx);
    if (RT_SUCCESS(rc))
    {
        pGlobals->pInstanceHead = NULL;
        pGlobals->RawPciFactory.pfnRelease = vpoxPciFactoryRelease;
        pGlobals->RawPciFactory.pfnCreateAndConnect = vpoxPciFactoryCreateAndConnect;
        pGlobals->RawPciFactory.pfnInitVm = vpoxPciFactoryInitVm;
        pGlobals->RawPciFactory.pfnDeinitVm = vpoxPciFactoryDeinitVm;
        memcpy(pGlobals->SupDrvFactory.szName, "VPoxRawPci", sizeof("VPoxRawPci"));
        pGlobals->SupDrvFactory.pfnQueryFactoryInterface = vpoxPciQueryFactoryInterface;
        pGlobals->fIDCOpen = false;
    }
    return rc;
}


/**
 * Deletes the globals.
 *
 * @param   pGlobals        Pointer to the globals.
 */
static void vpoxPciDeleteGlobals(PVPOXRAWPCIGLOBALS pGlobals)
{
    Assert(!pGlobals->fIDCOpen);

    /*
     * Release resources.
     */
    if (pGlobals->hFastMtx)
    {
        RTSemFastMutexDestroy(pGlobals->hFastMtx);
        pGlobals->hFastMtx = NIL_RTSEMFASTMUTEX;
    }
}


int  vpoxPciInit(PVPOXRAWPCIGLOBALS pGlobals)
{

    /*
     * Initialize the common portions of the structure.
     */
    int rc = vpoxPciInitGlobals(pGlobals);
    if (RT_SUCCESS(rc))
    {
        rc = vpoxPciInitIdc(pGlobals);
        if (RT_SUCCESS(rc))
            return rc;

        /* bail out. */
        vpoxPciDeleteGlobals(pGlobals);
    }

    return rc;
}

void vpoxPciShutdown(PVPOXRAWPCIGLOBALS pGlobals)
{
    int rc = vpoxPciDeleteIdc(pGlobals);
    if (RT_SUCCESS(rc))
        vpoxPciDeleteGlobals(pGlobals);
}

