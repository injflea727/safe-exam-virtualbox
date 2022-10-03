/* $Id: VPoxMPTypes.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPTypes_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPTypes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

typedef struct _VPOXMP_DEVEXT *PVPOXMP_DEVEXT;
typedef struct VPOXWDDM_CONTEXT *PVPOXWDDM_CONTEXT;
typedef struct VPOXWDDM_ALLOCATION *PVPOXWDDM_ALLOCATION;

#include "common/wddm/VPoxMPIf.h"
#include "VPoxMPMisc.h"
#include "VPoxMPCm.h"
#include "VPoxMPVdma.h"
#include "VPoxMPShgsmi.h"
#include "VPoxMPVbva.h"
#include "VPoxMPSa.h"
#include "VPoxMPVModes.h"

#if 0
#include <iprt/avl.h>
#endif

#define VPOXWDDM_DEFAULT_REFRESH_RATE 60

#ifndef VPOX_WITH_MESA3D
/* one page size */
#define VPOXWDDM_C_DMA_BUFFER_SIZE         0x1000
#define VPOXWDDM_C_DMA_PRIVATEDATA_SIZE    0x4000
#else
#define VPOXWDDM_C_DMA_BUFFER_SIZE         0x10000
#define VPOXWDDM_C_DMA_PRIVATEDATA_SIZE    0x8000
#endif
#define VPOXWDDM_C_ALLOC_LIST_SIZE         0xc00
#define VPOXWDDM_C_PATH_LOCATION_LIST_SIZE 0xc00

#ifndef VPOX_WITH_MESA3D
#define VPOXWDDM_C_POINTER_MAX_WIDTH  64
#define VPOXWDDM_C_POINTER_MAX_HEIGHT 64
#else
#define VPOXWDDM_C_POINTER_MAX_WIDTH  256
#define VPOXWDDM_C_POINTER_MAX_HEIGHT 256
#define VPOXWDDM_C_POINTER_MAX_WIDTH_LEGACY  64
#define VPOXWDDM_C_POINTER_MAX_HEIGHT_LEGACY 64
#endif

#define VPOXWDDM_DUMMY_DMABUFFER_SIZE 4

#define VPOXWDDM_POINTER_ATTRIBUTES_SIZE VPOXWDDM_ROUNDBOUND( \
         VPOXWDDM_ROUNDBOUND( sizeof (VIDEO_POINTER_ATTRIBUTES), 4 ) + \
         VPOXWDDM_ROUNDBOUND(VPOXWDDM_C_POINTER_MAX_WIDTH * VPOXWDDM_C_POINTER_MAX_HEIGHT * 4, 4) + \
         VPOXWDDM_ROUNDBOUND((VPOXWDDM_C_POINTER_MAX_WIDTH * VPOXWDDM_C_POINTER_MAX_HEIGHT + 7) >> 3, 4) \
          , 8)

typedef struct _VPOXWDDM_POINTER_INFO
{
    uint32_t xPos;
    uint32_t yPos;
    union
    {
        VIDEO_POINTER_ATTRIBUTES data;
        char buffer[VPOXWDDM_POINTER_ATTRIBUTES_SIZE];
    } Attributes;
} VPOXWDDM_POINTER_INFO, *PVPOXWDDM_POINTER_INFO;

typedef struct _VPOXWDDM_GLOBAL_POINTER_INFO
{
    uint32_t iLastReportedScreen;
} VPOXWDDM_GLOBAL_POINTER_INFO, *PVPOXWDDM_GLOBAL_POINTER_INFO;

#ifdef VPOX_WITH_VIDEOHWACCEL
typedef struct VPOXWDDM_VHWA
{
    VPOXVHWA_INFO Settings;
    volatile uint32_t cOverlaysCreated;
} VPOXWDDM_VHWA;
#endif

typedef struct VPOXWDDM_ADDR
{
    /* if SegmentId == NULL - the sysmem data is presented with pvMem */
    UINT SegmentId;
    union {
        VPOXVIDEOOFFSET offVram;
        void * pvMem;
    };
} VPOXWDDM_ADDR, *PVPOXWDDM_ADDR;

typedef struct VPOXWDDM_ALLOC_DATA
{
    VPOXWDDM_SURFACE_DESC SurfDesc;
    VPOXWDDM_ADDR Addr;
    uint32_t hostID;
    uint32_t cHostIDRefs;
} VPOXWDDM_ALLOC_DATA, *PVPOXWDDM_ALLOC_DATA;

