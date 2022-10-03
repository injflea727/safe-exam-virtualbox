/* $Id: VPoxDispVHWA.cpp $ */
/** @file
 * VPox XPDM Display driver
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
#include "VPoxDispMini.h"
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>

static void VPoxDispVHWACommandFree(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    VPoxHGSMIBufferFree(&pDev->hgsmi.ctx, pCmd);
}

static void VPoxDispVHWACommandRetain(VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

static void VPoxDispVHWACommandSubmitAsynchByEvent(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd,
                                                   VPOXPEVENT pEvent)
{
    pCmd->GuestVBVAReserved1 = (uintptr_t)pEvent;
    pCmd->GuestVBVAReserved2 = 0;
    /* ensure the command is not removed until we're processing it */
    VPoxDispVHWACommandRetain(pCmd);

    /* complete it asynchronously by setting event */
    pCmd->Flags |= VPOXVHWACMD_FLAG_GH_ASYNCH_EVENT;
    VPoxHGSMIBufferSubmit(&pDev->hgsmi.ctx, pCmd);

    if(!(ASMAtomicReadU32((volatile uint32_t *)&pCmd->Flags)  & VPOXVHWACMD_FLAG_HG_ASYNCH))
    {
        /* the command is completed */
        pDev->vpAPI.VideoPortProcs.pfnSetEvent(pDev->vpAPI.pContext, pEvent);
    }

    VPoxDispVHWACommandRelease(pDev, pCmd);
}

static void VPoxDispVHWAHanldeVHWACmdCompletion(PVPOXDISPDEV pDev, VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pHostCmd)
{
    VBVAHOSTCMDVHWACMDCOMPLETE RT_UNTRUSTED_VOLATILE_HOST *pComplete = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDVHWACMDCOMPLETE);
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST                *pComplCmd =
        (VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *)HGSMIOffsetToPointer(&pDev->hgsmi.ctx.heapCtx.area, pComplete->offCmd);

    PFNVPOXVHWACMDCOMPLETION pfnCompletion = (PFNVPOXVHWACMDCOMPLETION)(uintptr_t)pComplCmd->GuestVBVAReserved1;
    void                    *pContext      = (void *)(uintptr_t)pComplCmd->GuestVBVAReserved2;

    pfnCompletion(pDev, pComplCmd, pContext);

    VPoxDispVBVAHostCommandComplete(pDev, pHostCmd);
}

static void VPoxVHWAHostCommandHandler(PVPOXDISPDEV pDev, VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    switch (pCmd->customOpCode)
    {
        case VBVAHG_DCUSTOM_VHWA_CMDCOMPLETE:
            VPoxDispVHWAHanldeVHWACmdCompletion(pDev, pCmd);
            break;

        default:
            VPoxDispVBVAHostCommandComplete(pDev, pCmd);
    }
}

void VPoxDispVHWAInit(PVPOXDISPDEV pDev)
{
    VHWAQUERYINFO info;
    int rc;

    rc = VPoxDispMPVHWAQueryInfo(pDev->hDriver, &info);
    VPOX_WARNRC(rc);

    if (RT_SUCCESS(rc))
    {
        pDev->vhwa.offVramBase = info.offVramBase;
    }
}

int VPoxDispVHWAEnable(PVPOXDISPDEV pDev)
{
    int rc = VERR_GENERAL_FAILURE;

    if (!pDev->hgsmi.bSupported)
    {
        return VERR_NOT_SUPPORTED;
    }

    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_ENABLE, 0);
    if (!pCmd)
    {
        WARN(("VPoxDispVHWACommandCreate failed"));
        return rc;
    }

    if (VPoxDispVHWACommandSubmit(pDev, pCmd))
        if (RT_SUCCESS(pCmd->rc))
            rc = VINF_SUCCESS;

    VPoxDispVHWACommandRelease(pDev, pCmd);
    return rc;
}

VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *
VPoxDispVHWACommandCreate(PVPOXDISPDEV pDev, VPOXVHWACMD_TYPE enmCmd, VPOXVHWACMD_LENGTH cbCmd)
{
    uint32_t                                cbTotal = cbCmd + VPOXVHWACMD_HEADSIZE();
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pHdr
        = (VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *)VPoxHGSMIBufferAlloc(&pDev->hgsmi.ctx, cbTotal, HGSMI_CH_VBVA, VBVA_VHWA_CMD);
    if (!pHdr)
    {
        WARN(("HGSMIHeapAlloc failed"));
    }
    else
    {
        memset((void *)pHdr, 0, cbTotal); /* always clear the whole body so caller doesn't need to */
        pHdr->iDisplay = pDev->iDevice;
        pHdr->rc = VERR_GENERAL_FAILURE;
        pHdr->enmCmd = enmCmd;
        pHdr->cRefs = 1;
    }

    /** @todo temporary hack */
    VPoxDispVHWACommandCheckHostCmds(pDev);

    return pHdr;
}

void VPoxDispVHWACommandRelease(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if (!cRefs)
        VPoxDispVHWACommandFree(pDev, pCmd);
}

BOOL VPoxDispVHWACommandSubmit(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    VPOXPEVENT pEvent;
    VPOXVP_STATUS rc = pDev->vpAPI.VideoPortProcs.pfnCreateEvent(pDev->vpAPI.pContext, VPOXNOTIFICATION_EVENT, NULL, &pEvent);
    /* don't assert here, otherwise NT4 will be unhappy */
    if(rc == VPOXNO_ERROR)
    {
        pCmd->Flags |= VPOXVHWACMD_FLAG_GH_ASYNCH_IRQ;
        VPoxDispVHWACommandSubmitAsynchByEvent(pDev, pCmd, pEvent);

        rc = pDev->vpAPI.VideoPortProcs.pfnWaitForSingleObject(pDev->vpAPI.pContext, pEvent,
                NULL /*IN PLARGE_INTEGER  pTimeOut*/
                );
        Assert(rc == VPOXNO_ERROR);
        if(rc == VPOXNO_ERROR)
        {
            pDev->vpAPI.VideoPortProcs.pfnDeleteEvent(pDev->vpAPI.pContext, pEvent);
        }
    }
    return rc == VPOXNO_ERROR;
}

void VPoxDispVHWACommandCheckHostCmds(PVPOXDISPDEV pDev)
{
    VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pCmd;
    int rc = pDev->hgsmi.mp.pfnRequestCommandsHandler(pDev->hgsmi.mp.hContext, HGSMI_CH_VBVA, pDev->iDevice, &pCmd);
    /* don't assert here, otherwise NT4 will be unhappy */
    if (RT_SUCCESS(rc))
    {
        while (pCmd)
        {
            VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pNextCmd = pCmd->u.pNext;
            VPoxVHWAHostCommandHandler(pDev, pCmd);
            pCmd = pNextCmd;
        }
    }
}

static DECLCALLBACK(void) VPoxDispVHWACommandCompletionCallbackEvent(PVPOXDISPDEV pDev, VPOXVHWACMD * pCmd, void * pContext)
{
    RT_NOREF(pCmd);
    VPOXPEVENT pEvent = (VPOXPEVENT)pContext;
    LONG oldState = pDev->vpAPI.VideoPortProcs.pfnSetEvent(pDev->vpAPI.pContext, pEvent);
    Assert(!oldState); NOREF(oldState);
}

/* do not wait for completion */
void VPoxDispVHWACommandSubmitAsynch(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd,
                                     PFNVPOXVHWACMDCOMPLETION pfnCompletion, void * pContext)
{
    pCmd->GuestVBVAReserved1 = (uintptr_t)pfnCompletion;
    pCmd->GuestVBVAReserved2 = (uintptr_t)pContext;
    VPoxDispVHWACommandRetain(pCmd);

    VPoxHGSMIBufferSubmit(&pDev->hgsmi.ctx, pCmd);

    if(!(pCmd->Flags & VPOXVHWACMD_FLAG_HG_ASYNCH))
    {
        /* the command is completed */
        pfnCompletion(pDev, pCmd, pContext);
    }

    VPoxDispVHWACommandRelease(pDev, pCmd);
}

static DECLCALLBACK(void) VPoxDispVHWAFreeCmdCompletion(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd,
                                                        void *pvContext)
{
    RT_NOREF(pvContext);
    VPoxDispVHWACommandRelease(pDev, pCmd);
}

