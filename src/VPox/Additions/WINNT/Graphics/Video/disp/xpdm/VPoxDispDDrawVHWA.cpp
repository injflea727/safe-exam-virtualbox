/* $Id: VPoxDispDDrawVHWA.cpp $ */
/** @file
 * VPox XPDM Display driver, DirectDraw callbacks VHWA related
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

#include "VPoxDisp.h"
#include "VPoxDispDDraw.h"
#include <iprt/asm.h>

/** @callback_method_impl{FNVPOXVHWACMDCOMPLETION} */
static DECLCALLBACK(void)
VPoxDispVHWASurfBltCompletion(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, void *pvContext)
{
    RT_NOREF(pvContext);
    VPOXVHWACMD_SURF_BLT RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_BLT);
    PVPOXVHWASURFDESC pSrcDesc  = (PVPOXVHWASURFDESC)(uintptr_t)pBody->SrcGuestSurfInfo;
    PVPOXVHWASURFDESC pDestDesc = (PVPOXVHWASURFDESC)(uintptr_t)pBody->DstGuestSurfInfo;

    ASMAtomicDecU32(&pSrcDesc->cPendingBltsSrc);
    ASMAtomicDecU32(&pDestDesc->cPendingBltsDst);

    VPoxDispVHWACommandRelease(pDev, pCmd);
}

/** @callback_method_impl{FNVPOXVHWACMDCOMPLETION} */
static DECLCALLBACK(void)
VPoxDispVHWASurfFlipCompletion(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, void *pvContext)
{
    RT_NOREF(pvContext);
    VPOXVHWACMD_SURF_FLIP RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_FLIP);
    PVPOXVHWASURFDESC pCurrDesc = (PVPOXVHWASURFDESC)(uintptr_t)pBody->CurrGuestSurfInfo;
    PVPOXVHWASURFDESC pTargDesc = (PVPOXVHWASURFDESC)(uintptr_t)pBody->TargGuestSurfInfo;

    ASMAtomicDecU32(&pCurrDesc->cPendingFlipsCurr);
    ASMAtomicDecU32(&pTargDesc->cPendingFlipsTarg);

    VPoxDispVHWACommandRelease(pDev, pCmd);
}

#define VPOXVHWA_CAP(_pdev, _cap) ((_pdev)->vhwa.caps & (_cap))
#define ROP_INDEX(_rop) ((BYTE)((_rop)>>16))
#define SET_SUPPORT_ROP(_aRops, _rop) _aRops[ROP_INDEX(_rop)/32] |= 1L << ((DWORD)(ROP_INDEX(_rop)%32))

int VPoxDispVHWAUpdateDDHalInfo(PVPOXDISPDEV pDev, DD_HALINFO *pHalInfo)
{
    if (!VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_BLT) && !VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_OVERLAY))
    {
        return VERR_NOT_SUPPORTED;
    }

    pHalInfo->ddCaps.dwCaps |= VPoxDispVHWAToDDCAPS(pDev->vhwa.caps);
    if (VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_BLT))
    {
        /* we only support simple dst=src copy
         * Note: search "ternary raster operations" on msdn for more info
         */
        SET_SUPPORT_ROP(pHalInfo->ddCaps.dwRops, SRCCOPY);
    }

    pHalInfo->ddCaps.ddsCaps.dwCaps |= VPoxDispVHWAToDDSCAPS(pDev->vhwa.surfaceCaps);
    pHalInfo->ddCaps.dwCaps2 |= VPoxDispVHWAToDDCAPS2(pDev->vhwa.caps2);

    if (VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_BLT) && VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_BLTSTRETCH))
    {
        pHalInfo->ddCaps.dwFXCaps |= DDFXCAPS_BLTSTRETCHX  | DDFXCAPS_BLTSTRETCHY
                                   | DDFXCAPS_BLTSTRETCHXN | DDFXCAPS_BLTSTRETCHYN
                                   | DDFXCAPS_BLTSHRINKX   | DDFXCAPS_BLTSHRINKY
                                   | DDFXCAPS_BLTSHRINKXN  | DDFXCAPS_BLTSHRINKYN
                                   | DDFXCAPS_BLTARITHSTRETCHY;
    }

    if (VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_OVERLAY) && VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_OVERLAYSTRETCH))
    {
        pHalInfo->ddCaps.dwFXCaps |= DDFXCAPS_OVERLAYSTRETCHX  | DDFXCAPS_OVERLAYSTRETCHY
                                   | DDFXCAPS_OVERLAYSTRETCHXN | DDFXCAPS_OVERLAYSTRETCHYN
                                   | DDFXCAPS_OVERLAYSHRINKX   | DDFXCAPS_OVERLAYSHRINKY
                                   | DDFXCAPS_OVERLAYSHRINKXN  | DDFXCAPS_OVERLAYSHRINKYN
                                   | DDFXCAPS_OVERLAYARITHSTRETCHY;
    }

    pHalInfo->ddCaps.dwCKeyCaps = VPoxDispVHWAToDDCKEYCAPS(pDev->vhwa.colorKeyCaps);

    if (VPOXVHWA_CAP(pDev, VPOXVHWA_CAPS_OVERLAY))
    {
        pHalInfo->ddCaps.dwMaxVisibleOverlays = pDev->vhwa.numOverlays;
        pHalInfo->ddCaps.dwCurrVisibleOverlays = 0;
        pHalInfo->ddCaps.dwMinOverlayStretch = 1;
        pHalInfo->ddCaps.dwMaxOverlayStretch = 32000;
    }

    return VINF_SUCCESS;
}

