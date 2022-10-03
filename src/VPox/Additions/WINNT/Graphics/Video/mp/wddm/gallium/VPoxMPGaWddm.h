/* $Id: VPoxMPGaWddm.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VPoxMPGaWddm_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VPoxMPGaWddm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "common/VPoxMPDevExt.h"

NTSTATUS GaAdapterStart(PVPOXMP_DEVEXT pDevExt);
void GaAdapterStop(PVPOXMP_DEVEXT pDevExt);

NTSTATUS GaQueryInfo(PVPOXWDDM_EXT_GA pGaDevExt,
                     VPOXVIDEO_HWTYPE enmHwType,
                     VPOXGAHWINFO *pHWInfo);

NTSTATUS GaScreenDefine(PVPOXWDDM_EXT_GA pGaDevExt,
                        uint32_t u32Offset,
                        uint32_t u32ScreenId,
                        int32_t xOrigin,
                        int32_t yOrigin,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        bool fBlank);
NTSTATUS GaScreenDestroy(PVPOXWDDM_EXT_GA pGaDevExt,
                         uint32_t u32ScreenId);

NTSTATUS GaDeviceCreate(PVPOXWDDM_EXT_GA pGaDevExt,
                        PVPOXWDDM_DEVICE pDevice);
void GaDeviceDestroy(PVPOXWDDM_EXT_GA pGaDevExt,
                     PVPOXWDDM_DEVICE pDevice);

NTSTATUS GaContextCreate(PVPOXWDDM_EXT_GA pGaDevExt,
                         PVPOXWDDM_CREATECONTEXT_INFO pInfo,
                         PVPOXWDDM_CONTEXT pContext);
NTSTATUS GaContextDestroy(PVPOXWDDM_EXT_GA pGaDevExt,
                          PVPOXWDDM_CONTEXT pContext);
NTSTATUS GaUpdate(PVPOXWDDM_EXT_GA pGaDevExt,
                  uint32_t u32X,
                  uint32_t u32Y,
                  uint32_t u32Width,
                  uint32_t u32Height);

NTSTATUS GaDefineCursor(PVPOXWDDM_EXT_GA pGaDevExt,
                        uint32_t u32HotspotX,
                        uint32_t u32HotspotY,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        uint32_t u32AndMaskDepth,
                        uint32_t u32XorMaskDepth,
                        void const *pvAndMask,
                        uint32_t cbAndMask,
                        void const *pvXorMask,
                        uint32_t cbXorMask);

NTSTATUS GaDefineAlphaCursor(PVPOXWDDM_EXT_GA pGaDevExt,
                             uint32_t u32HotspotX,
                             uint32_t u32HotspotY,
                             uint32_t u32Width,
                             uint32_t u32Height,
                             void const *pvImage,
                             uint32_t cbImage);

NTSTATUS APIENTRY GaDxgkDdiBuildPagingBuffer(const HANDLE hAdapter,
                                             DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer);
NTSTATUS APIENTRY GaDxgkDdiPresentDisplayOnly(const HANDLE hAdapter,
                                              const DXGKARG_PRESENT_DISPLAYONLY *pPresentDisplayOnly);
NTSTATUS APIENTRY GaDxgkDdiPresent(const HANDLE hContext,
                                   DXGKARG_PRESENT *pPresent);
NTSTATUS APIENTRY GaDxgkDdiRender(const HANDLE hContext,
                                  DXGKARG_RENDER *pRender);
NTSTATUS APIENTRY GaDxgkDdiPatch(const HANDLE hAdapter,
                                 const DXGKARG_PATCH *pPatch);
NTSTATUS APIENTRY GaDxgkDdiSubmitCommand(const HANDLE hAdapter,
                                         const DXGKARG_SUBMITCOMMAND *pSubmitCommand);
NTSTATUS APIENTRY GaDxgkDdiPreemptCommand(const HANDLE hAdapter,
                                          const DXGKARG_PREEMPTCOMMAND *pPreemptCommand);
NTSTATUS APIENTRY GaDxgkDdiQueryCurrentFence(const HANDLE hAdapter,
                                             DXGKARG_QUERYCURRENTFENCE *pCurrentFence);
BOOLEAN GaDxgkDdiInterruptRoutine(const PVOID MiniportDeviceContext,
                                  ULONG MessageNumber);
VOID GaDxgkDdiDpcRoutine(const PVOID MiniportDeviceContext);
NTSTATUS APIENTRY GaDxgkDdiEscape(const HANDLE hAdapter,
                                  const DXGKARG_ESCAPE *pEscape);

DECLINLINE(bool) GaContextTypeIs(PVPOXWDDM_CONTEXT pContext, VPOXWDDM_CONTEXT_TYPE enmType)
{
    return (pContext && pContext->enmType == enmType);
}

DECLINLINE(bool) GaContextHwTypeIs(PVPOXWDDM_CONTEXT pContext, VPOXVIDEO_HWTYPE enmHwType)
{
    return (pContext && pContext->pDevice->pAdapter->enmHwType == enmHwType);
}

NTSTATUS GaVidPnSourceReport(PVPOXMP_DEVEXT pDevExt, VPOXWDDM_SOURCE *pSource);
NTSTATUS GaVidPnSourceCheckPos(PVPOXMP_DEVEXT pDevExt, UINT iSource);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VPoxMPGaWddm_h */
