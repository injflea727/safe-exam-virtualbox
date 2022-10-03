/* $Id: VPoxMPIf.h $ */
/** @file
 * VPox WDDM Miniport driver.
 *
 * Contains base definitions of constants & structures used to control & perform
 * rendering, such as DMA commands types, allocation types, escape codes, etc.
 * used by both miniport & display drivers.
 *
 * The latter uses these and only these defs to communicate with the former
 * by posting appropriate requests via D3D RT Krnl Svc accessing callbacks.
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_common_wddm_VPoxMPIf_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_common_wddm_VPoxMPIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPoxVideo.h>
#include "../../../../include/VPoxDisplay.h"
#include "../VPoxVideoTools.h"
#include <VPoxUhgsmi.h>
#include <VPox/VPoxGuestCoreTypes.h> /* for VBGLIOCHGCMCALL */

/* One would increase this whenever definitions in this file are changed */
#define VPOXVIDEOIF_VERSION 22

/** @todo VPOXVIDEO_HWTYPE probably needs to be in VPoxVideo.h */
typedef enum VPOXVIDEO_HWTYPE
{
    VPOXVIDEO_HWTYPE_VPOX   = 0,
    VPOXVIDEO_HWTYPE_VMSVGA = 1,
    VPOXVIDEO_HWTYPE_32BIT  = 0x7fffffff
} VPOXVIDEO_HWTYPE;
AssertCompileSize(VPOXVIDEO_HWTYPE, 4);

#define VPOXWDDM_NODE_ID_SYSTEM             0
#define VPOXWDDM_NODE_ID_3D                 (VPOXWDDM_NODE_ID_SYSTEM)
#define VPOXWDDM_NODE_ID_3D_KMT             (VPOXWDDM_NODE_ID_3D)
#define VPOXWDDM_NODE_ID_2D_VIDEO           (VPOXWDDM_NODE_ID_3D_KMT + 1)
#define VPOXWDDM_NUM_NODES                  (VPOXWDDM_NODE_ID_2D_VIDEO + 1)

#define VPOXWDDM_ENGINE_ID_SYSTEM           0
#if (VPOXWDDM_NODE_ID_3D == VPOXWDDM_NODE_ID_SYSTEM)
# define VPOXWDDM_ENGINE_ID_3D              (VPOXWDDM_ENGINE_ID_SYSTEM + 1)
#else
# define VPOXWDDM_ENGINE_ID_3D              0
#endif
#if (VPOXWDDM_NODE_ID_3D_KMT == VPOXWDDM_NODE_ID_3D)
# define VPOXWDDM_ENGINE_ID_3D_KMT          VPOXWDDM_ENGINE_ID_3D
#else
# define VPOXWDDM_ENGINE_ID_3D_KMT          0
#endif
#if (VPOXWDDM_NODE_ID_2D_VIDEO == VPOXWDDM_NODE_ID_3D)
# define VPOXWDDM_ENGINE_ID_2D_VIDEO        VPOXWDDM_ENGINE_ID_3D
#else
# define VPOXWDDM_ENGINE_ID_2D_VIDEO        0
#endif


/* create allocation func */
typedef enum
{
    VPOXWDDM_ALLOC_TYPE_UNEFINED = 0,
    VPOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE,
    VPOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE,
    VPOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE,
    /* this one is win 7-specific and hence unused for now */
    VPOXWDDM_ALLOC_TYPE_STD_GDISURFACE
    /* custom allocation types requested from user-mode d3d module will go here */
    , VPOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC
    , VPOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER
} VPOXWDDM_ALLOC_TYPE;

/* usage */
typedef enum
{
    VPOXWDDM_ALLOCUSAGE_TYPE_UNEFINED = 0,
    /* set for the allocation being primary */
    VPOXWDDM_ALLOCUSAGE_TYPE_PRIMARY,
} VPOXWDDM_ALLOCUSAGE_TYPE;

typedef struct VPOXWDDM_SURFACE_DESC
{
    UINT width;
    UINT height;
    D3DDDIFORMAT format;
    UINT bpp;
    UINT pitch;
    UINT depth;
    UINT slicePitch;
    UINT d3dWidth;
    UINT cbSize;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    D3DDDI_RATIONAL RefreshRate;
} VPOXWDDM_SURFACE_DESC, *PVPOXWDDM_SURFACE_DESC;

typedef struct VPOXWDDM_ALLOCINFO
{
    VPOXWDDM_ALLOC_TYPE enmType;
    union
    {
        struct
        {
            D3DDDI_RESOURCEFLAGS fFlags;
            /* id used to identify the allocation on the host */
            uint32_t hostID;
            uint64_t hSharedHandle;
            VPOXWDDM_SURFACE_DESC SurfDesc;
        };

        struct
        {
            uint32_t cbBuffer;
            VPOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType;
        };
    };
} VPOXWDDM_ALLOCINFO, *PVPOXWDDM_ALLOCINFO;

