/* $Id: GaDxva.h $ */
/** @file
 * VirtualPox WDDM DXVA for the Gallium based driver.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDxva_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDxva_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <d3dumddi.h>

typedef struct VPOXWDDMDISP_DEVICE *PVPOXWDDMDISP_DEVICE;

HRESULT VPoxDxvaGetDeviceGuidCount(UINT *pcGuids);
HRESULT VPoxDxvaGetDeviceGuids(GUID *paGuids, UINT cbGuids);

HRESULT VPoxDxvaGetOutputFormatCount(UINT *pcFormats, DXVADDI_VIDEOPROCESSORINPUT const *pVPI, bool fSubstream);
HRESULT VPoxDxvaGetOutputFormats(D3DDDIFORMAT *paFormats, UINT cbFormats, DXVADDI_VIDEOPROCESSORINPUT const *pVPI, bool fSubstream);

HRESULT VPoxDxvaGetCaps(DXVADDI_VIDEOPROCESSORCAPS *pVideoProcessorCaps, DXVADDI_VIDEOPROCESSORINPUT const *pVPI);

HRESULT VPoxDxvaCreateVideoProcessDevice(PVPOXWDDMDISP_DEVICE pDevice, D3DDDIARG_CREATEVIDEOPROCESSDEVICE *pData);
HRESULT VPoxDxvaDestroyVideoProcessDevice(PVPOXWDDMDISP_DEVICE pDevice, HANDLE hVideoProcessor);
HRESULT VPoxDxvaVideoProcessBeginFrame(PVPOXWDDMDISP_DEVICE pDevice, HANDLE hVideoProcessor);
HRESULT VPoxDxvaVideoProcessEndFrame(PVPOXWDDMDISP_DEVICE pDevice, D3DDDIARG_VIDEOPROCESSENDFRAME *pData);
HRESULT VPoxDxvaSetVideoProcessRenderTarget(PVPOXWDDMDISP_DEVICE pDevice, const D3DDDIARG_SETVIDEOPROCESSRENDERTARGET *pData);
HRESULT VPoxDxvaVideoProcessBlt(PVPOXWDDMDISP_DEVICE pDevice, const D3DDDIARG_VIDEOPROCESSBLT *pData);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDxva_h */
