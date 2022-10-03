/* $Id: VPoxMPVidPn.cpp $ */
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

#include "VPoxMPWddm.h"
#include "VPoxMPVidPn.h"
#include "common/VPoxMPCommon.h"


static NTSTATUS vpoxVidPnCheckMonitorModes(PVPOXMP_DEVEXT pDevExt, uint32_t u32Target, const CR_SORTARRAY *pSupportedTargetModes = NULL);

static D3DDDIFORMAT vpoxWddmCalcPixelFormat(const VIDEO_MODE_INFORMATION *pInfo)
{
    switch (pInfo->BitsPerPlane)
    {
        case 32:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_A8R8G8B8;
                WARN(("unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)",
                      pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 24:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_R8G8B8;
                WARN(("unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)",
                     pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 16:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xF800 && pInfo->GreenMask == 0x7E0 && pInfo->BlueMask == 0x1F)
                    return D3DDDIFMT_R5G6B5;
                WARN(("unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)",
                      pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 8:
            if((pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && (pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                return D3DDDIFMT_P8;
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        default:
            WARN(("unsupported bpp(%d)", pInfo->BitsPerPlane));
            AssertBreakpoint();
            break;
    }

    return D3DDDIFMT_UNKNOWN;
}

static int vpoxWddmResolutionFind(const D3DKMDT_2DREGION *pResolutions, int cResolutions, const D3DKMDT_2DREGION *pRes)
{
    for (int i = 0; i < cResolutions; ++i)
    {
        const D3DKMDT_2DREGION *pResolution = &pResolutions[i];
        if (pResolution->cx == pRes->cx && pResolution->cy == pRes->cy)
            return i;
    }
    return -1;
}

static bool vpoxWddmVideoModesMatch(const VIDEO_MODE_INFORMATION *pMode1, const VIDEO_MODE_INFORMATION *pMode2)
{
    return pMode1->VisScreenHeight == pMode2->VisScreenHeight
            && pMode1->VisScreenWidth == pMode2->VisScreenWidth
            && pMode1->BitsPerPlane == pMode2->BitsPerPlane;
}

static int vpoxWddmVideoModeFind(const VIDEO_MODE_INFORMATION *pModes, int cModes, const VIDEO_MODE_INFORMATION *pM)
{
    for (int i = 0; i < cModes; ++i)
    {
        const VIDEO_MODE_INFORMATION *pMode = &pModes[i];
        if (vpoxWddmVideoModesMatch(pMode, pM))
            return i;
    }
    return -1;
}

static NTSTATUS vpoxVidPnPopulateVideoSignalInfo(D3DKMDT_VIDEO_SIGNAL_INFO *pVsi,
        const RTRECTSIZE *pResolution,
        ULONG VSync)
{
    NTSTATUS Status = STATUS_SUCCESS;

    pVsi->VideoStandard  = D3DKMDT_VSS_OTHER;
    pVsi->ActiveSize.cx = pResolution->cx;
    pVsi->ActiveSize.cy = pResolution->cy;
    pVsi->TotalSize = pVsi->ActiveSize;
    if (VPOXWDDM_IS_DISPLAYONLY())
    {
        /* VSYNC is not implemented in display-only mode (#8228).
         * In this case Windows checks that frequencies are not specified.
         */
        pVsi->VSyncFreq.Numerator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
        pVsi->VSyncFreq.Denominator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
        pVsi->PixelRate = D3DKMDT_FREQUENCY_NOTSPECIFIED;
        pVsi->HSyncFreq.Numerator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
        pVsi->HSyncFreq.Denominator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    }
    else
    {
        pVsi->VSyncFreq.Numerator = VSync * 1000;
        pVsi->VSyncFreq.Denominator = 1000;
        pVsi->PixelRate = pVsi->TotalSize.cx * pVsi->TotalSize.cy * VSync;
        pVsi->HSyncFreq.Numerator = (UINT)((pVsi->PixelRate / pVsi->TotalSize.cy) * 1000);
        pVsi->HSyncFreq.Denominator = 1000;
    }
    pVsi->ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;

    return Status;
}

BOOLEAN vpoxVidPnMatchVideoSignal(const D3DKMDT_VIDEO_SIGNAL_INFO *pVsi1, const D3DKMDT_VIDEO_SIGNAL_INFO *pVsi2)
{
    if (pVsi1->VideoStandard != pVsi2->VideoStandard)
        return FALSE;
    if (pVsi1->TotalSize.cx != pVsi2->TotalSize.cx)
        return FALSE;
    if (pVsi1->TotalSize.cy != pVsi2->TotalSize.cy)
        return FALSE;
    if (pVsi1->ActiveSize.cx != pVsi2->ActiveSize.cx)
        return FALSE;
    if (pVsi1->ActiveSize.cy != pVsi2->ActiveSize.cy)
        return FALSE;
    if (pVsi1->VSyncFreq.Numerator != pVsi2->VSyncFreq.Numerator)
        return FALSE;
    if (pVsi1->VSyncFreq.Denominator != pVsi2->VSyncFreq.Denominator)
        return FALSE;
    if (pVsi1->HSyncFreq.Numerator != pVsi2->HSyncFreq.Numerator)
        return FALSE;
    if (pVsi1->HSyncFreq.Denominator != pVsi2->HSyncFreq.Denominator)
        return FALSE;
    if (pVsi1->PixelRate != pVsi2->PixelRate)
        return FALSE;
    if (pVsi1->ScanLineOrdering != pVsi2->ScanLineOrdering)
        return FALSE;

    return TRUE;
}

static void vpoxVidPnPopulateSourceModeInfo(D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, const RTRECTSIZE *pSize)
{
    /* this is a graphics mode */
    pNewVidPnSourceModeInfo->Type = D3DKMDT_RMT_GRAPHICS;
    pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx = pSize->cx;
    pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy = pSize->cy;
    pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize = pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize;
    pNewVidPnSourceModeInfo->Format.Graphics.Stride = pSize->cx * 4;
    pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat = D3DDDIFMT_A8R8G8B8;
    Assert(pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat != D3DDDIFMT_UNKNOWN);
    pNewVidPnSourceModeInfo->Format.Graphics.ColorBasis = D3DKMDT_CB_SRGB;
    if (pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat == D3DDDIFMT_P8)
        pNewVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_SETTABLEPALETTE;
    else
        pNewVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;
}

static void vpoxVidPnPopulateMonitorModeInfo(D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSourceMode, const RTRECTSIZE *pResolution)
{
    vpoxVidPnPopulateVideoSignalInfo(&pMonitorSourceMode->VideoSignalInfo, pResolution, g_RefreshRate);
    pMonitorSourceMode->ColorBasis = D3DKMDT_CB_SRGB;
    pMonitorSourceMode->ColorCoeffDynamicRanges.FirstChannel = 8;
    pMonitorSourceMode->ColorCoeffDynamicRanges.SecondChannel = 8;
    pMonitorSourceMode->ColorCoeffDynamicRanges.ThirdChannel = 8;
    pMonitorSourceMode->ColorCoeffDynamicRanges.FourthChannel = 0;
    pMonitorSourceMode->Origin = D3DKMDT_MCO_DRIVER;
    pMonitorSourceMode->Preference = D3DKMDT_MP_NOTPREFERRED;
}

static NTSTATUS vpoxVidPnPopulateTargetModeInfo(D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, const RTRECTSIZE *pResolution)
{
    pNewVidPnTargetModeInfo->Preference = D3DKMDT_MP_NOTPREFERRED;
    return vpoxVidPnPopulateVideoSignalInfo(&pNewVidPnTargetModeInfo->VideoSignalInfo, pResolution, g_RefreshRate);
}

void VPoxVidPnStTargetCleanup(PVPOXWDDM_SOURCE paSources, uint32_t cScreens, PVPOXWDDM_TARGET pTarget)
{
    RT_NOREF(cScreens);
    if (pTarget->VidPnSourceId == D3DDDI_ID_UNINITIALIZED)
        return;

    Assert(pTarget->VidPnSourceId < cScreens);

    PVPOXWDDM_SOURCE pSource = &paSources[pTarget->VidPnSourceId];
    if (!pSource)
        return;
    Assert(pSource->cTargets);
    Assert(ASMBitTest(pSource->aTargetMap, pTarget->u32Id));
    ASMBitClear(pSource->aTargetMap, pTarget->u32Id);
    pSource->cTargets--;
    pTarget->VidPnSourceId = D3DDDI_ID_UNINITIALIZED;

    pTarget->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY;
    pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY;
}

void VPoxVidPnStSourceTargetAdd(PVPOXWDDM_SOURCE paSources, uint32_t cScreens, PVPOXWDDM_SOURCE pSource, PVPOXWDDM_TARGET pTarget)
{
    if (pTarget->VidPnSourceId == pSource->AllocData.SurfDesc.VidPnSourceId)
        return;

    VPoxVidPnStTargetCleanup(paSources, cScreens, pTarget);

    ASMBitSet(pSource->aTargetMap, pTarget->u32Id);
    pSource->cTargets++;
    pTarget->VidPnSourceId = pSource->AllocData.SurfDesc.VidPnSourceId;

    pTarget->fBlankedByPowerOff = RT_BOOL(pSource->bBlankedByPowerOff);
    LOG(("src %d and tgt %d are now blank %d",
        pSource->AllocData.SurfDesc.VidPnSourceId, pTarget->u32Id, pTarget->fBlankedByPowerOff));

    pTarget->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY;
    pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY;
}

void VPoxVidPnStTIterInit(PVPOXWDDM_SOURCE pSource, PVPOXWDDM_TARGET paTargets, uint32_t cTargets, VPOXWDDM_TARGET_ITER *pIter)
{
    pIter->pSource = pSource;
    pIter->paTargets = paTargets;
    pIter->cTargets = cTargets;
    pIter->i = 0;
    pIter->c = 0;
}

PVPOXWDDM_TARGET VPoxVidPnStTIterNext(VPOXWDDM_TARGET_ITER *pIter)
{
    PVPOXWDDM_SOURCE pSource = pIter->pSource;
    if (pSource->cTargets <= pIter->c)
        return NULL;

    int i =  (!pIter->c) ? ASMBitFirstSet(pSource->aTargetMap, pIter->cTargets)
            : ASMBitNextSet(pSource->aTargetMap, pIter->cTargets, pIter->i);
    if (i < 0)
        STOP_FATAL();

    pIter->i = (uint32_t)i;
    pIter->c++;
    return &pIter->paTargets[i];
}

void VPoxVidPnStSourceCleanup(PVPOXWDDM_SOURCE paSources, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, PVPOXWDDM_TARGET paTargets, uint32_t cTargets)
{
    PVPOXWDDM_SOURCE pSource = &paSources[VidPnSourceId];
    VPOXWDDM_TARGET_ITER Iter;
    VPoxVidPnStTIterInit(pSource, paTargets, cTargets, &Iter);
    for (PVPOXWDDM_TARGET pTarget = VPoxVidPnStTIterNext(&Iter);
            pTarget;
            pTarget = VPoxVidPnStTIterNext(&Iter))
    {
        Assert(pTarget->VidPnSourceId == pSource->AllocData.SurfDesc.VidPnSourceId);
        VPoxVidPnStTargetCleanup(paSources, cTargets, pTarget);
        /* iterator is not safe wrt target removal, reinit it */
        VPoxVidPnStTIterInit(pSource, paTargets, cTargets, &Iter);
    }
}

void VPoxVidPnStCleanup(PVPOXWDDM_SOURCE paSources, PVPOXWDDM_TARGET paTargets, uint32_t cScreens)
{
    for (UINT i = 0; i < cScreens; ++i)
    {
        PVPOXWDDM_TARGET pTarget = &paTargets[i];
        VPoxVidPnStTargetCleanup(paSources, cScreens, pTarget);
    }
}

void VPoxVidPnAllocDataInit(VPOXWDDM_ALLOC_DATA *pData, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    memset(pData, 0, sizeof (*pData));
    pData->SurfDesc.VidPnSourceId = VidPnSourceId;
    pData->Addr.offVram = VPOXVIDEOOFFSET_VOID;
}

void VPoxVidPnSourceInit(PVPOXWDDM_SOURCE pSource, const D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, uint8_t u8SyncState)
{
    memset(pSource, 0, sizeof (*pSource));
    VPoxVidPnAllocDataInit(&pSource->AllocData, VidPnSourceId);
    pSource->u8SyncState = (u8SyncState & VPOXWDDM_HGSYNC_F_SYNCED_ALL);
}

void VPoxVidPnTargetInit(PVPOXWDDM_TARGET pTarget, const D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, uint8_t u8SyncState)
{
    memset(pTarget, 0, sizeof (*pTarget));
    pTarget->u32Id = VidPnTargetId;
    pTarget->VidPnSourceId = D3DDDI_ID_UNINITIALIZED;
    pTarget->u8SyncState = (u8SyncState & VPOXWDDM_HGSYNC_F_SYNCED_ALL);
}

void VPoxVidPnSourcesInit(PVPOXWDDM_SOURCE pSources, uint32_t cScreens, uint8_t u8SyncState)
{
    for (uint32_t i = 0; i < cScreens; ++i)
        VPoxVidPnSourceInit(&pSources[i], i, u8SyncState);
}

void VPoxVidPnTargetsInit(PVPOXWDDM_TARGET pTargets, uint32_t cScreens, uint8_t u8SyncState)
{
    for (uint32_t i = 0; i < cScreens; ++i)
        VPoxVidPnTargetInit(&pTargets[i], i, u8SyncState);
}

void VPoxVidPnSourceCopy(VPOXWDDM_SOURCE *pDst, const VPOXWDDM_SOURCE *pSrc)
{
    uint8_t u8SyncState = pDst->u8SyncState;
    *pDst = *pSrc;
    pDst->u8SyncState &= u8SyncState;
}

void VPoxVidPnTargetCopy(VPOXWDDM_TARGET *pDst, const VPOXWDDM_TARGET *pSrc)
{
    uint8_t u8SyncState = pDst->u8SyncState;
    *pDst = *pSrc;
    pDst->u8SyncState &= u8SyncState;
}

void VPoxVidPnSourcesCopy(VPOXWDDM_SOURCE *pDst, const VPOXWDDM_SOURCE *pSrc, uint32_t cScreens)
{
    for (uint32_t i = 0; i < cScreens; ++i)
        VPoxVidPnSourceCopy(&pDst[i], &pSrc[i]);
}

void VPoxVidPnTargetsCopy(VPOXWDDM_TARGET *pDst, const VPOXWDDM_TARGET *pSrc, uint32_t cScreens)
{
    for (uint32_t i = 0; i < cScreens; ++i)
        VPoxVidPnTargetCopy(&pDst[i], &pSrc[i]);
}

void VPoxDumpSourceTargetArrays(VPOXWDDM_SOURCE *paSources, VPOXWDDM_TARGET *paTargets, uint32_t cScreens)
{
    RT_NOREF(paSources, paTargets, cScreens);

    for (uint32_t i = 0; i < cScreens; i++)
    {
        LOG_EXACT(("source [%d] Sync 0x%x, cTgt %d, TgtMap0 0x%x, TgtRep %d, blanked %d\n",
            i, paSources[i].u8SyncState, paSources[i].cTargets, paSources[i].aTargetMap[0], paSources[i].fTargetsReported, paSources[i].bBlankedByPowerOff));

        LOG_EXACT(("target [%d] Sync 0x%x, VidPnSourceId %d, blanked %d\n",
            i, paTargets[i].u8SyncState, paTargets[i].VidPnSourceId, paTargets[i].fBlankedByPowerOff));
    }
}

static D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE vpoxVidPnCofuncModalityCurrentPathPivot(D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmPivot,
                    const DXGK_ENUM_PIVOT *pPivot,
                    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    switch (enmPivot)
    {
        case D3DKMDT_EPT_VIDPNSOURCE:
            if (pPivot->VidPnSourceId == VidPnSourceId)
                return D3DKMDT_EPT_VIDPNSOURCE;
            if (pPivot->VidPnSourceId == D3DDDI_ID_ALL)
            {
#ifdef DEBUG_misha
                AssertFailed();
#endif
                return D3DKMDT_EPT_VIDPNSOURCE;
            }
            return D3DKMDT_EPT_NOPIVOT;
        case D3DKMDT_EPT_VIDPNTARGET:
            if (pPivot->VidPnTargetId == VidPnTargetId)
                return D3DKMDT_EPT_VIDPNTARGET;
            if (pPivot->VidPnTargetId == D3DDDI_ID_ALL)
            {
#ifdef DEBUG_misha
                AssertFailed();
#endif
                return D3DKMDT_EPT_VIDPNTARGET;
            }
            return D3DKMDT_EPT_NOPIVOT;
        case D3DKMDT_EPT_SCALING:
        case D3DKMDT_EPT_ROTATION:
        case D3DKMDT_EPT_NOPIVOT:
            return D3DKMDT_EPT_NOPIVOT;
        default:
            WARN(("unexpected pivot"));
            return D3DKMDT_EPT_NOPIVOT;
    }
}

NTSTATUS vpoxVidPnQueryPinnedTargetMode(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, RTRECTSIZE *pSize)
{
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;
    pSize->cx = 0;
    pSize->cy = 0;
    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnAcquireTargetModeSet failed Status(0x%x)", Status));
        return Status;
    }

    CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;
    Status = pCurVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
    if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
    {
        pPinnedVidPnTargetModeInfo = NULL;
        Status = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(Status))
    {
        WARN(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));
    }
    else
    {
        Assert(pPinnedVidPnTargetModeInfo);
        pSize->cx = pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cx;
        pSize->cy = pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy;
        NTSTATUS rcNt2 = pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        AssertNtStatus(rcNt2);
    }

    NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    AssertNtStatusSuccess(rcNt2);

    return Status;
}

NTSTATUS vpoxVidPnQueryPinnedSourceMode(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RTRECTSIZE *pSize)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;
    pSize->cx = 0;
    pSize->cy = 0;
    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnAcquireSourceModeSet failed Status(0x%x)", Status));
        return Status;
    }

    CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;
    Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
    if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
    {
        pPinnedVidPnSourceModeInfo = NULL;
        Status = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(Status))
    {
        WARN(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));
    }
    else
    {
        Assert(pPinnedVidPnSourceModeInfo);
        pSize->cx = pPinnedVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cx;
        pSize->cy = pPinnedVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cy;
        NTSTATUS rcNt2 = pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        AssertNtStatus(rcNt2);
    }

    NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    AssertNtStatusSuccess(rcNt2);

    return Status;
}

static NTSTATUS vpoxVidPnSourceModeSetToArray(D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet,
                    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface,
                    CR_SORTARRAY *pArray)
{
    VPOXVIDPN_SOURCEMODE_ITER Iter;
    const D3DKMDT_VIDPN_SOURCE_MODE *pVidPnModeInfo;

    VPoxVidPnSourceModeIterInit(&Iter, hVidPnModeSet, pVidPnModeSetInterface);

    while ((pVidPnModeInfo = VPoxVidPnSourceModeIterNext(&Iter)) != NULL)
    {
        RTRECTSIZE size;
        size.cx = pVidPnModeInfo->Format.Graphics.VisibleRegionSize.cx;
        size.cy = pVidPnModeInfo->Format.Graphics.VisibleRegionSize.cy;
        int rc = CrSaAdd(pArray, CR_RSIZE2U64(size));
        if (RT_FAILURE(rc))
        {
            WARN(("CrSaAdd failed %d", rc));
            VPoxVidPnSourceModeIterTerm(&Iter);
            return STATUS_UNSUCCESSFUL;
        }
    }

    VPoxVidPnSourceModeIterTerm(&Iter);

    return VPoxVidPnSourceModeIterStatus(&Iter);
}

static NTSTATUS vpoxVidPnSourceModeSetFromArray(D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet,
        const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface,
        const CR_SORTARRAY *pArray)
{
    for (uint32_t i = 0; i < CrSaGetSize(pArray); ++i)
    {
        RTRECTSIZE size = CR_U642RSIZE(CrSaGetVal(pArray, i));

        D3DKMDT_VIDPN_SOURCE_MODE *pVidPnModeInfo;
        NTSTATUS Status = pVidPnModeSetInterface->pfnCreateNewModeInfo(hVidPnModeSet, &pVidPnModeInfo);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnCreateNewModeInfo failed, Status 0x%x", Status));
            return Status;
        }

        vpoxVidPnPopulateSourceModeInfo(pVidPnModeInfo, &size);

        Status = pVidPnModeSetInterface->pfnAddMode(hVidPnModeSet, pVidPnModeInfo);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnAddMode (%d x %d) failed, Status 0x%x", size.cx, size.cy, Status));
            VPoxVidPnDumpSourceMode("SourceMode: ", pVidPnModeInfo, "\n");
            NTSTATUS rcNt2 = pVidPnModeSetInterface->pfnReleaseModeInfo(hVidPnModeSet, pVidPnModeInfo);
            AssertNtStatusSuccess(rcNt2);
            // Continue adding modes into modeset even if a mode was rejected
            continue;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVidPnTargetModeSetToArray(D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet,
                    const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface,
                    CR_SORTARRAY *pArray)
{
    VPOXVIDPN_TARGETMODE_ITER Iter;
    const D3DKMDT_VIDPN_TARGET_MODE *pVidPnModeInfo;

    VPoxVidPnTargetModeIterInit(&Iter, hVidPnModeSet, pVidPnModeSetInterface);

    while ((pVidPnModeInfo = VPoxVidPnTargetModeIterNext(&Iter)) != NULL)
    {
        RTRECTSIZE size;
        size.cx = pVidPnModeInfo->VideoSignalInfo.ActiveSize.cx;
        size.cy = pVidPnModeInfo->VideoSignalInfo.ActiveSize.cy;
        int rc = CrSaAdd(pArray, CR_RSIZE2U64(size));
        if (RT_FAILURE(rc))
        {
            WARN(("CrSaAdd failed %d", rc));
            VPoxVidPnTargetModeIterTerm(&Iter);
            return STATUS_UNSUCCESSFUL;
        }
    }

    VPoxVidPnTargetModeIterTerm(&Iter);

    return VPoxVidPnTargetModeIterStatus(&Iter);
}

static NTSTATUS vpoxVidPnTargetModeSetFromArray(D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet,
        const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface,
        const CR_SORTARRAY *pArray)
{
    for (uint32_t i = 0; i < CrSaGetSize(pArray); ++i)
    {
        RTRECTSIZE size = CR_U642RSIZE(CrSaGetVal(pArray, i));

        D3DKMDT_VIDPN_TARGET_MODE *pVidPnModeInfo;
        NTSTATUS Status = pVidPnModeSetInterface->pfnCreateNewModeInfo(hVidPnModeSet, &pVidPnModeInfo);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnCreateNewModeInfo failed, Status 0x%x", Status));
            return Status;
        }

        vpoxVidPnPopulateTargetModeInfo(pVidPnModeInfo, &size);

        Status = pVidPnModeSetInterface->pfnAddMode(hVidPnModeSet, pVidPnModeInfo);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnAddMode (%d x %d) failed, Status 0x%x", size.cx, size.cy, Status));
            VPoxVidPnDumpTargetMode("TargetMode: ", pVidPnModeInfo, "\n");
            NTSTATUS rcNt2 = pVidPnModeSetInterface->pfnReleaseModeInfo(hVidPnModeSet, pVidPnModeInfo);
            AssertNtStatusSuccess(rcNt2);
            // Continue adding modes into modeset even if a mode was rejected
            continue;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVidPnMonitorModeSetToArray(D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet,
                    const DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface,
                    CR_SORTARRAY *pArray)
{
    VPOXVIDPN_MONITORMODE_ITER Iter;
    const D3DKMDT_MONITOR_SOURCE_MODE *pVidPnModeInfo;

    VPoxVidPnMonitorModeIterInit(&Iter, hVidPnModeSet, pVidPnModeSetInterface);

    while ((pVidPnModeInfo = VPoxVidPnMonitorModeIterNext(&Iter)) != NULL)
    {
        RTRECTSIZE size;
        size.cx = pVidPnModeInfo->VideoSignalInfo.ActiveSize.cx;
        size.cy = pVidPnModeInfo->VideoSignalInfo.ActiveSize.cy;
        int rc = CrSaAdd(pArray, CR_RSIZE2U64(size));
        if (RT_FAILURE(rc))
        {
            WARN(("CrSaAdd failed %d", rc));
            VPoxVidPnMonitorModeIterTerm(&Iter);
            return STATUS_UNSUCCESSFUL;
        }
    }

    VPoxVidPnMonitorModeIterTerm(&Iter);

    return VPoxVidPnMonitorModeIterStatus(&Iter);
}

static NTSTATUS vpoxVidPnMonitorModeSetFromArray(D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet,
        const DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface,
        const CR_SORTARRAY *pArray)
{
    for (uint32_t i = 0; i < CrSaGetSize(pArray); ++i)
    {
        RTRECTSIZE size = CR_U642RSIZE(CrSaGetVal(pArray, i));

        D3DKMDT_MONITOR_SOURCE_MODE *pVidPnModeInfo;
        NTSTATUS Status = pVidPnModeSetInterface->pfnCreateNewModeInfo(hVidPnModeSet, &pVidPnModeInfo);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnCreateNewModeInfo failed, Status 0x%x", Status));
            return Status;
        }

        vpoxVidPnPopulateMonitorModeInfo(pVidPnModeInfo, &size);

        Status = pVidPnModeSetInterface->pfnAddMode(hVidPnModeSet, pVidPnModeInfo);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnAddMode (%d x %d) failed, Status 0x%x", size.cx, size.cy, Status));
            NTSTATUS rcNt2 = pVidPnModeSetInterface->pfnReleaseModeInfo(hVidPnModeSet, pVidPnModeInfo);
            AssertNtStatusSuccess(rcNt2);
            // Continue adding modes into modeset even if a mode was rejected
            continue;
        }

        LOGF(("mode (%d x %d) added to monitor modeset", size.cx, size.cy));
    }

    return STATUS_SUCCESS;
}


static NTSTATUS vpoxVidPnCollectInfoForPathTarget(PVPOXMP_DEVEXT pDevExt,
        D3DKMDT_HVIDPN hVidPn,
        const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmCurPivot,
        uint32_t *aAdjustedModeMap,
        CR_SORTARRAY *aModes,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    const CR_SORTARRAY* pSupportedModes = VPoxWddmVModesGet(pDevExt, VidPnTargetId);
    NTSTATUS Status;
    if (enmCurPivot == D3DKMDT_EPT_VIDPNTARGET)
    {
        D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet;
        const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface;
        Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                    VidPnTargetId,
                    &hVidPnModeSet,
                    &pVidPnModeSetInterface);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnAcquireTargetModeSet failed %#x", Status));
            return Status;
        }

        /* intersect modes from target */
        if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
        {
            Status = vpoxVidPnTargetModeSetToArray(hVidPnModeSet, pVidPnModeSetInterface, &aModes[VidPnTargetId]);
            ASMBitSet(aAdjustedModeMap, VidPnTargetId);
        }
        else
        {
            CR_SORTARRAY Arr;
            CrSaInit(&Arr, 0);
            Status = vpoxVidPnTargetModeSetToArray(hVidPnModeSet, pVidPnModeSetInterface, &aModes[VidPnTargetId]);
            CrSaIntersect(&aModes[VidPnTargetId], &Arr);
            CrSaCleanup(&Arr);
        }

        NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hVidPnModeSet);
        AssertNtStatusSuccess(rcNt2);

        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnTargetModeSetToArray failed %#x", Status));
            return Status;
        }

        return STATUS_SUCCESS;
    }

    RTRECTSIZE pinnedSize = {0};
    Status = vpoxVidPnQueryPinnedTargetMode(hVidPn, pVidPnInterface, VidPnTargetId, &pinnedSize);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnQueryPinnedTargetMode failed %#x", Status));
        return Status;
    }

    if (pinnedSize.cx)
    {
        Assert(CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)));

        if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
        {
            Assert(CrSaGetSize(&aModes[VidPnTargetId]) == 0);
            int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
            if (!RT_SUCCESS(rc))
            {
                WARN(("CrSaAdd failed %d", rc));
                return STATUS_UNSUCCESSFUL;
            }
            ASMBitSet(aAdjustedModeMap, VidPnTargetId);
        }
        else
        {
            CrSaClear(&aModes[VidPnTargetId]);
            int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
            if (!RT_SUCCESS(rc))
            {
                WARN(("CrSaAdd failed %d", rc));
                return STATUS_UNSUCCESSFUL;
            }
        }

        return STATUS_SUCCESS;
    }


    Status = vpoxVidPnQueryPinnedSourceMode(hVidPn, pVidPnInterface, VidPnSourceId, &pinnedSize);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnQueryPinnedSourceMode failed %#x", Status));
        return Status;
    }

    if (pinnedSize.cx)
    {
        if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
        {
            Assert(CrSaGetSize(&aModes[VidPnTargetId]) == 0);
            if (CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)))
            {
                int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
                if (!RT_SUCCESS(rc))
                {
                    WARN(("CrSaAdd failed %d", rc));
                    return STATUS_UNSUCCESSFUL;
                }
            }
            ASMBitSet(aAdjustedModeMap, VidPnTargetId);
        }
        else
        {
            CrSaClear(&aModes[VidPnTargetId]);
            if (CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)))
            {
                int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
                if (!RT_SUCCESS(rc))
                {
                    WARN(("CrSaAdd failed %d", rc));
                    return STATUS_UNSUCCESSFUL;
                }
            }
        }

        return STATUS_SUCCESS;
    }

    /* now we are here because no pinned info is specified, we need to populate it based on the supported info
     * and modes already configured,
     * this is pretty simple actually */

    if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
    {
        Assert(CrSaGetSize(&aModes[VidPnTargetId]) == 0);
        int rc = CrSaClone(pSupportedModes, &aModes[VidPnTargetId]);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrSaClone failed %d", rc));
            return STATUS_UNSUCCESSFUL;
        }
        ASMBitSet(aAdjustedModeMap, VidPnTargetId);
    }
    else
    {
        CrSaIntersect(&aModes[VidPnTargetId], pSupportedModes);
    }

    /* we are done */
    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVidPnApplyInfoForPathTarget(PVPOXMP_DEVEXT pDevExt,
        D3DKMDT_HVIDPN hVidPn,
        const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmCurPivot,
        uint32_t *aAdjustedModeMap,
        const CR_SORTARRAY *aModes,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    RT_NOREF(aAdjustedModeMap, VidPnSourceId);
    Assert(ASMBitTest(aAdjustedModeMap, VidPnTargetId));

    if (enmCurPivot == D3DKMDT_EPT_VIDPNTARGET)
        return STATUS_SUCCESS;

    RTRECTSIZE pinnedSize = {0};
    NTSTATUS Status = vpoxVidPnQueryPinnedTargetMode(hVidPn, pVidPnInterface, VidPnTargetId, &pinnedSize);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnQueryPinnedTargetMode failed %#x", Status));
        return Status;
    }

    if (pinnedSize.cx)
        return STATUS_SUCCESS;

    /* now just create the new source mode set and apply it */
    D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface;
    Status = pVidPnInterface->pfnCreateNewTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hVidPnModeSet,
                        &pVidPnModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnCreateNewTargetModeSet failed Status(0x%x)", Status));
        return Status;
    }

    Status = vpoxVidPnTargetModeSetFromArray(hVidPnModeSet,
            pVidPnModeSetInterface,
            &aModes[VidPnTargetId]);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnTargetModeSetFromArray failed Status(0x%x)", Status));
        vpoxVidPnDumpVidPn("\nVidPn: ---------\n", pDevExt, hVidPn, pVidPnInterface, "\n------\n");
        VPoxVidPnDumpMonitorModeSet("MonModeSet: --------\n", pDevExt, VidPnTargetId, "\n------\n");
        NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hVidPnModeSet);
        AssertNtStatusSuccess(rcNt2);
        return Status;
    }

    Status = pVidPnInterface->pfnAssignTargetModeSet(hVidPn, VidPnTargetId, hVidPnModeSet);
    if (!NT_SUCCESS(Status))
    {
        WARN(("\n\n!!!!!!!\n\n pfnAssignTargetModeSet failed, Status(0x%x)", Status));
        vpoxVidPnDumpVidPn("\nVidPn: ---------\n", pDevExt, hVidPn, pVidPnInterface, "\n------\n");
        VPoxVidPnDumpMonitorModeSet("MonModeSet: --------\n", pDevExt, VidPnTargetId, "\n------\n");
        NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hVidPnModeSet);
        AssertNtStatusSuccess(rcNt2);
        return Status;
    }

    Status = vpoxVidPnCheckMonitorModes(pDevExt, VidPnTargetId, &aModes[VidPnTargetId]);

    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnCheckMonitorModes failed, Status(0x%x)", Status));
        return Status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVidPnApplyInfoForPathSource(PVPOXMP_DEVEXT pDevExt,
        D3DKMDT_HVIDPN hVidPn,
        const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmCurPivot,
        uint32_t *aAdjustedModeMap,
        const CR_SORTARRAY *aModes,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    RT_NOREF(aAdjustedModeMap);
    Assert(ASMBitTest(aAdjustedModeMap, VidPnTargetId));

    if (enmCurPivot == D3DKMDT_EPT_VIDPNSOURCE)
        return STATUS_SUCCESS;

    RTRECTSIZE pinnedSize = {0};
    NTSTATUS Status = vpoxVidPnQueryPinnedSourceMode(hVidPn, pVidPnInterface, VidPnSourceId, &pinnedSize);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnQueryPinnedSourceMode failed %#x", Status));
        return Status;
    }

    if (pinnedSize.cx)
        return STATUS_SUCCESS;

    /* now just create the new source mode set and apply it */
    D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;
    Status = pVidPnInterface->pfnCreateNewSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hVidPnModeSet,
                        &pVidPnModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnCreateNewSourceModeSet failed Status(0x%x)", Status));
        return Status;
    }

    Status = vpoxVidPnSourceModeSetFromArray(hVidPnModeSet,
            pVidPnModeSetInterface,
            &aModes[VidPnTargetId]); /* <- target modes always! */
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnSourceModeSetFromArray failed Status(0x%x)", Status));
        vpoxVidPnDumpVidPn("\nVidPn: ---------\n", pDevExt, hVidPn, pVidPnInterface, "\n------\n");
        VPoxVidPnDumpMonitorModeSet("MonModeSet: --------\n", pDevExt, VidPnTargetId, "\n------\n");
        NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hVidPnModeSet);
        AssertNtStatusSuccess(rcNt2);
        return Status;
    }

    Status = pVidPnInterface->pfnAssignSourceModeSet(hVidPn, VidPnSourceId, hVidPnModeSet);
    if (!NT_SUCCESS(Status))
    {
        WARN(("\n\n!!!!!!!\n\n pfnAssignSourceModeSet failed, Status(0x%x)", Status));
        vpoxVidPnDumpVidPn("\nVidPn: ---------\n", pDevExt, hVidPn, pVidPnInterface, "\n------\n");
        VPoxVidPnDumpMonitorModeSet("MonModeSet: --------\n", pDevExt, VidPnTargetId, "\n------\n");
        NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hVidPnModeSet);
        AssertNtStatusSuccess(rcNt2);
        return Status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVidPnCollectInfoForPathSource(PVPOXMP_DEVEXT pDevExt,
        D3DKMDT_HVIDPN hVidPn,
        const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmCurPivot,
        uint32_t *aAdjustedModeMap,
        CR_SORTARRAY *aModes,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    const CR_SORTARRAY* pSupportedModes = VPoxWddmVModesGet(pDevExt, VidPnTargetId); /* <- yes, modes are target-determined always */
    NTSTATUS Status;

    if (enmCurPivot == D3DKMDT_EPT_VIDPNSOURCE)
    {
        D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet;
        const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;
        Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                    VidPnSourceId,
                    &hVidPnModeSet,
                    &pVidPnModeSetInterface);
        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnAcquireSourceModeSet failed %#x", Status));
            return Status;
        }

        /* intersect modes from target */
        if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
        {
            Status = vpoxVidPnSourceModeSetToArray(hVidPnModeSet, pVidPnModeSetInterface, &aModes[VidPnTargetId]);
            ASMBitSet(aAdjustedModeMap, VidPnTargetId);
        }
        else
        {
            CR_SORTARRAY Arr;
            CrSaInit(&Arr, 0);
            Status = vpoxVidPnSourceModeSetToArray(hVidPnModeSet, pVidPnModeSetInterface, &aModes[VidPnTargetId]);
            CrSaIntersect(&aModes[VidPnTargetId], &Arr);
            CrSaCleanup(&Arr);
        }

        NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hVidPnModeSet);
        AssertNtStatusSuccess(rcNt2);

        if (!NT_SUCCESS(Status))
        {
            WARN(("pfnReleaseSourceModeSet failed %#x", Status));
            return Status;
        }

        /* intersect it with supported target modes, just in case */
        CrSaIntersect(&aModes[VidPnTargetId], pSupportedModes);

        return STATUS_SUCCESS;
    }

    RTRECTSIZE pinnedSize = {0};
    Status = vpoxVidPnQueryPinnedSourceMode(hVidPn, pVidPnInterface, VidPnSourceId, &pinnedSize);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnQueryPinnedSourceMode failed %#x", Status));
        return Status;
    }

    if (pinnedSize.cx)
    {
        Assert(CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)));

        if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
        {
            Assert(CrSaGetSize(&aModes[VidPnTargetId]) == 0);

            if (CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)))
            {
                int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
                if (!RT_SUCCESS(rc))
                {
                    WARN(("CrSaAdd failed %d", rc));
                    return STATUS_UNSUCCESSFUL;
                }
            }
            ASMBitSet(aAdjustedModeMap, VidPnTargetId);
        }
        else
        {
            CrSaClear(&aModes[VidPnTargetId]);
            if (CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)))
            {
                int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
                if (!RT_SUCCESS(rc))
                {
                    WARN(("CrSaAdd failed %d", rc));
                    return STATUS_UNSUCCESSFUL;
                }
            }
        }

        return STATUS_SUCCESS;
    }


    Status = vpoxVidPnQueryPinnedTargetMode(hVidPn, pVidPnInterface, VidPnTargetId, &pinnedSize);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnQueryPinnedTargetMode failed %#x", Status));
        return Status;
    }

    if (pinnedSize.cx)
    {
        Assert(CrSaContains(pSupportedModes, CR_RSIZE2U64(pinnedSize)));

        if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
        {
            Assert(CrSaGetSize(&aModes[VidPnTargetId]) == 0);
            int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
            if (!RT_SUCCESS(rc))
            {
                WARN(("CrSaAdd failed %d", rc));
                return STATUS_UNSUCCESSFUL;
            }
            ASMBitSet(aAdjustedModeMap, VidPnTargetId);
        }
        else
        {
            CrSaClear(&aModes[VidPnTargetId]);
            int rc = CrSaAdd(&aModes[VidPnTargetId], CR_RSIZE2U64(pinnedSize));
            if (!RT_SUCCESS(rc))
            {
                WARN(("CrSaAdd failed %d", rc));
                return STATUS_UNSUCCESSFUL;
            }
        }

        return STATUS_SUCCESS;
    }

    /* now we are here because no pinned info is specified, we need to populate it based on the supported info
     * and modes already configured,
     * this is pretty simple actually */

    if (!ASMBitTest(aAdjustedModeMap, VidPnTargetId))
    {
        Assert(CrSaGetSize(&aModes[VidPnTargetId]) == 0);
        int rc = CrSaClone(pSupportedModes, &aModes[VidPnTargetId]);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrSaClone failed %d", rc));
            return STATUS_UNSUCCESSFUL;
        }
        ASMBitSet(aAdjustedModeMap, VidPnTargetId);
    }
    else
    {
        CrSaIntersect(&aModes[VidPnTargetId], pSupportedModes);
    }

    /* we are done */
    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVidPnCheckMonitorModes(PVPOXMP_DEVEXT pDevExt, uint32_t u32Target, const CR_SORTARRAY *pSupportedModes)
{
    NTSTATUS Status;
    CONST DXGK_MONITOR_INTERFACE *pMonitorInterface;
    Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryMonitorInterface(pDevExt->u.primary.DxgkInterface.DeviceHandle, DXGK_MONITOR_INTERFACE_VERSION_V1, &pMonitorInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
        return Status;
    }

    D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet;
    CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;

    if (!pSupportedModes)
    {
        pSupportedModes = VPoxWddmVModesGet(pDevExt, u32Target);
    }

    CR_SORTARRAY DiffModes;
    int rc = CrSaInit(&DiffModes, CrSaGetSize(pSupportedModes));
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrSaInit failed"));
        return STATUS_NO_MEMORY;
    }


    Status = pMonitorInterface->pfnAcquireMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle,
                                        u32Target,
                                        &hVidPnModeSet,
                                        &pVidPnModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
