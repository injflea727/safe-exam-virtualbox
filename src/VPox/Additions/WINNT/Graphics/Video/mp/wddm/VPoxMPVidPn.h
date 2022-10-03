/* $Id: VPoxMPVidPn.h $ */
/** @file
 * VPox WDDM Miniport driver
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
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVidPn_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVidPn_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOXVDPN_C_DISPLAY_HBLANK_SIZE 200
#define VPOXVDPN_C_DISPLAY_VBLANK_SIZE 180

void VPoxVidPnAllocDataInit(struct VPOXWDDM_ALLOC_DATA *pData, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId);

void VPoxVidPnSourceInit(PVPOXWDDM_SOURCE pSource, const D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, uint8_t u8SyncState);
void VPoxVidPnTargetInit(PVPOXWDDM_TARGET pTarget, const D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, uint8_t u8SyncState);
void VPoxVidPnSourceCopy(VPOXWDDM_SOURCE *pDst, const VPOXWDDM_SOURCE *pSrc);
void VPoxVidPnTargetCopy(VPOXWDDM_TARGET *pDst, const VPOXWDDM_TARGET *pSrc);

void VPoxVidPnSourcesInit(PVPOXWDDM_SOURCE pSources, uint32_t cScreens, uint8_t u8SyncState);
void VPoxVidPnTargetsInit(PVPOXWDDM_TARGET pTargets, uint32_t cScreens, uint8_t u8SyncState);
void VPoxVidPnSourcesCopy(VPOXWDDM_SOURCE *pDst, const VPOXWDDM_SOURCE *pSrc, uint32_t cScreens);
void VPoxVidPnTargetsCopy(VPOXWDDM_TARGET *pDst, const VPOXWDDM_TARGET *pSrc, uint32_t cScreens);

typedef struct VPOXWDDM_TARGET_ITER
{
    PVPOXWDDM_SOURCE pSource;
    PVPOXWDDM_TARGET paTargets;
    uint32_t cTargets;
    uint32_t i;
    uint32_t c;
} VPOXWDDM_TARGET_ITER;

void VPoxVidPnStCleanup(PVPOXWDDM_SOURCE paSources, PVPOXWDDM_TARGET paTargets, uint32_t cScreens);
void VPoxVidPnStTIterInit(PVPOXWDDM_SOURCE pSource, PVPOXWDDM_TARGET paTargets, uint32_t cTargets, VPOXWDDM_TARGET_ITER *pIter);
PVPOXWDDM_TARGET VPoxVidPnStTIterNext(VPOXWDDM_TARGET_ITER *pIter);

void VPoxDumpSourceTargetArrays(VPOXWDDM_SOURCE *paSources, VPOXWDDM_TARGET *paTargets, uint32_t cScreens);

/* !!!NOTE: The callback is responsible for releasing the path */
typedef DECLCALLBACK(BOOLEAN) FNVPOXVIDPNENUMPATHS(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo, PVOID pContext);
typedef FNVPOXVIDPNENUMPATHS *PFNVPOXVIDPNENUMPATHS;

/* !!!NOTE: The callback is responsible for releasing the source mode info */
typedef DECLCALLBACK(BOOLEAN) FNVPOXVIDPNENUMSOURCEMODES(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext);
typedef FNVPOXVIDPNENUMSOURCEMODES *PFNVPOXVIDPNENUMSOURCEMODES;

/* !!!NOTE: The callback is responsible for releasing the target mode info */
typedef DECLCALLBACK(BOOLEAN) FNVPOXVIDPNENUMTARGETMODES(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext);
typedef FNVPOXVIDPNENUMTARGETMODES *PFNVPOXVIDPNENUMTARGETMODES;

/* !!!NOTE: The callback is responsible for releasing the source mode info */
typedef DECLCALLBACK(BOOLEAN) FNVPOXVIDPNENUMMONITORSOURCEMODES(D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI, PVOID pContext);
typedef FNVPOXVIDPNENUMMONITORSOURCEMODES *PFNVPOXVIDPNENUMMONITORSOURCEMODES;

typedef DECLCALLBACK(BOOLEAN) FNVPOXVIDPNENUMTARGETSFORSOURCE(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, SIZE_T cTgtPaths, PVOID pContext);
typedef FNVPOXVIDPNENUMTARGETSFORSOURCE *PFNVPOXVIDPNENUMTARGETSFORSOURCE;