void VPoxDispVHWACommandSubmitAsynchAndComplete (PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    pCmd->GuestVBVAReserved1 = (uintptr_t)VPoxDispVHWAFreeCmdCompletion;

    VPoxDispVHWACommandRetain(pCmd);

    pCmd->Flags |= VPOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION;

    VPoxHGSMIBufferSubmit(&pDev->hgsmi.ctx, pCmd);

    uint32_t const fCmdFlags = pCmd->Flags;
    if (   !(fCmdFlags & VPOXVHWACMD_FLAG_HG_ASYNCH)
        || (fCmdFlags & VPOXVHWACMD_FLAG_HG_ASYNCH_RETURNED))
    {
        /* the command is completed */
        VPoxDispVHWAFreeCmdCompletion(pDev, pCmd, NULL);
    }

    VPoxDispVHWACommandRelease(pDev, pCmd);
}

void VPoxDispVHWAFreeHostInfo1(PVPOXDISPDEV pDev, VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *pInfo)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPOXVHWACMD_HEAD(pInfo);
    VPoxDispVHWACommandRelease(pDev, pCmd);
}

void VPoxDispVHWAFreeHostInfo2(PVPOXDISPDEV pDev, VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *pInfo)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPOXVHWACMD_HEAD(pInfo);
    VPoxDispVHWACommandRelease(pDev, pCmd);
}

static VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *VPoxDispVHWAQueryHostInfo1(PVPOXDISPDEV pDev)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_QUERY_INFO1,
                                                                             sizeof(VPOXVHWACMD_QUERYINFO1));
    if (!pCmd)
    {
        WARN(("VPoxDispVHWACommandCreate failed"));
        return NULL;
    }

    VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *pInfo1= VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO1);
    pInfo1->u.in.guestVersion.maj = VPOXVHWA_VERSION_MAJ;
    pInfo1->u.in.guestVersion.min = VPOXVHWA_VERSION_MIN;
    pInfo1->u.in.guestVersion.bld = VPOXVHWA_VERSION_BLD;
    pInfo1->u.in.guestVersion.reserved = VPOXVHWA_VERSION_RSV;

    if(VPoxDispVHWACommandSubmit (pDev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            return VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO1);
        }
    }

    VPoxDispVHWACommandRelease(pDev, pCmd);
    return NULL;
}

static VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *VPoxDispVHWAQueryHostInfo2(PVPOXDISPDEV pDev, uint32_t numFourCC)
{
    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_QUERY_INFO2,
                                                                             VPOXVHWAINFO2_SIZE(numFourCC));
    if (!pCmd)
    {
        WARN(("VPoxDispVHWACommandCreate failed"));
        return NULL;
    }

    VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *pInfo2 = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO2);
    pInfo2->numFourCC = numFourCC;
    if (VPoxDispVHWACommandSubmit(pDev, pCmd))
        if (RT_SUCCESS(pCmd->rc))
            if (pInfo2->numFourCC == numFourCC)
                return pInfo2;

    VPoxDispVHWACommandRelease(pDev, pCmd);
    return NULL;
}

int VPoxDispVHWAInitHostInfo1(PVPOXDISPDEV pDev)
{

    if (!pDev->hgsmi.bSupported)
        return VERR_NOT_SUPPORTED;

    VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_HOST *pInfo = VPoxDispVHWAQueryHostInfo1(pDev);
    if(!pInfo)
    {
        pDev->vhwa.bEnabled = false;
        return VERR_OUT_OF_RESOURCES;
    }

    pDev->vhwa.caps = pInfo->u.out.caps;
    pDev->vhwa.caps2 = pInfo->u.out.caps2;
    pDev->vhwa.colorKeyCaps = pInfo->u.out.colorKeyCaps;
    pDev->vhwa.stretchCaps = pInfo->u.out.stretchCaps;
    pDev->vhwa.surfaceCaps = pInfo->u.out.surfaceCaps;
    pDev->vhwa.numOverlays = pInfo->u.out.numOverlays;
    pDev->vhwa.numFourCC = pInfo->u.out.numFourCC;
    pDev->vhwa.bEnabled = (pInfo->u.out.cfgFlags & VPOXVHWA_CFG_ENABLED);

    VPoxDispVHWAFreeHostInfo1(pDev, pInfo);
    return VINF_SUCCESS;
}