typedef struct VPOXWDDM_RC_DESC
{
    D3DDDI_RESOURCEFLAGS fFlags;
    D3DDDIFORMAT enmFormat;
    D3DDDI_POOL enmPool;
    D3DDDIMULTISAMPLE_TYPE enmMultisampleType;
    UINT MultisampleQuality;
    UINT MipLevels;
    UINT Fvf;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    D3DDDI_RATIONAL RefreshRate;
    D3DDDI_ROTATION enmRotation;
} VPOXWDDM_RC_DESC, *PVPOXWDDM_RC_DESC;

typedef struct VPOXWDDMDISP_RESOURCE_FLAGS
{
    union
    {
        struct
        {
            UINT Opened     : 1; /* this resource is OpenResource'd rather than CreateResource'd */
            UINT Generic    : 1; /* identifies this is a resource created with CreateResource, the VPOXWDDMDISP_RESOURCE::fRcFlags is valid */
            UINT KmResource : 1; /* this resource has underlying km resource */
            UINT Reserved   : 29; /* reserved */
        };
        UINT        Value;
    };
} VPOXWDDMDISP_RESOURCE_FLAGS, *PVPOXWDDMDISP_RESOURCE_FLAGS;

typedef struct VPOXWDDM_RCINFO
{
    VPOXWDDMDISP_RESOURCE_FLAGS fFlags;
    VPOXWDDM_RC_DESC RcDesc;
    uint32_t cAllocInfos;
//    VPOXWDDM_ALLOCINFO aAllocInfos[1];
} VPOXWDDM_RCINFO, *PVPOXWDDM_RCINFO;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_FLAFS
{
    union
    {
        struct
        {
            UINT bCmdInDmaBuffer : 1;
            UINT bReserved : 31;
        };
        uint32_t Value;
    };
} VPOXWDDM_DMA_PRIVATEDATA_FLAFS, *PVPOXWDDM_DMA_PRIVATEDATA_FLAFS;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_BASEHDR
{
    VPOXVDMACMD_TYPE enmCmd;
    union
    {
        VPOXWDDM_DMA_PRIVATEDATA_FLAFS fFlags;
        uint32_t u32CmdReserved;
    };
} VPOXWDDM_DMA_PRIVATEDATA_BASEHDR, *PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR;

typedef struct VPOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO
{
    uint32_t offData;
    uint32_t cbData;
} VPOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO, *PVPOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD
{
    VPOXWDDM_DMA_PRIVATEDATA_BASEHDR Base;
    VPOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO aBufInfos[1];
} VPOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD, *PVPOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD;


#define VPOXVHWA_F_ENABLED  0x00000001
#define VPOXVHWA_F_CKEY_DST 0x00000002
#define VPOXVHWA_F_CKEY_SRC 0x00000004

#define VPOXVHWA_MAX_FORMATS 8

typedef struct VPOXVHWA_INFO
{
    uint32_t fFlags;
    uint32_t cOverlaysSupported;
    uint32_t cFormats;
    D3DDDIFORMAT aFormats[VPOXVHWA_MAX_FORMATS];
} VPOXVHWA_INFO;

#define VPOXWDDM_OVERLAY_F_CKEY_DST      0x00000001
#define VPOXWDDM_OVERLAY_F_CKEY_DSTRANGE 0x00000002
#define VPOXWDDM_OVERLAY_F_CKEY_SRC      0x00000004
#define VPOXWDDM_OVERLAY_F_CKEY_SRCRANGE 0x00000008
#define VPOXWDDM_OVERLAY_F_BOB           0x00000010
#define VPOXWDDM_OVERLAY_F_INTERLEAVED   0x00000020
#define VPOXWDDM_OVERLAY_F_MIRROR_LR     0x00000040
#define VPOXWDDM_OVERLAY_F_MIRROR_UD     0x00000080
#define VPOXWDDM_OVERLAY_F_DEINTERLACED  0x00000100

typedef struct VPOXWDDM_OVERLAY_DESC
{
    uint32_t fFlags;
    UINT DstColorKeyLow;
    UINT DstColorKeyHigh;
    UINT SrcColorKeyLow;
    UINT SrcColorKeyHigh;
} VPOXWDDM_OVERLAY_DESC, *PVPOXWDDM_OVERLAY_DESC;

typedef struct VPOXWDDM_OVERLAY_INFO
{
    VPOXWDDM_OVERLAY_DESC OverlayDesc;
    VPOXWDDM_DIRTYREGION DirtyRegion; /* <- the dirty region of the overlay surface */
} VPOXWDDM_OVERLAY_INFO, *PVPOXWDDM_OVERLAY_INFO;

