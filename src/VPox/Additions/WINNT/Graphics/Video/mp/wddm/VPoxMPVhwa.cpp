/* $Id: VPoxMPVhwa.cpp $ */
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
#include "VPoxMPVhwa.h"

#include <iprt/semaphore.h>
#include <iprt/asm.h>

#define VPOXVHWA_PRIMARY_ALLOCATION(_pSrc) ((_pSrc)->pPrimaryAllocation)

#define VPOXVHWA_COPY_RECT(a_pDst, a_pSrc) do { \
        (a_pDst)->left    = (a_pSrc)->left; \
        (a_pDst)->top     = (a_pSrc)->top; \
        (a_pDst)->right   = (a_pSrc)->right; \
        (a_pDst)->bottom  = (a_pSrc)->bottom; \
    } while(0)


DECLINLINE(void) vpoxVhwaHdrInit(VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pHdr,
                                 D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, VPOXVHWACMD_TYPE enmCmd)
{
    memset((void *)pHdr, 0, sizeof(VPOXVHWACMD));
    pHdr->iDisplay = srcId;
    pHdr->rc = VERR_GENERAL_FAILURE;
    pHdr->enmCmd = enmCmd;
    pHdr->cRefs = 1;
}

DECLINLINE(void) vbvaVhwaCommandRelease(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if(!cRefs)
    {
        VPoxHGSMIBufferFree(&VPoxCommonFromDeviceExt(pDevExt)->guestCtx, pCmd);
    }
}

DECLINLINE(void) vbvaVhwaCommandRetain(VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

/* do not wait for completion */
void vpoxVhwaCommandSubmitAsynch(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd,
                                 PFNVPOXVHWACMDCOMPLETION pfnCompletion, void *pContext)
{
    pCmd->GuestVBVAReserved1 = (uintptr_t)pfnCompletion;
    pCmd->GuestVBVAReserved2 = (uintptr_t)pContext;
    vbvaVhwaCommandRetain(pCmd);

    VPoxHGSMIBufferSubmit(&VPoxCommonFromDeviceExt(pDevExt)->guestCtx, pCmd);

    uint32_t const fFlags = pCmd->Flags;
    if(   !(fFlags & VPOXVHWACMD_FLAG_HG_ASYNCH)
       || (   (fFlags & VPOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION)
           && (fFlags & VPOXVHWACMD_FLAG_HG_ASYNCH_RETURNED) ) )
    {
        /* the command is completed */
        pfnCompletion(pDevExt, pCmd, pContext);
    }

    vbvaVhwaCommandRelease(pDevExt, pCmd);
}

/** @callback_method_impl{FNVPOXVHWACMDCOMPLETION} */
static DECLCALLBACK(void)
vpoxVhwaCompletionSetEvent(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, void *pvContext)
{
    RT_NOREF(pDevExt, pCmd);
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

void vpoxVhwaCommandSubmitAsynchByEvent(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, RTSEMEVENT hEvent)
{
    vpoxVhwaCommandSubmitAsynch(pDevExt, pCmd, vpoxVhwaCompletionSetEvent, hEvent);
}

void vpoxVhwaCommandCheckCompletion(PVPOXMP_DEVEXT pDevExt)
{
    NTSTATUS Status = vpoxWddmCallIsr(pDevExt);
    AssertNtStatusSuccess(Status);
}

VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *vpoxVhwaCommandCreate(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId,
                                                              VPOXVHWACMD_TYPE enmCmd, VPOXVHWACMD_LENGTH cbCmd)
{
    vpoxVhwaCommandCheckCompletion(pDevExt);
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pHdr;
    pHdr = (VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *)VPoxHGSMIBufferAlloc(&VPoxCommonFromDeviceExt(pDevExt)->guestCtx,
                                                                          cbCmd + VPOXVHWACMD_HEADSIZE(),
                                                                          HGSMI_CH_VBVA,
                                                                          VBVA_VHWA_CMD);
    Assert(pHdr);
    if (!pHdr)
        LOGREL(("VPoxHGSMIBufferAlloc failed"));
    else
        vpoxVhwaHdrInit(pHdr, srcId, enmCmd);

    return pHdr;
}

void vpoxVhwaCommandFree(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    vbvaVhwaCommandRelease(pDevExt, pCmd);
}

int vpoxVhwaCommandSubmit(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    RTSEMEVENT hEvent;
    int rc = RTSemEventCreate(&hEvent);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        pCmd->Flags |= VPOXVHWACMD_FLAG_GH_ASYNCH_IRQ;
        vpoxVhwaCommandSubmitAsynchByEvent(pDevExt, pCmd, hEvent);
        rc = RTSemEventWait(hEvent, RT_INDEFINITE_WAIT);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            RTSemEventDestroy(hEvent);
    }
    return rc;
}

/** @callback_method_impl{FNVPOXVHWACMDCOMPLETION} */
static DECLCALLBACK(void)
vpoxVhwaCompletionFreeCmd(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, void *pvContext)
{
    RT_NOREF(pvContext);
    vpoxVhwaCommandFree(pDevExt, pCmd);
}

void vpoxVhwaCompletionListProcess(PVPOXMP_DEVEXT pDevExt, VPOXVTLIST *pList)
{
    PVPOXVTLIST_ENTRY pNext, pCur;
    for (pCur = pList->pFirst; pCur; pCur = pNext)
    {
        /* need to save next since the command may be released in a pfnCallback and thus its data might be invalid */
        pNext = pCur->pNext;
        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPOXVHWA_LISTENTRY2CMD(pCur);
        PFNVPOXVHWACMDCOMPLETION pfnCallback = (PFNVPOXVHWACMDCOMPLETION)pCmd->GuestVBVAReserved1;
        void *pvCallback = (void*)pCmd->GuestVBVAReserved2;
        pfnCallback(pDevExt, pCmd, pvCallback);
    }
}


void vpoxVhwaCommandSubmitAsynchAndComplete(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    pCmd->Flags |= VPOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION;

    vpoxVhwaCommandSubmitAsynch(pDevExt, pCmd, vpoxVhwaCompletionFreeCmd, NULL);
}

static void vpoxVhwaFreeHostInfo1(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *pInfo)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPOXVHWACMD_HEAD(pInfo);
    vpoxVhwaCommandFree(pDevExt, pCmd);
}

