/* $Id: VPoxMPVbva.cpp $ */
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
#include "common/VPoxMPCommon.h"

/*
 * Public hardware buffer methods.
 */
int vpoxVbvaEnable (PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva)
{
    if (VPoxVBVAEnable(&pVbva->Vbva, &VPoxCommonFromDeviceExt(pDevExt)->guestCtx,
            pVbva->Vbva.pVBVA, pVbva->srcId))
        return VINF_SUCCESS;

    WARN(("VPoxVBVAEnable failed!"));
    return VERR_GENERAL_FAILURE;
}

int vpoxVbvaDisable (PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva)
{
    VPoxVBVADisable(&pVbva->Vbva, &VPoxCommonFromDeviceExt(pDevExt)->guestCtx, pVbva->srcId);
    return VINF_SUCCESS;
}

int vpoxVbvaCreate(PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    memset(pVbva, 0, sizeof(VPOXVBVAINFO));

    KeInitializeSpinLock(&pVbva->Lock);

    int rc = VPoxMPCmnMapAdapterMemory(VPoxCommonFromDeviceExt(pDevExt),
                                       (void**)&pVbva->Vbva.pVBVA,
                                       offBuffer,
                                       cbBuffer);
    if (RT_SUCCESS(rc))
    {
        Assert(pVbva->Vbva.pVBVA);
        VPoxVBVASetupBufferContext(&pVbva->Vbva, offBuffer, cbBuffer);
        pVbva->srcId = srcId;
    }
    else
    {
        WARN(("VPoxMPCmnMapAdapterMemory failed rc %d", rc));
    }


    return rc;
}

int vpoxVbvaDestroy(PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva)
{
    int rc = VINF_SUCCESS;
    VPoxMPCmnUnmapAdapterMemory(VPoxCommonFromDeviceExt(pDevExt), (void**)&pVbva->Vbva.pVBVA);
    memset(pVbva, 0, sizeof (VPOXVBVAINFO));
    return rc;
}

int vpoxVbvaReportDirtyRect (PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_SOURCE pSrc, RECT *pRectOrig)
{
    VBVACMDHDR hdr;

    RECT rect = *pRectOrig;

//        if (rect.left < 0) rect.left = 0;
//        if (rect.top < 0) rect.top = 0;
//        if (rect.right > (int)ppdev->cxScreen) rect.right = ppdev->cxScreen;
//        if (rect.bottom > (int)ppdev->cyScreen) rect.bottom = ppdev->cyScreen;

    hdr.x = (int16_t)rect.left;
    hdr.y = (int16_t)rect.top;
    hdr.w = (uint16_t)(rect.right - rect.left);
    hdr.h = (uint16_t)(rect.bottom - rect.top);

    hdr.x += (int16_t)pSrc->VScreenPos.x;
    hdr.y += (int16_t)pSrc->VScreenPos.y;

    if (VPoxVBVAWrite(&pSrc->Vbva.Vbva, &VPoxCommonFromDeviceExt(pDevExt)->guestCtx, &hdr, sizeof(hdr)))
        return VINF_SUCCESS;

    WARN(("VPoxVBVAWrite failed"));
    return VERR_GENERAL_FAILURE;
}