/*
 * DirectDraw callbacks.
 */

#define IF_NOT_SUPPORTED(_guid)                  \
    if (IsEqualIID(&lpData->guidInfo, &(_guid))) \
    {                                            \
        LOG((#_guid));                           \
    }

DWORD APIENTRY VPoxDispDDGetDriverInfo(DD_GETDRIVERINFODATA *lpData)
{
    LOGF_ENTER();

    lpData->ddRVal = DDERR_CURRENTLYNOTAVAIL;

    if (IsEqualIID(&lpData->guidInfo, &GUID_NTPrivateDriverCaps))
    {
        LOG(("GUID_NTPrivateDriverCaps"));

        DD_NTPRIVATEDRIVERCAPS caps;
        memset(&caps, 0, sizeof(caps));
        caps.dwSize = sizeof(DD_NTPRIVATEDRIVERCAPS);
        caps.dwPrivateCaps = DDHAL_PRIVATECAP_NOTIFYPRIMARYCREATION;

        lpData->dwActualSize = sizeof(DD_NTPRIVATEDRIVERCAPS);
        lpData->ddRVal = DD_OK;
        memcpy(lpData->lpvData, &caps, min(lpData->dwExpectedSize, sizeof(DD_NTPRIVATEDRIVERCAPS)));
    }
    else IF_NOT_SUPPORTED(GUID_NTCallbacks)
    else IF_NOT_SUPPORTED(GUID_D3DCallbacks2)
    else IF_NOT_SUPPORTED(GUID_D3DCallbacks3)
    else IF_NOT_SUPPORTED(GUID_D3DExtendedCaps)
    else IF_NOT_SUPPORTED(GUID_ZPixelFormats)
    else IF_NOT_SUPPORTED(GUID_D3DParseUnknownCommandCallback)
    else IF_NOT_SUPPORTED(GUID_Miscellaneous2Callbacks)
    else IF_NOT_SUPPORTED(GUID_UpdateNonLocalHeap)
    else IF_NOT_SUPPORTED(GUID_GetHeapAlignment)
    else IF_NOT_SUPPORTED(GUID_DDStereoMode)
    else IF_NOT_SUPPORTED(GUID_NonLocalVidMemCaps)
    else IF_NOT_SUPPORTED(GUID_KernelCaps)
    else IF_NOT_SUPPORTED(GUID_KernelCallbacks)
    else IF_NOT_SUPPORTED(GUID_MotionCompCallbacks)
    else IF_NOT_SUPPORTED(GUID_VideoPortCallbacks)
    else IF_NOT_SUPPORTED(GUID_ColorControlCallbacks)
    else IF_NOT_SUPPORTED(GUID_VideoPortCaps)
    else IF_NOT_SUPPORTED(GUID_DDMoreSurfaceCaps)
    else
    {
        LOG(("unknown guid"));
    }


    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDSetColorKey(PDD_SETCOLORKEYDATA lpSetColorKey)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpSetColorKey->lpDD->dhpdev;
    LOGF_ENTER();

    DD_SURFACE_LOCAL *pSurf = lpSetColorKey->lpDDSurface;
    PVPOXVHWASURFDESC pDesc = (PVPOXVHWASURFDESC)pSurf->lpGbl->dwReserved1;

    lpSetColorKey->ddRVal = DD_OK;

    if (pDesc)
    {
        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd =
            VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_COLORKEY_SET, sizeof(VPOXVHWACMD_SURF_COLORKEY_SET));
        if (pCmd)
        {
            VPOXVHWACMD_SURF_COLORKEY_SET RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_COLORKEY_SET);

            pBody->u.in.offSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pSurf->lpGbl->fpVidMem);
            pBody->u.in.hSurf = pDesc->hHostHandle;
            pBody->u.in.flags = VPoxDispVHWAFromDDCKEYs(lpSetColorKey->dwFlags);
            VPoxDispVHWAFromDDCOLORKEY(&pBody->u.in.CKey, &lpSetColorKey->ckNew);

            VPoxDispVHWACommandSubmitAsynchAndComplete(pDev, pCmd);
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpSetColorKey->ddRVal = DDERR_GENERIC;
        }
    }
    else
    {
        WARN(("!pDesc"));
        lpSetColorKey->ddRVal = DDERR_GENERIC;
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDAddAttachedSurface(PDD_ADDATTACHEDSURFACEDATA lpAddAttachedSurface)
{
    LOGF_ENTER();

    lpAddAttachedSurface->ddRVal = DD_OK;

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDBlt(PDD_BLTDATA lpBlt)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpBlt->lpDD->dhpdev;
    LOGF_ENTER();

    DD_SURFACE_LOCAL *pSrcSurf = lpBlt->lpDDSrcSurface;
    DD_SURFACE_LOCAL *pDstSurf = lpBlt->lpDDDestSurface;
    PVPOXVHWASURFDESC pSrcDesc = (PVPOXVHWASURFDESC) pSrcSurf->lpGbl->dwReserved1;
    PVPOXVHWASURFDESC pDstDesc = (PVPOXVHWASURFDESC) pDstSurf->lpGbl->dwReserved1;

    if (pSrcDesc && pDstDesc)
    {
        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST  *pCmd;

        pCmd = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_BLT, sizeof(VPOXVHWACMD_SURF_BLT));
        if (pCmd)
        {
            VPOXVHWACMD_SURF_BLT RT_UNTRUSTED_VOLATILE_HOST  *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_BLT);

            pBody->u.in.offSrcSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pSrcSurf->lpGbl->fpVidMem);
            pBody->u.in.offDstSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pDstSurf->lpGbl->fpVidMem);

            pBody->u.in.hDstSurf = pDstDesc->hHostHandle;
            VPoxDispVHWAFromRECTL(&pBody->u.in.dstRect, &lpBlt->rDest);
            pBody->u.in.hSrcSurf = pSrcDesc->hHostHandle;
            VPoxDispVHWAFromRECTL(&pBody->u.in.srcRect, &lpBlt->rSrc);
            pBody->DstGuestSurfInfo = (uintptr_t)pDstDesc;
            pBody->SrcGuestSurfInfo = (uintptr_t)pSrcDesc;

            pBody->u.in.flags = VPoxDispVHWAFromDDBLTs(lpBlt->dwFlags);
            VPoxDispVHWAFromDDBLTFX(&pBody->u.in.desc, &lpBlt->bltFX);

            ASMAtomicIncU32(&pSrcDesc->cPendingBltsSrc);
            ASMAtomicIncU32(&pDstDesc->cPendingBltsDst);

            VPoxDispVHWARegionAdd(&pDstDesc->NonupdatedMemRegion, &lpBlt->rDest);
            VPoxDispVHWARegionTrySubstitute(&pDstDesc->UpdatedMemRegion, &lpBlt->rDest);

            if(pSrcDesc->UpdatedMemRegion.bValid)
            {
                pBody->u.in.xUpdatedSrcMemValid = 1;
                VPoxDispVHWAFromRECTL(&pBody->u.in.xUpdatedSrcMemRect, &pSrcDesc->UpdatedMemRegion.Rect);
                VPoxDispVHWARegionClear(&pSrcDesc->UpdatedMemRegion);
            }

            VPoxDispVHWACommandSubmitAsynch(pDev, pCmd, VPoxDispVHWASurfBltCompletion, NULL);

            lpBlt->ddRVal = DD_OK;
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpBlt->ddRVal = DDERR_GENERIC;
        }
    }
    else
    {
        WARN(("!(pSrcDesc && pDstDesc)"));
        lpBlt->ddRVal = DDERR_GENERIC;
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDFlip(PDD_FLIPDATA lpFlip)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpFlip->lpDD->dhpdev;
    LOGF_ENTER();

    DD_SURFACE_LOCAL *pCurrSurf = lpFlip->lpSurfCurr;
    DD_SURFACE_LOCAL *pTargSurf = lpFlip->lpSurfTarg;
    PVPOXVHWASURFDESC pCurrDesc = (PVPOXVHWASURFDESC) pCurrSurf->lpGbl->dwReserved1;
    PVPOXVHWASURFDESC pTargDesc = (PVPOXVHWASURFDESC) pTargSurf->lpGbl->dwReserved1;

    if (pCurrDesc && pTargDesc)
    {
        if(ASMAtomicUoReadU32(&pCurrDesc->cPendingFlipsTarg) || ASMAtomicUoReadU32(&pCurrDesc->cPendingFlipsCurr)
           || ASMAtomicUoReadU32(&pTargDesc->cPendingFlipsTarg) || ASMAtomicUoReadU32(&pTargDesc->cPendingFlipsCurr))
        {
            VPoxDispVHWACommandCheckHostCmds(pDev);

            if(ASMAtomicUoReadU32(&pCurrDesc->cPendingFlipsTarg) || ASMAtomicUoReadU32(&pCurrDesc->cPendingFlipsCurr)
               || ASMAtomicUoReadU32(&pTargDesc->cPendingFlipsTarg) || ASMAtomicUoReadU32(&pTargDesc->cPendingFlipsCurr))
            {
                lpFlip->ddRVal = DDERR_WASSTILLDRAWING;
                return DDHAL_DRIVER_HANDLED;
            }
        }

        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd
            = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_FLIP, sizeof(VPOXVHWACMD_SURF_FLIP));
        if (pCmd)
        {
            VPOXVHWACMD_SURF_FLIP RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_FLIP);

            pBody->u.in.offCurrSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pCurrSurf->lpGbl->fpVidMem);
            pBody->u.in.offTargSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pTargSurf->lpGbl->fpVidMem);

            pBody->u.in.hTargSurf = pTargDesc->hHostHandle;
            pBody->u.in.hCurrSurf = pCurrDesc->hHostHandle;
            pBody->TargGuestSurfInfo = (uintptr_t)pTargDesc;
            pBody->CurrGuestSurfInfo = (uintptr_t)pCurrDesc;

            pTargDesc->bVisible = pCurrDesc->bVisible;
            pCurrDesc->bVisible = false;


            ASMAtomicIncU32(&pCurrDesc->cPendingFlipsCurr);
            ASMAtomicIncU32(&pTargDesc->cPendingFlipsTarg);
