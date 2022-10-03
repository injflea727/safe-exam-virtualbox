/* $Id: VPoxDispDDraw.cpp $ */
/** @file
 * VPox XPDM Display driver, DirectDraw callbacks
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
#include "VPoxDispMini.h"
#include <iprt/asm.h>

/* Called to check if our driver can create surface with requested attributes */
DWORD APIENTRY VPoxDispDDCanCreateSurface(PDD_CANCREATESURFACEDATA lpCanCreateSurface)
{
    LOGF_ENTER();

    PDD_SURFACEDESC lpDDS = lpCanCreateSurface->lpDDSurfaceDesc;

#ifdef VPOX_WITH_VIDEOHWACCEL
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpCanCreateSurface->lpDD->dhpdev;
    if(pDev->vhwa.bEnabled)
    {
        uint32_t unsupportedSCaps = VPoxDispVHWAUnsupportedDDSCAPS(lpDDS->ddsCaps.dwCaps);
        if(unsupportedSCaps)
        {
            WARN(("unsupported ddscaps: %#x", unsupportedSCaps));
            lpCanCreateSurface->ddRVal = DDERR_INVALIDCAPS;
            return DDHAL_DRIVER_HANDLED;
        }

        unsupportedSCaps = VPoxDispVHWAUnsupportedDDPFS(lpDDS->ddpfPixelFormat.dwFlags);
        if(unsupportedSCaps)
        {
            WARN(("unsupported pixel format: %#x", unsupportedSCaps));
            lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
            return DDHAL_DRIVER_HANDLED;
        }

        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST * pCmd
            = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_CANCREATE, sizeof(VPOXVHWACMD_SURF_CANCREATE));
        if (pCmd)
        {
            int rc;
            VPOXVHWACMD_SURF_CANCREATE RT_UNTRUSTED_VOLATILE_HOST  *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_CANCREATE);

            rc = VPoxDispVHWAFromDDSURFACEDESC(&pBody->SurfInfo, lpDDS);
            pBody->u.in.bIsDifferentPixelFormat = lpCanCreateSurface->bIsDifferentPixelFormat;

            VPoxDispVHWACommandSubmit(pDev, pCmd);

            if (RT_SUCCESS(pCmd->rc))
            {
                if(pBody->u.out.ErrInfo)
                {
                    WARN(("pBody->u.out.ErrInfo = %#x", pBody->u.out.ErrInfo));
                    lpCanCreateSurface->ddRVal = DDERR_GENERIC;
                }
                else
                {
                    lpCanCreateSurface->ddRVal = DD_OK;
                }
            }
            else
            {
                WARN(("VPoxDispVHWACommandSubmit failed with rc=%#x", rc));
                lpCanCreateSurface->ddRVal = DDERR_GENERIC;
            }
            VPoxDispVHWACommandRelease(pDev, pCmd);
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpCanCreateSurface->ddRVal = DDERR_GENERIC;
        }
        return DDHAL_DRIVER_HANDLED;
    }
