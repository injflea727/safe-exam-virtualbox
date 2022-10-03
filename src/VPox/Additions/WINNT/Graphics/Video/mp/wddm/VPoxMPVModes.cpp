/* $Id: VPoxMPVModes.cpp $ */
/** @file
 * VPox WDDM Miniport driver
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
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
#include "common/VPoxMPCommon.h"
#include <iprt/param.h> /* PAGE_OFFSET_MASK */

#include <stdio.h> /* for swprintf */


int VPoxVModesInit(VPOX_VMODES *pModes, uint32_t cTargets)
{
    if (cTargets >= VPOX_VIDEO_MAX_SCREENS)
    {
        WARN(("invalid target"));
        return VERR_INVALID_PARAMETER;
    }

    pModes->cTargets = cTargets;
    for (uint32_t i = 0; i < cTargets; ++i)
    {
        int rc = CrSaInit(&pModes->aTargets[i], 16);
        if (RT_FAILURE(rc))
        {
            WARN(("CrSaInit failed"));

            for (uint32_t j = 0; j < i; ++j)
            {
                CrSaCleanup(&pModes->aTargets[j]);
            }
            return rc;
        }
    }

    return VINF_SUCCESS;
}

void VPoxVModesCleanup(VPOX_VMODES *pModes)
{
    for (uint32_t i = 0; i < pModes->cTargets; ++i)
    {
        CrSaCleanup(&pModes->aTargets[i]);
    }
}

int VPoxVModesAdd(VPOX_VMODES *pModes, uint32_t u32Target, uint64_t u64)
{
    if (u32Target >= pModes->cTargets)
    {
        WARN(("invalid target id"));
        return VERR_INVALID_PARAMETER;
    }

    return CrSaAdd(&pModes->aTargets[u32Target], u64);
}

int VPoxVModesRemove(VPOX_VMODES *pModes, uint32_t u32Target, uint64_t u64)
{
    if (u32Target >= pModes->cTargets)
    {
        WARN(("invalid target id"));
        return VERR_INVALID_PARAMETER;
    }

    return CrSaRemove(&pModes->aTargets[u32Target], u64);
}

static void vpoxWddmVModesInit(VPOXWDDM_VMODES *pModes, uint32_t cTargets)
{
    VPoxVModesInit(&pModes->Modes, cTargets);
    memset(pModes->aTransientResolutions, 0, cTargets * sizeof (pModes->aTransientResolutions[0]));
    memset(pModes->aPendingRemoveCurResolutions, 0, cTargets * sizeof (pModes->aPendingRemoveCurResolutions[0]));
}

static void vpoxWddmVModesCleanup(VPOXWDDM_VMODES *pModes)
{
    VPoxVModesCleanup(&pModes->Modes);
    memset(pModes->aTransientResolutions, 0, sizeof (pModes->aTransientResolutions));
    memset(pModes->aPendingRemoveCurResolutions, 0, sizeof (pModes->aPendingRemoveCurResolutions));
}

/*
static void vpoxWddmVModesClone(const VPOXWDDM_VMODES *pModes, VPOXWDDM_VMODES *pDst)
{
    VPoxVModesClone(&pModes->Modes, pDst->Modes);
    memcpy(pDst->aTransientResolutions, pModes->aTransientResolutions, pModes->Modes.cTargets * sizeof (pModes->aTransientResolutions[0]));
    memcpy(pDst->aPendingRemoveCurResolutions, pModes->aPendingRemoveCurResolutions, pModes->Modes.cTargets * sizeof (pModes->aPendingRemoveCurResolutions[0]));
}
*/

static const RTRECTSIZE g_VPoxBuiltinResolutions[] =
{
    /* standard modes */
    { 640,   480 },
    { 800,   600 },
    { 1024,  768 },
    { 1152,  864 },
    { 1280,  960 },
    { 1280, 1024 },
    { 1400, 1050 },
    { 1600, 1200 },
    { 1920, 1440 },
};

DECLINLINE(bool) vpoxVModesRMatch(const RTRECTSIZE *pResolution1, const RTRECTSIZE *pResolution2)
{
    return !memcmp(pResolution1, pResolution2, sizeof (*pResolution1));
}

