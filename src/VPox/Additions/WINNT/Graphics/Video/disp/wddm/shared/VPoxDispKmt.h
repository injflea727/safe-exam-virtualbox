/* $Id: VPoxDispKmt.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_shared_VPoxDispKmt_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_shared_VPoxDispKmt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <D3dkmthk.h>

#include "../../../common/wddm/VPoxMPIf.h"

/* win8 release preview-specific stuff */
typedef struct _D3DKMT_ADAPTERINFO
{
  D3DKMT_HANDLE hAdapter;
  LUID          AdapterLuid;
  ULONG         NumOfSources;
  BOOL          bPresentMoveRegionsPreferred;
} D3DKMT_ADAPTERINFO;

#define MAX_ENUM_ADAPTERS 16

typedef struct _D3DKMT_ENUMADAPTERS
{
  ULONG              NumAdapters;
  D3DKMT_ADAPTERINFO Adapters[MAX_ENUM_ADAPTERS];
} D3DKMT_ENUMADAPTERS;

typedef NTSTATUS (APIENTRY *PFND3DKMT_ENUMADAPTERS)(IN OUT D3DKMT_ENUMADAPTERS*);

typedef struct _D3DKMT_OPENADAPTERFROMLUID
{
  LUID          AdapterLuid;
  D3DKMT_HANDLE hAdapter;
} D3DKMT_OPENADAPTERFROMLUID;

typedef NTSTATUS (APIENTRY *PFND3DKMT_OPENADAPTERFROMLUID)(IN OUT D3DKMT_OPENADAPTERFROMLUID*);
/* END OF win8 release preview-specific stuff */

typedef enum
{
    VPOXDISPKMT_CALLBACKS_VERSION_UNDEFINED = 0,
    VPOXDISPKMT_CALLBACKS_VERSION_VISTA_WIN7,
    VPOXDISPKMT_CALLBACKS_VERSION_WIN8
} VPOXDISPKMT_CALLBACKS_VERSION;

typedef struct VPOXDISPKMT_CALLBACKS
{
    HMODULE hGdi32;
    VPOXDISPKMT_CALLBACKS_VERSION enmVersion;
    /* open adapter */
    PFND3DKMT_OPENADAPTERFROMHDC pfnD3DKMTOpenAdapterFromHdc;
    PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME pfnD3DKMTOpenAdapterFromGdiDisplayName;
    /* close adapter */
    PFND3DKMT_CLOSEADAPTER pfnD3DKMTCloseAdapter;
    /* escape */
    PFND3DKMT_ESCAPE pfnD3DKMTEscape;

    PFND3DKMT_QUERYADAPTERINFO pfnD3DKMTQueryAdapterInfo;

    PFND3DKMT_CREATEDEVICE pfnD3DKMTCreateDevice;
    PFND3DKMT_DESTROYDEVICE pfnD3DKMTDestroyDevice;
    PFND3DKMT_CREATECONTEXT pfnD3DKMTCreateContext;
    PFND3DKMT_DESTROYCONTEXT pfnD3DKMTDestroyContext;

    PFND3DKMT_RENDER pfnD3DKMTRender;

    PFND3DKMT_CREATEALLOCATION pfnD3DKMTCreateAllocation;
    PFND3DKMT_DESTROYALLOCATION pfnD3DKMTDestroyAllocation;

    PFND3DKMT_LOCK pfnD3DKMTLock;
    PFND3DKMT_UNLOCK pfnD3DKMTUnlock;

    /* auto resize support */
    PFND3DKMT_INVALIDATEACTIVEVIDPN pfnD3DKMTInvalidateActiveVidPn;
    PFND3DKMT_POLLDISPLAYCHILDREN pfnD3DKMTPollDisplayChildren;

    /* win8 specifics */
    PFND3DKMT_ENUMADAPTERS pfnD3DKMTEnumAdapters;
    PFND3DKMT_OPENADAPTERFROMLUID pfnD3DKMTOpenAdapterFromLuid;
} VPOXDISPKMT_CALLBACKS, *PVPOXDISPKMT_CALLBACKS;

typedef struct VPOXDISPKMT_ADAPTER
{
    D3DKMT_HANDLE hAdapter;
    HDC hDc;
    LUID Luid;
    const VPOXDISPKMT_CALLBACKS *pCallbacks;
}VPOXDISPKMT_ADAPTER, *PVPOXDISPKMT_ADAPTER;

typedef struct VPOXDISPKMT_DEVICE
{
    struct VPOXDISPKMT_ADAPTER *pAdapter;
    D3DKMT_HANDLE hDevice;
    VOID *pCommandBuffer;
    UINT CommandBufferSize;
    D3DDDI_ALLOCATIONLIST *pAllocationList;
    UINT AllocationListSize;
    D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
    UINT PatchLocationListSize;
}VPOXDISPKMT_DEVICE, *PVPOXDISPKMT_DEVICE;

typedef struct VPOXDISPKMT_CONTEXT
{
    struct VPOXDISPKMT_DEVICE *pDevice;
    D3DKMT_HANDLE hContext;
    VOID *pCommandBuffer;
    UINT CommandBufferSize;
    D3DDDI_ALLOCATIONLIST *pAllocationList;
    UINT AllocationListSize;
    D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
    UINT PatchLocationListSize;
} VPOXDISPKMT_CONTEXT, *PVPOXDISPKMT_CONTEXT;

HRESULT vpoxDispKmtCallbacksInit(PVPOXDISPKMT_CALLBACKS pCallbacks);
HRESULT vpoxDispKmtCallbacksTerm(PVPOXDISPKMT_CALLBACKS pCallbacks);

HRESULT vpoxDispKmtOpenAdapter(const VPOXDISPKMT_CALLBACKS *pCallbacks, PVPOXDISPKMT_ADAPTER pAdapter);
HRESULT vpoxDispKmtCloseAdapter(PVPOXDISPKMT_ADAPTER pAdapter);
HRESULT vpoxDispKmtCreateDevice(PVPOXDISPKMT_ADAPTER pAdapter, PVPOXDISPKMT_DEVICE pDevice);
HRESULT vpoxDispKmtDestroyDevice(PVPOXDISPKMT_DEVICE pDevice);
HRESULT vpoxDispKmtCreateContext(PVPOXDISPKMT_DEVICE pDevice, PVPOXDISPKMT_CONTEXT pContext,
        VPOXWDDM_CONTEXT_TYPE enmType,
        HANDLE hEvent, uint64_t u64UmInfo);
HRESULT vpoxDispKmtDestroyContext(PVPOXDISPKMT_CONTEXT pContext);


#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_shared_VPoxDispKmt_h */
