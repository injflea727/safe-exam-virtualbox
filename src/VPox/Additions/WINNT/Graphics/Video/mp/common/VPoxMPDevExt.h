/* $Id: VPoxMPDevExt.h $ */
/** @file
 * VPox Miniport device extension header
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_common_VPoxMPDevExt_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_common_VPoxMPDevExt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxMPUtils.h"
#include <VPoxVideoGuest.h>
#include <HGSMIHostCmd.h>

#ifdef VPOX_XPDM_MINIPORT
# include <iprt/nt/miniport.h>
# include <ntddvdeo.h>
# include <iprt/nt/video.h>
# include "common/xpdm/VPoxVideoPortAPI.h"
#endif

#ifdef VPOX_WDDM_MINIPORT
extern DWORD g_VPoxDisplayOnly;
# include "wddm/VPoxMPTypes.h"
#endif

#ifdef VPOX_WDDM_MINIPORT
typedef struct VPOXWDDM_HWRESOURCES
{
    PHYSICAL_ADDRESS phVRAM;
    ULONG cbVRAM;
    ULONG ulApertureSize;
#ifdef VPOX_WITH_MESA3D
    PHYSICAL_ADDRESS phFIFO;
    ULONG cbFIFO;
    PHYSICAL_ADDRESS phIO;
    ULONG cbIO;
#endif
} VPOXWDDM_HWRESOURCES, *PVPOXWDDM_HWRESOURCES;

#ifdef VPOX_WITH_MESA3D
typedef struct VPOXWDDM_EXT_GA *PVPOXWDDM_EXT_GA;
#endif

#endif /* VPOX_WDDM_MINIPORT */

#define VPOXMP_MAX_VIDEO_MODES 128
typedef struct VPOXMP_COMMON
{
    int cDisplays;                      /* Number of displays. */

    uint32_t cbVRAM;                    /* The VRAM size. */

    PHYSICAL_ADDRESS phVRAM;            /* Physical VRAM base. */

    ULONG ulApertureSize;               /* Size of the LFB aperture (>= VRAM size). */

    uint32_t cbMiniportHeap;            /* The size of reserved VRAM for miniport driver heap.
                                         * It is at offset:
                                         *   cbAdapterMemorySize - VPOX_VIDEO_ADAPTER_INFORMATION_SIZE - cbMiniportHeap
                                         */
    void *pvMiniportHeap;               /* The pointer to the miniport heap VRAM.
                                         * This is mapped by miniport separately.
                                         */
    void *pvAdapterInformation;         /* The pointer to the last 4K of VRAM.
                                         * This is mapped by miniport separately.
                                         */

    /** Whether HGSMI is enabled. */
    bool bHGSMI;
    /** Context information needed to receive commands from the host. */
    HGSMIHOSTCOMMANDCONTEXT hostCtx;
    /** Context information needed to submit commands to the host. */
    HGSMIGUESTCOMMANDCONTEXT guestCtx;

    BOOLEAN fAnyX;                      /* Unrestricted horizontal resolution flag. */
    uint16_t u16SupportedScreenFlags;   /* VBVA_SCREEN_F_* flags supported by the host. */
} VPOXMP_COMMON, *PVPOXMP_COMMON;