typedef struct VPOXWDDM_OVERLAYFLIP_INFO
{
    VPOXWDDM_DIRTYREGION DirtyRegion; /* <- the dirty region of the overlay surface */
} VPOXWDDM_OVERLAYFLIP_INFO, *PVPOXWDDM_OVERLAYFLIP_INFO;


typedef enum
{
    VPOXWDDM_CONTEXT_TYPE_UNDEFINED = 0,
    /* system-created context (for GDI rendering) */
    VPOXWDDM_CONTEXT_TYPE_SYSTEM,
    /* context created by the D3D User-mode driver when crogl IS available */
    obsolete_VPOXWDDM_CONTEXT_TYPE_CUSTOM_3D,
    /* context created by the D3D User-mode driver when crogl is NOT available or for ddraw overlay acceleration */
    obsolete_VPOXWDDM_CONTEXT_TYPE_CUSTOM_2D,
    /* contexts created by the cromium HGSMI transport for HGSMI commands submission */
    obsolete_VPOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_3D,
    obsolete_VPOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_GL,
    /* context created by the kernel->user communication mechanism for visible rects reporting, etc.  */
    VPOXWDDM_CONTEXT_TYPE_CUSTOM_SESSION,
    /* context created by VPoxTray to handle resize operations */
    VPOXWDDM_CONTEXT_TYPE_CUSTOM_DISPIF_RESIZE,
    /* context created by VPoxTray to handle seamless operations */
    VPOXWDDM_CONTEXT_TYPE_CUSTOM_DISPIF_SEAMLESS
#ifdef VPOX_WITH_MESA3D
    /* Gallium driver context. */
    , VPOXWDDM_CONTEXT_TYPE_GA_3D
#endif
} VPOXWDDM_CONTEXT_TYPE;

typedef struct VPOXWDDM_CREATECONTEXT_INFO
{
    /* interface version, i.e. 9 for d3d9, 8 for d3d8, etc. */
    uint32_t u32IfVersion;
    /* What kind of context to create. */
    VPOXWDDM_CONTEXT_TYPE enmType;
    union
    {
        struct
        {
            uint32_t crVersionMajor;
            uint32_t crVersionMinor;
            /* we use uint64_t instead of HANDLE to ensure structure def is the same for both 32-bit and 64-bit
             * since x64 kernel driver can be called by 32-bit UMD */
            uint64_t hUmEvent;
            /* info to be passed to UMD notification to identify the context */
            uint64_t u64UmInfo;
        } vpox;
#ifdef VPOX_WITH_MESA3D
        struct
        {
            /* VPOXWDDM_F_GA_CONTEXT_* */
            uint32_t u32Flags;
        } vmsvga;
#endif
    } u;
} VPOXWDDM_CREATECONTEXT_INFO, *PVPOXWDDM_CREATECONTEXT_INFO;

typedef uint64_t VPOXDISP_UMHANDLE;
typedef uint32_t VPOXDISP_KMHANDLE;

typedef struct VPOXWDDM_RECTS_FLAFS
{
    union
    {
        struct
        {
            /* used only in conjunction with bSetVisibleRects.
             * if set - VPOXWDDM_RECTS_INFO::aRects[0] contains view rectangle */
            UINT bSetViewRect : 1;
            /* adds visible regions */
            UINT bAddVisibleRects : 1;
            /* adds hidden regions */
            UINT bAddHiddenRects : 1;
            /* hide entire window */
            UINT bHide : 1;
            /* reserved */
            UINT Reserved : 28;
        };
        uint32_t Value;
    };
} VPOXWDDM_RECTS_FLAFS, *PVPOXWDDM_RECTS_FLAFS;

typedef struct VPOXWDDM_RECTS_INFO
{
    uint32_t cRects;
    RECT aRects[1];
} VPOXWDDM_RECTS_INFO, *PVPOXWDDM_RECTS_INFO;

#define VPOXWDDM_RECTS_INFO_SIZE4CRECTS(_cRects) (RT_UOFFSETOF_DYN(VPOXWDDM_RECTS_INFO, aRects[(_cRects)]))
#define VPOXWDDM_RECTS_INFO_SIZE(_pRects) (VPOXVIDEOCM_CMD_RECTS_SIZE4CRECTS((_pRects)->cRects))

typedef enum
{
    /* command to be post to user mode */
    VPOXVIDEOCM_CMD_TYPE_UM = 0,
    /* control command processed in kernel mode */
    VPOXVIDEOCM_CMD_TYPE_CTL_KM,
    VPOXVIDEOCM_CMD_DUMMY_32BIT = 0x7fffffff
} VPOXVIDEOCM_CMD_TYPE;