int vpoxWddmVModesRemove(PVPOXMP_DEVEXT pExt, VPOXWDDM_VMODES *pModes, uint32_t u32Target, const RTRECTSIZE *pResolution)
{
    if (!pResolution->cx || !pResolution->cy)
    {
        WARN(("invalid resolution data"));
        return VERR_INVALID_PARAMETER;
    }

    if (u32Target >= pModes->Modes.cTargets)
    {
        WARN(("invalid target id"));
        return VERR_INVALID_PARAMETER;
    }

    if (CR_RSIZE2U64(*pResolution) == pModes->aTransientResolutions[u32Target])
        pModes->aTransientResolutions[u32Target] = 0;

    if (vpoxVModesRMatch(pResolution, &pExt->aTargets[u32Target].Size))
    {
        if (CR_RSIZE2U64(*pResolution) == pModes->aPendingRemoveCurResolutions[u32Target])
            return VINF_ALREADY_INITIALIZED;

        if (pModes->aPendingRemoveCurResolutions[u32Target])
        {
            VPoxVModesRemove(&pModes->Modes, u32Target, pModes->aPendingRemoveCurResolutions[u32Target]);
            pModes->aPendingRemoveCurResolutions[u32Target] = 0;
        }

        pModes->aPendingRemoveCurResolutions[u32Target] = CR_RSIZE2U64(*pResolution);
        return VINF_ALREADY_INITIALIZED;
    }
    else if (CR_RSIZE2U64(*pResolution) == pModes->aPendingRemoveCurResolutions[u32Target])
        pModes->aPendingRemoveCurResolutions[u32Target] = 0;

    int rc = VPoxVModesRemove(&pModes->Modes, u32Target, CR_RSIZE2U64(*pResolution));
    if (RT_FAILURE(rc))
    {
        WARN(("VPoxVModesRemove failed %d, can never happen", rc));
        return rc;
    }

    if (rc == VINF_ALREADY_INITIALIZED)
        return rc;

    return VINF_SUCCESS;
}

static void vpoxWddmVModesSaveTransient(PVPOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution)
{
    VPOXMPCMNREGISTRY Registry;
    VP_STATUS rc;

    rc = VPoxMPCmnRegInit(pExt, &Registry);
    VPOXMP_WARN_VPS(rc);

    if (u32Target==0)
    {
        /*First name without a suffix*/
        rc = VPoxMPCmnRegSetDword(Registry, L"CustomXRes", pResolution->cx);
        VPOXMP_WARN_VPS(rc);
        rc = VPoxMPCmnRegSetDword(Registry, L"CustomYRes", pResolution->cy);
        VPOXMP_WARN_VPS(rc);
        rc = VPoxMPCmnRegSetDword(Registry, L"CustomBPP", 32); /* <- just in case for older driver usage */
        VPOXMP_WARN_VPS(rc);
    }
    else
    {
        wchar_t keyname[32];
        swprintf(keyname, L"CustomXRes%d", u32Target);
        rc = VPoxMPCmnRegSetDword(Registry, keyname, pResolution->cx);
        VPOXMP_WARN_VPS(rc);
        swprintf(keyname, L"CustomYRes%d", u32Target);
        rc = VPoxMPCmnRegSetDword(Registry, keyname, pResolution->cy);
        VPOXMP_WARN_VPS(rc);
        swprintf(keyname, L"CustomBPP%d", u32Target);
        rc = VPoxMPCmnRegSetDword(Registry, keyname, 32); /* <- just in case for older driver usage */
        VPOXMP_WARN_VPS(rc);
    }

    rc = VPoxMPCmnRegFini(Registry);
    VPOXMP_WARN_VPS(rc);
}