typedef struct _VPOXMP_DEVEXT
{
   struct _VPOXMP_DEVEXT *pNext;               /* Next extension in the DualView extension list.
                                                * The primary extension is the first one.
                                                */
#ifdef VPOX_XPDM_MINIPORT
   struct _VPOXMP_DEVEXT *pPrimary;            /* Pointer to the primary device extension. */

   ULONG iDevice;                              /* Device index: 0 for primary, otherwise a secondary device. */
   /* Standart video modes list.
    * Additional space is reserved for a custom video mode for this guest monitor.
    * The custom video mode index is alternating for each mode set and 2 indexes are needed for the custom mode.
    */
   VIDEO_MODE_INFORMATION aVideoModes[VPOXMP_MAX_VIDEO_MODES + 2];
   /* Number of available video modes, set by VPoxMPCmnBuildVideoModesTable. */
   uint32_t cVideoModes;
   ULONG CurrentMode;                          /* Saved information about video modes */
   ULONG CurrentModeWidth;
   ULONG CurrentModeHeight;
   ULONG CurrentModeBPP;

   ULONG ulFrameBufferOffset;                  /* The framebuffer position in the VRAM. */
   ULONG ulFrameBufferSize;                    /* The size of the current framebuffer. */

   uint8_t  iInvocationCounter;
   uint32_t Prev_xres;
   uint32_t Prev_yres;
   uint32_t Prev_bpp;
#endif /*VPOX_XPDM_MINIPORT*/

#ifdef VPOX_WDDM_MINIPORT
   PDEVICE_OBJECT pPDO;
   UNICODE_STRING RegKeyName;
   UNICODE_STRING VideoGuid;

   uint8_t * pvVisibleVram;

   VPOXVIDEOCM_MGR CmMgr;
   VPOXVIDEOCM_MGR SeamlessCtxMgr;
   /* hgsmi allocation manager */
   VPOXVIDEOCM_ALLOC_MGR AllocMgr;
   /* mutex for context list operations */
   VPOXVDMADDI_NODE aNodes[VPOXWDDM_NUM_NODES];
   LIST_ENTRY DpcCmdQueue;
   KSPIN_LOCK ContextLock;
   KSPIN_LOCK SynchLock;
   volatile uint32_t cContexts3D;
   volatile uint32_t cContexts2D;
   volatile uint32_t cContextsDispIfResize;
   volatile uint32_t cUnlockedVBVADisabled;

   volatile uint32_t fCompletingCommands;

   DWORD dwDrvCfgFlags;

   BOOLEAN f3DEnabled;
   BOOLEAN fCmdVbvaEnabled;
   BOOLEAN fComplexTopologiesEnabled;

   VPOXWDDM_GLOBAL_POINTER_INFO PointerInfo;

   VPOXVTLIST CtlList;
   VPOXVTLIST DmaCmdList;
#ifdef VPOX_WITH_VIDEOHWACCEL
   VPOXVTLIST VhwaCmdList;
#endif
   BOOLEAN bNotifyDxDpc;

   BOOLEAN fDisableTargetUpdate;



   BOOL bVSyncTimerEnabled;
   volatile uint32_t fVSyncInVBlank;
   volatile LARGE_INTEGER VSyncTime;
   KTIMER VSyncTimer;
   KDPC VSyncDpc;

#if 0
   FAST_MUTEX ShRcTreeMutex;
   AVLPVTREE ShRcTree;
#endif

   VPOXWDDM_SOURCE aSources[VPOX_VIDEO_MAX_SCREENS];
   VPOXWDDM_TARGET aTargets[VPOX_VIDEO_MAX_SCREENS];
#endif /*VPOX_WDDM_MINIPORT*/

   union {
       /* Information that is only relevant to the primary device or is the same for all devices. */
       struct {

           void *pvReqFlush;                   /* Pointer to preallocated generic request structure for
                                                * VMMDevReq_VideoAccelFlush. Allocated when VBVA status
                                                * is changed. Deallocated on HwReset.
                                                */
           ULONG ulVbvaEnabled;                /* Indicates that VBVA mode is enabled. */
           ULONG ulMaxFrameBufferSize;         /* The size of the VRAM allocated for the a single framebuffer. */
           BOOLEAN fMouseHidden;               /* Has the mouse cursor been hidden by the guest? */
           VPOXMP_COMMON commonInfo;
#ifdef VPOX_XPDM_MINIPORT
           /* Video Port API dynamically picked up at runtime for binary backwards compatibility with older NT versions */
           VPOXVIDEOPORTPROCS VideoPortProcs;
#endif

#ifdef VPOX_WDDM_MINIPORT
           VPOXVDMAINFO Vdma;
           UINT uLastCompletedPagingBufferCmdFenceId; /* Legacy */
# ifdef VPOXVDMA_WITH_VBVA
           VPOXVBVAINFO Vbva;
# endif
           D3DKMDT_HVIDPN hCommittedVidPn;      /* committed VidPn handle */
           DXGKRNL_INTERFACE DxgkInterface;     /* Display Port handle and callbacks */
#endif
       } primary;

       /* Secondary device information. */
       struct {
           BOOLEAN bEnabled;                   /* Device enabled flag */
       } secondary;
   } u;

   HGSMIAREA areaDisplay;                      /* Entire VRAM chunk for this display device. */

#ifdef VPOX_WDDM_MINIPORT
   VPOXVIDEO_HWTYPE enmHwType;
   VPOXWDDM_HWRESOURCES HwResources;
#endif

#ifdef VPOX_WITH_MESA3D
   PVPOXWDDM_EXT_GA pGa;                       /* Pointer to Gallium backend data. */
#endif

   ULONG cbVRAMCpuVisible;                     /* How much video memory is available for the CPU visible segment. */
} VPOXMP_DEVEXT, *PVPOXMP_DEVEXT;

DECLINLINE(PVPOXMP_DEVEXT) VPoxCommonToPrimaryExt(PVPOXMP_COMMON pCommon)
{
    return RT_FROM_MEMBER(pCommon, VPOXMP_DEVEXT, u.primary.commonInfo);
}

DECLINLINE(PVPOXMP_COMMON) VPoxCommonFromDeviceExt(PVPOXMP_DEVEXT pExt)
{
#ifdef VPOX_XPDM_MINIPORT
    return &pExt->pPrimary->u.primary.commonInfo;
#else
    return &pExt->u.primary.commonInfo;
#endif
}

#ifdef VPOX_WDDM_MINIPORT
DECLINLINE(ULONG) vpoxWddmVramCpuVisibleSize(PVPOXMP_DEVEXT pDevExt)
{
    return pDevExt->cbVRAMCpuVisible;
}

DECLINLINE(ULONG) vpoxWddmVramCpuVisibleSegmentSize(PVPOXMP_DEVEXT pDevExt)
{
    return vpoxWddmVramCpuVisibleSize(pDevExt);
}

/* 128 MB */
DECLINLINE(ULONG) vpoxWddmVramCpuInvisibleSegmentSize(PVPOXMP_DEVEXT pDevExt)
{
    RT_NOREF(pDevExt);
    return 128 * 1024 * 1024;
}

#ifdef VPOXWDDM_RENDER_FROM_SHADOW

DECLINLINE(bool) vpoxWddmCmpSurfDescsBase(VPOXWDDM_SURFACE_DESC *pDesc1, VPOXWDDM_SURFACE_DESC *pDesc2)
{
    if (pDesc1->width != pDesc2->width)
        return false;
    if (pDesc1->height != pDesc2->height)
        return false;
    if (pDesc1->format != pDesc2->format)
        return false;
    if (pDesc1->bpp != pDesc2->bpp)
        return false;
    if (pDesc1->pitch != pDesc2->pitch)
        return false;
    return true;
}

#endif
#endif /*VPOX_WDDM_MINIPORT*/

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_common_VPoxMPDevExt_h */