static void vpoxVhwaFreeHostInfo2(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *pInfo)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPOXVHWACMD_HEAD(pInfo);
    vpoxVhwaCommandFree(pDevExt, pCmd);
}

static VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *
vpoxVhwaQueryHostInfo1(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pDevExt, srcId, VPOXVHWACMD_TYPE_QUERY_INFO1,
                                                                         sizeof(VPOXVHWACMD_QUERYINFO1));
    AssertReturnStmt(pCmd, LOGREL(("vpoxVhwaCommandCreate failed")), NULL);

    VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *pInfo1 = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO1);
    pInfo1->u.in.guestVersion.maj = VPOXVHWA_VERSION_MAJ;
    pInfo1->u.in.guestVersion.min = VPOXVHWA_VERSION_MIN;
    pInfo1->u.in.guestVersion.bld = VPOXVHWA_VERSION_BLD;
    pInfo1->u.in.guestVersion.reserved = VPOXVHWA_VERSION_RSV;

    int rc = vpoxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        if (RT_SUCCESS(pCmd->rc))
            return VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO1);

    vpoxVhwaCommandFree(pDevExt, pCmd);
    return NULL;
}

static VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *
vpoxVhwaQueryHostInfo2(PVPOXMP_DEVEXT pDevExt,  D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, uint32_t numFourCC)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pDevExt, srcId, VPOXVHWACMD_TYPE_QUERY_INFO2,
                                                                         VPOXVHWAINFO2_SIZE(numFourCC));
    AssertReturnStmt(pCmd, LOGREL(("vpoxVhwaCommandCreate failed")), NULL);

    VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *pInfo2 = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO2);
    pInfo2->numFourCC = numFourCC;

    int rc = vpoxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        AssertRC(pCmd->rc);
        if(RT_SUCCESS(pCmd->rc))
            if(pInfo2->numFourCC == numFourCC)
                return pInfo2;
    }

    vpoxVhwaCommandFree(pDevExt, pCmd);
    return NULL;
}

int vpoxVhwaEnable(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pDevExt, srcId, VPOXVHWACMD_TYPE_ENABLE, 0);
    AssertReturnStmt(pCmd, LOGREL(("vpoxVhwaCommandCreate failed")), VERR_GENERAL_FAILURE);

    int rc = vpoxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        AssertRC(pCmd->rc);
        if(RT_SUCCESS(pCmd->rc))
            rc = VINF_SUCCESS;
        else
            rc = pCmd->rc;
    }

    vpoxVhwaCommandFree(pDevExt, pCmd);
    return rc;
}

int vpoxVhwaDisable(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    vpoxVhwaCommandCheckCompletion(pDevExt);

    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd  = vpoxVhwaCommandCreate(pDevExt, srcId, VPOXVHWACMD_TYPE_DISABLE, 0);
    AssertReturnStmt(pCmd, LOGREL(("vpoxVhwaCommandCreate failed")), VERR_GENERAL_FAILURE);

    int rc = vpoxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        if(RT_SUCCESS(pCmd->rc))
            rc = VINF_SUCCESS;
        else
            rc = pCmd->rc;
    }

    vpoxVhwaCommandFree(pDevExt, pCmd);
    return rc;
}

DECLINLINE(VOID) vpoxVhwaHlpOverlayListInit(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];
    pSource->cOverlays = 0;
    InitializeListHead(&pSource->OverlayList);
    KeInitializeSpinLock(&pSource->OverlayListLock);
}

