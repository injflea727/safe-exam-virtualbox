/* $Id: VPoxDispD3DIf.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3DIf_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3DIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VPOX_WITH_MESA3D
#include "gallium/VPoxGallium.h"
#endif

/* D3D headers */
#include <iprt/critsect.h>
#include <iprt/semaphore.h>
#include <iprt/win/d3d9.h>
#include <d3dumddi.h>
#include "../../common/wddm/VPoxMPIf.h"

typedef struct VPOXWDDMDISP_FORMATS
{
    uint32_t cFormatOps;
    const struct _FORMATOP* paFormatOps;
    uint32_t cSurfDescs;
    struct _DDSURFACEDESC *paSurfDescs;
} VPOXWDDMDISP_FORMATS, *PVPOXWDDMDISP_FORMATS;

typedef struct VPOXWDDMDISP_D3D *PVPOXWDDMDISP_D3D;
typedef void FNVPOXDISPD3DBACKENDCLOSE(PVPOXWDDMDISP_D3D pD3D);
typedef FNVPOXDISPD3DBACKENDCLOSE *PFNVPOXDISPD3DBACKENDCLOSE;

typedef struct VPOXWDDMDISP_D3D
{
    PFNVPOXDISPD3DBACKENDCLOSE pfnD3DBackendClose;

    D3DCAPS9 Caps;
    UINT cMaxSimRTs;

#ifdef VPOX_WITH_MESA3D
    /* Gallium backend. */
    IGalliumStack *pGalliumStack;
#endif
} VPOXWDDMDISP_D3D;

void VPoxDispD3DGlobalInit(void);
void VPoxDispD3DGlobalTerm(void);
HRESULT VPoxDispD3DGlobalOpen(PVPOXWDDMDISP_D3D pD3D, PVPOXWDDMDISP_FORMATS pFormats, VPOXWDDM_QAI const *pAdapterInfo);
void VPoxDispD3DGlobalClose(PVPOXWDDMDISP_D3D pD3D, PVPOXWDDMDISP_FORMATS pFormats);

#ifdef VPOX_WITH_VIDEOHWACCEL
HRESULT VPoxDispD3DGlobal2DFormatsInit(struct VPOXWDDMDISP_ADAPTER *pAdapter);
void VPoxDispD3DGlobal2DFormatsTerm(struct VPOXWDDMDISP_ADAPTER *pAdapter);
#endif

#ifdef DEBUG
void vpoxDispCheckCapsLevel(const D3DCAPS9 *pCaps);
#endif

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3DIf_h */