#endif /*VPOX_WITH_VIDEOHWACCEL*/

    if (lpDDS->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
    {
        LOG(("No Z-Bufer support"));
        lpCanCreateSurface->ddRVal = DDERR_UNSUPPORTED;
        return DDHAL_DRIVER_HANDLED;
    }
    if (lpDDS->ddsCaps.dwCaps & DDSCAPS_TEXTURE)
    {
        LOG(("No texture support"));
        lpCanCreateSurface->ddRVal = DDERR_UNSUPPORTED;
        return DDHAL_DRIVER_HANDLED;
    }
    if (lpCanCreateSurface->bIsDifferentPixelFormat && (lpDDS->ddpfPixelFormat.dwFlags & DDPF_FOURCC))
    {
        LOG(("FOURCC not supported"));
        lpCanCreateSurface->ddRVal = DDERR_UNSUPPORTED;
        return DDHAL_DRIVER_HANDLED;
    }

    lpCanCreateSurface->ddRVal = DD_OK;
    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

/* Called to create DirectDraw surface.
 * Note: we always return DDHAL_DRIVER_NOTHANDLED, which asks DirectDraw memory manager
 * to perform actual memory allocation in our DDraw heap.
 */
DWORD APIENTRY VPoxDispDDCreateSurface(PDD_CREATESURFACEDATA lpCreateSurface)
{
    LOGF_ENTER();

    PDD_SURFACE_LOCAL pSurf = lpCreateSurface->lplpSList[0];

    if (pSurf->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        LOG(("primary surface"));
        pSurf->lpGbl->fpVidMem = 0;
    }
    else
    {
        LOG(("non primary surface"));
        pSurf->lpGbl->fpVidMem = DDHAL_PLEASEALLOC_BLOCKSIZE;
    }
    pSurf->lpGbl->dwReserved1 = 0;

#ifdef VPOX_WITH_VIDEOHWACCEL
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpCreateSurface->lpDD->dhpdev;
    if(pDev->vhwa.bEnabled)
    {
        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd
            = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_CREATE, sizeof(VPOXVHWACMD_SURF_CREATE));
        if (pCmd)
        {
            VPOXVHWACMD_SURF_CREATE RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_CREATE);
            PVPOXVHWASURFDESC pDesc;
            int rc;

            rc = VPoxDispVHWAFromDDSURFACEDESC(&pBody->SurfInfo, lpCreateSurface->lpDDSurfaceDesc);
            VPOX_WARNRC(rc);

            pBody->SurfInfo.surfCaps = VPoxDispVHWAFromDDSCAPS(pSurf->ddsCaps.dwCaps);
            pBody->SurfInfo.flags |= DDSD_CAPS;

            pBody->SurfInfo.height = pSurf->lpGbl->wHeight;
            pBody->SurfInfo.width = pSurf->lpGbl->wWidth;
            pBody->SurfInfo.flags |= DDSD_HEIGHT | DDSD_WIDTH;

            VPoxDispVHWAFromDDPIXELFORMAT(&pBody->SurfInfo.PixelFormat, &pSurf->lpGbl->ddpfSurface);
            pBody->SurfInfo.flags |= VPOXVHWA_SD_PIXELFORMAT;

            if (pSurf->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                pBody->SurfInfo.offSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, 0);
            }
            else
            {
                pBody->SurfInfo.offSurface = VPOXVHWA_OFFSET64_VOID;
            }

            pDesc = VPoxDispVHWASurfDescAlloc();
            if (pDesc)
            {
                VPoxDispVHWACommandSubmit(pDev, pCmd);
                if (RT_SUCCESS(pCmd->rc))
                {
                    pDesc->hHostHandle = pBody->SurfInfo.hSurf;

                    if(!!(pSurf->ddsCaps.dwCaps & DDSCAPS_OVERLAY)
                       && !!(pSurf->ddsCaps.dwCaps & DDSCAPS_VISIBLE))
                    {
                        pDesc->bVisible = true;
                    }

                    pSurf->lpGbl->dwBlockSizeX = pBody->SurfInfo.sizeX;
                    pSurf->lpGbl->dwBlockSizeY = pBody->SurfInfo.sizeY;
                    pSurf->lpGbl->lPitch       = pBody->SurfInfo.pitch;

                    lpCreateSurface->lpDDSurfaceDesc->lPitch = pSurf->lpGbl->lPitch;
                    lpCreateSurface->lpDDSurfaceDesc->dwFlags |= DDSD_PITCH;


                    /** @todo it's probably a memory leak, because DDDestroySurface wouldn't be called for
                     *       primary surfaces.
                     */
                    pSurf->lpGbl->dwReserved1 = (ULONG_PTR)pDesc;
                }
                else
                {
                    WARN(("VPoxDispVHWACommandSubmit failed with rc=%#x", rc));
                    VPoxDispVHWASurfDescFree(pDesc);
                }
            }
            else
            {
                WARN(("VPoxDispVHWASurfDescAlloc failed"));
            }
            VPoxDispVHWACommandRelease(pDev, pCmd);
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed"));
        }
        return DDHAL_DRIVER_NOTHANDLED;
    }