static void vpoxVhwaInitSrc(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    Assert(srcId < (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);
    VPOXVHWA_INFO *pSettings = &pDevExt->aSources[srcId].Vhwa.Settings;
    memset (pSettings, 0, sizeof (VPOXVHWA_INFO));

    vpoxVhwaHlpOverlayListInit(pDevExt, srcId);

    VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *pInfo1 = vpoxVhwaQueryHostInfo1(pDevExt, srcId);
    if (pInfo1)
    {
        if ((pInfo1->u.out.cfgFlags & VPOXVHWA_CFG_ENABLED)
                && pInfo1->u.out.numOverlays)
        {
            if ((pInfo1->u.out.caps & VPOXVHWA_CAPS_OVERLAY)
                    && (pInfo1->u.out.caps & VPOXVHWA_CAPS_OVERLAYSTRETCH)
                    && (pInfo1->u.out.surfaceCaps & VPOXVHWA_SCAPS_OVERLAY)
                    && (pInfo1->u.out.surfaceCaps & VPOXVHWA_SCAPS_FLIP)
                    && (pInfo1->u.out.surfaceCaps & VPOXVHWA_SCAPS_LOCALVIDMEM)
                    && pInfo1->u.out.numOverlays)
            {
                pSettings->fFlags |= VPOXVHWA_F_ENABLED;

                if (pInfo1->u.out.caps & VPOXVHWA_CAPS_COLORKEY)
                {
                    if (pInfo1->u.out.colorKeyCaps & VPOXVHWA_CKEYCAPS_SRCOVERLAY)
                    {
                        pSettings->fFlags |= VPOXVHWA_F_CKEY_SRC;
                        /** @todo VPOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE ? */
                    }

                    if (pInfo1->u.out.colorKeyCaps & VPOXVHWA_CKEYCAPS_DESTOVERLAY)
                    {
                        pSettings->fFlags |= VPOXVHWA_F_CKEY_DST;
                        /** @todo VPOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE ? */
                    }
                }

                pSettings->cOverlaysSupported = pInfo1->u.out.numOverlays;

                pSettings->cFormats = 0;

                pSettings->aFormats[pSettings->cFormats] = D3DDDIFMT_X8R8G8B8;
                ++pSettings->cFormats;

                if (pInfo1->u.out.numFourCC
                        && (pInfo1->u.out.caps & VPOXVHWA_CAPS_OVERLAYFOURCC))
                {
                    VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *pInfo2 =
                        vpoxVhwaQueryHostInfo2(pDevExt, srcId, pInfo1->u.out.numFourCC);
                    if (pInfo2)
                    {
                        for (uint32_t i = 0; i < pInfo2->numFourCC; ++i)
                        {
                            pSettings->aFormats[pSettings->cFormats] = (D3DDDIFORMAT)pInfo2->FourCC[i];
                            ++pSettings->cFormats;
                        }
                        vpoxVhwaFreeHostInfo2(pDevExt, pInfo2);
                    }
                }
            }
        }
        vpoxVhwaFreeHostInfo1(pDevExt, pInfo1);
    }
}

void vpoxVhwaInit(PVPOXMP_DEVEXT pDevExt)
{
    for (int i = 0; i < VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        vpoxVhwaInitSrc(pDevExt, (D3DDDI_VIDEO_PRESENT_SOURCE_ID)i);
    }
}

void vpoxVhwaFree(PVPOXMP_DEVEXT pDevExt)
{
    /* we do not allocate/map anything, just issue a Disable command
     * to ensure all pending commands are flushed */
    for (int i = 0; i < VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        vpoxVhwaDisable(pDevExt, i);
    }
}

static int vpoxVhwaHlpTranslateFormat(VPOXVHWA_PIXELFORMAT RT_UNTRUSTED_VOLATILE_HOST *pFormat, D3DDDIFORMAT enmFormat)
{
    pFormat->Reserved = 0;
    switch (enmFormat)
    {
        case D3DDDIFMT_A8R8G8B8:
        case D3DDDIFMT_X8R8G8B8:
            pFormat->flags = VPOXVHWA_PF_RGB;
            pFormat->c.rgbBitCount = 32;
            pFormat->m1.rgbRBitMask = 0xff0000;
            pFormat->m2.rgbGBitMask = 0xff00;
            pFormat->m3.rgbBBitMask = 0xff;
            /* always zero for now */
            pFormat->m4.rgbABitMask = 0;
            return VINF_SUCCESS;
        case D3DDDIFMT_R8G8B8:
            pFormat->flags = VPOXVHWA_PF_RGB;
            pFormat->c.rgbBitCount = 24;
            pFormat->m1.rgbRBitMask = 0xff0000;
            pFormat->m2.rgbGBitMask = 0xff00;
            pFormat->m3.rgbBBitMask = 0xff;
            /* always zero for now */
            pFormat->m4.rgbABitMask = 0;
            return VINF_SUCCESS;
        case D3DDDIFMT_R5G6B5:
            pFormat->flags = VPOXVHWA_PF_RGB;
            pFormat->c.rgbBitCount = 16;
            pFormat->m1.rgbRBitMask = 0xf800;
            pFormat->m2.rgbGBitMask = 0x7e0;
            pFormat->m3.rgbBBitMask = 0x1f;
            /* always zero for now */
            pFormat->m4.rgbABitMask = 0;
            return VINF_SUCCESS;
        case D3DDDIFMT_P8:
        case D3DDDIFMT_A8:
        case D3DDDIFMT_X1R5G5B5:
        case D3DDDIFMT_A1R5G5B5:
        case D3DDDIFMT_A4R4G4B4:
        case D3DDDIFMT_R3G3B2:
        case D3DDDIFMT_A8R3G3B2:
        case D3DDDIFMT_X4R4G4B4:
        case D3DDDIFMT_A2B10G10R10:
        case D3DDDIFMT_A8B8G8R8:
        case D3DDDIFMT_X8B8G8R8:
        case D3DDDIFMT_G16R16:
        case D3DDDIFMT_A2R10G10B10:
        case D3DDDIFMT_A16B16G16R16:
        case D3DDDIFMT_A8P8:
        default:
        {
            uint32_t fourcc = vpoxWddmFormatToFourcc(enmFormat);
            Assert(fourcc);
            if (fourcc)
            {
                pFormat->flags = VPOXVHWA_PF_FOURCC;
                pFormat->fourCC = fourcc;
                return VINF_SUCCESS;
            }
            return VERR_NOT_SUPPORTED;
        }
    }
}

int vpoxVhwaHlpDestroySurface(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pSurf,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    Assert(pSurf->hHostHandle);
    if (!pSurf->hHostHandle)
        return VERR_INVALID_STATE;

    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pDevExt, VidPnSourceId, VPOXVHWACMD_TYPE_SURF_DESTROY,
                                                                         sizeof(VPOXVHWACMD_SURF_DESTROY));
    Assert(pCmd);
    if (pCmd)
    {
        VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_DESTROY);

        memset((void *)pBody, 0, sizeof(VPOXVHWACMD_SURF_DESTROY));

        pBody->u.in.hSurf = pSurf->hHostHandle;

        /* we're not interested in completion, just send the command */
        vpoxVhwaCommandSubmitAsynchAndComplete(pDevExt, pCmd);

        pSurf->hHostHandle = VPOXVHWA_SURFHANDLE_INVALID;

        return VINF_SUCCESS;
    }

    return VERR_OUT_OF_RESOURCES;
}