int vpoxWddmVModesAdd(PVPOXMP_DEVEXT pExt, VPOXWDDM_VMODES *pModes, uint32_t u32Target, const RTRECTSIZE *pResolution, BOOLEAN fTransient)
{
    if (!pResolution->cx || !pResolution->cy)
    {
        WARN(("invalid resolution data"));
        return VERR_INVALID_PARAMETER;
    }

    if (u32Target >= pModes->Modes.cTargets)
    {
        WARN(("invalid target id"));
        return VERR_INVALID_PARAMETER;
    }

    ULONG vramSize = vpoxWddmVramCpuVisibleSegmentSize(pExt);
    vramSize /= pExt->u.primary.commonInfo.cDisplays;
    if (!g_VPoxDisplayOnly)
    {
        /* at least two surfaces will be needed: primary & shadow */
        vramSize /= 2;
    }
    vramSize &= ~PAGE_OFFSET_MASK;

    /* prevent potensial overflow */
    if (   pResolution->cx > 0x7fff
        || pResolution->cy > 0x7fff)
    {
        WARN(("resolution %dx%d insane", pResolution->cx, pResolution->cy));
        return VERR_INVALID_PARAMETER;
    }

    uint32_t cbSurfMem = pResolution->cx * pResolution->cy * 4;
    if (cbSurfMem > vramSize)
    {
        WARN(("resolution %dx%d too big for available VRAM (%d bytes)\n", pResolution->cx, pResolution->cy, vramSize));
        return VERR_NOT_SUPPORTED;
    }

    if (!VPoxLikesVideoMode(u32Target, pResolution->cx, pResolution->cy, 32))
    {
        WARN(("resolution %dx%d not accepted by the frontend\n", pResolution->cx, pResolution->cy));
        return VERR_NOT_SUPPORTED;
    }

    if (pModes->aTransientResolutions[u32Target] == CR_RSIZE2U64(*pResolution))
    {
        if (!fTransient) /* if the mode is not transient anymore, remove it from transient */
            pModes->aTransientResolutions[u32Target] = 0;
        return VINF_ALREADY_INITIALIZED;
    }

    int rc;
    bool fTransientIfExists = false;
    if (pModes->aPendingRemoveCurResolutions[u32Target] == CR_RSIZE2U64(*pResolution))
    {
        /* no need to remove it anymore */
        pModes->aPendingRemoveCurResolutions[u32Target] = 0;
        rc = VINF_ALREADY_INITIALIZED;
        fTransientIfExists = true;
    }
    else
    {
        rc = VPoxVModesAdd(&pModes->Modes, u32Target, CR_RSIZE2U64(*pResolution));
        if (RT_FAILURE(rc))
        {
            WARN(("VPoxVModesAdd failed %d", rc));
            return rc;
        }
    }

    if (rc == VINF_ALREADY_INITIALIZED && !fTransientIfExists)
        return rc;

    if (fTransient)
    {
        if (pModes->aTransientResolutions[u32Target])
        {
            /* note that we can not overwrite rc here, because it holds the "existed" status, which we need to return */
            RTRECTSIZE size = CR_U642RSIZE(pModes->aTransientResolutions[u32Target]);
            int tmpRc = vpoxWddmVModesRemove(pExt, pModes, u32Target, &size);
            if (RT_FAILURE(tmpRc))
            {
                WARN(("vpoxWddmVModesRemove failed %d, can never happen", tmpRc));
                return tmpRc;
            }
        }
        Assert(!pModes->aTransientResolutions[u32Target]);

        pModes->aTransientResolutions[u32Target] = CR_RSIZE2U64(*pResolution);
        vpoxWddmVModesSaveTransient(pExt, u32Target, pResolution);
    }

    return rc;
}