#endif /*VPOX_WITH_VIDEOHWACCEL*/

    LPDDSURFACEDESC pDesc = lpCreateSurface->lpDDSurfaceDesc;

    if (pDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED4)
    {
        pSurf->lpGbl->lPitch = RT_ALIGN_T(pSurf->lpGbl->wWidth/2, 32, LONG);
    }
    else
    if (pDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8)
    {
        pSurf->lpGbl->lPitch = RT_ALIGN_T(pSurf->lpGbl->wWidth, 32, LONG);
    }
    else
    {
        pSurf->lpGbl->lPitch = pSurf->lpGbl->wWidth*(pDesc->ddpfPixelFormat.dwRGBBitCount/8);
    }

    pSurf->lpGbl->dwBlockSizeX = pSurf->lpGbl->lPitch;
    pSurf->lpGbl->dwBlockSizeY = pSurf->lpGbl->wHeight;

    pDesc->lPitch = pSurf->lpGbl->lPitch;
    pDesc->dwFlags |= DDSD_PITCH;

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}

/* Called to destroy DirectDraw surface,
 * in particular we should free vhwa resources allocated on VPoxDispDDCreateSurface.
 * Note: we're always returning DDHAL_DRIVER_NOTHANDLED because we rely on DirectDraw memory manager.
 */
DWORD APIENTRY VPoxDispDDDestroySurface(PDD_DESTROYSURFACEDATA lpDestroySurface)
{
    LOGF_ENTER();

    lpDestroySurface->ddRVal = DD_OK;

#ifdef VPOX_WITH_VIDEOHWACCEL
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpDestroySurface->lpDD->dhpdev;
    if (pDev->vhwa.bEnabled)
    {
        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST  *pCmd
            = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_DESTROY, sizeof(VPOXVHWACMD_SURF_DESTROY));
        if (pCmd)
        {
            PVPOXVHWASURFDESC pDesc = (PVPOXVHWASURFDESC)lpDestroySurface->lpDDSurface->lpGbl->dwReserved1;
            if (pDesc)
            {
                VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_DESTROY);
                pBody->u.in.hSurf = pDesc->hHostHandle;

                VPoxDispVHWACommandSubmit(pDev, pCmd);

                VPoxDispVHWACommandRelease(pDev, pCmd);

                VPoxDispVHWASurfDescFree(pDesc);

                lpDestroySurface->lpDDSurface->lpGbl->dwReserved1 = (ULONG_PTR)NULL;
            }
            else
            {
                WARN(("!pDesc, memory overwrite somewhere?"));
                lpDestroySurface->ddRVal = DDERR_GENERIC;
            }
        }
        else
        {
            WARN(("VPoxDispVHWACommandCreate failed!"));
            lpDestroySurface->ddRVal = DDERR_GENERIC;
        }
    }
    else
#endif /*VPOX_WITH_VIDEOHWACCEL*/

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}

/* Called before first DDLock/after last DDUnlock to map/unmap surface memory from given process address space
 * We go easy way and map whole framebuffer and offscreen DirectDraw heap every time.
 */
DWORD APIENTRY VPoxDispDDMapMemory(PDD_MAPMEMORYDATA lpMapMemory)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpMapMemory->lpDD->dhpdev;
    VIDEO_SHARE_MEMORY smem;
    int rc;
    LOGF_ENTER();

    lpMapMemory->ddRVal = DDERR_GENERIC;

    memset(&smem, 0, sizeof(smem));
    smem.ProcessHandle = lpMapMemory->hProcess;

    if (lpMapMemory->bMap)
    {
        VIDEO_SHARE_MEMORY_INFORMATION  smemInfo;

        smem.ViewSize = pDev->layout.offDDrawHeap + pDev->layout.cbDDrawHeap;

        rc = VPoxDispMPShareVideoMemory(pDev->hDriver, &smem, &smemInfo);
        VPOX_WARNRC_RETV(rc, DDHAL_DRIVER_HANDLED);

        lpMapMemory->fpProcess = (FLATPTR) smemInfo.VirtualAddress;
    }
    else
    {
        smem.RequestedVirtualAddress = (PVOID) lpMapMemory->fpProcess;

        rc = VPoxDispMPUnshareVideoMemory(pDev->hDriver, &smem);
        VPOX_WARNRC_RETV(rc, DDHAL_DRIVER_HANDLED);
    }


    lpMapMemory->ddRVal = DD_OK;
    LOGF_LEAVE();
    return DDHAL_DRIVER_HANDLED;
}