int vpoxVhwaHlpPopulateSurInfo(VPOXVHWA_SURFACEDESC RT_UNTRUSTED_VOLATILE_HOST *pInfo, PVPOXWDDM_ALLOCATION pSurf,
                               uint32_t fFlags, uint32_t cBackBuffers, uint32_t fSCaps,
                               D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    RT_NOREF(VidPnSourceId);
    memset((void *)pInfo, 0, sizeof(VPOXVHWA_SURFACEDESC));

#if 0
    /**
     * The following breaks 2D accelerated video playback because this method is called just after the surface was created
     * and most its members are still 0.
     *
     * @todo: Not 100% sure this is the correct way. It looks like the SegmentId specifies where the  memory
     *        for the surface is stored (VRAM vs. system memory) but because this method is only used
     *        to query some parameters (using VPOXVHWACMD_SURF_GETINFO) and this command doesn't access any surface memory
     *        on the host it should be safe.
     */
    if (pSurf->AllocData.Addr.SegmentId != 1)
    {
        WARN(("invalid segment id!"));
        return VERR_INVALID_PARAMETER;
    }
#endif

    pInfo->height = pSurf->AllocData.SurfDesc.height;
    pInfo->width = pSurf->AllocData.SurfDesc.width;
    pInfo->flags |= VPOXVHWA_SD_HEIGHT | VPOXVHWA_SD_WIDTH;
    if (fFlags & VPOXVHWA_SD_PITCH)
    {
        pInfo->pitch = pSurf->AllocData.SurfDesc.pitch;
        pInfo->flags |= VPOXVHWA_SD_PITCH;
        pInfo->sizeX = pSurf->AllocData.SurfDesc.cbSize;
        pInfo->sizeY = 1;
    }

    if (cBackBuffers)
    {
        pInfo->cBackBuffers = cBackBuffers;
        pInfo->flags |= VPOXVHWA_SD_BACKBUFFERCOUNT;
    }
    else
        pInfo->cBackBuffers = 0;
    pInfo->Reserved = 0;
        /** @todo color keys */
//                        pInfo->DstOverlayCK;
//                        pInfo->DstBltCK;
//                        pInfo->SrcOverlayCK;
//                        pInfo->SrcBltCK;
    int rc = vpoxVhwaHlpTranslateFormat(&pInfo->PixelFormat, pSurf->AllocData.SurfDesc.format);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        pInfo->flags |= VPOXVHWA_SD_PIXELFORMAT;
        pInfo->surfCaps = fSCaps;
        pInfo->flags |= VPOXVHWA_SD_CAPS;
        pInfo->offSurface = pSurf->AllocData.Addr.offVram;
    }

    return rc;
}

int vpoxVhwaHlpCheckApplySurfInfo(PVPOXWDDM_ALLOCATION pSurf, VPOXVHWA_SURFACEDESC RT_UNTRUSTED_VOLATILE_HOST *pInfo,
                                  uint32_t fFlags, bool bApplyHostHandle)
{
    int rc = VINF_SUCCESS;
    if (!(fFlags & VPOXVHWA_SD_PITCH))
    {
        /* should be set by host */
//        Assert(pInfo->flags & VPOXVHWA_SD_PITCH);
        pSurf->AllocData.SurfDesc.cbSize = pInfo->sizeX * pInfo->sizeY;
        Assert(pSurf->AllocData.SurfDesc.cbSize);
        pSurf->AllocData.SurfDesc.pitch = pInfo->pitch;
        Assert(pSurf->AllocData.SurfDesc.pitch);
        /** @todo make this properly */
        pSurf->AllocData.SurfDesc.bpp = pSurf->AllocData.SurfDesc.pitch * 8 / pSurf->AllocData.SurfDesc.width;
        Assert(pSurf->AllocData.SurfDesc.bpp);
    }
    else
    {
        Assert(pSurf->AllocData.SurfDesc.cbSize ==  pInfo->sizeX);
        Assert(pInfo->sizeY == 1);
        Assert(pInfo->pitch == pSurf->AllocData.SurfDesc.pitch);
        if (pSurf->AllocData.SurfDesc.cbSize !=  pInfo->sizeX
                || pInfo->sizeY != 1
                || pInfo->pitch != pSurf->AllocData.SurfDesc.pitch)
        {
            rc = VERR_INVALID_PARAMETER;
        }
    }

    if (bApplyHostHandle && RT_SUCCESS(rc))
    {
        pSurf->hHostHandle = pInfo->hSurf;
    }

    return rc;
}

int vpoxVhwaHlpCreateSurface(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pSurf,
        uint32_t fFlags, uint32_t cBackBuffers, uint32_t fSCaps,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    /* the first thing we need is to post create primary */
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pDevExt, VidPnSourceId, VPOXVHWACMD_TYPE_SURF_CREATE,
                                                                         sizeof(VPOXVHWACMD_SURF_CREATE));
    Assert(pCmd);
    if (pCmd)
    {
        VPOXVHWACMD_SURF_CREATE RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_CREATE);
        int rc = VINF_SUCCESS;

        memset((void *)pBody, 0, sizeof(VPOXVHWACMD_SURF_CREATE));

        rc = vpoxVhwaHlpPopulateSurInfo(&pBody->SurfInfo, pSurf, fFlags, cBackBuffers, fSCaps, VidPnSourceId);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            vpoxVhwaCommandSubmit(pDevExt, pCmd);
            Assert(pCmd->rc == VINF_SUCCESS);
            if(pCmd->rc == VINF_SUCCESS)
                rc = vpoxVhwaHlpCheckApplySurfInfo(pSurf, &pBody->SurfInfo, fFlags, true);
            else
                rc = pCmd->rc;
        }
        vpoxVhwaCommandFree(pDevExt, pCmd);
        return rc;
    }

    return VERR_OUT_OF_RESOURCES;
}