NTSTATUS VPoxVidPnCommitSourceModeForSrcId(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        PVPOXWDDM_ALLOCATION pAllocation,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId, VPOXWDDM_SOURCE *paSources, VPOXWDDM_TARGET *paTargets, BOOLEAN bPathPowerTransition);

NTSTATUS VPoxVidPnCommitAll(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        PVPOXWDDM_ALLOCATION pAllocation,
        VPOXWDDM_SOURCE *paSources, VPOXWDDM_TARGET *paTargets);

NTSTATUS vpoxVidPnEnumPaths(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        PFNVPOXVIDPNENUMPATHS pfnCallback, PVOID pContext);

NTSTATUS vpoxVidPnEnumSourceModes(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        PFNVPOXVIDPNENUMSOURCEMODES pfnCallback, PVOID pContext);

NTSTATUS vpoxVidPnEnumTargetModes(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        PFNVPOXVIDPNENUMTARGETMODES pfnCallback, PVOID pContext);

NTSTATUS vpoxVidPnEnumMonitorSourceModes(D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        PFNVPOXVIDPNENUMMONITORSOURCEMODES pfnCallback, PVOID pContext);

NTSTATUS vpoxVidPnEnumTargetsForSource(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        PFNVPOXVIDPNENUMTARGETSFORSOURCE pfnCallback, PVOID pContext);

void VPoxVidPnDumpTargetMode(const char *pPrefix, const D3DKMDT_VIDPN_TARGET_MODE* CONST  pVidPnTargetModeInfo, const char *pSuffix);
void VPoxVidPnDumpMonitorMode(const char *pPrefix, const D3DKMDT_MONITOR_SOURCE_MODE *pVidPnModeInfo, const char *pSuffix);
NTSTATUS VPoxVidPnDumpMonitorModeSet(const char *pPrefix, PVPOXMP_DEVEXT pDevExt, uint32_t u32Target, const char *pSuffix);
void VPoxVidPnDumpSourceMode(const char *pPrefix, const D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, const char *pSuffix);
void VPoxVidPnDumpCofuncModalityInfo(const char *pPrefix, D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmEnumPivotType, const DXGK_ENUM_PIVOT *pPivot, const char *pSuffix);

void vpoxVidPnDumpVidPn(const char * pPrefix, PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, const char * pSuffix);
void vpoxVidPnDumpCofuncModalityArg(const char *pPrefix, D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmPivot, const DXGK_ENUM_PIVOT *pPivot, const char *pSuffix);
DECLCALLBACK(BOOLEAN) vpoxVidPnDumpSourceModeSetEnum(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext);
DECLCALLBACK(BOOLEAN) vpoxVidPnDumpTargetModeSetEnum(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext);


typedef struct VPOXVIDPN_SOURCEMODE_ITER
{
    D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;
    const D3DKMDT_VIDPN_SOURCE_MODE *pCurVidPnModeInfo;
    NTSTATUS Status;
} VPOXVIDPN_SOURCEMODE_ITER;

DECLINLINE(void) VPoxVidPnSourceModeIterInit(VPOXVIDPN_SOURCEMODE_ITER *pIter, D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface)
{
    pIter->hVidPnModeSet = hVidPnModeSet;
    pIter->pVidPnModeSetInterface = pVidPnModeSetInterface;
    pIter->pCurVidPnModeInfo = NULL;
    pIter->Status = STATUS_SUCCESS;
}

DECLINLINE(void) VPoxVidPnSourceModeIterTerm(VPOXVIDPN_SOURCEMODE_ITER *pIter)
{
    if (pIter->pCurVidPnModeInfo)
    {
        pIter->pVidPnModeSetInterface->pfnReleaseModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo);
        pIter->pCurVidPnModeInfo = NULL;
    }
}

DECLINLINE(const D3DKMDT_VIDPN_SOURCE_MODE *) VPoxVidPnSourceModeIterNext(VPOXVIDPN_SOURCEMODE_ITER *pIter)
{
    NTSTATUS Status;
    const D3DKMDT_VIDPN_SOURCE_MODE *pCurVidPnModeInfo;

    if (!pIter->pCurVidPnModeInfo)
        Status = pIter->pVidPnModeSetInterface->pfnAcquireFirstModeInfo(pIter->hVidPnModeSet, &pCurVidPnModeInfo);
    else
        Status = pIter->pVidPnModeSetInterface->pfnAcquireNextModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo, &pCurVidPnModeInfo);

    if (Status == STATUS_SUCCESS)
    {
        Assert(pCurVidPnModeInfo);

        if (pIter->pCurVidPnModeInfo)
            pIter->pVidPnModeSetInterface->pfnReleaseModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo);

        pIter->pCurVidPnModeInfo = pCurVidPnModeInfo;
        return pCurVidPnModeInfo;
    }

    if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET
            || Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        return NULL;

    WARN(("getting Source info failed %#x", Status));

    pIter->Status = Status;
    return NULL;
}