int VPoxDispVHWAInitHostInfo2(PVPOXDISPDEV pDev, DWORD *pFourCC)
{
    int rc = VINF_SUCCESS;

    if (!pDev->hgsmi.bSupported)
        return VERR_NOT_SUPPORTED;

    VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_HOST *pInfo = VPoxDispVHWAQueryHostInfo2(pDev, pDev->vhwa.numFourCC);
    Assert(pInfo);
    if(!pInfo)
        return VERR_OUT_OF_RESOURCES;

    if (pDev->vhwa.numFourCC)
        memcpy(pFourCC, (void const *)pInfo->FourCC, pDev->vhwa.numFourCC * sizeof(pFourCC[0]));
    else
    {
        Assert(0);
        rc = VERR_GENERAL_FAILURE;
    }

    VPoxDispVHWAFreeHostInfo2(pDev, pInfo);

    return rc;
}

int VPoxDispVHWADisable(PVPOXDISPDEV pDev)
{
    int rc = VERR_GENERAL_FAILURE;

    if (!pDev->hgsmi.bSupported)
        return VERR_NOT_SUPPORTED;

    VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd = VPoxDispVHWACommandCreate(pDev, VPOXVHWACMD_TYPE_DISABLE, 0);
    if (!pCmd)
    {
        WARN(("VPoxDispVHWACommandCreate failed"));
        return rc;
    }

    if (VPoxDispVHWACommandSubmit(pDev, pCmd))
        if(RT_SUCCESS(pCmd->rc))
            rc = VINF_SUCCESS;

    VPoxDispVHWACommandRelease(pDev, pCmd);

    VPoxDispVHWACommandCheckHostCmds(pDev);

    return rc;
}

#define MEMTAG 'AWHV'
PVPOXVHWASURFDESC VPoxDispVHWASurfDescAlloc()
{
    return (PVPOXVHWASURFDESC) EngAllocMem(FL_NONPAGED_MEMORY | FL_ZERO_MEMORY, sizeof(VPOXVHWASURFDESC), MEMTAG);
}

void VPoxDispVHWASurfDescFree(PVPOXVHWASURFDESC pDesc)
{
    EngFreeMem(pDesc);
}

uint64_t VPoxDispVHWAVramOffsetFromPDEV(PVPOXDISPDEV pDev, ULONG_PTR offPdev)
{
    return (uint64_t)(pDev->vhwa.offVramBase + offPdev);
}

#define VPOX_DD(_f) DD##_f
#define VPOX_VHWA(_f) VPOXVHWA_##_f
#define VPOX_DD2VHWA(_out, _in, _f) do {if((_in) & VPOX_DD(_f)) _out |= VPOX_VHWA(_f); }while(0)
#define VPOX_DD_VHWA_PAIR(_v) {VPOX_DD(_v), VPOX_VHWA(_v)}
#define VPOX_DD_DUMMY_PAIR(_v) {VPOX_DD(_v), 0}

#define VPOXVHWA_SUPPORTED_CAPS ( \
        VPOXVHWA_CAPS_BLT \
        | VPOXVHWA_CAPS_BLTCOLORFILL \
        | VPOXVHWA_CAPS_BLTFOURCC \
        | VPOXVHWA_CAPS_BLTSTRETCH \
        | VPOXVHWA_CAPS_BLTQUEUE \
        | VPOXVHWA_CAPS_OVERLAY \
        | VPOXVHWA_CAPS_OVERLAYFOURCC \
        | VPOXVHWA_CAPS_OVERLAYSTRETCH \
        | VPOXVHWA_CAPS_OVERLAYCANTCLIP \
        | VPOXVHWA_CAPS_COLORKEY \
        | VPOXVHWA_CAPS_COLORKEYHWASSIST \
        )

#define VPOXVHWA_SUPPORTED_SCAPS ( \
        VPOXVHWA_SCAPS_BACKBUFFER \
        | VPOXVHWA_SCAPS_COMPLEX \
        | VPOXVHWA_SCAPS_FLIP \
        | VPOXVHWA_SCAPS_FRONTBUFFER \
        | VPOXVHWA_SCAPS_OFFSCREENPLAIN \
        | VPOXVHWA_SCAPS_OVERLAY \
        | VPOXVHWA_SCAPS_PRIMARYSURFACE \
        | VPOXVHWA_SCAPS_SYSTEMMEMORY \
        | VPOXVHWA_SCAPS_VIDEOMEMORY \
        | VPOXVHWA_SCAPS_VISIBLE \
        | VPOXVHWA_SCAPS_LOCALVIDMEM \
        )