int vpoxVhwaHlpGetSurfInfoForSource(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pSurf, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    /* the first thing we need is to post create primary */
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pDevExt, VidPnSourceId, VPOXVHWACMD_TYPE_SURF_GETINFO,
                                                                         sizeof(VPOXVHWACMD_SURF_GETINFO));
    Assert(pCmd);
    if (pCmd)
    {
        VPOXVHWACMD_SURF_GETINFO RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_GETINFO);
        int rc = VINF_SUCCESS;

        memset((void *)pBody, 0, sizeof(VPOXVHWACMD_SURF_GETINFO));

        rc = vpoxVhwaHlpPopulateSurInfo(&pBody->SurfInfo, pSurf, 0, 0,
                                          VPOXVHWA_SCAPS_OVERLAY | VPOXVHWA_SCAPS_VIDEOMEMORY
                                        | VPOXVHWA_SCAPS_LOCALVIDMEM | VPOXVHWA_SCAPS_COMPLEX,
                                        VidPnSourceId);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            vpoxVhwaCommandSubmit(pDevExt, pCmd);
            Assert(pCmd->rc == VINF_SUCCESS);
            if(pCmd->rc == VINF_SUCCESS)
                rc = vpoxVhwaHlpCheckApplySurfInfo(pSurf, &pBody->SurfInfo, 0, true);
            else
                rc = pCmd->rc;
        }
        vpoxVhwaCommandFree(pDevExt, pCmd);
        return rc;
    }

    return VERR_OUT_OF_RESOURCES;
}

int vpoxVhwaHlpGetSurfInfo(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pSurf)
{
    for (int i = 0; i < VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[i];
        if (pSource->Vhwa.Settings.fFlags & VPOXVHWA_F_ENABLED)
        {
            int rc = vpoxVhwaHlpGetSurfInfoForSource(pDevExt, pSurf, i);
            AssertRC(rc);
            return rc;
        }
    }
    AssertBreakpoint();
    return VERR_NOT_SUPPORTED;
}

int vpoxVhwaHlpDestroyPrimary(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_SOURCE pSource, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    PVPOXWDDM_ALLOCATION pFbSurf = VPOXVHWA_PRIMARY_ALLOCATION(pSource);

    int rc = vpoxVhwaHlpDestroySurface(pDevExt, pFbSurf, VidPnSourceId);
    AssertRC(rc);
    return rc;
}

int vpoxVhwaHlpCreatePrimary(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_SOURCE pSource, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    PVPOXWDDM_ALLOCATION pFbSurf = VPOXVHWA_PRIMARY_ALLOCATION(pSource);
    Assert(pSource->Vhwa.cOverlaysCreated == 1);
    Assert(pFbSurf->hHostHandle == VPOXVHWA_SURFHANDLE_INVALID);
    if (pFbSurf->hHostHandle != VPOXVHWA_SURFHANDLE_INVALID)
        return VERR_INVALID_STATE;

    int rc = vpoxVhwaHlpCreateSurface(pDevExt, pFbSurf,
            VPOXVHWA_SD_PITCH, 0, VPOXVHWA_SCAPS_PRIMARYSURFACE | VPOXVHWA_SCAPS_VIDEOMEMORY | VPOXVHWA_SCAPS_LOCALVIDMEM,
            VidPnSourceId);
    AssertRC(rc);
    return rc;
}

int vpoxVhwaHlpCheckInit(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    Assert(VidPnSourceId < (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);
    if (VidPnSourceId >= (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VPoxCommonFromDeviceExt(pDevExt)->cDisplays)
        return VERR_INVALID_PARAMETER;

    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];

    Assert(!!(pSource->Vhwa.Settings.fFlags & VPOXVHWA_F_ENABLED));
    if (!(pSource->Vhwa.Settings.fFlags & VPOXVHWA_F_ENABLED))
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;
    /** @todo need a better sync */
    uint32_t cNew = ASMAtomicIncU32(&pSource->Vhwa.cOverlaysCreated);
    if (cNew == 1)
    {
        rc = vpoxVhwaEnable(pDevExt, VidPnSourceId);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = vpoxVhwaHlpCreatePrimary(pDevExt, pSource, VidPnSourceId);
            AssertRC(rc);
            if (RT_FAILURE(rc))
            {
                int tmpRc = vpoxVhwaDisable(pDevExt, VidPnSourceId);
                AssertRC(tmpRc);
            }
        }
    }
    else
    {
        PVPOXWDDM_ALLOCATION pFbSurf = VPOXVHWA_PRIMARY_ALLOCATION(pSource);
        Assert(pFbSurf->hHostHandle);
        if (pFbSurf->hHostHandle)
            rc = VINF_ALREADY_INITIALIZED;
        else
            rc = VERR_INVALID_STATE;
    }

    if (RT_FAILURE(rc))
        ASMAtomicDecU32(&pSource->Vhwa.cOverlaysCreated);

    return rc;
}