//        if (Status == STATUS_GRAPHICS_MONITOR_NOT_CONNECTED)
        CrSaCleanup(&DiffModes);
        return Status;
    }

    VPOXVIDPN_MONITORMODE_ITER Iter;
    const D3DKMDT_MONITOR_SOURCE_MODE *pVidPnModeInfo;

    rc = CrSaClone(pSupportedModes, &DiffModes);
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrSaClone failed"));
        Status = STATUS_NO_MEMORY;
        goto done;
    }

    VPoxVidPnMonitorModeIterInit(&Iter, hVidPnModeSet, pVidPnModeSetInterface);

    while ((pVidPnModeInfo = VPoxVidPnMonitorModeIterNext(&Iter)) != NULL)
    {
        RTRECTSIZE size;
        size.cx = pVidPnModeInfo->VideoSignalInfo.ActiveSize.cx;
        size.cy = pVidPnModeInfo->VideoSignalInfo.ActiveSize.cy;
        CrSaRemove(&DiffModes, CR_RSIZE2U64(size));
        LOGF(("mode (%d x %d) is already in monitor modeset\n", size.cx, size.cy));
    }

    VPoxVidPnMonitorModeIterTerm(&Iter);

    Status = VPoxVidPnMonitorModeIterStatus(&Iter);
    if (!NT_SUCCESS(Status))
    {
        WARN(("iter status failed %#x", Status));
        goto done;
    }

    LOGF(("Adding %d additional modes to monitor modeset\n", CrSaGetSize(&DiffModes)));

    Status = vpoxVidPnMonitorModeSetFromArray(hVidPnModeSet, pVidPnModeSetInterface, &DiffModes);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnMonitorModeSetFromArray failed %#x", Status));
        goto done;
    }