#define VPOXVHWA_SUPPORTED_SCAPS2 ( \
        VPOXVHWA_CAPS2_CANRENDERWINDOWED \
        | VPOXVHWA_CAPS2_WIDESURFACES \
        | VPOXVHWA_CAPS2_COPYFOURCC \
        )

#define VPOXVHWA_SUPPORTED_PF ( \
        VPOXVHWA_PF_PALETTEINDEXED8 \
        | VPOXVHWA_PF_RGB \
        | VPOXVHWA_PF_RGBTOYUV \
        | VPOXVHWA_PF_YUV \
        | VPOXVHWA_PF_FOURCC \
        )

#define VPOXVHWA_SUPPORTED_SD ( \
        VPOXVHWA_SD_BACKBUFFERCOUNT \
        | VPOXVHWA_SD_CAPS \
        | VPOXVHWA_SD_CKDESTBLT \
        | VPOXVHWA_SD_CKDESTOVERLAY \
        | VPOXVHWA_SD_CKSRCBLT \
        | VPOXVHWA_SD_CKSRCOVERLAY \
        | VPOXVHWA_SD_HEIGHT \
        | VPOXVHWA_SD_PITCH \
        | VPOXVHWA_SD_PIXELFORMAT \
        | VPOXVHWA_SD_WIDTH \
        )

#define VPOXVHWA_SUPPORTED_CKEYCAPS ( \
        VPOXVHWA_CKEYCAPS_DESTBLT \
        | VPOXVHWA_CKEYCAPS_DESTBLTCLRSPACE \
        | VPOXVHWA_CKEYCAPS_DESTBLTCLRSPACEYUV \
        | VPOXVHWA_CKEYCAPS_DESTBLTYUV \
        | VPOXVHWA_CKEYCAPS_DESTOVERLAY \
        | VPOXVHWA_CKEYCAPS_DESTOVERLAYCLRSPACE \
        | VPOXVHWA_CKEYCAPS_DESTOVERLAYCLRSPACEYUV \
        | VPOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE \
        | VPOXVHWA_CKEYCAPS_DESTOVERLAYYUV \
        | VPOXVHWA_CKEYCAPS_SRCBLT \
        | VPOXVHWA_CKEYCAPS_SRCBLTCLRSPACE \
        | VPOXVHWA_CKEYCAPS_SRCBLTCLRSPACEYUV \
        | VPOXVHWA_CKEYCAPS_SRCBLTYUV \
        | VPOXVHWA_CKEYCAPS_SRCOVERLAY \
        | VPOXVHWA_CKEYCAPS_SRCOVERLAYCLRSPACE \
        | VPOXVHWA_CKEYCAPS_SRCOVERLAYCLRSPACEYUV \
        | VPOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE \
        | VPOXVHWA_CKEYCAPS_SRCOVERLAYYUV \
        | VPOXVHWA_CKEYCAPS_NOCOSTOVERLAY \
        )

#define VPOXVHWA_SUPPORTED_CKEY ( \
        VPOXVHWA_CKEY_COLORSPACE \
        | VPOXVHWA_CKEY_DESTBLT \
        | VPOXVHWA_CKEY_DESTOVERLAY \
        | VPOXVHWA_CKEY_SRCBLT \
        | VPOXVHWA_CKEY_SRCOVERLAY \
        )

#define VPOXVHWA_SUPPORTED_OVER ( \
        VPOXVHWA_OVER_DDFX \
        | VPOXVHWA_OVER_HIDE \
        | VPOXVHWA_OVER_KEYDEST \
        | VPOXVHWA_OVER_KEYDESTOVERRIDE \
        | VPOXVHWA_OVER_KEYSRC \
        | VPOXVHWA_OVER_KEYSRCOVERRIDE \
        | VPOXVHWA_OVER_SHOW \
        )

uint32_t VPoxDispVHWAUnsupportedDDCAPS(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_CAPS);
}

uint32_t VPoxDispVHWAUnsupportedDDSCAPS(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_SCAPS);
}

uint32_t VPoxDispVHWAUnsupportedDDPFS(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_PF);
}

uint32_t VPoxDispVHWAUnsupportedDSS(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_SD);
}

