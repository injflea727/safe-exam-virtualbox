/* $Id: VPoxMPLegacy.h $ */
/** @file
 * VirtualPox Windows Guest Mesa3D - Gallium driver interface for WDDM kernel mode driver.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPLegacy_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPLegacy_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "common/VPoxMPDevExt.h"

NTSTATUS APIENTRY DxgkDdiBuildPagingBufferLegacy(CONST HANDLE hAdapter,
                                                 DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer);
NTSTATUS APIENTRY DxgkDdiPresentLegacy(CONST HANDLE hContext,
                                       DXGKARG_PRESENT *pPresent);
NTSTATUS APIENTRY DxgkDdiRenderLegacy(CONST HANDLE hContext,
                                      DXGKARG_RENDER *pRender);
NTSTATUS APIENTRY DxgkDdiPatchLegacy(CONST HANDLE hAdapter,
                                     CONST DXGKARG_PATCH *pPatch);
NTSTATUS APIENTRY DxgkDdiSubmitCommandLegacy(CONST HANDLE hAdapter,
                                             CONST DXGKARG_SUBMITCOMMAND *pSubmitCommand);
NTSTATUS APIENTRY DxgkDdiPreemptCommandLegacy(CONST HANDLE hAdapter,
                                              CONST DXGKARG_PREEMPTCOMMAND *pPreemptCommand);
NTSTATUS APIENTRY DxgkDdiQueryCurrentFenceLegacy(CONST HANDLE hAdapter,
                                                 DXGKARG_QUERYCURRENTFENCE *pCurrentFence);
BOOLEAN DxgkDdiInterruptRoutineLegacy(CONST PVOID MiniportDeviceContext,
                                      ULONG MessageNumber);
VOID DxgkDdiDpcRoutineLegacy(CONST PVOID MiniportDeviceContext);

VOID vpoxVdmaDdiNodesInit(PVPOXMP_DEVEXT pDevExt);

NTSTATUS vpoxVdmaGgDmaBltPerform(PVPOXMP_DEVEXT pDevExt, struct VPOXWDDM_ALLOC_DATA *pSrcAlloc, RECT *pSrcRect,
                                 struct VPOXWDDM_ALLOC_DATA *pDstAlloc, RECT *pDstRect);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPLegacy_h */