done:
    NTSTATUS rcNt2 = pMonitorInterface->pfnReleaseMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle, hVidPnModeSet);
    if (!NT_SUCCESS(rcNt2))
        WARN(("pfnReleaseMonitorSourceModeSet failed rcNt2(0x%x)", rcNt2));

    CrSaCleanup(&DiffModes);

    return Status;
}

static NTSTATUS vpoxVidPnPathAdd(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        const D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, const D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId,
        D3DKMDT_VIDPN_PRESENT_PATH_IMPORTANCE enmImportance)
{
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    NTSTATUS Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo;
    Status = pVidPnTopologyInterface->pfnCreateNewPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    pNewVidPnPresentPathInfo->VidPnSourceId = VidPnSourceId;
    pNewVidPnPresentPathInfo->VidPnTargetId = VidPnTargetId;
    pNewVidPnPresentPathInfo->ImportanceOrdinal = enmImportance;
    pNewVidPnPresentPathInfo->ContentTransformation.Scaling = D3DKMDT_VPPS_IDENTITY;
    memset(&pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport,
            0, sizeof (pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport));
    pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Identity = 1;
    pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Stretched = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.Rotation = D3DKMDT_VPPR_IDENTITY;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Identity = 1;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate180 = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate270 = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate90 = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy = 0;
    pNewVidPnPresentPathInfo->VidPnTargetColorBasis = D3DKMDT_CB_SRGB; /** @todo how does it matters? */
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FirstChannel =  8;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.SecondChannel =  8;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.ThirdChannel =  8;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel =  0;
    pNewVidPnPresentPathInfo->Content = D3DKMDT_VPPC_GRAPHICS;
    pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType = D3DKMDT_VPPMT_UNINITIALIZED;
//                    pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType = D3DKMDT_VPPMT_NOPROTECTION;
    pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits = 0;
    memset(&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, 0, sizeof (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport));
//            pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport.NoProtection  = 1;
    memset (&pNewVidPnPresentPathInfo->GammaRamp, 0, sizeof (pNewVidPnPresentPathInfo->GammaRamp));
//            pNewVidPnPresentPathInfo->GammaRamp.Type = D3DDDI_GAMMARAMP_DEFAULT;
//            pNewVidPnPresentPathInfo->GammaRamp.DataSize = 0;
    Status = pVidPnTopologyInterface->pfnAddPath(hVidPnTopology, pNewVidPnPresentPathInfo);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        NTSTATUS rcNt2 = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);
        AssertNtStatus(rcNt2);
    }

    LOG(("Recommended Path (%d->%d)", VidPnSourceId, VidPnTargetId));

    return Status;
}