typedef struct VPOXVIDEOCM_CMD_HDR
{
    uint64_t u64UmData;
    uint32_t cbCmd;
    VPOXVIDEOCM_CMD_TYPE enmType;
}VPOXVIDEOCM_CMD_HDR, *PVPOXVIDEOCM_CMD_HDR;

AssertCompile((sizeof (VPOXVIDEOCM_CMD_HDR) & 7) == 0);

typedef struct VPOXVIDEOCM_CMD_RECTS
{
    VPOXWDDM_RECTS_FLAFS fFlags;
    VPOXWDDM_RECTS_INFO RectsInfo;
} VPOXVIDEOCM_CMD_RECTS, *PVPOXVIDEOCM_CMD_RECTS;

typedef struct VPOXWDDM_GETVPOXVIDEOCMCMD_HDR
{
    uint32_t cbCmdsReturned;
    uint32_t cbRemainingCmds;
    uint32_t cbRemainingFirstCmd;
    uint32_t u32Reserved;
} VPOXWDDM_GETVPOXVIDEOCMCMD_HDR, *PVPOXWDDM_GETVPOXVIDEOCMCMD_HDR;

typedef struct VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD
{
    VPOXDISPIFESCAPE EscapeHdr;
    VPOXWDDM_GETVPOXVIDEOCMCMD_HDR Hdr;
} VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD, *PVPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD;

AssertCompile((sizeof (VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD) & 7) == 0);
AssertCompile(RT_OFFSETOF(VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD, EscapeHdr) == 0);

typedef struct VPOXDISPIFESCAPE_DBGPRINT
{
    VPOXDISPIFESCAPE EscapeHdr;
    /* null-terminated string to DbgPrint including \0 */
    char aStringBuf[1];
} VPOXDISPIFESCAPE_DBGPRINT, *PVPOXDISPIFESCAPE_DBGPRINT;
AssertCompile(RT_OFFSETOF(VPOXDISPIFESCAPE_DBGPRINT, EscapeHdr) == 0);

typedef enum
{
    VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE_UNDEFINED = 0,
    VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9 = 1,
    VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE_DUMMY32BIT = 0x7fffffff
} VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE;

typedef struct VPOXDISPIFESCAPE_DBGDUMPBUF_FLAGS
{
    union
    {
        struct
        {
            UINT WoW64      : 1;
            UINT Reserved   : 31; /* reserved */
        };
        UINT  Value;
    };
} VPOXDISPIFESCAPE_DBGDUMPBUF_FLAGS, *PVPOXDISPIFESCAPE_DBGDUMPBUF_FLAGS;

typedef struct VPOXDISPIFESCAPE_DBGDUMPBUF
{
    VPOXDISPIFESCAPE EscapeHdr;
    VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE enmType;
    VPOXDISPIFESCAPE_DBGDUMPBUF_FLAGS Flags;
    char aBuf[1];
} VPOXDISPIFESCAPE_DBGDUMPBUF, *PVPOXDISPIFESCAPE_DBGDUMPBUF;
AssertCompile(RT_OFFSETOF(VPOXDISPIFESCAPE_DBGDUMPBUF, EscapeHdr) == 0);

typedef struct VPOXVIDEOCM_UM_ALLOC
{
    VPOXDISP_KMHANDLE hAlloc;
    uint32_t cbData;
    uint64_t pvData;
    uint64_t hSynch;
    VPOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType;
} VPOXVIDEOCM_UM_ALLOC, *PVPOXVIDEOCM_UM_ALLOC;

typedef struct VPOXDISPIFESCAPE_SETALLOCHOSTID
{
    VPOXDISPIFESCAPE EscapeHdr;
    int32_t rc;
    uint32_t hostID;
    uint64_t hAlloc;

} VPOXDISPIFESCAPE_SETALLOCHOSTID, *PVPOXDISPIFESCAPE_SETALLOCHOSTID;

#ifdef VPOX_WITH_MESA3D

#define VPOXWDDM_F_GA_CONTEXT_EXTENDED 0x00000001
#define VPOXWDDM_F_GA_CONTEXT_VGPU10   0x00000002

#define VPOXESC_GAGETCID            0xA0000002
#define VPOXESC_GAREGION            0xA0000003
#define VPOXESC_GAPRESENT           0xA0000004
#define VPOXESC_GASURFACEDEFINE     0xA0000005
#define VPOXESC_GASURFACEDESTROY    0xA0000006
#define VPOXESC_GASHAREDSID         0xA0000008
#define VPOXESC_GAFENCECREATE       0xA0000020
#define VPOXESC_GAFENCEQUERY        0xA0000021
#define VPOXESC_GAFENCEWAIT         0xA0000022
#define VPOXESC_GAFENCEUNREF        0xA0000023