int voxWddmVModesInitForTarget(PVPOXMP_DEVEXT pExt, VPOXWDDM_VMODES *pModes, uint32_t u32Target)
{
    for (uint32_t i = 0; i < RT_ELEMENTS(g_VPoxBuiltinResolutions); ++i)
    {
        vpoxWddmVModesAdd(pExt, pModes, u32Target, &g_VPoxBuiltinResolutions[i], FALSE);
    }

    if (pExt->aTargets[u32Target].Size.cx)
    {
        vpoxWddmVModesAdd(pExt, pModes, u32Target, &pExt->aTargets[u32Target].Size, TRUE);
    }

    /* Check registry for manually added modes, up to 128 entries is supported
     * Give up on the first error encountered.
     */
    VPOXMPCMNREGISTRY Registry;
    VP_STATUS vpRc;

    vpRc = VPoxMPCmnRegInit(pExt, &Registry);
    if (vpRc != NO_ERROR)
    {
        WARN(("VPoxMPCmnRegInit failed %d, ignore", vpRc));
        return VINF_SUCCESS;
    }

    uint32_t CustomXRes = 0, CustomYRes = 0;

    if (u32Target == 0)
    {
        /*First name without a suffix*/
        vpRc = VPoxMPCmnRegQueryDword(Registry, L"CustomXRes", &CustomXRes);
        VPOXMP_WARN_VPS_NOBP(vpRc);
        vpRc = VPoxMPCmnRegQueryDword(Registry, L"CustomYRes", &CustomYRes);
        VPOXMP_WARN_VPS_NOBP(vpRc);
    }
    else
    {
        wchar_t keyname[32];
        swprintf(keyname, L"CustomXRes%d", u32Target);
        vpRc = VPoxMPCmnRegQueryDword(Registry, keyname, &CustomXRes);
        VPOXMP_WARN_VPS_NOBP(vpRc);
        swprintf(keyname, L"CustomYRes%d", u32Target);
        vpRc = VPoxMPCmnRegQueryDword(Registry, keyname, &CustomYRes);
        VPOXMP_WARN_VPS_NOBP(vpRc);
    }

    LOG(("got stored custom resolution[%d] %dx%dx", u32Target, CustomXRes, CustomYRes));

    if (CustomXRes || CustomYRes)
    {
        if (CustomXRes == 0)
            CustomXRes = pExt->aTargets[u32Target].Size.cx ? pExt->aTargets[u32Target].Size.cx : 800;
        if (CustomYRes == 0)
            CustomYRes = pExt->aTargets[u32Target].Size.cy ? pExt->aTargets[u32Target].Size.cy : 600;

        RTRECTSIZE Resolution = {CustomXRes, CustomYRes};
        vpoxWddmVModesAdd(pExt, pModes, u32Target, &Resolution, TRUE);
    }


    for (int curKey=0; curKey<128; curKey++)
    {
        wchar_t keyname[24];

        swprintf(keyname, L"CustomMode%dWidth", curKey);
        vpRc = VPoxMPCmnRegQueryDword(Registry, keyname, &CustomXRes);
        VPOXMP_CHECK_VPS_BREAK(vpRc);

        swprintf(keyname, L"CustomMode%dHeight", curKey);
        vpRc = VPoxMPCmnRegQueryDword(Registry, keyname, &CustomYRes);
        VPOXMP_CHECK_VPS_BREAK(vpRc);

        LOG(("got custom mode[%u]=%ux%u", curKey, CustomXRes, CustomYRes));

        /* round down width to be a multiple of 8 if necessary */
        if (!VPoxCommonFromDeviceExt(pExt)->fAnyX)
        {
            CustomXRes &= 0xFFF8;
        }

        LOG(("adding video mode from registry."));

        RTRECTSIZE Resolution = {CustomXRes, CustomYRes};

        vpoxWddmVModesAdd(pExt, pModes, u32Target, &Resolution, FALSE);
    }

    vpRc = VPoxMPCmnRegFini(Registry);
    VPOXMP_WARN_VPS(vpRc);

    return VINF_SUCCESS;
}

static VPOXWDDM_VMODES g_VPoxWddmVModes;

void VPoxWddmVModesCleanup()
{
    VPOXWDDM_VMODES *pModes = &g_VPoxWddmVModes;
    vpoxWddmVModesCleanup(pModes);
}

int VPoxWddmVModesInit(PVPOXMP_DEVEXT pExt)
{
    VPOXWDDM_VMODES *pModes = &g_VPoxWddmVModes;

    vpoxWddmVModesInit(pModes, VPoxCommonFromDeviceExt(pExt)->cDisplays);

    int rc;

    for (int i = 0; i < VPoxCommonFromDeviceExt(pExt)->cDisplays; ++i)
    {
        rc = voxWddmVModesInitForTarget(pExt, pModes, (uint32_t)i);
        if (RT_FAILURE(rc))
        {
            WARN(("voxWddmVModesInitForTarget failed %d", rc));
            return rc;
        }
    }

    return VINF_SUCCESS;
}

const CR_SORTARRAY* VPoxWddmVModesGet(PVPOXMP_DEVEXT pExt, uint32_t u32Target)
{
    if (u32Target >= (uint32_t)VPoxCommonFromDeviceExt(pExt)->cDisplays)
    {
        WARN(("invalid target"));
        return NULL;
    }

    return &g_VPoxWddmVModes.Modes.aTargets[u32Target];
}

int VPoxWddmVModesRemove(PVPOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution)
{
    return vpoxWddmVModesRemove(pExt, &g_VPoxWddmVModes, u32Target, pResolution);
}

int VPoxWddmVModesAdd(PVPOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution, BOOLEAN fTrancient)
{
    return vpoxWddmVModesAdd(pExt, &g_VPoxWddmVModes, u32Target, pResolution, fTrancient);
}