int vpoxVhwaHlpCheckTerm(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    Assert(VidPnSourceId < (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VPoxCommonFromDeviceExt(pDevExt)->cDisplays);
    if (VidPnSourceId >= (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VPoxCommonFromDeviceExt(pDevExt)->cDisplays)
        return VERR_INVALID_PARAMETER;

    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];

    Assert(!!(pSource->Vhwa.Settings.fFlags & VPOXVHWA_F_ENABLED));

    /** @todo need a better sync */
    uint32_t cNew = ASMAtomicDecU32(&pSource->Vhwa.cOverlaysCreated);
    int rc = VINF_SUCCESS;
    if (!cNew)
    {
        rc = vpoxVhwaHlpDestroyPrimary(pDevExt, pSource, VidPnSourceId);
        AssertRC(rc);
    }
    else
    {
        Assert(cNew < UINT32_MAX / 2);
    }

    return rc;
}

int vpoxVhwaHlpOverlayFlip(PVPOXWDDM_OVERLAY pOverlay, const DXGKARG_FLIPOVERLAY *pFlipInfo)
{
    PVPOXWDDM_ALLOCATION pAlloc = (PVPOXWDDM_ALLOCATION)pFlipInfo->hSource;
    Assert(pAlloc->hHostHandle);
    Assert(pAlloc->pResource);
    Assert(pAlloc->pResource == pOverlay->pResource);
    Assert(pFlipInfo->PrivateDriverDataSize == sizeof (VPOXWDDM_OVERLAYFLIP_INFO));
    Assert(pFlipInfo->pPrivateDriverData);
    PVPOXWDDM_SOURCE pSource = &pOverlay->pDevExt->aSources[pOverlay->VidPnSourceId];
    Assert(!!(pSource->Vhwa.Settings.fFlags & VPOXVHWA_F_ENABLED));
    PVPOXWDDM_ALLOCATION pFbSurf = VPOXVHWA_PRIMARY_ALLOCATION(pSource);
    Assert(pFbSurf);
    Assert(pFbSurf->hHostHandle);
    Assert(pFbSurf->AllocData.Addr.offVram != VPOXVIDEOOFFSET_VOID);
    Assert(pOverlay->pCurentAlloc);
    Assert(pOverlay->pCurentAlloc->pResource == pOverlay->pResource);
    Assert(pOverlay->pCurentAlloc != pAlloc);
    int rc = VINF_SUCCESS;

    if (pFbSurf->AllocData.Addr.SegmentId != 1)
    {
        WARN(("invalid segment id on flip"));
        return VERR_INVALID_PARAMETER;
    }

    if (pFlipInfo->PrivateDriverDataSize == sizeof (VPOXWDDM_OVERLAYFLIP_INFO))
    {
        PVPOXWDDM_OVERLAYFLIP_INFO pOurInfo = (PVPOXWDDM_OVERLAYFLIP_INFO)pFlipInfo->pPrivateDriverData;

        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = vpoxVhwaCommandCreate(pOverlay->pDevExt, pOverlay->VidPnSourceId,
                                                                             VPOXVHWACMD_TYPE_SURF_FLIP,
                                                                             sizeof(VPOXVHWACMD_SURF_FLIP));
        Assert(pCmd);
        if (pCmd)
        {
            VPOXVHWACMD_SURF_FLIP RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_FLIP);

            memset((void *)pBody, 0, sizeof(VPOXVHWACMD_SURF_FLIP));

//            pBody->TargGuestSurfInfo;
//            pBody->CurrGuestSurfInfo;
            pBody->u.in.hTargSurf = pAlloc->hHostHandle;
            pBody->u.in.offTargSurface = pFlipInfo->SrcPhysicalAddress.QuadPart;
            pAlloc->AllocData.Addr.offVram = pFlipInfo->SrcPhysicalAddress.QuadPart;
            pBody->u.in.hCurrSurf = pOverlay->pCurentAlloc->hHostHandle;
            pBody->u.in.offCurrSurface = pOverlay->pCurentAlloc->AllocData.Addr.offVram;
            if (pOurInfo->DirtyRegion.fFlags & VPOXWDDM_DIRTYREGION_F_VALID)
            {
                pBody->u.in.xUpdatedTargMemValid = 1;
                if (pOurInfo->DirtyRegion.fFlags & VPOXWDDM_DIRTYREGION_F_RECT_VALID)
                    VPOXVHWA_COPY_RECT(&pBody->u.in.xUpdatedTargMemRect, &pOurInfo->DirtyRegion.Rect);
                else
                {
                    pBody->u.in.xUpdatedTargMemRect.right = pAlloc->AllocData.SurfDesc.width;
                    pBody->u.in.xUpdatedTargMemRect.bottom = pAlloc->AllocData.SurfDesc.height;
                    /* top & left are zero-inited with the above memset */
                }
            }

            /* we're not interested in completion, just send the command */
            vpoxVhwaCommandSubmitAsynchAndComplete(pOverlay->pDevExt, pCmd);

            pOverlay->pCurentAlloc = pAlloc;

            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_OUT_OF_RESOURCES;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}

AssertCompile(sizeof (RECT) == sizeof (VPOXVHWA_RECTL));
AssertCompile(RT_SIZEOFMEMB(RECT, left) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, left));
AssertCompile(RT_SIZEOFMEMB(RECT, right) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, right));
AssertCompile(RT_SIZEOFMEMB(RECT, top) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, top));
AssertCompile(RT_SIZEOFMEMB(RECT, bottom) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, bottom));
AssertCompile(RT_OFFSETOF(RECT, left) == RT_OFFSETOF(VPOXVHWA_RECTL, left));
AssertCompile(RT_OFFSETOF(RECT, right) == RT_OFFSETOF(VPOXVHWA_RECTL, right));
AssertCompile(RT_OFFSETOF(RECT, top) == RT_OFFSETOF(VPOXVHWA_RECTL, top));
AssertCompile(RT_OFFSETOF(RECT, bottom) == RT_OFFSETOF(VPOXVHWA_RECTL, bottom));

static void vpoxVhwaHlpOverlayDstRectSet(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_OVERLAY pOverlay, const RECT *pRect)
{
    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[pOverlay->VidPnSourceId];
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->OverlayListLock, &OldIrql);
    pOverlay->DstRect = *pRect;
    KeReleaseSpinLock(&pSource->OverlayListLock, OldIrql);
}

static void vpoxVhwaHlpOverlayListAdd(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_OVERLAY pOverlay)
{
    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[pOverlay->VidPnSourceId];
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->OverlayListLock, &OldIrql);
    ASMAtomicIncU32(&pSource->cOverlays);
    InsertHeadList(&pSource->OverlayList, &pOverlay->ListEntry);
    KeReleaseSpinLock(&pSource->OverlayListLock, OldIrql);
}

static void vpoxVhwaHlpOverlayListRemove(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_OVERLAY pOverlay)
{
    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[pOverlay->VidPnSourceId];
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->OverlayListLock, &OldIrql);
    ASMAtomicDecU32(&pSource->cOverlays);
    RemoveEntryList(&pOverlay->ListEntry);
    KeReleaseSpinLock(&pSource->OverlayListLock, OldIrql);
}

