/* $Id: VPoxDispD3D.h $ */
/** @file
 * VPoxVideo Display D3D User mode dll
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3D_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3D_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxDispD3DIf.h"
#include "../../common/wddm/VPoxMPIf.h"

#include <iprt/cdefs.h>
#include <iprt/list.h>

#define VPOXWDDMDISP_MAX_VERTEX_STREAMS 16
#define VPOXWDDMDISP_MAX_TEX_SAMPLERS 16
#define VPOXWDDMDISP_TOTAL_SAMPLERS VPOXWDDMDISP_MAX_TEX_SAMPLERS + 5
#define VPOXWDDMDISP_SAMPLER_IDX_IS_SPECIAL(_i) ((_i) >= D3DDMAPSAMPLER && (_i) <= D3DVERTEXTEXTURESAMPLER3)
#define VPOXWDDMDISP_SAMPLER_IDX_SPECIAL(_i) (VPOXWDDMDISP_SAMPLER_IDX_IS_SPECIAL(_i) ? (int)((_i) - D3DDMAPSAMPLER + VPOXWDDMDISP_MAX_TEX_SAMPLERS) : (int)-1)
#define VPOXWDDMDISP_SAMPLER_IDX(_i) (((_i) < VPOXWDDMDISP_MAX_TEX_SAMPLERS) ? (int)(_i) : VPOXWDDMDISP_SAMPLER_IDX_SPECIAL(_i))


/* maximum number of direct render targets to be used before
 * switching to offscreen rendering */
#ifdef VPOXWDDMDISP_DEBUG
# define VPOXWDDMDISP_MAX_DIRECT_RTS      g_VPoxVDbgCfgMaxDirectRts
#else
# define VPOXWDDMDISP_MAX_DIRECT_RTS      3
#endif

#define VPOXWDDMDISP_IS_TEXTURE(_f) ((_f).Texture || (_f).Value == 0)

#ifdef VPOX_WITH_VIDEOHWACCEL
typedef struct VPOXDISPVHWA_INFO
{
    VPOXVHWA_INFO Settings;
}VPOXDISPVHWA_INFO;

/* represents settings secific to
 * display device (head) on the multiple-head graphics card
 * currently used for 2D (overlay) only since in theory its settings
 * can differ per each frontend's framebuffer. */
typedef struct VPOXWDDMDISP_HEAD
{
    VPOXDISPVHWA_INFO Vhwa;
} VPOXWDDMDISP_HEAD;
#endif

typedef struct VPOXWDDMDISP_ADAPTER
{
    HANDLE hAdapter;
    UINT uIfVersion;
    UINT uRtVersion;
    D3DDDI_ADAPTERCALLBACKS RtCallbacks;

    VPOXVIDEO_HWTYPE enmHwType;     /* VPOXVIDEO_HWTYPE_* */

    VPOXWDDMDISP_D3D D3D;
    VPOXWDDMDISP_FORMATS Formats;
    uint32_t u32VPox3DCaps;
    bool f3D;
    bool fReserved[3];

    VPOXWDDM_QAI AdapterInfo;

#ifdef VPOX_WITH_VIDEOHWACCEL
    uint32_t cHeads;
    VPOXWDDMDISP_HEAD aHeads[1];
#endif
} VPOXWDDMDISP_ADAPTER, *PVPOXWDDMDISP_ADAPTER;

typedef struct VPOXWDDMDISP_CONTEXT
{
    RTLISTNODE ListNode;
    struct VPOXWDDMDISP_DEVICE *pDevice;
    D3DDDICB_CREATECONTEXT ContextInfo;
} VPOXWDDMDISP_CONTEXT, *PVPOXWDDMDISP_CONTEXT;

typedef struct VPOXWDDMDISP_STREAMSOURCEUM
{
    CONST VOID* pvBuffer;
    UINT cbStride;
} VPOXWDDMDISP_STREAMSOURCEUM, *PVPOXWDDMDISP_STREAMSOURCEUM;

typedef struct VPOXWDDMDISP_INDICIESUM
{
    CONST VOID* pvBuffer;
    UINT cbSize;
} VPOXWDDMDISP_INDICIESUM, *PVPOXWDDMDISP_INDICIESUM;

struct VPOXWDDMDISP_ALLOCATION;

typedef struct VPOXWDDMDISP_STREAM_SOURCE_INFO
{
  UINT   uiOffset;
  UINT   uiStride;
} VPOXWDDMDISP_STREAM_SOURCE_INFO;