NTSTATUS VPoxVidPnRecommendMonitorModes(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_TARGET_ID VideoPresentTargetId,
                        D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet, const DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface)
{
    const CR_SORTARRAY *pSupportedModes = VPoxWddmVModesGet(pDevExt, VideoPresentTargetId);

    NTSTATUS Status = vpoxVidPnMonitorModeSetFromArray(hVidPnModeSet, pVidPnModeSetInterface, pSupportedModes);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vpoxVidPnMonitorModeSetFromArray failed %d", Status));
        return Status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS VPoxVidPnUpdateModes(PVPOXMP_DEVEXT pDevExt, uint32_t u32TargetId, const RTRECTSIZE *pSize)
{
    LOGF(("ENTER u32TargetId(%d) mode(%d x %d)", u32TargetId, pSize->cx, pSize->cy));

    if (u32TargetId >= (uint32_t)VPoxCommonFromDeviceExt(pDevExt)->cDisplays)
    {
        WARN(("invalid target id"));
        return STATUS_INVALID_PARAMETER;
    }

    int rc = VPoxWddmVModesAdd(pDevExt, u32TargetId, pSize, TRUE);
    LOGF(("VPoxWddmVModesAdd returned (%d)", rc));

    if (RT_FAILURE(rc))
    {
        WARN(("VPoxWddmVModesAdd failed %d", rc));
        return STATUS_UNSUCCESSFUL;
    }

    if (rc == VINF_ALREADY_INITIALIZED)
    {
        /* mode was already in list, just return */
        Assert(CrSaContains(VPoxWddmVModesGet(pDevExt, u32TargetId), CR_RSIZE2U64(*pSize)));
        LOGF(("LEAVE mode was already in modeset, just return"));
        return STATUS_SUCCESS;
    }

#ifdef VPOX_WDDM_REPLUG_ON_MODE_CHANGE
    /* The VPOXESC_UPDATEMODES is a hint for the driver to use new display mode as soon as VidPn
     * manager will ask for it.
     * Probably, some new interface is required to plug/unplug displays by calling
     * VPoxWddmChildStatusReportReconnected.
     * But it is a bad idea to mix sending a display mode hint and (un)plug displays in VPOXESC_UPDATEMODES.
     */

    /* modes have changed, need to replug */
    NTSTATUS Status = VPoxWddmChildStatusReportReconnected(pDevExt, u32TargetId);
    LOG(("VPoxWddmChildStatusReportReconnected returned (%d)", Status));
    if (!NT_SUCCESS(Status))
    {
        WARN(("VPoxWddmChildStatusReportReconnected failed Status(%#x)", Status));
        return Status;
    }
#endif

    LOGF(("LEAVE u32TargetId(%d)", u32TargetId));
    return STATUS_SUCCESS;
}

NTSTATUS VPoxVidPnRecommendFunctional(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, const VPOXWDDM_RECOMMENDVIDPN *pData)
{
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(hVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryVidPnInterface failed Status(%#x)", Status));
        return Status;
    }

    VPOXCMDVBVA_SCREENMAP_DECL(uint32_t, aVisitedSourceMap);

    memset(aVisitedSourceMap, 0, sizeof (aVisitedSourceMap));

    uint32_t Importance = (uint32_t)D3DKMDT_VPPI_PRIMARY;

    for (uint32_t i = 0; i < (uint32_t)VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        int32_t iSource = pData->aTargets[i].iSource;
        if (iSource < 0)
            continue;

        if (iSource >= VPoxCommonFromDeviceExt(pDevExt)->cDisplays)
        {
            WARN(("invalid iSource"));
            return STATUS_INVALID_PARAMETER;
        }

        if (!pDevExt->fComplexTopologiesEnabled && iSource != (int32_t)i)
        {
            WARN(("complex topologies not supported!"));
            return STATUS_INVALID_PARAMETER;
        }

        bool fNewSource = false;

        if (!ASMBitTest(aVisitedSourceMap, iSource))
        {
            int rc = VPoxWddmVModesAdd(pDevExt, i, &pData->aSources[iSource].Size, TRUE);
            if (RT_FAILURE(rc))
            {
                WARN(("VPoxWddmVModesAdd failed %d", rc));
                return STATUS_UNSUCCESSFUL;
            }

            Assert(CrSaContains(VPoxWddmVModesGet(pDevExt, i), CR_RSIZE2U64(pData->aSources[iSource].Size)));

            Status = vpoxVidPnCheckMonitorModes(pDevExt, i);
            if (!NT_SUCCESS(Status))
            {
                WARN(("vpoxVidPnCheckMonitorModes failed %#x", Status));
                return Status;
            }

            ASMBitSet(aVisitedSourceMap, iSource);
            fNewSource = true;
        }

        Status = vpoxVidPnPathAdd(hVidPn, pVidPnInterface,
                (const D3DDDI_VIDEO_PRESENT_SOURCE_ID)iSource, (const D3DDDI_VIDEO_PRESENT_TARGET_ID)i,
                (D3DKMDT_VIDPN_PRESENT_PATH_IMPORTANCE)Importance);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnPathAdd failed Status()0x%x\n", Status));
            return Status;
        }

        Importance++;

        do {
            D3DKMDT_HVIDPNTARGETMODESET hVidPnModeSet;
            const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnModeSetInterface;

            Status = pVidPnInterface->pfnCreateNewTargetModeSet(hVidPn,
                                i,
                                &hVidPnModeSet,
                                &pVidPnModeSetInterface);
            if (NT_SUCCESS(Status))
            {
                D3DKMDT_VIDPN_TARGET_MODE *pVidPnModeInfo;
                Status = pVidPnModeSetInterface->pfnCreateNewModeInfo(hVidPnModeSet, &pVidPnModeInfo);
                if (NT_SUCCESS(Status))
                {
                    vpoxVidPnPopulateTargetModeInfo(pVidPnModeInfo, &pData->aSources[iSource].Size);

                    IN_CONST_D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID idMode = pVidPnModeInfo->Id;

                    Status = pVidPnModeSetInterface->pfnAddMode(hVidPnModeSet, pVidPnModeInfo);
                    if (NT_SUCCESS(Status))
                    {
                        pVidPnModeInfo = NULL;

                        Status = pVidPnModeSetInterface->pfnPinMode(hVidPnModeSet, idMode);
                        if (NT_SUCCESS(Status))
                        {
                            Status = pVidPnInterface->pfnAssignTargetModeSet(hVidPn, i, hVidPnModeSet);
                            if (NT_SUCCESS(Status))
                            {
                                LOG(("Recommended Target[%d] (%dx%d)", i, pData->aSources[iSource].Size.cx, pData->aSources[iSource].Size.cy));
                                break;
                            }
                            else
                                WARN(("pfnAssignTargetModeSet failed %#x", Status));
                        }
                        else
                            WARN(("pfnPinMode failed %#x", Status));

                    }
                    else
                        WARN(("pfnAddMode failed %#x", Status));

                    if (pVidPnModeInfo)
                    {
                        NTSTATUS rcNt2 = pVidPnModeSetInterface->pfnReleaseModeInfo(hVidPnModeSet, pVidPnModeInfo);
                        AssertNtStatusSuccess(rcNt2);
                    }
                }
                else
                    WARN(("pfnCreateNewTargetModeSet failed %#x", Status));

                NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hVidPnModeSet);
                AssertNtStatusSuccess(rcNt2);
            }
            else
                WARN(("pfnCreateNewTargetModeSet failed %#x", Status));

            Assert(!NT_SUCCESS(Status));

            return Status;
        } while (0);

        if (fNewSource)
        {
            do {
                D3DKMDT_HVIDPNSOURCEMODESET hVidPnModeSet;
                const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;

                Status = pVidPnInterface->pfnCreateNewSourceModeSet(hVidPn,
                                    iSource,
                                    &hVidPnModeSet,
                                    &pVidPnModeSetInterface);
                if (NT_SUCCESS(Status))
                {
                    D3DKMDT_VIDPN_SOURCE_MODE *pVidPnModeInfo;
                    Status = pVidPnModeSetInterface->pfnCreateNewModeInfo(hVidPnModeSet, &pVidPnModeInfo);
                    if (NT_SUCCESS(Status))
                    {
                        vpoxVidPnPopulateSourceModeInfo(pVidPnModeInfo, &pData->aSources[iSource].Size);

                        IN_CONST_D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID idMode = pVidPnModeInfo->Id;

                        Status = pVidPnModeSetInterface->pfnAddMode(hVidPnModeSet, pVidPnModeInfo);
                        if (NT_SUCCESS(Status))
                        {
                            pVidPnModeInfo = NULL;

                            Status = pVidPnModeSetInterface->pfnPinMode(hVidPnModeSet, idMode);
                            if (NT_SUCCESS(Status))
                            {
                                Status = pVidPnInterface->pfnAssignSourceModeSet(hVidPn, iSource, hVidPnModeSet);
                                if (NT_SUCCESS(Status))
                                {
                                    LOG(("Recommended Source[%d] (%dx%d)", iSource, pData->aSources[iSource].Size.cx, pData->aSources[iSource].Size.cy));
                                    break;
                                }
                                else
                                    WARN(("pfnAssignSourceModeSet failed %#x", Status));
                            }
                            else
                                WARN(("pfnPinMode failed %#x", Status));

                        }
                        else
                            WARN(("pfnAddMode failed %#x", Status));

                        if (pVidPnModeInfo)
                        {
                            NTSTATUS rcNt2 = pVidPnModeSetInterface->pfnReleaseModeInfo(hVidPnModeSet, pVidPnModeInfo);
                            AssertNtStatusSuccess(rcNt2);
                        }
                    }
                    else
                        WARN(("pfnCreateNewSourceModeSet failed %#x", Status));

                    NTSTATUS rcNt2 = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hVidPnModeSet);
                    AssertNtStatusSuccess(rcNt2);
                }
                else
                    WARN(("pfnCreateNewSourceModeSet failed %#x", Status));

                Assert(!NT_SUCCESS(Status));

                return Status;
            } while (0);
        }
    }

    Assert(NT_SUCCESS(Status));
    return STATUS_SUCCESS;
}

