/* $Id: VPoxD3DIf.h $ */
/** @file
 * VPoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxD3DIf_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxD3DIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxDispD3DCmn.h"

D3DFORMAT vpoxDDI2D3DFormat(D3DDDIFORMAT format);
D3DMULTISAMPLE_TYPE vpoxDDI2D3DMultiSampleType(D3DDDIMULTISAMPLE_TYPE enmType);
D3DPOOL vpoxDDI2D3DPool(D3DDDI_POOL enmPool);
D3DRENDERSTATETYPE vpoxDDI2D3DRenderStateType(D3DDDIRENDERSTATETYPE enmType);
VPOXWDDMDISP_TSS_LOOKUP vpoxDDI2D3DTestureStageStateType(D3DDDITEXTURESTAGESTATETYPE enmType);
DWORD vpoxDDI2D3DUsage(D3DDDI_RESOURCEFLAGS fFlags);
DWORD vpoxDDI2D3DLockFlags(D3DDDI_LOCKFLAGS fLockFlags);
D3DTEXTUREFILTERTYPE vpoxDDI2D3DBltFlags(D3DDDI_BLTFLAGS fFlags);
D3DQUERYTYPE vpoxDDI2D3DQueryType(D3DDDIQUERYTYPE enmType);
DWORD vpoxDDI2D3DIssueQueryFlags(D3DDDI_ISSUEQUERYFLAGS Flags);

HRESULT VPoxD3DIfCreateForRc(struct VPOXWDDMDISP_RESOURCE *pRc);
HRESULT VPoxD3DIfLockRect(struct VPOXWDDMDISP_RESOURCE *pRc, UINT iAlloc,
        D3DLOCKED_RECT * pLockedRect,
        CONST RECT *pRect,
        DWORD fLockFlags);
HRESULT VPoxD3DIfUnlockRect(struct VPOXWDDMDISP_RESOURCE *pRc, UINT iAlloc);
void VPoxD3DIfLockUnlockMemSynch(struct VPOXWDDMDISP_ALLOCATION *pAlloc, D3DLOCKED_RECT *pLockInfo, RECT *pRect, bool bToLockInfo);

IUnknown* vpoxD3DIfCreateSharedPrimary(PVPOXWDDMDISP_ALLOCATION pAlloc);


/* NOTE: does NOT increment a ref counter! NO Release needed!! */
DECLINLINE(IUnknown*) vpoxD3DIfGet(PVPOXWDDMDISP_ALLOCATION pAlloc)
{
    if (pAlloc->pD3DIf)
        return pAlloc->pD3DIf;

    if (pAlloc->enmType != VPOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
    {
        WARN(("dynamic creation is supported for VPOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE only!, current type is %d", pAlloc->enmType));
        return NULL;
    }

    Assert(pAlloc->pRc->pDevice->pfnCreateSharedPrimary);
    return pAlloc->pRc->pDevice->pfnCreateSharedPrimary(pAlloc);
}

/* on success increments the surface ref counter,
 * i.e. one must call pSurf->Release() once the surface is not needed*/
DECLINLINE(HRESULT) VPoxD3DIfSurfGet(PVPOXWDDMDISP_RESOURCE pRc, UINT iAlloc, IDirect3DSurface9 **ppSurf)
{
    HRESULT hr = S_OK;
    Assert(pRc->cAllocations > iAlloc);
    *ppSurf = NULL;
    IUnknown* pD3DIf = vpoxD3DIfGet(&pRc->aAllocations[iAlloc]);

    switch (pRc->aAllocations[0].enmD3DIfType)
    {
        case VPOXDISP_D3DIFTYPE_SURFACE:
        {
            IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pD3DIf;
            Assert(pD3DIfSurf);
            pD3DIfSurf->AddRef();
            *ppSurf = pD3DIfSurf;
            break;
        }
        case VPOXDISP_D3DIFTYPE_TEXTURE:
        {
            /* @todo VPoxD3DIfSurfGet is typically used in Blt & ColorFill functions
             * in this case, if texture is used as a destination,
             * we should update sub-layers as well which is not done currently. */
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pD3DIf;
            IDirect3DSurface9 *pSurfaceLevel;
            Assert(pD3DIfTex);
            hr = pD3DIfTex->GetSurfaceLevel(iAlloc, &pSurfaceLevel);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                *ppSurf = pSurfaceLevel;
            }
            break;
        }
        case VPOXDISP_D3DIFTYPE_CUBE_TEXTURE:
        {
            IDirect3DCubeTexture9 *pD3DIfCubeTex = (IDirect3DCubeTexture9*)pD3DIf;
            IDirect3DSurface9 *pSurfaceLevel;
            Assert(pD3DIfCubeTex);
            hr = pD3DIfCubeTex->GetCubeMapSurface(VPOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, iAlloc),
                                                  VPOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, iAlloc), &pSurfaceLevel);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                *ppSurf = pSurfaceLevel;
            }
            break;
        }
        default:
        {
            WARN(("unexpected enmD3DIfType %d", pRc->aAllocations[0].enmD3DIfType));
            hr = E_FAIL;
            break;
        }
    }
    return hr;
}

VOID VPoxD3DIfFillPresentParams(D3DPRESENT_PARAMETERS *pParams, PVPOXWDDMDISP_RESOURCE pRc, UINT cRTs);
HRESULT VPoxD3DIfDeviceCreateDummy(PVPOXWDDMDISP_DEVICE pDevice);

DECLINLINE(IDirect3DDevice9*) VPoxD3DIfDeviceGet(PVPOXWDDMDISP_DEVICE pDevice)
{
    if (pDevice->pDevice9If)
        return pDevice->pDevice9If;

#ifdef VPOXWDDMDISP_DEBUG
    g_VPoxVDbgInternalDevice = pDevice;
#endif

    Assert(pDevice->pfnCreateDirect3DDevice);
    HRESULT hr = pDevice->pfnCreateDirect3DDevice(pDevice);
    Assert(hr == S_OK); NOREF(hr);
    Assert(pDevice->pDevice9If);
    return pDevice->pDevice9If;
}

#define VPOXDISPMODE_IS_3D(_p) ((_p)->f3D)
#define VPOXDISP_D3DEV(_p) VPoxD3DIfDeviceGet(_p)

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxD3DIf_h */