DECLINLINE(NTSTATUS) VPoxVidPnSourceModeIterStatus(VPOXVIDPN_SOURCEMODE_ITER *pIter)
{
    return pIter->Status;
}

typedef struct VPOXVIDPN_TARGETMODE_ITER
{
    D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface;
    const D3DKMDT_VIDPN_TARGET_MODE *pCurVidPnModeInfo;
    NTSTATUS Status;
} VPOXVIDPN_TARGETMODE_ITER;

DECLINLINE(void) VPoxVidPnTargetModeIterInit(VPOXVIDPN_TARGETMODE_ITER *pIter,D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface)
{
    pIter->hVidPnModeSet = hVidPnModeSet;
    pIter->pVidPnModeSetInterface = pVidPnModeSetInterface;
    pIter->pCurVidPnModeInfo = NULL;
    pIter->Status = STATUS_SUCCESS;
}

DECLINLINE(void) VPoxVidPnTargetModeIterTerm(VPOXVIDPN_TARGETMODE_ITER *pIter)
{
    if (pIter->pCurVidPnModeInfo)
    {
        pIter->pVidPnModeSetInterface->pfnReleaseModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo);
        pIter->pCurVidPnModeInfo = NULL;
    }
}

DECLINLINE(const D3DKMDT_VIDPN_TARGET_MODE *) VPoxVidPnTargetModeIterNext(VPOXVIDPN_TARGETMODE_ITER *pIter)
{
    NTSTATUS Status;
    const D3DKMDT_VIDPN_TARGET_MODE *pCurVidPnModeInfo;

    if (!pIter->pCurVidPnModeInfo)
        Status = pIter->pVidPnModeSetInterface->pfnAcquireFirstModeInfo(pIter->hVidPnModeSet, &pCurVidPnModeInfo);
    else
        Status = pIter->pVidPnModeSetInterface->pfnAcquireNextModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo, &pCurVidPnModeInfo);

    if (Status == STATUS_SUCCESS)
    {
        Assert(pCurVidPnModeInfo);

        if (pIter->pCurVidPnModeInfo)
            pIter->pVidPnModeSetInterface->pfnReleaseModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo);

        pIter->pCurVidPnModeInfo = pCurVidPnModeInfo;
        return pCurVidPnModeInfo;
    }

    if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET
            || Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        return NULL;

    WARN(("getting Target info failed %#x", Status));

    pIter->Status = Status;
    return NULL;
}

DECLINLINE(NTSTATUS) VPoxVidPnTargetModeIterStatus(VPOXVIDPN_TARGETMODE_ITER *pIter)
{
    return pIter->Status;
}


typedef struct VPOXVIDPN_MONITORMODE_ITER
{
    D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet;
    const DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;
    const D3DKMDT_MONITOR_SOURCE_MODE *pCurVidPnModeInfo;
    NTSTATUS Status;
} VPOXVIDPN_MONITORMODE_ITER;


DECLINLINE(void) VPoxVidPnMonitorModeIterInit(VPOXVIDPN_MONITORMODE_ITER *pIter, D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet, const DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface)
{
    pIter->hVidPnModeSet = hVidPnModeSet;
    pIter->pVidPnModeSetInterface = pVidPnModeSetInterface;
    pIter->pCurVidPnModeInfo = NULL;
    pIter->Status = STATUS_SUCCESS;
}

DECLINLINE(void) VPoxVidPnMonitorModeIterTerm(VPOXVIDPN_MONITORMODE_ITER *pIter)
{
    if (pIter->pCurVidPnModeInfo)
    {
        pIter->pVidPnModeSetInterface->pfnReleaseModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo);
        pIter->pCurVidPnModeInfo = NULL;
    }
}