typedef struct VPOXWDDMDISP_INDICES_INFO
{
    struct VPOXWDDMDISP_ALLOCATION *pIndicesAlloc;
    const void *pvIndicesUm;
    UINT uiStride;
} VPOXWDDMDISP_INDICES_INFO;

typedef struct VPOXWDDMDISP_RENDERTGT_FLAGS
{
    union
    {
        struct
        {
            UINT bAdded : 1;
            UINT bRemoved : 1;
            UINT Reserved : 30;
        };
        uint32_t Value;
    };
}VPOXWDDMDISP_RENDERTGT_FLAGS;

typedef struct VPOXWDDMDISP_RENDERTGT
{
    struct VPOXWDDMDISP_ALLOCATION *pAlloc;
    UINT cNumFlips;
    VPOXWDDMDISP_RENDERTGT_FLAGS fFlags;
} VPOXWDDMDISP_RENDERTGT, *PVPOXWDDMDISP_RENDERTGT;

typedef struct VPOXWDDMDISP_DEVICE *PVPOXWDDMDISP_DEVICE;
typedef HRESULT FNVPOXWDDMCREATEDIRECT3DDEVICE(PVPOXWDDMDISP_DEVICE pDevice);
typedef FNVPOXWDDMCREATEDIRECT3DDEVICE *PFNVPOXWDDMCREATEDIRECT3DDEVICE;

typedef IUnknown* FNVPOXWDDMCREATESHAREDPRIMARY(struct VPOXWDDMDISP_ALLOCATION *pAlloc);
typedef FNVPOXWDDMCREATESHAREDPRIMARY *PFNVPOXWDDMCREATESHAREDPRIMARY;

typedef struct VPOXWDDMDISP_DEVICE
{
    HANDLE hDevice;
    PVPOXWDDMDISP_ADAPTER pAdapter;
    PFNVPOXWDDMCREATEDIRECT3DDEVICE pfnCreateDirect3DDevice;
    PFNVPOXWDDMCREATESHAREDPRIMARY pfnCreateSharedPrimary;
    IDirect3DDevice9 *pDevice9If;
    UINT u32IfVersion;
    UINT uRtVersion;
    D3DDDI_DEVICECALLBACKS RtCallbacks;
    VOID *pvCmdBuffer;
    UINT cbCmdBuffer;
    D3DDDI_CREATEDEVICEFLAGS fFlags;
    /* number of StreamSources set */
    UINT cStreamSources;
    UINT cStreamSourcesUm;
    VPOXWDDMDISP_STREAMSOURCEUM aStreamSourceUm[VPOXWDDMDISP_MAX_VERTEX_STREAMS];
    struct VPOXWDDMDISP_ALLOCATION *aStreamSource[VPOXWDDMDISP_MAX_VERTEX_STREAMS];
    VPOXWDDMDISP_STREAM_SOURCE_INFO StreamSourceInfo[VPOXWDDMDISP_MAX_VERTEX_STREAMS];
    VPOXWDDMDISP_INDICES_INFO IndiciesInfo;
    /* Need to cache the ViewPort data because IDirect3DDevice9::SetViewport
     * is split into two calls: SetViewport & SetZRange.
     * Also the viewport must be restored after IDirect3DDevice9::SetRenderTarget.
     */
    D3DVIEWPORT9 ViewPort;
    /* The scissor rectangle must be restored after IDirect3DDevice9::SetRenderTarget. */
    RECT ScissorRect;
    /* Whether the ViewPort field is valid, i.e. GaDdiSetViewport has been called. */
    bool fViewPort : 1;
    /* Whether the ScissorRect field is valid, i.e. GaDdiSetScissorRect has been called. */
    bool fScissorRect : 1;
    VPOXWDDMDISP_CONTEXT DefaultContext;

    /* no lock is needed for this since we're guaranteed the per-device calls are not reentrant */
    RTLISTANCHOR DirtyAllocList;

    UINT cSamplerTextures;
    struct VPOXWDDMDISP_RESOURCE *aSamplerTextures[VPOXWDDMDISP_TOTAL_SAMPLERS];

    struct VPOXWDDMDISP_RESOURCE *pDepthStencilRc;

    HMODULE hHgsmiTransportModule;

    UINT cRTs;
    struct VPOXWDDMDISP_ALLOCATION * apRTs[1];
} VPOXWDDMDISP_DEVICE, *PVPOXWDDMDISP_DEVICE;