static BOOLEAN vpoxVidPnIsPathSupported(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo)
{
    if (!pDevExt->fComplexTopologiesEnabled && pNewVidPnPresentPathInfo->VidPnSourceId != pNewVidPnPresentPathInfo->VidPnTargetId)
    {
        LOG(("unsupported source(%d)->target(%d) pair", pNewVidPnPresentPathInfo->VidPnSourceId, pNewVidPnPresentPathInfo->VidPnTargetId));
        return FALSE;
    }

    /*
    ImportanceOrdinal does not matter for now
    pNewVidPnPresentPathInfo->ImportanceOrdinal
    */

    if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_UNPINNED
            && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY
            && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_NOTSPECIFIED)
    {
        WARN(("unsupported Scaling (%d)", pNewVidPnPresentPathInfo->ContentTransformation.Scaling));
        return FALSE;
    }

    if (    !pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Identity
         || pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered
         || pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Stretched)
    {
        WARN(("unsupported Scaling support"));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_UNPINNED
            && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY
            && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_NOTSPECIFIED)
    {
        WARN(("unsupported rotation (%d)", pNewVidPnPresentPathInfo->ContentTransformation.Rotation));
        return FALSE;
    }

    if (    !pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Identity
         || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate90
         || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate180
         || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate270)
    {
        WARN(("unsupported RotationSupport"));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx
            || pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy)
    {
        WARN(("Non-zero TLOffset: cx(%d), cy(%d)",
                pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx,
                pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx
            || pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy)
    {
        WARN(("Non-zero TLOffset: cx(%d), cy(%d)",
                pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx,
                pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_SRGB
            && pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_UNINITIALIZED)
    {
        WARN(("unsupported VidPnTargetColorBasis (%d)", pNewVidPnPresentPathInfo->VidPnTargetColorBasis));
        return FALSE;
    }

    /* channels?
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FirstChannel;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.SecondChannel;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.ThirdChannel;
    we definitely not support fourth channel
    */
    if (pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel)
    {
        WARN(("Non-zero FourthChannel (%d)", pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel));
        return FALSE;
    }

    /* Content (D3DKMDT_VPPC_GRAPHICS, _NOTSPECIFIED, _VIDEO), does not matter for now
    pNewVidPnPresentPathInfo->Content
    */
    /* not support copy protection for now */
    if (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_NOPROTECTION
            && pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_UNINITIALIZED)
    {
        WARN(("Copy protection not supported CopyProtectionType(%d)", pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits)
    {
        WARN(("Copy protection not supported APSTriggerBits(%d)", pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits));
        return FALSE;
    }

    D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION_SUPPORT tstCPSupport = {0};
    tstCPSupport.NoProtection = 1;
    if (memcmp(&tstCPSupport, &pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, sizeof(tstCPSupport)))
    {
        WARN(("Copy protection support (0x%x)", *((UINT*)&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport)));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_DEFAULT
            && pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_UNINITIALIZED)
    {
        WARN(("Unsupported GammaRamp.Type (%d)", pNewVidPnPresentPathInfo->GammaRamp.Type));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->GammaRamp.DataSize != 0)
    {
        WARN(("Warning: non-zero GammaRamp.DataSize (%d), treating as supported", pNewVidPnPresentPathInfo->GammaRamp.DataSize));
    }

    return TRUE;
}

NTSTATUS VPoxVidPnIsSupported(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, BOOLEAN *pfSupported)
{
    /* According Microsoft Docs we must return pfSupported = TRUE here if hVidPn is NULL, as
     * the display adapter can always be configured to display nothing. */
    if (hVidPn == NULL)
    {
        *pfSupported = TRUE;
        return STATUS_SUCCESS;
    }

    *pfSupported = FALSE;

    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(hVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryVidPnInterface failed Status()0x%x\n", Status));
        return Status;
    }

#ifdef VPOXWDDM_DEBUG_VIDPN
    vpoxVidPnDumpVidPn(">>>>IsSupported VidPN (IN) : >>>>\n", pDevExt, hVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<\n");
#endif

    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnGetTopology failed Status()0x%x\n", Status));
        return Status;
    }

    VPOXVIDPN_PATH_ITER PathIter;
    const D3DKMDT_VIDPN_PRESENT_PATH * pPath;
    VPOXCMDVBVA_SCREENMAP_DECL(uint32_t, aVisitedTargetMap);

    memset(aVisitedTargetMap, 0, sizeof (aVisitedTargetMap));

    BOOLEAN fSupported = TRUE;
    /* collect info first */
    VPoxVidPnPathIterInit(&PathIter, hVidPnTopology, pVidPnTopologyInterface);
    while ((pPath = VPoxVidPnPathIterNext(&PathIter)) != NULL)
    {
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId = pPath->VidPnSourceId;
        D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId = pPath->VidPnTargetId;
        /* actually vidpn topology should contain only one target info, right? */
        Assert(!ASMBitTest(aVisitedTargetMap, VidPnTargetId));
        ASMBitSet(aVisitedTargetMap, VidPnTargetId);

        if (!vpoxVidPnIsPathSupported(pDevExt, pPath))
        {
            fSupported = FALSE;
            break;
        }

        RTRECTSIZE TargetSize;
        RTRECTSIZE SourceSize;
        Status = vpoxVidPnQueryPinnedTargetMode(hVidPn, pVidPnInterface, VidPnTargetId, &TargetSize);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnQueryPinnedTargetMode failed %#x", Status));
            break;
        }

        Status = vpoxVidPnQueryPinnedSourceMode(hVidPn, pVidPnInterface, VidPnSourceId, &SourceSize);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnQueryPinnedSourceMode failed %#x", Status));
            break;
        }

        if (memcmp(&TargetSize, &SourceSize, sizeof (TargetSize)) && TargetSize.cx)
        {
            if (!SourceSize.cx)
                WARN(("not expected?"));

            fSupported = FALSE;
            break;
        }
    }

    VPoxVidPnPathIterTerm(&PathIter);

    if (!NT_SUCCESS(Status))
        goto done;

    Status = VPoxVidPnPathIterStatus(&PathIter);
    if (!NT_SUCCESS(Status))
    {
        WARN(("PathIter failed Status()0x%x\n", Status));
        goto done;
    }

    *pfSupported = fSupported;
done:

    return Status;
}

NTSTATUS VPoxVidPnCofuncModality(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmPivot, const DXGK_ENUM_PIVOT *pPivot)
{
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(hVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryVidPnInterface failed Status()0x%x\n", Status));
        return Status;
    }

#ifdef VPOXWDDM_DEBUG_VIDPN
    vpoxVidPnDumpCofuncModalityArg(">>>>MODALITY Args: ", enmPivot, pPivot, "\n");
    vpoxVidPnDumpVidPn(">>>>MODALITY VidPN (IN) : >>>>\n", pDevExt, hVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<\n");
#endif

    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnGetTopology failed Status()0x%x\n", Status));
        return Status;
    }

    VPOXVIDPN_PATH_ITER PathIter;
    const D3DKMDT_VIDPN_PRESENT_PATH * pPath;
    VPOXCMDVBVA_SCREENMAP_DECL(uint32_t, aVisitedTargetMap);
    VPOXCMDVBVA_SCREENMAP_DECL(uint32_t, aAdjustedModeMap);
    CR_SORTARRAY aModes[VPOX_VIDEO_MAX_SCREENS];

    memset(aVisitedTargetMap, 0, sizeof (aVisitedTargetMap));
    memset(aAdjustedModeMap, 0, sizeof (aAdjustedModeMap));
    memset(aModes, 0, sizeof (aModes));

    /* collect info first */
    VPoxVidPnPathIterInit(&PathIter, hVidPnTopology, pVidPnTopologyInterface);
    while ((pPath = VPoxVidPnPathIterNext(&PathIter)) != NULL)
    {
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId = pPath->VidPnSourceId;
        D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId = pPath->VidPnTargetId;
        /* actually vidpn topology should contain only one target info, right? */
        Assert(!ASMBitTest(aVisitedTargetMap, VidPnTargetId));
        ASMBitSet(aVisitedTargetMap, VidPnTargetId);

        D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmCurPivot = vpoxVidPnCofuncModalityCurrentPathPivot(enmPivot, pPivot, VidPnSourceId, VidPnTargetId);

        Status = vpoxVidPnCollectInfoForPathTarget(pDevExt,
                hVidPn,
                pVidPnInterface,
                enmCurPivot,
                aAdjustedModeMap,
                aModes,
                VidPnSourceId, VidPnTargetId);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnCollectInfoForPathTarget failed Status(0x%x\n", Status));
            VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
            break;
        }

        Assert(CrSaCovers(VPoxWddmVModesGet(pDevExt, VidPnTargetId), &aModes[VidPnTargetId]));

        Status = vpoxVidPnCollectInfoForPathSource(pDevExt,
                hVidPn,
                pVidPnInterface,
                enmCurPivot,
                aAdjustedModeMap,
                aModes,
                VidPnSourceId, VidPnTargetId);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnCollectInfoForPathSource failed Status(0x%x\n", Status));
            VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
            break;
        }

        Assert(CrSaCovers(VPoxWddmVModesGet(pDevExt, VidPnTargetId), &aModes[VidPnTargetId]));
    }

    VPoxVidPnPathIterTerm(&PathIter);

    if (!NT_SUCCESS(Status))
        goto done;

    Status = VPoxVidPnPathIterStatus(&PathIter);
    if (!NT_SUCCESS(Status))
    {
        WARN(("PathIter failed Status()0x%x\n", Status));
        VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
        goto done;
    }

    /* now we have collected all the necessary info,
     * go ahead and apply it */
    memset(aVisitedTargetMap, 0, sizeof (aVisitedTargetMap));
    VPoxVidPnPathIterInit(&PathIter, hVidPnTopology, pVidPnTopologyInterface);
    while ((pPath = VPoxVidPnPathIterNext(&PathIter)) != NULL)
    {
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId = pPath->VidPnSourceId;
        D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId = pPath->VidPnTargetId;
        /* actually vidpn topology should contain only one target info, right? */
        Assert(!ASMBitTest(aVisitedTargetMap, VidPnTargetId));
        ASMBitSet(aVisitedTargetMap, VidPnTargetId);

        D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmCurPivot = vpoxVidPnCofuncModalityCurrentPathPivot(enmPivot, pPivot, VidPnSourceId, VidPnTargetId);

        bool bUpdatePath = false;
        D3DKMDT_VIDPN_PRESENT_PATH AdjustedPath = {0};
        AdjustedPath.VidPnSourceId = pPath->VidPnSourceId;
        AdjustedPath.VidPnTargetId = pPath->VidPnTargetId;
        AdjustedPath.ContentTransformation = pPath->ContentTransformation;
        AdjustedPath.CopyProtection = pPath->CopyProtection;

        if (pPath->ContentTransformation.Scaling == D3DKMDT_VPPS_UNPINNED)
        {
            AdjustedPath.ContentTransformation.ScalingSupport.Identity = TRUE;
            bUpdatePath = true;
        }

        if (pPath->ContentTransformation.Rotation == D3DKMDT_VPPR_UNPINNED)
        {
            AdjustedPath.ContentTransformation.RotationSupport.Identity = TRUE;
            bUpdatePath = true;
        }

        if (bUpdatePath)
        {
            Status = pVidPnTopologyInterface->pfnUpdatePathSupportInfo(hVidPnTopology, &AdjustedPath);
            if (!NT_SUCCESS(Status))
            {
                WARN(("pfnUpdatePathSupportInfo failed Status()0x%x\n", Status));
                VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
                goto done;
            }
        }

        Assert(CrSaCovers(VPoxWddmVModesGet(pDevExt, VidPnTargetId), &aModes[VidPnTargetId]));

        Status = vpoxVidPnApplyInfoForPathTarget(pDevExt,
                hVidPn,
                pVidPnInterface,
                enmCurPivot,
                aAdjustedModeMap,
                aModes,
                VidPnSourceId, VidPnTargetId);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnApplyInfoForPathTarget failed Status(0x%x\n", Status));
            VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
            break;
        }

        Status = vpoxVidPnApplyInfoForPathSource(pDevExt,
                hVidPn,
                pVidPnInterface,
                enmCurPivot,
                aAdjustedModeMap,
                aModes,
                VidPnSourceId, VidPnTargetId);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vpoxVidPnApplyInfoForPathSource failed Status(0x%x\n", Status));
            VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
            break;
        }
    }

    VPoxVidPnPathIterTerm(&PathIter);

    if (!NT_SUCCESS(Status))
        goto done;

    Status = VPoxVidPnPathIterStatus(&PathIter);
    if (!NT_SUCCESS(Status))
    {
        WARN(("PathIter failed Status()0x%x\n", Status));
        VPoxVidPnDumpCofuncModalityInfo("Modality Info: ", enmPivot, pPivot, "\n");
        goto done;
    }

done:

    for (uint32_t i = 0; i < (uint32_t)VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        CrSaCleanup(&aModes[i]);
    }

    return Status;
}

NTSTATUS vpoxVidPnEnumMonitorSourceModes(D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        PFNVPOXVIDPNENUMMONITORSOURCEMODES pfnCallback, PVOID pContext)
{
    CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI;
    NTSTATUS Status = pMonitorSMSIf->pfnAcquireFirstModeInfo(hMonitorSMS, &pMonitorSMI);
    Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_DATASET_IS_EMPTY);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pMonitorSMI);
        while (1)
        {
            CONST D3DKMDT_MONITOR_SOURCE_MODE *pNextMonitorSMI;
            Status = pMonitorSMSIf->pfnAcquireNextModeInfo(hMonitorSMS, pMonitorSMI, &pNextMonitorSMI);
            if (!pfnCallback(hMonitorSMS, pMonitorSMSIf, pMonitorSMI, pContext))
            {
                Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET);
                if (Status == STATUS_SUCCESS)
                    pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pNextMonitorSMI);
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }
                break;
            }
            else if (Status == STATUS_SUCCESS)
                pMonitorSMI = pNextMonitorSMI;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                pNextMonitorSMI = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vpoxVidPnEnumSourceModes(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
                                    PFNVPOXVIDPNENUMSOURCEMODES pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
    NTSTATUS Status = pVidPnSourceModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pNewVidPnSourceModeInfo);
        while (1)
        {
            const D3DKMDT_VIDPN_SOURCE_MODE *pNextVidPnSourceModeInfo;
            Status = pVidPnSourceModeSetInterface->pfnAcquireNextModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo, &pNextVidPnSourceModeInfo);
            if (!pfnCallback(hNewVidPnSourceModeSet, pVidPnSourceModeSetInterface,
                    pNewVidPnSourceModeInfo, pContext))
            {
                AssertNtStatusSuccess(Status);
                if (Status == STATUS_SUCCESS)
                    pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNextVidPnSourceModeInfo);
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnSourceModeInfo = pNextVidPnSourceModeInfo;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                pNewVidPnSourceModeInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vpoxVidPnEnumTargetModes(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        PFNVPOXVIDPNENUMTARGETMODES pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
    NTSTATUS Status = pVidPnTargetModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pNewVidPnTargetModeInfo);
        while (1)
        {
            const D3DKMDT_VIDPN_TARGET_MODE *pNextVidPnTargetModeInfo;
            Status = pVidPnTargetModeSetInterface->pfnAcquireNextModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo, &pNextVidPnTargetModeInfo);
            if (!pfnCallback(hNewVidPnTargetModeSet, pVidPnTargetModeSetInterface,
                    pNewVidPnTargetModeInfo, pContext))
            {
                AssertNtStatusSuccess(Status);
                if (Status == STATUS_SUCCESS)
                    pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNextVidPnTargetModeInfo);
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnTargetModeInfo = pNextVidPnTargetModeInfo;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                pNewVidPnTargetModeInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vpoxVidPnEnumTargetsForSource(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        PFNVPOXVIDPNENUMTARGETSFORSOURCE pfnCallback, PVOID pContext)
{
    SIZE_T cTgtPaths;
    NTSTATUS Status = pVidPnTopologyInterface->pfnGetNumPathsFromSource(hVidPnTopology, VidPnSourceId, &cTgtPaths);
    Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY);
    if (Status == STATUS_SUCCESS)
    {
        for (SIZE_T i = 0; i < cTgtPaths; ++i)
        {
            D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId;
            Status = pVidPnTopologyInterface->pfnEnumPathTargetsFromSource(hVidPnTopology, VidPnSourceId, i, &VidPnTargetId);
            AssertNtStatusSuccess(Status);
            if (Status == STATUS_SUCCESS)
            {
                if (!pfnCallback(pDevExt, hVidPnTopology, pVidPnTopologyInterface, VidPnSourceId, VidPnTargetId, cTgtPaths, pContext))
                    break;
            }
            else
            {
                LOGREL(("pfnEnumPathTargetsFromSource failed Status(0x%x)", Status));
                break;
            }
        }
    }
    else if (Status != STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY)
        LOGREL(("pfnGetNumPathsFromSource failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vpoxVidPnEnumPaths(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        PFNVPOXVIDPNENUMPATHS pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo = NULL;
    NTSTATUS Status = pVidPnTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
    if (Status == STATUS_SUCCESS)
    {
        while (1)
        {
            const D3DKMDT_VIDPN_PRESENT_PATH *pNextVidPnPresentPathInfo;
            Status = pVidPnTopologyInterface->pfnAcquireNextPathInfo(hVidPnTopology, pNewVidPnPresentPathInfo, &pNextVidPnPresentPathInfo);

            if (!pfnCallback(hVidPnTopology, pVidPnTopologyInterface, pNewVidPnPresentPathInfo, pContext))
            {
                if (Status == STATUS_SUCCESS)
                    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNextVidPnPresentPathInfo);
                else
                {
                    if (Status != STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                        WARN(("pfnAcquireNextPathInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnPresentPathInfo = pNextVidPnPresentPathInfo;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                WARN(("pfnAcquireNextPathInfo Failed Status(0x%x)", Status));
                pNewVidPnPresentPathInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        WARN(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vpoxVidPnSetupSourceInfo(PVPOXMP_DEVEXT pDevExt, CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo,
                                  PVPOXWDDM_ALLOCATION pAllocation, D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId,
                                  VPOXWDDM_SOURCE *paSources)
{
    RT_NOREF(pDevExt);
    PVPOXWDDM_SOURCE pSource = &paSources[VidPnSourceId];
    /* pVidPnSourceModeInfo could be null if STATUS_GRAPHICS_MODE_NOT_PINNED,
     * see VPoxVidPnCommitSourceModeForSrcId */
    uint8_t fChanges = 0;
    if (pVidPnSourceModeInfo)
    {
        if (pSource->AllocData.SurfDesc.width != pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx)
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.width = pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx;
        }
        if (pSource->AllocData.SurfDesc.height != pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy)
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.height = pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy;
        }
        if (pSource->AllocData.SurfDesc.format != pVidPnSourceModeInfo->Format.Graphics.PixelFormat)
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.format = pVidPnSourceModeInfo->Format.Graphics.PixelFormat;
        }
        if (pSource->AllocData.SurfDesc.bpp != vpoxWddmCalcBitsPerPixel(pVidPnSourceModeInfo->Format.Graphics.PixelFormat))
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.bpp = vpoxWddmCalcBitsPerPixel(pVidPnSourceModeInfo->Format.Graphics.PixelFormat);
        }
        if(pSource->AllocData.SurfDesc.pitch != pVidPnSourceModeInfo->Format.Graphics.Stride)
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.pitch = pVidPnSourceModeInfo->Format.Graphics.Stride;
        }
        pSource->AllocData.SurfDesc.depth = 1;
        if (pSource->AllocData.SurfDesc.slicePitch != pVidPnSourceModeInfo->Format.Graphics.Stride)
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.slicePitch = pVidPnSourceModeInfo->Format.Graphics.Stride;
        }
        if (pSource->AllocData.SurfDesc.cbSize != pVidPnSourceModeInfo->Format.Graphics.Stride * pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy)
        {
            fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;
            pSource->AllocData.SurfDesc.cbSize = pVidPnSourceModeInfo->Format.Graphics.Stride * pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy;
        }

        if (g_VPoxDisplayOnly)
        {
            vpoxWddmDmSetupDefaultVramLocation(pDevExt, VidPnSourceId, paSources);
        }
    }
    else
    {
        VPoxVidPnAllocDataInit(&pSource->AllocData, VidPnSourceId);
        Assert(!pAllocation);
        fChanges |= VPOXWDDM_HGSYNC_F_SYNCED_ALL;
    }

    Assert(!g_VPoxDisplayOnly || !pAllocation);
    if (!g_VPoxDisplayOnly)
    {
        vpoxWddmAssignPrimary(pSource, pAllocation, VidPnSourceId);
    }

    Assert(pSource->AllocData.SurfDesc.VidPnSourceId == VidPnSourceId);
    pSource->u8SyncState &= ~fChanges;
    return STATUS_SUCCESS;
}

NTSTATUS vpoxVidPnCommitSourceMode(PVPOXMP_DEVEXT pDevExt, CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, PVPOXWDDM_ALLOCATION pAllocation,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId, VPOXWDDM_SOURCE *paSources)
{
    if (VidPnSourceId < (UINT)VPoxCommonFromDeviceExt(pDevExt)->cDisplays)
        return vpoxVidPnSetupSourceInfo(pDevExt, pVidPnSourceModeInfo, pAllocation, VidPnSourceId, paSources);

    WARN(("invalid srcId (%d), cSources(%d)", VidPnSourceId, VPoxCommonFromDeviceExt(pDevExt)->cDisplays));
    return STATUS_INVALID_PARAMETER;
}

typedef struct VPOXVIDPNCOMMITTARGETMODE
{
    NTSTATUS Status;
    D3DKMDT_HVIDPN hVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface;
    VPOXWDDM_SOURCE *paSources;
    VPOXWDDM_TARGET *paTargets;
} VPOXVIDPNCOMMITTARGETMODE;

DECLCALLBACK(BOOLEAN) vpoxVidPnCommitTargetModeEnum(PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
                                                    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
                                                    CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
                                                    D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, SIZE_T cTgtPaths,
                                                    PVOID pContext)
{
    RT_NOREF(hVidPnTopology, pVidPnTopologyInterface, cTgtPaths);
    VPOXVIDPNCOMMITTARGETMODE *pInfo = (VPOXVIDPNCOMMITTARGETMODE*)pContext;
    Assert(cTgtPaths <= (SIZE_T)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);
    D3DKMDT_HVIDPNTARGETMODESET hVidPnTargetModeSet;
    CONST DXGK_VIDPNTARGETMODESET_INTERFACE* pVidPnTargetModeSetInterface;
    NTSTATUS Status = pInfo->pVidPnInterface->pfnAcquireTargetModeSet(pInfo->hVidPn, VidPnTargetId, &hVidPnTargetModeSet, &pVidPnTargetModeSetInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;
        Status = pVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            VPOXWDDM_SOURCE *pSource = &pInfo->paSources[VidPnSourceId];
            VPOXWDDM_TARGET *pTarget = &pInfo->paTargets[VidPnTargetId];
            pTarget->Size.cx = pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cx;
            pTarget->Size.cy = pPinnedVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy;

            VPoxVidPnStSourceTargetAdd(pInfo->paSources, VPoxCommonFromDeviceExt(pDevExt)->cDisplays, pSource, pTarget);

            pTarget->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;

            pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        }
        else
            WARN(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));

        pInfo->pVidPnInterface->pfnReleaseTargetModeSet(pInfo->hVidPn, hVidPnTargetModeSet);
    }
    else
        WARN(("pfnAcquireTargetModeSet failed Status(0x%x)", Status));

    pInfo->Status = Status;
    return Status == STATUS_SUCCESS;
}

NTSTATUS VPoxVidPnCommitSourceModeForSrcId(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        PVPOXWDDM_ALLOCATION pAllocation,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId, VPOXWDDM_SOURCE *paSources, VPOXWDDM_TARGET *paTargets, BOOLEAN bPathPowerTransition)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

    PVPOXWDDM_SOURCE pSource = &paSources[VidPnSourceId];
    NTSTATUS Status;

    if (bPathPowerTransition)
    {
        RTRECTSIZE PinnedModeSize;
        bool bHasPinnedMode;

        Status = vpoxVidPnQueryPinnedSourceMode(hDesiredVidPn, pVidPnInterface, VidPnSourceId, &PinnedModeSize);
        bHasPinnedMode = Status == STATUS_SUCCESS && PinnedModeSize.cx > 0 && PinnedModeSize.cy > 0;
        pSource->bBlankedByPowerOff = !bHasPinnedMode;

        LOG(("Path power transition: srcId %d goes blank %d", VidPnSourceId, pSource->bBlankedByPowerOff));
    }

    VPOXWDDM_TARGET_ITER Iter;
    VPoxVidPnStTIterInit(pSource, paTargets, (uint32_t)VPoxCommonFromDeviceExt(pDevExt)->cDisplays, &Iter);
    for (PVPOXWDDM_TARGET pTarget = VPoxVidPnStTIterNext(&Iter);
            pTarget;
            pTarget = VPoxVidPnStTIterNext(&Iter))
    {
        Assert(pTarget->VidPnSourceId == pSource->AllocData.SurfDesc.VidPnSourceId);
        pTarget->Size.cx = 0;
        pTarget->Size.cy = 0;
        pTarget->fBlankedByPowerOff = RT_BOOL(pSource->bBlankedByPowerOff);
        pTarget->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_ALL;
    }

    VPoxVidPnStSourceCleanup(paSources, VidPnSourceId, paTargets, (uint32_t)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);

    Status = pVidPnInterface->pfnAcquireSourceModeSet(hDesiredVidPn,
                VidPnSourceId,
                &hCurVidPnSourceModeSet,
                &pCurVidPnSourceModeSetInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;
        Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_SUCCESS)
        {
            Assert(pPinnedVidPnSourceModeInfo);
            Status = vpoxVidPnCommitSourceMode(pDevExt, pPinnedVidPnSourceModeInfo, pAllocation, VidPnSourceId, paSources);
            AssertNtStatusSuccess(Status);
            if (Status == STATUS_SUCCESS)
            {
                D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
                CONST DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
                Status = pVidPnInterface->pfnGetTopology(hDesiredVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
                AssertNtStatusSuccess(Status);
                if (Status == STATUS_SUCCESS)
                {
                    VPOXVIDPNCOMMITTARGETMODE TgtModeInfo = {0};
                    TgtModeInfo.Status = STATUS_SUCCESS; /* <- to ensure we're succeeded if no targets are set */
                    TgtModeInfo.hVidPn = hDesiredVidPn;
                    TgtModeInfo.pVidPnInterface = pVidPnInterface;
                    TgtModeInfo.paSources = paSources;
                    TgtModeInfo.paTargets = paTargets;
                    Status = vpoxVidPnEnumTargetsForSource(pDevExt, hVidPnTopology, pVidPnTopologyInterface,
                            VidPnSourceId,
                            vpoxVidPnCommitTargetModeEnum, &TgtModeInfo);
                    Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY);
                    if (Status == STATUS_SUCCESS)
                    {
                        Status = TgtModeInfo.Status;
                        AssertNtStatusSuccess(Status);
                    }
                    else if (Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY)
                    {
                        Status = STATUS_SUCCESS;
                    }
                    else
                        WARN(("vpoxVidPnEnumTargetsForSource failed Status(0x%x)", Status));
                }
                else
                    WARN(("pfnGetTopology failed Status(0x%x)", Status));
            }
            else
                WARN(("vpoxVidPnCommitSourceMode failed Status(0x%x)", Status));
            /* release */
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            Status = vpoxVidPnCommitSourceMode(pDevExt, NULL, pAllocation, VidPnSourceId, paSources);
            AssertNtStatusSuccess(Status);
        }
        else
            WARN(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));

        pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        WARN(("pfnAcquireSourceModeSet failed Status(0x%x)", Status));
    }

    return Status;
}