DECLINLINE(const D3DKMDT_MONITOR_SOURCE_MODE *) VPoxVidPnMonitorModeIterNext(VPOXVIDPN_MONITORMODE_ITER *pIter)
{
    NTSTATUS Status;
    const D3DKMDT_MONITOR_SOURCE_MODE *pCurVidPnModeInfo;

    if (!pIter->pCurVidPnModeInfo)
        Status = pIter->pVidPnModeSetInterface->pfnAcquireFirstModeInfo(pIter->hVidPnModeSet, &pCurVidPnModeInfo);
    else
        Status = pIter->pVidPnModeSetInterface->pfnAcquireNextModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo, &pCurVidPnModeInfo);

    if (Status == STATUS_SUCCESS)
    {
        Assert(pCurVidPnModeInfo);

        if (pIter->pCurVidPnModeInfo)
            pIter->pVidPnModeSetInterface->pfnReleaseModeInfo(pIter->hVidPnModeSet, pIter->pCurVidPnModeInfo);

        pIter->pCurVidPnModeInfo = pCurVidPnModeInfo;
        return pCurVidPnModeInfo;
    }

    if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET
            || Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        return NULL;

    WARN(("getting Monitor info failed %#x", Status));

    pIter->Status = Status;
    return NULL;
}

DECLINLINE(NTSTATUS) VPoxVidPnMonitorModeIterStatus(VPOXVIDPN_MONITORMODE_ITER *pIter)
{
    return pIter->Status;
}



typedef struct VPOXVIDPN_PATH_ITER
{
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    const D3DKMDT_VIDPN_PRESENT_PATH *pCurVidPnPathInfo;
    NTSTATUS Status;
} VPOXVIDPN_PATH_ITER;


DECLINLINE(void) VPoxVidPnPathIterInit(VPOXVIDPN_PATH_ITER *pIter, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface)
{
    pIter->hVidPnTopology = hVidPnTopology;
    pIter->pVidPnTopologyInterface = pVidPnTopologyInterface;
    pIter->pCurVidPnPathInfo = NULL;
    pIter->Status = STATUS_SUCCESS;
}

DECLINLINE(void) VPoxVidPnPathIterTerm(VPOXVIDPN_PATH_ITER *pIter)
{
    if (pIter->pCurVidPnPathInfo)
    {
        pIter->pVidPnTopologyInterface->pfnReleasePathInfo(pIter->hVidPnTopology, pIter->pCurVidPnPathInfo);
        pIter->pCurVidPnPathInfo = NULL;
    }
}

DECLINLINE(const D3DKMDT_VIDPN_PRESENT_PATH *) VPoxVidPnPathIterNext(VPOXVIDPN_PATH_ITER *pIter)
{
    NTSTATUS Status;
    const D3DKMDT_VIDPN_PRESENT_PATH *pCurVidPnPathInfo;

    if (!pIter->pCurVidPnPathInfo)
        Status = pIter->pVidPnTopologyInterface->pfnAcquireFirstPathInfo(pIter->hVidPnTopology, &pCurVidPnPathInfo);
    else
        Status = pIter->pVidPnTopologyInterface->pfnAcquireNextPathInfo(pIter->hVidPnTopology, pIter->pCurVidPnPathInfo, &pCurVidPnPathInfo);

    if (Status == STATUS_SUCCESS)
    {
        Assert(pCurVidPnPathInfo);

        if (pIter->pCurVidPnPathInfo)
            pIter->pVidPnTopologyInterface->pfnReleasePathInfo(pIter->hVidPnTopology, pIter->pCurVidPnPathInfo);

        pIter->pCurVidPnPathInfo = pCurVidPnPathInfo;
        return pCurVidPnPathInfo;
    }

    if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET
            || Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        return NULL;

    WARN(("getting Path info failed %#x", Status));

    pIter->Status = Status;
    return NULL;
}

DECLINLINE(NTSTATUS)  VPoxVidPnPathIterStatus(VPOXVIDPN_PATH_ITER *pIter)
{
    return pIter->Status;
}

NTSTATUS VPoxVidPnRecommendMonitorModes(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_TARGET_ID VideoPresentTargetId,
                        D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet, const DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface);

NTSTATUS VPoxVidPnRecommendFunctional(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, const VPOXWDDM_RECOMMENDVIDPN *pData);

NTSTATUS VPoxVidPnCofuncModality(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmPivot, const DXGK_ENUM_PIVOT *pPivot);

NTSTATUS VPoxVidPnIsSupported(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, BOOLEAN *pfSupported);

NTSTATUS VPoxVidPnUpdateModes(PVPOXMP_DEVEXT pDevExt, uint32_t u32TargetId, const RTRECTSIZE *pSize);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVidPn_h */