/* Get Gallium context id (cid) of the WDDM context. */
typedef struct VPOXDISPIFESCAPE_GAGETCID
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32Cid;
} VPOXDISPIFESCAPE_GAGETCID;

/* Create or delete a Guest Memory Region (GMR). */
#define GA_REGION_CMD_CREATE  0
#define GA_REGION_CMD_DESTROY 1
typedef struct VPOXDISPIFESCAPE_GAREGION
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32Command;
    uint32_t u32GmrId;
    uint32_t u32NumPages;
    uint32_t u32Reserved;
    uint64_t u64UserAddress;
} VPOXDISPIFESCAPE_GAREGION;

/* Debug helper. Present the specified surface by copying to the guest screen VRAM. */
typedef struct VPOXDISPIFESCAPE_GAPRESENT
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32Sid;
    uint32_t u32Width;
    uint32_t u32Height;
} VPOXDISPIFESCAPE_GAPRESENT;

/* Create a host surface. */
typedef struct VPOXDISPIFESCAPE_GASURFACEDEFINE
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32Sid; /* Returned surface id. */
    uint32_t cbReq;  /* Size of data after cSizes field. */
    uint32_t cSizes; /* Number of GASURFSIZE structures. */
    /* GASURFCREATE */
    /* GASURFSIZE[cSizes] */
} VPOXDISPIFESCAPE_GASURFACEDEFINE;

/* Delete a host surface. */
typedef struct VPOXDISPIFESCAPE_GASURFACEDESTROY
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32Sid;
} VPOXDISPIFESCAPE_GASURFACEDESTROY;

/* Inform the miniport that 'u32Sid' actually maps to 'u32SharedSid'.
 * If 'u32SharedSid' is ~0, then remove the mapping.
 */
typedef struct VPOXDISPIFESCAPE_GASHAREDSID
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32Sid;
    uint32_t u32SharedSid;
} VPOXDISPIFESCAPE_GASHAREDSID;

/* Create a user mode fence object. */
typedef struct VPOXDISPIFESCAPE_GAFENCECREATE
{
    VPOXDISPIFESCAPE EscapeHdr;

    /* IN: The miniport's handle of the fence.
     * Assigned by the miniport. Not DXGK fence id!
     */
    uint32_t u32FenceHandle;
} VPOXDISPIFESCAPE_GAFENCECREATE;

/* Query a user mode fence object state. */
#define GA_FENCE_STATUS_NULL      0 /* Fence not found */
#define GA_FENCE_STATUS_IDLE      1
#define GA_FENCE_STATUS_SUBMITTED 2
#define GA_FENCE_STATUS_SIGNALED  3
typedef struct VPOXDISPIFESCAPE_GAFENCEQUERY
{
    VPOXDISPIFESCAPE EscapeHdr;

    /* IN: The miniport's handle of the fence.
     * Assigned by the miniport. Not DXGK fence id!
     */
    uint32_t u32FenceHandle;

    /* OUT: The miniport's sequence number associated with the command buffer.
     */
    uint32_t u32SubmittedSeqNo;

    /* OUT: The miniport's sequence number associated with the last command buffer completed on host.
     */
    uint32_t u32ProcessedSeqNo;

    /* OUT: GA_FENCE_STATUS_*. */
    uint32_t u32FenceStatus;
} VPOXDISPIFESCAPE_GAFENCEQUERY;

/* Wait on a user mode fence object. */
typedef struct VPOXDISPIFESCAPE_GAFENCEWAIT
{
    VPOXDISPIFESCAPE EscapeHdr;

    /* IN: The miniport's handle of the fence.
     * Assigned by the miniport. Not DXGK fence id!
     */
    uint32_t u32FenceHandle;

    /* IN: Timeout in microseconds.
     */
    uint32_t u32TimeoutUS;
} VPOXDISPIFESCAPE_GAFENCEWAIT;

/* Delete a user mode fence object. */
typedef struct VPOXDISPIFESCAPE_GAFENCEUNREF
{
    VPOXDISPIFESCAPE EscapeHdr;

    /* IN: The miniport's handle of the fence.
     * Assigned by the miniport. Not DXGK fence id!
     */
    uint32_t u32FenceHandle;
} VPOXDISPIFESCAPE_GAFENCEUNREF;

#include <VPoxGaHWInfo.h>
#endif /* VPOX_WITH_MESA3D */

#define VPOXWDDM_QAI_CAP_3D     0x00000001 /* 3D is enabled in the VM settings. */
#define VPOXWDDM_QAI_CAP_DXVA   0x00000002 /* DXVA is not disabled in the guest registry. */
#define VPOXWDDM_QAI_CAP_DXVAHD 0x00000004 /* DXVA-HD is not disabled in the guest registry. */
#define VPOXWDDM_QAI_CAP_WIN7   0x00000008 /* User mode driver can report D3D_UMD_INTERFACE_VERSION_WIN7. */