static NTSTATUS vpoxWddmChildStatusReportPerform(PVPOXMP_DEVEXT pDevExt, PVPOXVDMA_CHILD_STATUS pChildStatus, D3DDDI_VIDEO_PRESENT_TARGET_ID iChild)
{
    DXGK_CHILD_STATUS DdiChildStatus;

    Assert(iChild < UINT32_MAX/2);
    Assert(iChild < (UINT)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);

    PVPOXWDDM_TARGET pTarget = &pDevExt->aTargets[iChild];

    if ((pChildStatus->fFlags & VPOXVDMA_CHILD_STATUS_F_DISCONNECTED)
            && pTarget->fConnected)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusConnection;
        DdiChildStatus.ChildUid = iChild;
        DdiChildStatus.HotPlug.Connected = FALSE;

        LOG(("Reporting DISCONNECT to child %d", DdiChildStatus.ChildUid));

        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
        pTarget->fConnected = FALSE;
    }

    if ((pChildStatus->fFlags & VPOXVDMA_CHILD_STATUS_F_CONNECTED)
            && !pTarget->fConnected)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusConnection;
        DdiChildStatus.ChildUid = iChild;
        DdiChildStatus.HotPlug.Connected = TRUE;

        LOG(("Reporting CONNECT to child %d", DdiChildStatus.ChildUid));

        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
        pTarget->fConnected = TRUE;
    }

    if (pChildStatus->fFlags & VPOXVDMA_CHILD_STATUS_F_ROTATED)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusRotation;
        DdiChildStatus.ChildUid = iChild;
        DdiChildStatus.Rotation.Angle = pChildStatus->u8RotationAngle;

        LOG(("Reporting ROTATED to child %d", DdiChildStatus.ChildUid));

        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS vpoxWddmChildStatusHandleRequest(PVPOXMP_DEVEXT pDevExt, VPOXVDMACMD_CHILD_STATUS_IRQ *pBody)
{
    NTSTATUS Status = STATUS_SUCCESS;

    for (UINT i = 0; i < pBody->cInfos; ++i)
    {
        PVPOXVDMA_CHILD_STATUS pInfo = &pBody->aInfos[i];
        if (pBody->fFlags & VPOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL)
        {
            for (D3DDDI_VIDEO_PRESENT_TARGET_ID iChild = 0; iChild < (UINT)VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++iChild)
            {
                Status = vpoxWddmChildStatusReportPerform(pDevExt, pInfo, iChild);
                if (!NT_SUCCESS(Status))
                {
                    WARN(("vpoxWddmChildStatusReportPerform failed with Status (0x%x)", Status));
                    break;
                }
            }
        }
        else
        {
            Status = vpoxWddmChildStatusReportPerform(pDevExt, pInfo, pInfo->iChild);
            if (!NT_SUCCESS(Status))
            {
                WARN(("vpoxWddmChildStatusReportPerform failed with Status (0x%x)", Status));
                break;
            }
        }
    }

    return Status;
}

NTSTATUS VPoxWddmChildStatusReportReconnected(PVPOXMP_DEVEXT pDevExt, uint32_t iChild)
{
    VPOXVDMACMD_CHILD_STATUS_IRQ Body = {0};
    Body.cInfos = 1;
    if (iChild == D3DDDI_ID_ALL)
    {
        Body.fFlags |= VPOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL;
    }
    Body.aInfos[0].iChild = iChild;
    Body.aInfos[0].fFlags = VPOXVDMA_CHILD_STATUS_F_DISCONNECTED | VPOXVDMA_CHILD_STATUS_F_CONNECTED;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    return vpoxWddmChildStatusHandleRequest(pDevExt, &Body);
}

NTSTATUS VPoxWddmChildStatusConnect(PVPOXMP_DEVEXT pDevExt, uint32_t iChild, BOOLEAN fConnect)
{
    Assert(iChild < (uint32_t)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);
    NTSTATUS Status = STATUS_SUCCESS;
    VPOXVDMACMD_CHILD_STATUS_IRQ Body = {0};
    Body.cInfos = 1;
    Body.aInfos[0].iChild = iChild;
    Body.aInfos[0].fFlags = fConnect ? VPOXVDMA_CHILD_STATUS_F_CONNECTED : VPOXVDMA_CHILD_STATUS_F_DISCONNECTED;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    Status = vpoxWddmChildStatusHandleRequest(pDevExt, &Body);
    if (!NT_SUCCESS(Status))
        WARN(("vpoxWddmChildStatusHandleRequest failed Status 0x%x", Status));

    return Status;
}