uint32_t VPoxDispVHWAUnsupportedDDCEYCAPS(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_CKEYCAPS);
}

uint32_t VPoxDispVHWASupportedDDCEYCAPS(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_CKEYCAPS);
}


uint32_t VPoxDispVHWASupportedDDCAPS(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_CAPS);
}

uint32_t VPoxDispVHWASupportedDDSCAPS(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_SCAPS);
}

uint32_t VPoxDispVHWASupportedDDPFS(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_PF);
}

uint32_t VPoxDispVHWASupportedDSS(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_SD);
}

uint32_t VPoxDispVHWASupportedOVERs(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_OVER);
}

uint32_t VPoxDispVHWAUnsupportedOVERs(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_OVER);
}

uint32_t VPoxDispVHWASupportedCKEYs(uint32_t caps)
{
    return caps & (VPOXVHWA_SUPPORTED_CKEY);
}

uint32_t VPoxDispVHWAUnsupportedCKEYs(uint32_t caps)
{
    return caps & (~VPOXVHWA_SUPPORTED_CKEY);
}

uint32_t VPoxDispVHWAFromDDOVERs(uint32_t caps) { return caps; }
uint32_t VPoxDispVHWAToDDOVERs(uint32_t caps)   { return caps; }
uint32_t VPoxDispVHWAFromDDCKEYs(uint32_t caps) { return caps; }
uint32_t VPoxDispVHWAToDDCKEYs(uint32_t caps)   { return caps; }