AssertCompile(sizeof (RECT) == sizeof (VPOXVHWA_RECTL));
AssertCompile(RT_SIZEOFMEMB(RECT, left) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, left));
AssertCompile(RT_SIZEOFMEMB(RECT, right) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, right));
AssertCompile(RT_SIZEOFMEMB(RECT, top) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, top));
AssertCompile(RT_SIZEOFMEMB(RECT, bottom) == RT_SIZEOFMEMB(VPOXVHWA_RECTL, bottom));
AssertCompile(RT_OFFSETOF(RECT, left) == RT_OFFSETOF(VPOXVHWA_RECTL, left));
AssertCompile(RT_OFFSETOF(RECT, right) == RT_OFFSETOF(VPOXVHWA_RECTL, right));
AssertCompile(RT_OFFSETOF(RECT, top) == RT_OFFSETOF(VPOXVHWA_RECTL, top));
AssertCompile(RT_OFFSETOF(RECT, bottom) == RT_OFFSETOF(VPOXVHWA_RECTL, bottom));

int vpoxVhwaHlpOverlayUpdate(PVPOXWDDM_OVERLAY pOverlay, const DXGK_OVERLAYINFO *pOverlayInfo, RECT * pDstUpdateRect)
{
    PVPOXWDDM_ALLOCATION pAlloc = (PVPOXWDDM_ALLOCATION)pOverlayInfo->hAllocation;
    Assert(pAlloc->hHostHandle);
    Assert(pAlloc->pResource);
    Assert(pAlloc->pResource == pOverlay->pResource);
    Assert(pOverlayInfo->PrivateDriverDataSize == sizeof (VPOXWDDM_OVERLAY_INFO));
    Assert(pOverlayInfo->pPrivateDriverData);
    PVPOXWDDM_SOURCE pSource = &pOverlay->pDevExt->aSources[pOverlay->VidPnSourceId];
    Assert(!!(pSource->Vhwa.Settings.fFlags & VPOXVHWA_F_ENABLED));
    PVPOXWDDM_ALLOCATION pFbSurf = VPOXVHWA_PRIMARY_ALLOCATION(pSource);
    Assert(pFbSurf);
    Assert(pFbSurf->hHostHandle);
    Assert(pFbSurf->AllocData.Addr.offVram != VPOXVIDEOOFFSET_VOID);
    int rc = VINF_SUCCESS;

    if (pFbSurf->AllocData.Addr.SegmentId != 1)
    {
        WARN(("invalid segment id on overlay update"));
        return VERR_INVALID_PARAMETER;
    }

    if (pOverlayInfo->PrivateDriverDataSize == sizeof (VPOXWDDM_OVERLAY_INFO))
    {
        PVPOXWDDM_OVERLAY_INFO pOurInfo = (PVPOXWDDM_OVERLAY_INFO)pOverlayInfo->pPrivateDriverData;

        VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST * pCmd = vpoxVhwaCommandCreate(pOverlay->pDevExt, pOverlay->VidPnSourceId,
                                                                              VPOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE,
                                                                              sizeof(VPOXVHWACMD_SURF_OVERLAY_UPDATE));
        Assert(pCmd);
        if (pCmd)
        {
            VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_HOST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_OVERLAY_UPDATE);

            memset((void *)pBody, 0, sizeof(VPOXVHWACMD_SURF_OVERLAY_UPDATE));

            pBody->u.in.hDstSurf = pFbSurf->hHostHandle;
            pBody->u.in.offDstSurface = pFbSurf->AllocData.Addr.offVram;
            VPOXVHWA_COPY_RECT(&pBody->u.in.dstRect, &pOverlayInfo->DstRect);

            pBody->u.in.hSrcSurf = pAlloc->hHostHandle;
            pBody->u.in.offSrcSurface = pOverlayInfo->PhysicalAddress.QuadPart;
            pAlloc->AllocData.Addr.offVram = pOverlayInfo->PhysicalAddress.QuadPart;
            VPOXVHWA_COPY_RECT(&pBody->u.in.srcRect, &pOverlayInfo->SrcRect);

            pBody->u.in.flags |= VPOXVHWA_OVER_SHOW;
            if (pOurInfo->OverlayDesc.fFlags & VPOXWDDM_OVERLAY_F_CKEY_DST)
            {
                pBody->u.in.flags |= VPOXVHWA_OVER_KEYDESTOVERRIDE /* ?? VPOXVHWA_OVER_KEYDEST */;
                pBody->u.in.desc.DstCK.high = pOurInfo->OverlayDesc.DstColorKeyHigh;
                pBody->u.in.desc.DstCK.low = pOurInfo->OverlayDesc.DstColorKeyLow;
            }

            if (pOurInfo->OverlayDesc.fFlags & VPOXWDDM_OVERLAY_F_CKEY_SRC)
            {
                pBody->u.in.flags |= VPOXVHWA_OVER_KEYSRCOVERRIDE /* ?? VPOXVHWA_OVER_KEYSRC */;
                pBody->u.in.desc.SrcCK.high = pOurInfo->OverlayDesc.SrcColorKeyHigh;
                pBody->u.in.desc.SrcCK.low = pOurInfo->OverlayDesc.SrcColorKeyLow;
            }

            if (pOurInfo->DirtyRegion.fFlags & VPOXWDDM_DIRTYREGION_F_VALID)
            {
                pBody->u.in.xFlags |= VPOXVHWACMD_SURF_OVERLAY_UPDATE_F_SRCMEMRECT;
                if (pOurInfo->DirtyRegion.fFlags & VPOXWDDM_DIRTYREGION_F_RECT_VALID)
                    VPOXVHWA_COPY_RECT(&pBody->u.in.xUpdatedSrcMemRect, &pOurInfo->DirtyRegion.Rect);
                else
                {
                    pBody->u.in.xUpdatedSrcMemRect.right = pAlloc->AllocData.SurfDesc.width;
                    pBody->u.in.xUpdatedSrcMemRect.bottom = pAlloc->AllocData.SurfDesc.height;
                    /* top & left are zero-inited with the above memset */
                }
            }

            if (pDstUpdateRect)
            {
                pBody->u.in.xFlags |= VPOXVHWACMD_SURF_OVERLAY_UPDATE_F_DSTMEMRECT;
                VPOXVHWA_COPY_RECT(&pBody->u.in.xUpdatedDstMemRect, pDstUpdateRect);
            }

            /* we're not interested in completion, just send the command */
            vpoxVhwaCommandSubmitAsynchAndComplete(pOverlay->pDevExt, pCmd);

            pOverlay->pCurentAlloc = pAlloc;

            vpoxVhwaHlpOverlayDstRectSet(pOverlay->pDevExt, pOverlay, &pOverlayInfo->DstRect);

            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_OUT_OF_RESOURCES;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}

int vpoxVhwaHlpOverlayUpdate(PVPOXWDDM_OVERLAY pOverlay, const DXGK_OVERLAYINFO *pOverlayInfo)
{
    return vpoxVhwaHlpOverlayUpdate(pOverlay, pOverlayInfo, NULL);
}

int vpoxVhwaHlpOverlayDestroy(PVPOXWDDM_OVERLAY pOverlay)
{
    int rc = VINF_SUCCESS;

    vpoxVhwaHlpOverlayListRemove(pOverlay->pDevExt, pOverlay);

    for (uint32_t i = 0; i < pOverlay->pResource->cAllocations; ++i)
    {
        PVPOXWDDM_ALLOCATION pCurAlloc = &pOverlay->pResource->aAllocations[i];
        rc = vpoxVhwaHlpDestroySurface(pOverlay->pDevExt, pCurAlloc, pOverlay->VidPnSourceId);
        AssertRC(rc);
    }

    if (RT_SUCCESS(rc))
    {
        int tmpRc = vpoxVhwaHlpCheckTerm(pOverlay->pDevExt, pOverlay->VidPnSourceId);
        AssertRC(tmpRc);
    }

    return rc;
}


int vpoxVhwaHlpOverlayCreate(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, DXGK_OVERLAYINFO *pOverlayInfo,
        /* OUT */ PVPOXWDDM_OVERLAY pOverlay)
{
    int rc = vpoxVhwaHlpCheckInit(pDevExt, VidPnSourceId);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        PVPOXWDDM_ALLOCATION pAlloc = (PVPOXWDDM_ALLOCATION)pOverlayInfo->hAllocation;
        PVPOXWDDM_RESOURCE pRc = pAlloc->pResource;
        Assert(pRc);
        for (uint32_t i = 0; i < pRc->cAllocations; ++i)
        {
            PVPOXWDDM_ALLOCATION pCurAlloc = &pRc->aAllocations[i];
            rc = vpoxVhwaHlpCreateSurface(pDevExt, pCurAlloc,
                        0, pRc->cAllocations - 1, VPOXVHWA_SCAPS_OVERLAY | VPOXVHWA_SCAPS_VIDEOMEMORY | VPOXVHWA_SCAPS_LOCALVIDMEM | VPOXVHWA_SCAPS_COMPLEX,
                        VidPnSourceId);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
            {
                int tmpRc;
                for (uint32_t j = 0; j < i; ++j)
                {
                    PVPOXWDDM_ALLOCATION pDestroyAlloc = &pRc->aAllocations[j];
                    tmpRc = vpoxVhwaHlpDestroySurface(pDevExt, pDestroyAlloc, VidPnSourceId);
                    AssertRC(tmpRc);
                }
                break;
            }
        }

        if (RT_SUCCESS(rc))
        {
            pOverlay->pDevExt = pDevExt;
            pOverlay->pResource = pRc;
            pOverlay->VidPnSourceId = VidPnSourceId;

            vpoxVhwaHlpOverlayListAdd(pDevExt, pOverlay);

            RECT DstRect;
            vpoxVhwaHlpOverlayDstRectGet(pDevExt, pOverlay, &DstRect);

            rc = vpoxVhwaHlpOverlayUpdate(pOverlay, pOverlayInfo, DstRect.right ? &DstRect : NULL);
            if (!RT_SUCCESS(rc))
            {
                int tmpRc = vpoxVhwaHlpOverlayDestroy(pOverlay);
                AssertRC(tmpRc);
            }
        }

        if (RT_FAILURE(rc))
        {
            int tmpRc = vpoxVhwaHlpCheckTerm(pDevExt, VidPnSourceId);
            AssertRC(tmpRc);
            AssertRC(rc);
        }
    }

    return rc;
}