/* Lock specified area of surface */
DWORD APIENTRY VPoxDispDDLock(PDD_LOCKDATA lpLock)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpLock->lpDD->dhpdev;
    LOGF_ENTER();

    DD_SURFACE_LOCAL* pSurf = lpLock->lpDDSurface;

    lpLock->ddRVal = DD_OK;

#ifdef VPOX_WITH_VIDEOHWACCEL
    if(pDev->vhwa.bEnabled)
    {
        PVPOXVHWASURFDESC pDesc = (PVPOXVHWASURFDESC) pSurf->lpGbl->dwReserved1;
        RECTL tmpRect, *pRect;

        if (!pDesc)
        {
            WARN(("!pDesc, memory overwrite somewhere?"));
            lpLock->ddRVal = DDERR_GENERIC;
            return DDHAL_DRIVER_HANDLED;
        }

        /* Check if host is still processing drawing commands */
        if (ASMAtomicUoReadU32(&pDesc->cPendingBltsSrc)
            || ASMAtomicUoReadU32(&pDesc->cPendingFlipsCurr)
            || ASMAtomicUoReadU32(&pDesc->cPendingBltsDst)
            || ASMAtomicUoReadU32(&pDesc->cPendingFlipsTarg))
        {
            VPoxDispVHWACommandCheckHostCmds(pDev);
            if(ASMAtomicUoReadU32(&pDesc->cPendingBltsSrc)
               || ASMAtomicUoReadU32(&pDesc->cPendingFlipsCurr)
               || ASMAtomicUoReadU32(&pDesc->cPendingBltsDst)
               || ASMAtomicUoReadU32(&pDesc->cPendingFlipsTarg))
            {
                lpLock->ddRVal = DDERR_WASSTILLDRAWING;
                return DDHAL_DRIVER_HANDLED;
            }
        }

        if (lpLock->bHasRect)
        {
            pRect = &lpLock->rArea;
        }
        else
        {
            tmpRect.left = 0;
            tmpRect.top = 0;
            tmpRect.right = pSurf->lpGbl->wWidth-1;
            tmpRect.bottom = pSurf->lpGbl->wHeight-1;
            pRect = &tmpRect;
        }

        if (lpLock->dwFlags & DDLOCK_DISCARDCONTENTS)
        {
            VPoxDispVHWARegionTrySubstitute(&pDesc->NonupdatedMemRegion, pRect);
            VPoxDispVHWARegionAdd(&pDesc->UpdatedMemRegion, pRect);
        }
        else if (!VPoxDispVHWARegionIntersects(&pDesc->NonupdatedMemRegion, pRect))
        {
            VPoxDispVHWARegionAdd(&pDesc->UpdatedMemRegion, pRect);
        }
        else
        {
            VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd
                = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_LOCK, sizeof(VPOXVHWACMD_SURF_LOCK));
            if (pCmd)
            {
                VPOXVHWACMD_SURF_LOCK RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_LOCK);

                pBody->u.in.offSurface = VPoxDispVHWAVramOffsetFromPDEV(pDev, pSurf->lpGbl->fpVidMem);

                VPoxDispVHWAFromRECTL(&pBody->u.in.rect, &pDesc->NonupdatedMemRegion.Rect);
                pBody->u.in.rectValid = 1;

                pBody->u.in.hSurf = pDesc->hHostHandle;

                /* wait for the surface to be locked and memory buffer updated */
                VPoxDispVHWACommandSubmit(pDev, pCmd);
                VPOX_WARNRC(pCmd->rc);
                VPoxDispVHWACommandRelease(pDev, pCmd);
                VPoxDispVHWARegionClear(&pDesc->NonupdatedMemRegion);
            }
            else
            {
                WARN(("VPoxDispVHWACommandCreate failed!"));
                lpLock->ddRVal = DDERR_GENERIC;
            }
        }

        return DDHAL_DRIVER_NOTHANDLED;
    }