uint32_t VPoxDispVHWAFromDDCAPS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAToDDCAPS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAFromDDCAPS2(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAToDDCAPS2(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAFromDDSCAPS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAToDDSCAPS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAFromDDPFS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAToDDPFS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAFromDDCKEYCAPS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAToDDCKEYCAPS(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAToDDBLTs(uint32_t caps)
{
    return caps;
}

uint32_t VPoxDispVHWAFromDDBLTs(uint32_t caps)
{
    return caps;
}

void VPoxDispVHWAFromDDCOLORKEY(VPOXVHWA_COLORKEY RT_UNTRUSTED_VOLATILE_HOST *pVHWACKey, DDCOLORKEY  *pDdCKey)
{
    pVHWACKey->low = pDdCKey->dwColorSpaceLowValue;
    pVHWACKey->high = pDdCKey->dwColorSpaceHighValue;
}

void VPoxDispVHWAFromDDOVERLAYFX(VPOXVHWA_OVERLAYFX RT_UNTRUSTED_VOLATILE_HOST *pVHWAOverlay, DDOVERLAYFX *pDdOverlay)
{
    /// @todo fxFlags
    VPoxDispVHWAFromDDCOLORKEY(&pVHWAOverlay->DstCK, &pDdOverlay->dckDestColorkey);
    VPoxDispVHWAFromDDCOLORKEY(&pVHWAOverlay->SrcCK, &pDdOverlay->dckSrcColorkey);
}

void VPoxDispVHWAFromDDBLTFX(VPOXVHWA_BLTFX RT_UNTRUSTED_VOLATILE_HOST *pVHWABlt, DDBLTFX *pDdBlt)
{
    pVHWABlt->fillColor = pDdBlt->dwFillColor;

    VPoxDispVHWAFromDDCOLORKEY(&pVHWABlt->DstCK, &pDdBlt->ddckDestColorkey);
    VPoxDispVHWAFromDDCOLORKEY(&pVHWABlt->SrcCK, &pDdBlt->ddckSrcColorkey);
}

int VPoxDispVHWAFromDDPIXELFORMAT(VPOXVHWA_PIXELFORMAT RT_UNTRUSTED_VOLATILE_HOST *pVHWAFormat, DDPIXELFORMAT *pDdFormat)
{
    uint32_t unsup = VPoxDispVHWAUnsupportedDDPFS(pDdFormat->dwFlags);
    Assert(!unsup);
    if(unsup)
        return VERR_GENERAL_FAILURE;

    pVHWAFormat->flags = VPoxDispVHWAFromDDPFS(pDdFormat->dwFlags);
    pVHWAFormat->fourCC = pDdFormat->dwFourCC;
    pVHWAFormat->c.rgbBitCount = pDdFormat->dwRGBBitCount;
    pVHWAFormat->m1.rgbRBitMask = pDdFormat->dwRBitMask;
    pVHWAFormat->m2.rgbGBitMask = pDdFormat->dwGBitMask;
    pVHWAFormat->m3.rgbBBitMask = pDdFormat->dwBBitMask;
    return VINF_SUCCESS;
}

int VPoxDispVHWAFromDDSURFACEDESC(VPOXVHWA_SURFACEDESC RT_UNTRUSTED_VOLATILE_HOST *pVHWADesc, DDSURFACEDESC *pDdDesc)
{
    uint32_t unsupds = VPoxDispVHWAUnsupportedDSS(pDdDesc->dwFlags);
    Assert(!unsupds);
    if(unsupds)
        return VERR_GENERAL_FAILURE;

    pVHWADesc->flags = 0;

    if(pDdDesc->dwFlags & DDSD_BACKBUFFERCOUNT)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_BACKBUFFERCOUNT;
        pVHWADesc->cBackBuffers = pDdDesc->dwBackBufferCount;
    }
    if(pDdDesc->dwFlags & DDSD_CAPS)
    {
        uint32_t unsup = VPoxDispVHWAUnsupportedDDSCAPS(pDdDesc->ddsCaps.dwCaps);
        Assert(!unsup);
        if(unsup)
            return VERR_GENERAL_FAILURE;
        pVHWADesc->flags |= VPOXVHWA_SD_CAPS;
        pVHWADesc->surfCaps = VPoxDispVHWAFromDDSCAPS(pDdDesc->ddsCaps.dwCaps);
    }
    if(pDdDesc->dwFlags & DDSD_CKDESTBLT)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_CKDESTBLT;
        VPoxDispVHWAFromDDCOLORKEY(&pVHWADesc->DstBltCK, &pDdDesc->ddckCKDestBlt);
    }
    if(pDdDesc->dwFlags & DDSD_CKDESTOVERLAY)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_CKDESTOVERLAY;
        VPoxDispVHWAFromDDCOLORKEY(&pVHWADesc->DstOverlayCK, &pDdDesc->ddckCKDestOverlay);
    }
    if(pDdDesc->dwFlags & DDSD_CKSRCBLT)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_CKSRCBLT;
        VPoxDispVHWAFromDDCOLORKEY(&pVHWADesc->SrcBltCK, &pDdDesc->ddckCKSrcBlt);
    }
    if(pDdDesc->dwFlags & DDSD_CKSRCOVERLAY)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_CKSRCOVERLAY;
        VPoxDispVHWAFromDDCOLORKEY(&pVHWADesc->SrcOverlayCK, &pDdDesc->ddckCKSrcOverlay);
    }
    if(pDdDesc->dwFlags & DDSD_HEIGHT)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_HEIGHT;
        pVHWADesc->height = pDdDesc->dwHeight;
    }
    if(pDdDesc->dwFlags & DDSD_WIDTH)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_WIDTH;
        pVHWADesc->width = pDdDesc->dwWidth;
    }
    if(pDdDesc->dwFlags & DDSD_PITCH)
    {
        pVHWADesc->flags |= VPOXVHWA_SD_PITCH;
        pVHWADesc->pitch = pDdDesc->lPitch;
    }
    if(pDdDesc->dwFlags & DDSD_PIXELFORMAT)
    {
        int rc = VPoxDispVHWAFromDDPIXELFORMAT(&pVHWADesc->PixelFormat, &pDdDesc->ddpfPixelFormat);
        if(RT_FAILURE(rc))
            return rc;
        pVHWADesc->flags |= VPOXVHWA_SD_PIXELFORMAT;
    }
    return VINF_SUCCESS;
}

void VPoxDispVHWAFromRECTL(VPOXVHWA_RECTL *pDst, RECTL const *pSrc)
{
    pDst->left = pSrc->left;
    pDst->top = pSrc->top;
    pDst->right = pSrc->right;
    pDst->bottom = pSrc->bottom;
}

void VPoxDispVHWAFromRECTL(VPOXVHWA_RECTL RT_UNTRUSTED_VOLATILE_HOST *pDst, RECTL const *pSrc)
{
    pDst->left = pSrc->left;
    pDst->top = pSrc->top;
    pDst->right = pSrc->right;
    pDst->bottom = pSrc->bottom;
}