NTSTATUS VPoxVidPnCommitAll(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        PVPOXWDDM_ALLOCATION pAllocation,
        VPOXWDDM_SOURCE *paSources, VPOXWDDM_TARGET *paTargets)
{
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    NTSTATUS Status = pVidPnInterface->pfnGetTopology(hDesiredVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnGetTopology failed Status 0x%x", Status));
        return Status;
    }

    for (int i = 0; i < VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        PVPOXWDDM_TARGET pTarget = &paTargets[i];
        pTarget->Size.cx = 0;
        pTarget->Size.cy = 0;
        pTarget->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_ALL;

        if (pTarget->VidPnSourceId == D3DDDI_ID_UNINITIALIZED)
            continue;

        Assert(pTarget->VidPnSourceId < (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);

        VPOXWDDM_SOURCE *pSource = &paSources[pTarget->VidPnSourceId];
        VPoxVidPnAllocDataInit(&pSource->AllocData, pTarget->VidPnSourceId);
        pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_ALL;
    }

    VPoxVidPnStCleanup(paSources, paTargets, VPoxCommonFromDeviceExt(pDevExt)->cDisplays);

    VPOXVIDPN_PATH_ITER PathIter;
    const D3DKMDT_VIDPN_PRESENT_PATH *pPath;
    VPoxVidPnPathIterInit(&PathIter, hVidPnTopology, pVidPnTopologyInterface);
    while ((pPath = VPoxVidPnPathIterNext(&PathIter)) != NULL)
    {
        Status = VPoxVidPnCommitSourceModeForSrcId(pDevExt, hDesiredVidPn, pVidPnInterface, pAllocation,
                    pPath->VidPnSourceId, paSources, paTargets, FALSE);
        if (Status != STATUS_SUCCESS)
        {
            WARN(("VPoxVidPnCommitSourceModeForSrcId failed Status(0x%x)", Status));
            break;
        }
    }

    VPoxVidPnPathIterTerm(&PathIter);

    if (!NT_SUCCESS(Status))
    {
        WARN((""));
        return Status;
    }

    Status = VPoxVidPnPathIterStatus(&PathIter);
    if (!NT_SUCCESS(Status))
    {
        WARN(("VPoxVidPnPathIterStatus failed Status 0x%x", Status));
        return Status;
    }

    return STATUS_SUCCESS;
}

#define VPOXVIDPNDUMP_STRCASE(_t) \
        case _t: return #_t;
#define VPOXVIDPNDUMP_STRCASE_UNKNOWN() \
        default: Assert(0); return "Unknown";

#define VPOXVIDPNDUMP_STRFLAGS(_v, _t) \
        if ((_v)._t return #_t;

const char* vpoxVidPnDumpStrImportance(D3DKMDT_VIDPN_PRESENT_PATH_IMPORTANCE ImportanceOrdinal)
{
    switch (ImportanceOrdinal)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_PRIMARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_SECONDARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_TERTIARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_QUATERNARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_QUINARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_SENARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_SEPTENARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_OCTONARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_NONARY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_DENARY);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrScaling(D3DKMDT_VIDPN_PRESENT_PATH_SCALING Scaling)
{
    switch (Scaling)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_IDENTITY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_CENTERED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_STRETCHED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_UNPINNED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_NOTSPECIFIED);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrRotation(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION Rotation)
{
    switch (Rotation)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_IDENTITY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_ROTATE90);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_ROTATE180);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_ROTATE270);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_UNPINNED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_NOTSPECIFIED);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrColorBasis(const D3DKMDT_COLOR_BASIS ColorBasis)
{
    switch (ColorBasis)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_CB_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_CB_INTENSITY);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_CB_SRGB);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_CB_SCRGB);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_CB_YCBCR);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_CB_YPBPR);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char * vpoxVidPnDumpStrMonCapabilitiesOrigin(D3DKMDT_MONITOR_CAPABILITIES_ORIGIN enmOrigin)
{
    switch (enmOrigin)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MCO_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MCO_DEFAULTMONITORPROFILE);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MCO_MONITORDESCRIPTOR);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MCO_MONITORDESCRIPTOR_REGISTRYOVERRIDE);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MCO_SPECIFICCAP_REGISTRYOVERRIDE);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MCO_DRIVER);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrPvam(D3DKMDT_PIXEL_VALUE_ACCESS_MODE PixelValueAccessMode)
{
    switch (PixelValueAccessMode)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_DIRECT);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_PRESETPALETTE);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_SETTABLEPALETTE);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}