typedef struct VPOXWDDMDISP_LOCKINFO
{
    uint32_t cLocks;
    union {
        D3DDDIRANGE  Range;
        RECT  Area;
        D3DDDIBOX  Box;
    };
    D3DDDI_LOCKFLAGS fFlags;
    union {
        D3DLOCKED_RECT LockedRect;
        D3DLOCKED_BOX LockedBox;
    };
#ifdef VPOXWDDMDISP_DEBUG
    PVOID pvData;
#endif
} VPOXWDDMDISP_LOCKINFO;

typedef enum
{
    VPOXDISP_D3DIFTYPE_UNDEFINED = 0,
    VPOXDISP_D3DIFTYPE_SURFACE,
    VPOXDISP_D3DIFTYPE_TEXTURE,
    VPOXDISP_D3DIFTYPE_CUBE_TEXTURE,
    VPOXDISP_D3DIFTYPE_VOLUME_TEXTURE,
    VPOXDISP_D3DIFTYPE_VERTEXBUFFER,
    VPOXDISP_D3DIFTYPE_INDEXBUFFER
} VPOXDISP_D3DIFTYPE;

typedef struct VPOXWDDMDISP_ALLOCATION
{
    D3DKMT_HANDLE hAllocation;
    VPOXWDDM_ALLOC_TYPE enmType;
    UINT iAlloc;
    struct VPOXWDDMDISP_RESOURCE *pRc;
    void* pvMem;
    /* object type is defined by enmD3DIfType enum */
    IUnknown *pD3DIf;
    VPOXDISP_D3DIFTYPE enmD3DIfType;
    /* list entry used to add allocation to the dirty alloc list */
    RTLISTNODE DirtyAllocListEntry;
    BOOLEAN fEverWritten;
    BOOLEAN fDirtyWrite;
    BOOLEAN fAllocLocked;
    HANDLE hSharedHandle;
    VPOXWDDMDISP_LOCKINFO LockInfo;
    VPOXWDDM_DIRTYREGION DirtyRegion; /* <- dirty region to notify host about */
    VPOXWDDM_SURFACE_DESC SurfDesc;
#ifdef VPOX_WITH_MESA3D
    uint32_t hostID;
#endif
} VPOXWDDMDISP_ALLOCATION, *PVPOXWDDMDISP_ALLOCATION;

typedef struct VPOXWDDMDISP_RESOURCE
{
    HANDLE hResource;
    D3DKMT_HANDLE hKMResource;
    PVPOXWDDMDISP_DEVICE pDevice;
    VPOXWDDMDISP_RESOURCE_FLAGS fFlags;
    VPOXWDDM_RC_DESC RcDesc;
    UINT cAllocations;
    VPOXWDDMDISP_ALLOCATION aAllocations[1];
} VPOXWDDMDISP_RESOURCE, *PVPOXWDDMDISP_RESOURCE;

typedef struct VPOXWDDMDISP_QUERY
{
    D3DDDIQUERYTYPE enmType;
    D3DDDI_ISSUEQUERYFLAGS fQueryState;
    IDirect3DQuery9 *pQueryIf;
} VPOXWDDMDISP_QUERY, *PVPOXWDDMDISP_QUERY;

typedef struct VPOXWDDMDISP_TSS_LOOKUP
{
    BOOL  bSamplerState;
    DWORD dType;
} VPOXWDDMDISP_TSS_LOOKUP;

typedef struct VPOXWDDMDISP_OVERLAY
{
    D3DKMT_HANDLE hOverlay;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    PVPOXWDDMDISP_RESOURCE *pResource;
} VPOXWDDMDISP_OVERLAY, *PVPOXWDDMDISP_OVERLAY;

#define VPOXDISP_CUBEMAP_LEVELS_COUNT(pRc) (((pRc)->cAllocations)/6)
#define VPOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, idx) ((D3DCUBEMAP_FACES)(D3DCUBEMAP_FACE_POSITIVE_X+(idx)/VPOXDISP_CUBEMAP_LEVELS_COUNT(pRc)))
#define VPOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, idx) ((idx)%VPOXDISP_CUBEMAP_LEVELS_COUNT(pRc))

void vpoxWddmResourceInit(PVPOXWDDMDISP_RESOURCE pRc, UINT cAllocs);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3D_h */