#define MIN(_a, _b) (_a) < (_b) ? (_a) : (_b)
#define MAX(_a, _b) (_a) > (_b) ? (_a) : (_b)

void VPoxDispVHWARectUnited(RECTL * pDst, RECTL * pRect1, RECTL * pRect2)
{
    pDst->left = MIN(pRect1->left, pRect2->left);
    pDst->top = MIN(pRect1->top, pRect2->top);
    pDst->right = MAX(pRect1->right, pRect2->right);
    pDst->bottom = MAX(pRect1->bottom, pRect2->bottom);
}

bool VPoxDispVHWARectIsEmpty(RECTL * pRect)
{
    return pRect->left == pRect->right-1 && pRect->top == pRect->bottom-1;
}

bool VPoxDispVHWARectIntersect(RECTL * pRect1, RECTL * pRect2)
{
    return !((pRect1->left < pRect2->left && pRect1->right < pRect2->left)
            || (pRect2->left < pRect1->left && pRect2->right < pRect1->left)
            || (pRect1->top < pRect2->top && pRect1->bottom < pRect2->top)
            || (pRect2->top < pRect1->top && pRect2->bottom < pRect1->top));
}

bool VPoxDispVHWARectInclude(RECTL * pRect1, RECTL * pRect2)
{
    return ((pRect1->left <= pRect2->left && pRect1->right >= pRect2->right)
            && (pRect1->top <= pRect2->top && pRect1->bottom >= pRect2->bottom));
}


bool VPoxDispVHWARegionIntersects(PVPOXVHWAREGION pReg, RECTL * pRect)
{
    if(!pReg->bValid)
        return false;
    return VPoxDispVHWARectIntersect(&pReg->Rect, pRect);
}

bool VPoxDispVHWARegionIncludes(PVPOXVHWAREGION pReg, RECTL * pRect)
{
    if(!pReg->bValid)
        return false;
    return VPoxDispVHWARectInclude(&pReg->Rect, pRect);
}

bool VPoxDispVHWARegionIncluded(PVPOXVHWAREGION pReg, RECTL * pRect)
{
    if(!pReg->bValid)
        return true;
    return VPoxDispVHWARectInclude(pRect, &pReg->Rect);
}

void VPoxDispVHWARegionSet(PVPOXVHWAREGION pReg, RECTL * pRect)
{
    if(VPoxDispVHWARectIsEmpty(pRect))
    {
        pReg->bValid = false;
    }
    else
    {
        pReg->Rect = *pRect;
        pReg->bValid = true;
    }
}

void VPoxDispVHWARegionAdd(PVPOXVHWAREGION pReg, RECTL * pRect)
{
    if(VPoxDispVHWARectIsEmpty(pRect))
    {
        return;
    }
    else if(!pReg->bValid)
    {
        VPoxDispVHWARegionSet(pReg, pRect);
    }
    else
    {
        VPoxDispVHWARectUnited(&pReg->Rect, &pReg->Rect, pRect);
    }
}

void VPoxDispVHWARegionInit(PVPOXVHWAREGION pReg)
{
    pReg->bValid = false;
}

void VPoxDispVHWARegionClear(PVPOXVHWAREGION pReg)
{
    pReg->bValid = false;
}

bool VPoxDispVHWARegionValid(PVPOXVHWAREGION pReg)
{
    return pReg->bValid;
}

void VPoxDispVHWARegionTrySubstitute(PVPOXVHWAREGION pReg, const RECTL *pRect)
{
    if(!pReg->bValid)
        return;

    if(pReg->Rect.left >= pRect->left && pReg->Rect.right <= pRect->right)
    {
        LONG t = MAX(pReg->Rect.top, pRect->top);
        LONG b = MIN(pReg->Rect.bottom, pRect->bottom);
        if(t < b)
        {
            pReg->Rect.top = t;
            pReg->Rect.bottom = b;
        }
        else
        {
            pReg->bValid = false;
        }
    }
    else if(pReg->Rect.top >= pRect->top && pReg->Rect.bottom <= pRect->bottom)
    {
        LONG l = MAX(pReg->Rect.left, pRect->left);
        LONG r = MIN(pReg->Rect.right, pRect->right);
        if(l < r)
        {
            pReg->Rect.left = l;
            pReg->Rect.right = r;
        }
        else
        {
            pReg->bValid = false;
        }
    }
}