#ifdef DEBUG
            ASMAtomicIncU32(&pCurrDesc->cFlipsCurr);
            ASMAtomicIncU32(&pTargDesc->cFlipsTarg);
#endif

            if(pTargDesc->UpdatedMemRegion.bValid)
            {
                pBody->u.in.xUpdatedTargMemValid = 1;
                VPoxDispVHWAFromRECTL(&pBody->u.in.xUpdatedTargMemRect, &pTargDesc->UpdatedMemRegion.Rect);
                VPoxDispVHWARegionClear(&pTargDesc->UpdatedMemRegion);
            }

            VPoxDispVHWACommandSubmitAsynch(pDev, pCmd, VPoxDispVHWASurfFlipCompletion, NULL);

            lpFlip->ddRVal = DD_OK;
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpFlip->ddRVal = DDERR_GENERIC;
        }
    }
    else
    {
        WARN(("!(pCurrDesc && pTargDesc)"));
        lpFlip->ddRVal = DDERR_GENERIC;
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDGetBltStatus(PDD_GETBLTSTATUSDATA lpGetBltStatus)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpGetBltStatus->lpDD->dhpdev;
    PVPOXVHWASURFDESC pDesc = (PVPOXVHWASURFDESC)lpGetBltStatus->lpDDSurface->lpGbl->dwReserved1;
    LOGF_ENTER();

    if(lpGetBltStatus->dwFlags == DDGBS_CANBLT)
    {
        lpGetBltStatus->ddRVal = DD_OK;
    }
    else /* DDGBS_ISBLTDONE */
    {
        if (pDesc)
        {
            if(ASMAtomicUoReadU32(&pDesc->cPendingBltsSrc) || ASMAtomicUoReadU32(&pDesc->cPendingBltsDst))
            {
                VPoxDispVHWACommandCheckHostCmds(pDev);

                if(ASMAtomicUoReadU32(&pDesc->cPendingBltsSrc) || ASMAtomicUoReadU32(&pDesc->cPendingBltsDst))
                {
                    lpGetBltStatus->ddRVal = DDERR_WASSTILLDRAWING;
                }
                else
                {
                    lpGetBltStatus->ddRVal = DD_OK;
                }
            }
            else
            {
                lpGetBltStatus->ddRVal = DD_OK;
            }
        }
        else
        {
            WARN(("!pDesc"));
            lpGetBltStatus->ddRVal = DDERR_GENERIC;
        }
    }


    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDGetFlipStatus(PDD_GETFLIPSTATUSDATA lpGetFlipStatus)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpGetFlipStatus->lpDD->dhpdev;
    PVPOXVHWASURFDESC pDesc = (PVPOXVHWASURFDESC)lpGetFlipStatus->lpDDSurface->lpGbl->dwReserved1;
    LOGF_ENTER();

    /*can't flip is there's a flip pending, so result is same for DDGFS_CANFLIP/DDGFS_ISFLIPDONE */

    if (pDesc)
    {
        if(ASMAtomicUoReadU32(&pDesc->cPendingFlipsTarg) || ASMAtomicUoReadU32(&pDesc->cPendingFlipsCurr))
        {
            VPoxDispVHWACommandCheckHostCmds(pDev);

            if(ASMAtomicUoReadU32(&pDesc->cPendingFlipsTarg) || ASMAtomicUoReadU32(&pDesc->cPendingFlipsCurr))
            {
                lpGetFlipStatus->ddRVal = DDERR_WASSTILLDRAWING;
            }
            else
            {
                lpGetFlipStatus->ddRVal = DD_OK;
            }
        }
        else
        {
            lpGetFlipStatus->ddRVal = DD_OK;
        }
    }
    else
    {
        WARN(("!pDesc"));
        lpGetFlipStatus->ddRVal = DDERR_GENERIC;
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDSetOverlayPosition(PDD_SETOVERLAYPOSITIONDATA lpSetOverlayPosition)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpSetOverlayPosition->lpDD->dhpdev;
    DD_SURFACE_LOCAL *pSrcSurf = lpSetOverlayPosition->lpDDSrcSurface;
    DD_SURFACE_LOCAL *pDstSurf = lpSetOverlayPosition->lpDDDestSurface;
    PVPOXVHWASURFDESC pSrcDesc = (PVPOXVHWASURFDESC) pSrcSurf->lpGbl->dwReserved1;
    PVPOXVHWASURFDESC pDstDesc = (PVPOXVHWASURFDESC) pDstSurf->lpGbl->dwReserved1;

    LOGF_ENTER();

    if (pSrcDesc && pDstDesc)
    {
        if (!pSrcDesc->bVisible)
        {
            WARN(("!pSrcDesc->bVisible"));
            lpSetOverlayPosition->ddRVal = DDERR_GENERIC;
            return DDHAL_DRIVER_HANDLED;
        }

        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd
            = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION, sizeof(VPOXVHWACMD_SURF_OVERLAY_SETPOSITION));
        if (pCmd)
        {
            VPOXVHWACMD_SURF_OVERLAY_SETPOSITION RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_OVERLAY_SETPOSITION);

            pBody->u.in.offSrcSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pSrcSurf->lpGbl->fpVidMem);
            pBody->u.in.offDstSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pDstSurf->lpGbl->fpVidMem);

            pBody->u.in.hSrcSurf = pSrcDesc->hHostHandle;
            pBody->u.in.hDstSurf = pDstDesc->hHostHandle;

            pBody->u.in.xPos = lpSetOverlayPosition->lXPos;
            pBody->u.in.yPos = lpSetOverlayPosition->lYPos;

            VPoxDispVHWACommandSubmitAsynchAndComplete(pDev, pCmd);

            lpSetOverlayPosition->ddRVal = DD_OK;
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpSetOverlayPosition->ddRVal = DDERR_GENERIC;
        }
    }
    else
    {
        WARN(("!(pSrcDesc && pDstDesc)"));
        lpSetOverlayPosition->ddRVal = DDERR_GENERIC;
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

DWORD APIENTRY VPoxDispDDUpdateOverlay(PDD_UPDATEOVERLAYDATA lpUpdateOverlay)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpUpdateOverlay->lpDD->dhpdev;
    DD_SURFACE_LOCAL* pSrcSurf = lpUpdateOverlay->lpDDSrcSurface;
    DD_SURFACE_LOCAL* pDstSurf = lpUpdateOverlay->lpDDDestSurface;
    PVPOXVHWASURFDESC pSrcDesc = (PVPOXVHWASURFDESC) pSrcSurf->lpGbl->dwReserved1;

    LOGF_ENTER();

    if (pSrcDesc)
    {
        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd
            = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE, sizeof(VPOXVHWACMD_SURF_OVERLAY_UPDATE));
        if (pCmd)
        {
            VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_OVERLAY_UPDATE);

            pBody->u.in.offSrcSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pSrcSurf->lpGbl->fpVidMem);

            pBody->u.in.hSrcSurf = pSrcDesc->hHostHandle;

            VPoxDispVHWAFromRECTL(&pBody->u.in.dstRect, &lpUpdateOverlay->rDest);
            VPoxDispVHWAFromRECTL(&pBody->u.in.srcRect, &lpUpdateOverlay->rSrc);

            pBody->u.in.flags = VPoxDispVHWAFromDDOVERs(lpUpdateOverlay->dwFlags);
            VPoxDispVHWAFromDDOVERLAYFX(&pBody->u.in.desc, &lpUpdateOverlay->overlayFX);

            if (lpUpdateOverlay->dwFlags & DDOVER_HIDE)
            {
                pSrcDesc->bVisible = false;
            }
            else if(lpUpdateOverlay->dwFlags & DDOVER_SHOW)
            {
                pSrcDesc->bVisible = true;
                if(pSrcDesc->UpdatedMemRegion.bValid)
                {
                    pBody->u.in.xFlags = VPOXVHWACMD_SURF_OVERLAY_UPDATE_F_SRCMEMRECT;
                    VPoxDispVHWAFromRECTL(&pBody->u.in.xUpdatedSrcMemRect, &pSrcDesc->UpdatedMemRegion.Rect);
                    VPoxDispVHWARegionClear(&pSrcDesc->UpdatedMemRegion);
                }
            }

            if(pDstSurf)
            {
                PVPOXVHWASURFDESC pDstDesc = (PVPOXVHWASURFDESC) pDstSurf->lpGbl->dwReserved1;

                if (!pDstDesc)
                {
                    WARN(("!pDstDesc"));
                    lpUpdateOverlay->ddRVal = DDERR_GENERIC;
                    return DDHAL_DRIVER_HANDLED;
                }

                pBody->u.in.hDstSurf = pDstDesc->hHostHandle;
                pBody->u.in.offDstSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pDstSurf->lpGbl->fpVidMem);
            }

            VPoxDispVHWACommandSubmitAsynchAndComplete(pDev, pCmd);

            lpUpdateOverlay->ddRVal = DD_OK;
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpUpdateOverlay->ddRVal = DDERR_GENERIC;
        }
    }
    else
    {
        WARN(("!pSrcDesc"));
        lpUpdateOverlay->ddRVal = DDERR_GENERIC;
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}