BOOLEAN vpoxVhwaHlpOverlayListIsEmpty(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];
    return !ASMAtomicReadU32(&pSource->cOverlays);
}

#define VPOXWDDM_OVERLAY_FROM_ENTRY(_pEntry) ((PVPOXWDDM_OVERLAY)(((uint8_t*)(_pEntry)) - RT_UOFFSETOF(VPOXWDDM_OVERLAY, ListEntry)))

void vpoxVhwaHlpOverlayDstRectUnion(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RECT *pRect)
{
    if (vpoxVhwaHlpOverlayListIsEmpty(pDevExt, VidPnSourceId))
    {
        memset(pRect, 0, sizeof (*pRect));
        return;
    }

    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->OverlayListLock, &OldIrql);
    if (pSource->cOverlays)
    {
        PVPOXWDDM_OVERLAY pOverlay = VPOXWDDM_OVERLAY_FROM_ENTRY(pSource->OverlayList.Flink);
        *pRect = pOverlay->DstRect;
        while (pOverlay->ListEntry.Flink != &pSource->OverlayList)
        {
            pOverlay = VPOXWDDM_OVERLAY_FROM_ENTRY(pOverlay->ListEntry.Flink);
            vpoxWddmRectUnite(pRect, &pOverlay->DstRect);
        }
    }
    KeReleaseSpinLock(&pSource->OverlayListLock, OldIrql);
}

void vpoxVhwaHlpOverlayDstRectGet(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_OVERLAY pOverlay, RECT *pRect)
{
    PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[pOverlay->VidPnSourceId];
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->OverlayListLock, &OldIrql);
    *pRect = pOverlay->DstRect;
    KeReleaseSpinLock(&pSource->OverlayListLock, OldIrql);
}