#endif /*VPOX_WITH_VIDEOHWACCEL*/

    /* We only care about primary surface as we'd have to report dirty rectangles to the host in the DDUnlock*/
    if (pSurf->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        pDev->ddpsLock.bLocked = TRUE;

        if (lpLock->bHasRect)
        {
            pDev->ddpsLock.rect = lpLock->rArea;
        }
        else
        {
            pDev->ddpsLock.rect.left = 0;
            pDev->ddpsLock.rect.top = 0;
            pDev->ddpsLock.rect.right = pDev->mode.ulWidth;
            pDev->ddpsLock.rect.bottom = pDev->mode.ulHeight;
        }
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}

/* Unlock previously locked surface */
DWORD APIENTRY VPoxDispDDUnlock(PDD_UNLOCKDATA lpUnlock)
{
    PVPOXDISPDEV pDev = (PVPOXDISPDEV) lpUnlock->lpDD->dhpdev;
    LOGF_ENTER();

    lpUnlock->ddRVal = DD_OK;

#ifdef VPOX_WITH_VIDEOHWACCEL
    if(pDev->vhwa.bEnabled)
    {
        DD_SURFACE_LOCAL *pSurf = lpUnlock->lpDDSurface;
        PVPOXVHWASURFDESC pDesc = (PVPOXVHWASURFDESC) pSurf->lpGbl->dwReserved1;

        if (!pDesc)
        {
            WARN(("!pDesc, memory overwrite somewhere?"));
            lpUnlock->ddRVal = DDERR_GENERIC;
            return DDHAL_DRIVER_HANDLED;
        }

        if((pSurf->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && pDesc->UpdatedMemRegion.bValid
           && VPoxVBVABufferBeginUpdate(&pDev->vbvaCtx, &pDev->hgsmi.ctx))
        {
            vbvaReportDirtyRect(pDev, &pDesc->UpdatedMemRegion.Rect);

            if (pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents & VPOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET)
            {
                vrdpReset(pDev);
                pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents &= ~VPOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;
            }

            if (pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_VRDP)
            {
                vrdpReportDirtyRect(pDev, &pDesc->UpdatedMemRegion.Rect);
            }

            VPoxVBVABufferEndUpdate(&pDev->vbvaCtx);
        }
        else if ((pSurf->ddsCaps.dwCaps & DDSCAPS_VISIBLE)
                 || ((pSurf->ddsCaps.dwCaps & DDSCAPS_OVERLAY) && pDesc->bVisible))
        {
            VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd
                = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_SURF_UNLOCK, sizeof(VPOXVHWACMD_SURF_UNLOCK));
            if (pCmd)
            {
                VPOXVHWACMD_SURF_UNLOCK RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_UNLOCK);

                pBody->u.in.hSurf = pDesc->hHostHandle;
                if(pDesc->UpdatedMemRegion.bValid)
                {
                    pBody->u.in.xUpdatedMemValid = 1;
                    VPoxDispVHWAFromRECTL(&pBody->u.in.xUpdatedMemRect, &pDesc->UpdatedMemRegion.Rect);
                    VPoxDispVHWARegionClear(&pDesc->UpdatedMemRegion);
                }

                VPoxDispVHWACommandSubmitAsynchAndComplete(pDev, pCmd);
            }
            else
            {
                WARN(("VPoxDispVHWACommandCreate failed!"));
                lpUnlock->ddRVal = DDERR_GENERIC;
            }

        }

        return DDHAL_DRIVER_NOTHANDLED;
    }
#endif /*VPOX_WITH_VIDEOHWACCEL*/

    if (pDev->ddpsLock.bLocked)
    {
        pDev->ddpsLock.bLocked = FALSE;

        if (pDev->hgsmi.bSupported && VPoxVBVABufferBeginUpdate(&pDev->vbvaCtx, &pDev->hgsmi.ctx))
        {
            vbvaReportDirtyRect(pDev, &pDev->ddpsLock.rect);

            if (pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents & VPOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET)
            {
                vrdpReset(pDev);
                pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents &= ~VPOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;
            }

            if (pDev->vbvaCtx.pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_VRDP)
            {
                vrdpReportDirtyRect(pDev, &pDev->ddpsLock.rect);
            }

            VPoxVBVABufferEndUpdate(&pDev->vbvaCtx);
        }
    }

    LOGF_LEAVE();
    return DDHAL_DRIVER_NOTHANDLED;
}