/* D3DDDICB_QUERYADAPTERINFO::pPrivateDriverData */
typedef struct VPOXWDDM_QAI
{
    uint32_t            u32Version;      /* VPOXVIDEOIF_VERSION */
    uint32_t            u32Reserved;     /* Must be 0. */
    VPOXVIDEO_HWTYPE    enmHwType;       /* Hardware type. Determines what kind of data is returned. */
    uint32_t            u32AdapterCaps;  /* VPOXWDDM_QAI_CAP_* */
    uint32_t            cInfos;          /* Number of initialized elements in aInfos (equal to number of guest
                                          * displays). 0 if VPOX_WITH_VIDEOHWACCEL is not defined. */
    VPOXVHWA_INFO       aInfos[VPOX_VIDEO_MAX_SCREENS]; /* cInfos elements are initialized. */
    union
    {
        struct
        {
            /* VPOXVIDEO_HWTYPE_VPOX */
            uint32_t    u32VPox3DCaps;   /* CR_VPOX_CAP_* */
        } vpox;
#ifdef VPOX_WITH_MESA3D
        struct
        {
            /* VPOXVIDEO_HWTYPE_VMSVGA */
            VPOXGAHWINFO HWInfo;
        } vmsvga;
#endif
    } u;
} VPOXWDDM_QAI;

/** Convert a given FourCC code to a D3DDDIFORMAT enum. */
#define VPOXWDDM_D3DDDIFORMAT_FROM_FOURCC(_a, _b, _c, _d) \
    ((D3DDDIFORMAT)MAKEFOURCC(_a, _b, _c, _d))

/* submit cmd func */
DECLINLINE(D3DDDIFORMAT) vpoxWddmFmtNoAlphaFormat(D3DDDIFORMAT enmFormat)
{
    switch (enmFormat)
    {
        case D3DDDIFMT_A8R8G8B8:
            return D3DDDIFMT_X8R8G8B8;
        case D3DDDIFMT_A1R5G5B5:
            return D3DDDIFMT_X1R5G5B5;
        case D3DDDIFMT_A4R4G4B4:
            return D3DDDIFMT_X4R4G4B4;
        case D3DDDIFMT_A8B8G8R8:
            return D3DDDIFMT_X8B8G8R8;
        default:
            return enmFormat;
    }
}

/* tooling */
DECLINLINE(UINT) vpoxWddmCalcBitsPerPixel(D3DDDIFORMAT enmFormat)
{
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable:4063) /* VPOXWDDM_D3DDDIFORMAT_FROM_FOURCC('Y', 'V', '1', '2'): isn't part of the enum */
#endif
    switch (enmFormat)
    {
        case D3DDDIFMT_R8G8B8:
            return 24;
        case D3DDDIFMT_A8R8G8B8:
        case D3DDDIFMT_X8R8G8B8:
            return 32;
        case D3DDDIFMT_R5G6B5:
        case D3DDDIFMT_X1R5G5B5:
        case D3DDDIFMT_A1R5G5B5:
        case D3DDDIFMT_A4R4G4B4:
            return 16;
        case D3DDDIFMT_R3G3B2:
        case D3DDDIFMT_A8:
            return 8;
        case D3DDDIFMT_A8R3G3B2:
        case D3DDDIFMT_X4R4G4B4:
            return 16;
        case D3DDDIFMT_A2B10G10R10:
        case D3DDDIFMT_A8B8G8R8:
        case D3DDDIFMT_X8B8G8R8:
        case D3DDDIFMT_G16R16:
        case D3DDDIFMT_A2R10G10B10:
            return 32;
        case D3DDDIFMT_A16B16G16R16:
        case D3DDDIFMT_A16B16G16R16F:
            return 64;
        case D3DDDIFMT_A32B32G32R32F:
            return 128;
        case D3DDDIFMT_A8P8:
            return 16;
        case D3DDDIFMT_P8:
        case D3DDDIFMT_L8:
            return 8;
        case D3DDDIFMT_L16:
        case D3DDDIFMT_A8L8:
            return 16;
        case D3DDDIFMT_A4L4:
            return 8;
        case D3DDDIFMT_V8U8:
        case D3DDDIFMT_L6V5U5:
            return 16;
        case D3DDDIFMT_X8L8V8U8:
        case D3DDDIFMT_Q8W8V8U8:
        case D3DDDIFMT_V16U16:
        case D3DDDIFMT_W11V11U10:
        case D3DDDIFMT_A2W10V10U10:
            return 32;
        case D3DDDIFMT_D16_LOCKABLE:
        case D3DDDIFMT_D16:
        case D3DDDIFMT_D15S1:
            return 16;
        case D3DDDIFMT_D32:
        case D3DDDIFMT_D24S8:
        case D3DDDIFMT_D24X8:
        case D3DDDIFMT_D24X4S4:
        case D3DDDIFMT_D24FS8:
        case D3DDDIFMT_D32_LOCKABLE:
        case D3DDDIFMT_D32F_LOCKABLE:
            return 32;
        case D3DDDIFMT_S8_LOCKABLE:
            return 8;
        case D3DDDIFMT_DXT1:
            return 4;
        case D3DDDIFMT_DXT2:
        case D3DDDIFMT_DXT3:
        case D3DDDIFMT_DXT4:
        case D3DDDIFMT_DXT5:
        case D3DDDIFMT_VERTEXDATA:
        case D3DDDIFMT_INDEX16: /* <- yes, dx runtime treats it as such */
            return 8;
        case D3DDDIFMT_INDEX32:
            return 8;
        case D3DDDIFMT_R32F:
            return 32;
        case D3DDDIFMT_G32R32F:
            return 64;
        case D3DDDIFMT_R16F:
            return 16;
        case D3DDDIFMT_G16R16F:
            return 32;
        case D3DDDIFMT_YUY2: /* 4 bytes per 2 pixels. */
        case VPOXWDDM_D3DDDIFORMAT_FROM_FOURCC('Y', 'V', '1', '2'):
            return 16;
        default:
            AssertBreakpoint();
            return 0;
    }