const char* vpoxVidPnDumpStrContent(D3DKMDT_VIDPN_PRESENT_PATH_CONTENT Content)
{
    switch (Content)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_GRAPHICS);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_VIDEO);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_NOTSPECIFIED);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrCopyProtectionType(D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION_TYPE CopyProtectionType)
{
    switch (CopyProtectionType)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_NOPROTECTION);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_MACROVISION_APSTRIGGER);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_MACROVISION_FULLSUPPORT);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrGammaRampType(D3DDDI_GAMMARAMP_TYPE Type)
{
    switch (Type)
    {
        VPOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_DEFAULT);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_RGB256x3x16);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_DXGI_1);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrSourceModeType(D3DKMDT_VIDPN_SOURCE_MODE_TYPE Type)
{
    switch (Type)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_RMT_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_RMT_GRAPHICS);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_RMT_TEXT);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrScanLineOrdering(D3DDDI_VIDEO_SIGNAL_SCANLINE_ORDERING ScanLineOrdering)
{
    switch (ScanLineOrdering)
    {
        VPOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_PROGRESSIVE);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_INTERLACED_UPPERFIELDFIRST);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_INTERLACED_LOWERFIELDFIRST);
        VPOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_OTHER);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrCFMPivotType(D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE EnumPivotType)
{
    switch (EnumPivotType)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_VIDPNSOURCE);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_VIDPNTARGET);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_SCALING);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_ROTATION);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_NOPIVOT);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrModePreference(D3DKMDT_MODE_PREFERENCE Preference)
{
    switch (Preference)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MP_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MP_PREFERRED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_MP_NOTPREFERRED);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrSignalStandard(D3DKMDT_VIDEO_SIGNAL_STANDARD VideoStandard)
{
    switch (VideoStandard)
    {
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_UNINITIALIZED);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_VESA_DMT);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_VESA_GTF);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_VESA_CVT);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_IBM);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_APPLE);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_NTSC_M);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_NTSC_J);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_NTSC_443);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_B);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_B1);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_G);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_H);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_I);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_D);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_N);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_NC);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_B);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_D);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_G);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_H);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_K);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_K1);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_L);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_L1);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_EIA_861);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_EIA_861A);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_EIA_861B);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_K);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_K1);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_L);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_M);
        VPOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_OTHER);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vpoxVidPnDumpStrPixFormat(D3DDDIFORMAT PixelFormat)
{
    switch (PixelFormat)
    {
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_UNKNOWN);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_R8G8B8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8R8G8B8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8R8G8B8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_R5G6B5);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X1R5G5B5);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A1R5G5B5);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A4R4G4B4);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_R3G3B2);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8R3G3B2);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X4R4G4B4);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A2B10G10R10);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8B8G8R8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8B8G8R8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_G16R16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A2R10G10B10);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A16B16G16R16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8P8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_R32F);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_G32R32F);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A32B32G32R32F);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_CxV8U8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A1);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_BINARYBUFFER);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_VERTEXDATA);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_INDEX16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_INDEX32);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_Q16W16V16U16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_MULTI2_ARGB8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_R16F);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_G16R16F);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A16B16G16R16F);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D32F_LOCKABLE);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24FS8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D32_LOCKABLE);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_S8_LOCKABLE);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_S1D15);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_S8D24);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8D24);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X4S4D24);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_L16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_UYVY);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_R8G8_B8G8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_YUY2);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_G8R8_G8B8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT1);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT2);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT3);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT4);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT5);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D16_LOCKABLE);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D32);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D15S1);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24S8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24X8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24X4S4);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_D16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_P8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_L8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8L8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A4L4);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_V8U8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_L6V5U5);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8L8V8U8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_Q8W8V8U8);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_V16U16);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_W11V11U10);
        VPOXVIDPNDUMP_STRCASE(D3DDDIFMT_A2W10V10U10);
        VPOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

void vpoxVidPnDumpCopyProtectoin(const char *pPrefix, const D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION *pCopyProtection, const char *pSuffix)
{
    LOGREL_EXACT(("%sType(%s), TODO%s", pPrefix,
            vpoxVidPnDumpStrCopyProtectionType(pCopyProtection->CopyProtectionType), pSuffix));
}


void vpoxVidPnDumpPathTransformation(const D3DKMDT_VIDPN_PRESENT_PATH_TRANSFORMATION *pContentTransformation)
{
    LOGREL_EXACT(("  --Transformation: Scaling(%s), ScalingSupport(%d), Rotation(%s), RotationSupport(%d)--",
            vpoxVidPnDumpStrScaling(pContentTransformation->Scaling), pContentTransformation->ScalingSupport,
            vpoxVidPnDumpStrRotation(pContentTransformation->Rotation), pContentTransformation->RotationSupport));
}

void vpoxVidPnDumpRegion(const char *pPrefix, const D3DKMDT_2DREGION *pRegion, const char *pSuffix)
{
    LOGREL_EXACT(("%s%dX%d%s", pPrefix, pRegion->cx, pRegion->cy, pSuffix));
}

void vpoxVidPnDumpRational(const char *pPrefix, const D3DDDI_RATIONAL *pRational, const char *pSuffix)
{
    LOGREL_EXACT(("%s%d/%d=%d%s", pPrefix, pRational->Numerator, pRational->Denominator, pRational->Numerator/pRational->Denominator, pSuffix));
}

void vpoxVidPnDumpRanges(const char *pPrefix, const D3DKMDT_COLOR_COEFF_DYNAMIC_RANGES *pDynamicRanges, const char *pSuffix)
{
    LOGREL_EXACT(("%sFirstChannel(%d), SecondChannel(%d), ThirdChannel(%d), FourthChannel(%d)%s", pPrefix,
            pDynamicRanges->FirstChannel,
            pDynamicRanges->SecondChannel,
            pDynamicRanges->ThirdChannel,
            pDynamicRanges->FourthChannel,
            pSuffix));
}