#define VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS 0x01
#define VPOXWDDM_HGSYNC_F_SYNCED_LOCATION   0x02
#define VPOXWDDM_HGSYNC_F_SYNCED_VISIBILITY 0x04
#define VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY   0x08
#define VPOXWDDM_HGSYNC_F_SYNCED_ALL        (VPOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS | VPOXWDDM_HGSYNC_F_SYNCED_LOCATION | VPOXWDDM_HGSYNC_F_SYNCED_VISIBILITY | VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY)
#define VPOXWDDM_HGSYNC_F_CHANGED_LOCATION_ONLY        (VPOXWDDM_HGSYNC_F_SYNCED_ALL & ~VPOXWDDM_HGSYNC_F_SYNCED_LOCATION)
#define VPOXWDDM_HGSYNC_F_CHANGED_TOPOLOGY_ONLY        (VPOXWDDM_HGSYNC_F_SYNCED_ALL & ~VPOXWDDM_HGSYNC_F_SYNCED_TOPOLOGY)

typedef struct VPOXWDDM_SOURCE
{
    struct VPOXWDDM_ALLOCATION * pPrimaryAllocation;
    VPOXWDDM_ALLOC_DATA AllocData;
    uint8_t u8SyncState;
    BOOLEAN fTargetsReported;
    BOOLEAN bVisible;
    BOOLEAN bBlankedByPowerOff;
    VPOXVBVAINFO Vbva;
#ifdef VPOX_WITH_VIDEOHWACCEL
    /* @todo: in our case this seems more like a target property,
     * but keep it here for now */
    VPOXWDDM_VHWA Vhwa;
    volatile uint32_t cOverlays;
    LIST_ENTRY OverlayList;
    KSPIN_LOCK OverlayListLock;
#endif
    KSPIN_LOCK AllocationLock;
    POINT VScreenPos;
    VPOXWDDM_POINTER_INFO PointerInfo;
    uint32_t cTargets;
    VPOXCMDVBVA_SCREENMAP_DECL(uint32_t, aTargetMap);
} VPOXWDDM_SOURCE, *PVPOXWDDM_SOURCE;

typedef struct VPOXWDDM_TARGET
{
    RTRECTSIZE Size;
    uint32_t u32Id;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    /* since there coul be multiple state changes on auto-resize,
     * we pend notifying host to avoid flickering */
    uint8_t u8SyncState;
    bool fConnected;
    bool fConfigured;
    bool fBlankedByPowerOff;

    /* Whether the host has disabled the virtual screen. */
    /** @todo This should be merged with fConnected. */
    bool fDisabled;
} VPOXWDDM_TARGET, *PVPOXWDDM_TARGET;

/* allocation */
//#define VPOXWDDM_ALLOCATIONINDEX_VOID (~0U)
typedef struct VPOXWDDM_ALLOCATION
{
    VPOXWDDM_ALLOC_TYPE enmType;
    D3DDDI_RESOURCEFLAGS fRcFlags;
#ifdef VPOX_WITH_VIDEOHWACCEL
    VPOXVHWA_SURFHANDLE hHostHandle;
#endif
    BOOLEAN fDeleted;
    BOOLEAN bVisible;
    BOOLEAN bAssigned;
#ifdef DEBUG
    /* current for shared rc handling assumes that once resource has no opens, it can not be openned agaion */
    BOOLEAN fAssumedDeletion;
#endif
    VPOXWDDM_ALLOC_DATA AllocData;
    struct VPOXWDDM_RESOURCE *pResource;
    /* to return to the Runtime on DxgkDdiCreateAllocation */
    DXGK_ALLOCATIONUSAGEHINT UsageHint;
    uint32_t iIndex;
    uint32_t cOpens;
    KSPIN_LOCK OpenLock;
    LIST_ENTRY OpenList;
    /* helps tracking when to release wine shared resource */
    uint32_t cShRcRefs;
    HANDLE hSharedHandle;
#if 0
    AVLPVNODECORE ShRcTreeEntry;
#endif
    VPOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType;
} VPOXWDDM_ALLOCATION, *PVPOXWDDM_ALLOCATION;