#ifdef _MSC_VER
# pragma warning(pop)
#endif
}

DECLINLINE(uint32_t) vpoxWddmFormatToFourcc(D3DDDIFORMAT enmFormat)
{
    uint32_t uFormat = (uint32_t)enmFormat;
    /* assume that in case both four bytes are non-zero, this is a fourcc */
    if ((uFormat & 0xff000000)
            && (uFormat & 0x00ff0000)
            && (uFormat & 0x0000ff00)
            && (uFormat & 0x000000ff)
            )
        return uFormat;
    return 0;
}

#define VPOXWDDM_ROUNDBOUND(_v, _b) (((_v) + ((_b) - 1)) & ~((_b) - 1))

DECLINLINE(UINT) vpoxWddmCalcOffXru(UINT w, D3DDDIFORMAT enmFormat)
{
    switch (enmFormat)
    {
        /* pitch for the DXT* (aka compressed) formats is the size in bytes of blocks that fill in an image width
         * i.e. each block decompressed into 4 x 4 pixels, so we have ((Width + 3) / 4) blocks for Width.
         * then each block has 64 bits (8 bytes) for DXT1 and 64+64 bits (16 bytes) for DXT2-DXT5, so.. : */
        case D3DDDIFMT_DXT1:
        {
            UINT Pitch = (w + 3) / 4; /* <- pitch size in blocks */
            Pitch *= 8;               /* <- pitch size in bytes */
            return Pitch;
        }
        case D3DDDIFMT_DXT2:
        case D3DDDIFMT_DXT3:
        case D3DDDIFMT_DXT4:
        case D3DDDIFMT_DXT5:
        {
            UINT Pitch = (w + 3) / 4; /* <- pitch size in blocks */
            Pitch *= 16;              /* <- pitch size in bytes */
            return Pitch;
        }
        default:
        {
            /* the default is just to calculate the pitch from bpp */
            UINT bpp = vpoxWddmCalcBitsPerPixel(enmFormat);
            UINT Pitch = bpp * w;
            /* pitch is now in bits, translate in bytes */
            return VPOXWDDM_ROUNDBOUND(Pitch, 8) >> 3;
        }
    }
}

DECLINLINE(UINT) vpoxWddmCalcOffXrd(UINT w, D3DDDIFORMAT enmFormat)
{
    switch (enmFormat)
    {
        /* pitch for the DXT* (aka compressed) formats is the size in bytes of blocks that fill in an image width
         * i.e. each block decompressed into 4 x 4 pixels, so we have ((Width + 3) / 4) blocks for Width.
         * then each block has 64 bits (8 bytes) for DXT1 and 64+64 bits (16 bytes) for DXT2-DXT5, so.. : */
        case D3DDDIFMT_DXT1:
        {
            UINT Pitch = w / 4; /* <- pitch size in blocks */
            Pitch *= 8;         /* <- pitch size in bytes */
            return Pitch;
        }
        case D3DDDIFMT_DXT2:
        case D3DDDIFMT_DXT3:
        case D3DDDIFMT_DXT4:
        case D3DDDIFMT_DXT5:
        {
            UINT Pitch = w / 4; /* <- pitch size in blocks */
            Pitch *= 16;               /* <- pitch size in bytes */
            return Pitch;
        }
        default:
        {
            /* the default is just to calculate the pitch from bpp */
            UINT bpp = vpoxWddmCalcBitsPerPixel(enmFormat);
            UINT Pitch = bpp * w;
            /* pitch is now in bits, translate in bytes */
            return Pitch >> 3;
        }
    }
}