void vpoxVidPnDumpGammaRamp(const char *pPrefix, const D3DKMDT_GAMMA_RAMP *pGammaRamp, const char *pSuffix)
{
    LOGREL_EXACT(("%sType(%s), DataSize(%d), TODO: dump the rest%s", pPrefix,
            vpoxVidPnDumpStrGammaRampType(pGammaRamp->Type), pGammaRamp->DataSize,
            pSuffix));
}

void VPoxVidPnDumpSourceMode(const char *pPrefix, const D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%sType(%s), ", pPrefix, vpoxVidPnDumpStrSourceModeType(pVidPnSourceModeInfo->Type)));
    vpoxVidPnDumpRegion("surf(", &pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize, "), ");
    vpoxVidPnDumpRegion("vis(", &pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize, "), ");
    LOGREL_EXACT(("stride(%d), ", pVidPnSourceModeInfo->Format.Graphics.Stride));
    LOGREL_EXACT(("format(%s), ", vpoxVidPnDumpStrPixFormat(pVidPnSourceModeInfo->Format.Graphics.PixelFormat)));
    LOGREL_EXACT(("clrBasis(%s), ", vpoxVidPnDumpStrColorBasis(pVidPnSourceModeInfo->Format.Graphics.ColorBasis)));
    LOGREL_EXACT(("pvam(%s)%s", vpoxVidPnDumpStrPvam(pVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode), pSuffix));
}

void vpoxVidPnDumpSignalInfo(const char *pPrefix, const D3DKMDT_VIDEO_SIGNAL_INFO *pVideoSignalInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%sVStd(%s), ", pPrefix, vpoxVidPnDumpStrSignalStandard(pVideoSignalInfo->VideoStandard)));
    vpoxVidPnDumpRegion("totSize(", &pVideoSignalInfo->TotalSize, "), ");
    vpoxVidPnDumpRegion("activeSize(", &pVideoSignalInfo->ActiveSize, "), ");
    vpoxVidPnDumpRational("VSynch(", &pVideoSignalInfo->VSyncFreq, "), ");
    LOGREL_EXACT(("PixelRate(%d), ScanLineOrdering(%s)%s", pVideoSignalInfo->PixelRate, vpoxVidPnDumpStrScanLineOrdering(pVideoSignalInfo->ScanLineOrdering), pSuffix));
}

void VPoxVidPnDumpTargetMode(const char *pPrefix, const D3DKMDT_VIDPN_TARGET_MODE* CONST  pVidPnTargetModeInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%s", pPrefix));
    LOGREL_EXACT(("ID: %d, ", pVidPnTargetModeInfo->Id));
    vpoxVidPnDumpSignalInfo("VSI: ", &pVidPnTargetModeInfo->VideoSignalInfo, ", ");
    LOGREL_EXACT(("Preference(%s)%s", vpoxVidPnDumpStrModePreference(pVidPnTargetModeInfo->Preference), pSuffix));
}

void VPoxVidPnDumpMonitorMode(const char *pPrefix, const D3DKMDT_MONITOR_SOURCE_MODE *pVidPnModeInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%s", pPrefix));

    LOGREL_EXACT(("ID: %d, ", pVidPnModeInfo->Id));

    vpoxVidPnDumpSignalInfo("VSI: ", &pVidPnModeInfo->VideoSignalInfo, ", ");

    LOGREL_EXACT(("ColorBasis: %s, ", vpoxVidPnDumpStrColorBasis(pVidPnModeInfo->ColorBasis)));

    vpoxVidPnDumpRanges("Ranges: ", &pVidPnModeInfo->ColorCoeffDynamicRanges, ", ");

    LOGREL_EXACT(("MonCapOr: %s, ", vpoxVidPnDumpStrMonCapabilitiesOrigin(pVidPnModeInfo->Origin)));

    LOGREL_EXACT(("Preference(%s)%s", vpoxVidPnDumpStrModePreference(pVidPnModeInfo->Preference), pSuffix));
}

NTSTATUS VPoxVidPnDumpMonitorModeSet(const char *pPrefix, PVPOXMP_DEVEXT pDevExt, uint32_t u32Target, const char *pSuffix)
{
    LOGREL_EXACT(("%s Tgt[%d]\n", pPrefix, u32Target));

    NTSTATUS Status;
    CONST DXGK_MONITOR_INTERFACE *pMonitorInterface;
    Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryMonitorInterface(pDevExt->u.primary.DxgkInterface.DeviceHandle, DXGK_MONITOR_INTERFACE_VERSION_V1, &pMonitorInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
        return Status;
    }

    D3DKMDT_HMONITORSOURCEMODESET hVidPnModeSet;
    CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pVidPnModeSetInterface;

    Status = pMonitorInterface->pfnAcquireMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle,
                                        u32Target,
                                        &hVidPnModeSet,
                                        &pVidPnModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
        return Status;
    }

    VPOXVIDPN_MONITORMODE_ITER Iter;
    const D3DKMDT_MONITOR_SOURCE_MODE *pVidPnModeInfo;

    VPoxVidPnMonitorModeIterInit(&Iter, hVidPnModeSet, pVidPnModeSetInterface);

    while ((pVidPnModeInfo = VPoxVidPnMonitorModeIterNext(&Iter)) != NULL)
    {
        VPoxVidPnDumpMonitorMode("MonitorMode: ",pVidPnModeInfo, "\n");
    }

    VPoxVidPnMonitorModeIterTerm(&Iter);

    Status = VPoxVidPnMonitorModeIterStatus(&Iter);
    if (!NT_SUCCESS(Status))
    {
        WARN(("iter status failed %#x", Status));
    }

    NTSTATUS rcNt2 = pMonitorInterface->pfnReleaseMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle, hVidPnModeSet);
    if (!NT_SUCCESS(rcNt2))
        WARN(("pfnReleaseMonitorSourceModeSet failed rcNt2(0x%x)", rcNt2));

    LOGREL_EXACT(("%s", pSuffix));

    return Status;
}

void vpoxVidPnDumpPinnedSourceMode(const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;

        Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_SUCCESS)
        {
            VPoxVidPnDumpSourceMode("Source Pinned: ", pPinnedVidPnSourceModeInfo, "\n");
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            LOGREL_EXACT(("Source NOT Pinned\n"));
        }
        else
        {
            LOGREL_EXACT(("ERROR getting piined Source Mode(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting SourceModeSet(0x%x)\n", Status));
    }
}


DECLCALLBACK(BOOLEAN) vpoxVidPnDumpSourceModeSetEnum(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet,
                                                     const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
                                                     const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{

    RT_NOREF(hNewVidPnSourceModeSet, pVidPnSourceModeSetInterface, pNewVidPnSourceModeInfo, pContext);
    VPoxVidPnDumpSourceMode("SourceMode: ", pNewVidPnSourceModeInfo, "\n");
    return TRUE;
}

void vpoxVidPnDumpSourceModeSet(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
                                D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    RT_NOREF(pDevExt);
    LOGREL_EXACT(("\n  >>>+++SourceMode Set for Source(%d)+++\n", VidPnSourceId));
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {

        Status = vpoxVidPnEnumSourceModes(hCurVidPnSourceModeSet, pCurVidPnSourceModeSetInterface,
                vpoxVidPnDumpSourceModeSetEnum, NULL);
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL_EXACT(("ERROR enumerating Source Modes(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting SourceModeSet for Source(%d), Status(0x%x)\n", VidPnSourceId, Status));
    }

    LOGREL_EXACT(("  <<<+++End Of SourceMode Set for Source(%d)+++", VidPnSourceId));
}

DECLCALLBACK(BOOLEAN) vpoxVidPnDumpTargetModeSetEnum(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet,
                                                     const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
                                                     const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext)
{
    RT_NOREF(hNewVidPnTargetModeSet, pVidPnTargetModeSetInterface, pNewVidPnTargetModeInfo, pContext);
    VPoxVidPnDumpTargetMode("TargetMode: ", pNewVidPnTargetModeInfo, "\n");
    return TRUE;
}

void vpoxVidPnDumpTargetModeSet(PVPOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
                                D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    RT_NOREF(pDevExt);
    LOGREL_EXACT(("\n  >>>---TargetMode Set for Target(%d)---\n", VidPnTargetId));
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {

        Status = vpoxVidPnEnumTargetModes(hCurVidPnTargetModeSet, pCurVidPnTargetModeSetInterface,
                vpoxVidPnDumpTargetModeSetEnum, NULL);
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL_EXACT(("ERROR enumerating Target Modes(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting TargetModeSet for Target(%d), Status(0x%x)\n", VidPnTargetId, Status));
    }

    LOGREL_EXACT(("  <<<---End Of TargetMode Set for Target(%d)---", VidPnTargetId));
}


void vpoxVidPnDumpPinnedTargetMode(const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;

        Status = pCurVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_SUCCESS)
        {
            VPoxVidPnDumpTargetMode("Target Pinned: ", pPinnedVidPnTargetModeInfo, "\n");
            pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            LOGREL_EXACT(("Target NOT Pinned\n"));
        }
        else
        {
            LOGREL_EXACT(("ERROR getting piined Target Mode(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting TargetModeSet(0x%x)\n", Status));
    }
}

void VPoxVidPnDumpCofuncModalityInfo(const char *pPrefix, D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmEnumPivotType, const DXGK_ENUM_PIVOT *pPivot, const char *pSuffix)
{
    LOGREL_EXACT(("%sPivotType(%s), SourceId(0x%x), TargetId(0x%x),%s", pPrefix, vpoxVidPnDumpStrCFMPivotType(enmEnumPivotType),
            pPivot->VidPnSourceId, pPivot->VidPnTargetId, pSuffix));
}

void vpoxVidPnDumpCofuncModalityArg(const char *pPrefix, D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmPivot, const DXGK_ENUM_PIVOT *pPivot, const char *pSuffix)
{
    LOGREL_EXACT(("%sPivotType(%s), SourceId(0x%x), TargetId(0x%x),%s", pPrefix, vpoxVidPnDumpStrCFMPivotType(enmPivot),
            pPivot->VidPnSourceId, pPivot->VidPnTargetId, pSuffix));
}

void vpoxVidPnDumpPath(const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo)
{
    LOGREL_EXACT((" >>**** Start Dump VidPn Path ****>>\n"));
    LOGREL_EXACT(("VidPnSourceId(%d),  VidPnTargetId(%d)\n",
            pVidPnPresentPathInfo->VidPnSourceId, pVidPnPresentPathInfo->VidPnTargetId));

    vpoxVidPnDumpPinnedSourceMode(hVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnSourceId);
    vpoxVidPnDumpPinnedTargetMode(hVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnTargetId);

    vpoxVidPnDumpPathTransformation(&pVidPnPresentPathInfo->ContentTransformation);

    LOGREL_EXACT(("Importance(%s), TargetColorBasis(%s), Content(%s), ",
            vpoxVidPnDumpStrImportance(pVidPnPresentPathInfo->ImportanceOrdinal),
            vpoxVidPnDumpStrColorBasis(pVidPnPresentPathInfo->VidPnTargetColorBasis),
            vpoxVidPnDumpStrContent(pVidPnPresentPathInfo->Content)));
    vpoxVidPnDumpRegion("VFA_TL_O(", &pVidPnPresentPathInfo->VisibleFromActiveTLOffset, "), ");
    vpoxVidPnDumpRegion("VFA_BR_O(", &pVidPnPresentPathInfo->VisibleFromActiveBROffset, "), ");
    vpoxVidPnDumpRanges("CCDynamicRanges: ", &pVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges, "| ");
    vpoxVidPnDumpCopyProtectoin("CProtection: ", &pVidPnPresentPathInfo->CopyProtection, "| ");
    vpoxVidPnDumpGammaRamp("GammaRamp: ", &pVidPnPresentPathInfo->GammaRamp, "\n");

    LOGREL_EXACT((" <<**** Stop Dump VidPn Path ****<<"));
}

typedef struct VPOXVIDPNDUMPPATHENUM
{
    D3DKMDT_HVIDPN hVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface;
} VPOXVIDPNDUMPPATHENUM, *PVPOXVIDPNDUMPPATHENUM;

static DECLCALLBACK(BOOLEAN) vpoxVidPnDumpPathEnum(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo, PVOID pContext)
{
    PVPOXVIDPNDUMPPATHENUM pData = (PVPOXVIDPNDUMPPATHENUM)pContext;
    vpoxVidPnDumpPath(pData->hVidPn, pData->pVidPnInterface, pVidPnPresentPathInfo);

    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathInfo);
    return TRUE;
}

void vpoxVidPnDumpVidPn(const char * pPrefix, PVPOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, const char * pSuffix)
{
    LOGREL_EXACT(("%s", pPrefix));

    VPOXVIDPNDUMPPATHENUM CbData;
    CbData.hVidPn = hVidPn;
    CbData.pVidPnInterface = pVidPnInterface;
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    NTSTATUS Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vpoxVidPnEnumPaths(hVidPnTopology, pVidPnTopologyInterface,
                                        vpoxVidPnDumpPathEnum, &CbData);
        AssertNtStatusSuccess(Status);
    }

    for (int i = 0; i < VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        vpoxVidPnDumpSourceModeSet(pDevExt, hVidPn, pVidPnInterface, (D3DDDI_VIDEO_PRESENT_SOURCE_ID)i);
        vpoxVidPnDumpTargetModeSet(pDevExt, hVidPn, pVidPnInterface, (D3DDDI_VIDEO_PRESENT_TARGET_ID)i);
    }

    LOGREL_EXACT(("%s", pSuffix));
}