typedef struct VPOXWDDM_RESOURCE
{
    VPOXWDDMDISP_RESOURCE_FLAGS fFlags;
    volatile uint32_t cRefs;
    VPOXWDDM_RC_DESC RcDesc;
    BOOLEAN fDeleted;
    uint32_t cAllocations;
    VPOXWDDM_ALLOCATION aAllocations[1];
} VPOXWDDM_RESOURCE, *PVPOXWDDM_RESOURCE;

typedef struct VPOXWDDM_OVERLAY
{
    LIST_ENTRY ListEntry;
    PVPOXMP_DEVEXT pDevExt;
    PVPOXWDDM_RESOURCE pResource;
    PVPOXWDDM_ALLOCATION pCurentAlloc;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    RECT DstRect;
} VPOXWDDM_OVERLAY, *PVPOXWDDM_OVERLAY;

typedef enum
{
    VPOXWDDM_DEVICE_TYPE_UNDEFINED = 0,
    VPOXWDDM_DEVICE_TYPE_SYSTEM
} VPOXWDDM_DEVICE_TYPE;

typedef struct VPOXWDDM_DEVICE
{
    PVPOXMP_DEVEXT pAdapter; /* Adapder info */
    HANDLE hDevice; /* handle passed to CreateDevice */
    VPOXWDDM_DEVICE_TYPE enmType; /* device creation flags passed to DxgkDdiCreateDevice, not sure we need it */
} VPOXWDDM_DEVICE, *PVPOXWDDM_DEVICE;

typedef enum
{
    VPOXWDDM_OBJSTATE_TYPE_UNKNOWN = 0,
    VPOXWDDM_OBJSTATE_TYPE_INITIALIZED,
    VPOXWDDM_OBJSTATE_TYPE_TERMINATED
} VPOXWDDM_OBJSTATE_TYPE;

#define VPOXWDDM_INVALID_COORD ((LONG)((~0UL) >> 1))

typedef struct VPOXWDDM_CONTEXT
{
    struct VPOXWDDM_DEVICE * pDevice;
    HANDLE hContext;
    VPOXWDDM_CONTEXT_TYPE enmType;
    UINT  NodeOrdinal;
    UINT  EngineAffinity;
    BOOLEAN fRenderFromShadowDisabled;
    VPOXVIDEOCM_CTX CmContext;
    VPOXVIDEOCM_ALLOC_CONTEXT AllocContext;
#ifdef VPOX_WITH_MESA3D
    uint32_t u32Cid;               /* SVGA context id of this context. */
#endif
} VPOXWDDM_CONTEXT, *PVPOXWDDM_CONTEXT;

typedef struct VPOXWDDM_OPENALLOCATION
{
    LIST_ENTRY ListEntry;
    D3DKMT_HANDLE  hAllocation;
    PVPOXWDDM_ALLOCATION pAllocation;
    PVPOXWDDM_DEVICE pDevice;
    uint32_t cShRcRefs;
    uint32_t cOpens;
    uint32_t cHostIDRefs;
} VPOXWDDM_OPENALLOCATION, *PVPOXWDDM_OPENALLOCATION;

#define VPOX_VMODES_MAX_COUNT 128

typedef struct VPOX_VMODES
{
    uint32_t cTargets;
    CR_SORTARRAY aTargets[VPOX_VIDEO_MAX_SCREENS];
} VPOX_VMODES;

typedef struct VPOXWDDM_VMODES
{
    VPOX_VMODES Modes;
    /* note that we not use array indices to indentify modes, because indices may change due to element removal */
    uint64_t aTransientResolutions[VPOX_VIDEO_MAX_SCREENS];
    uint64_t aPendingRemoveCurResolutions[VPOX_VIDEO_MAX_SCREENS];
} VPOXWDDM_VMODES;

typedef struct VPOXVDMADDI_CMD_QUEUE
{
    volatile uint32_t cQueuedCmds;
    LIST_ENTRY CmdQueue;
} VPOXVDMADDI_CMD_QUEUE, *PVPOXVDMADDI_CMD_QUEUE;

typedef struct VPOXVDMADDI_NODE
{
    VPOXVDMADDI_CMD_QUEUE CmdQueue;
    UINT uLastCompletedFenceId;
} VPOXVDMADDI_NODE, *PVPOXVDMADDI_NODE;

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPTypes_h */