DECLINLINE(UINT) vpoxWddmCalcHightPacking(D3DDDIFORMAT enmFormat)
{
    switch (enmFormat)
    {
        /* for the DXT* (aka compressed) formats each block is decompressed into 4 x 4 pixels,
         * so packing is 4
         */
        case D3DDDIFMT_DXT1:
        case D3DDDIFMT_DXT2:
        case D3DDDIFMT_DXT3:
        case D3DDDIFMT_DXT4:
        case D3DDDIFMT_DXT5:
            return 4;
        default:
            return 1;
    }
}

DECLINLINE(UINT) vpoxWddmCalcOffYru(UINT height, D3DDDIFORMAT enmFormat)
{
    UINT packing = vpoxWddmCalcHightPacking(enmFormat);
    /* round it up */
    return (height + packing - 1) / packing;
}

DECLINLINE(UINT) vpoxWddmCalcOffYrd(UINT height, D3DDDIFORMAT enmFormat)
{
    UINT packing = vpoxWddmCalcHightPacking(enmFormat);
    /* round it up */
    return height / packing;
}

DECLINLINE(UINT) vpoxWddmCalcPitch(UINT w, D3DDDIFORMAT enmFormat)
{
    return vpoxWddmCalcOffXru(w, enmFormat);
}

DECLINLINE(UINT) vpoxWddmCalcWidthForPitch(UINT Pitch, D3DDDIFORMAT enmFormat)
{
    switch (enmFormat)
    {
        /* pitch for the DXT* (aka compressed) formats is the size in bytes of blocks that fill in an image width
         * i.e. each block decompressed into 4 x 4 pixels, so we have ((Width + 3) / 4) blocks for Width.
         * then each block has 64 bits (8 bytes) for DXT1 and 64+64 bits (16 bytes) for DXT2-DXT5, so.. : */
        case D3DDDIFMT_DXT1:
        {
            return (Pitch / 8) * 4;
        }
        case D3DDDIFMT_DXT2:
        case D3DDDIFMT_DXT3:
        case D3DDDIFMT_DXT4:
        case D3DDDIFMT_DXT5:
        {
            return (Pitch / 16) * 4;;
        }
        default:
        {
            /* the default is just to calculate it from bpp */
            UINT bpp = vpoxWddmCalcBitsPerPixel(enmFormat);
            return (Pitch << 3) / bpp;
        }
    }
}

DECLINLINE(UINT) vpoxWddmCalcNumRows(UINT top, UINT bottom, D3DDDIFORMAT enmFormat)
{
    Assert(bottom > top);
    top = top ? vpoxWddmCalcOffYrd(top, enmFormat) : 0; /* <- just to optimize it a bit */
    bottom = vpoxWddmCalcOffYru(bottom, enmFormat);
    return bottom - top;
}

DECLINLINE(UINT) vpoxWddmCalcRowSize(UINT left, UINT right, D3DDDIFORMAT enmFormat)
{
    Assert(right > left);
    left = left ? vpoxWddmCalcOffXrd(left, enmFormat) : 0; /* <- just to optimize it a bit */
    right = vpoxWddmCalcOffXru(right, enmFormat);
    return right - left;
}

DECLINLINE(UINT) vpoxWddmCalcSize(UINT pitch, UINT height, D3DDDIFORMAT enmFormat)
{
    UINT cRows = vpoxWddmCalcNumRows(0, height, enmFormat);
    return pitch * cRows;
}

DECLINLINE(UINT) vpoxWddmCalcOffXYrd(UINT x, UINT y, UINT pitch, D3DDDIFORMAT enmFormat)
{
    UINT offY = 0;
    if (y)
        offY = vpoxWddmCalcSize(pitch, y, enmFormat);

    return offY + vpoxWddmCalcOffXrd(x, enmFormat);
}

#define VPOXWDDM_ARRAY_MAXELEMENTSU32(_t) ((uint32_t)((UINT32_MAX) / sizeof (_t)))
#define VPOXWDDM_TRAILARRAY_MAXELEMENTSU32(_t, _af) ((uint32_t)(((~(0UL)) - (uint32_t)RT_OFFSETOF(_t, _af[0])) / RT_SIZEOFMEMB(_t, _af[0])))

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_common_wddm_VPoxMPIf_h */
