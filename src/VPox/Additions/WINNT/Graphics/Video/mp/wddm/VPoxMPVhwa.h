/* $Id: VPoxMPVhwa.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVhwa_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVhwa_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

#include "VPoxMPShgsmi.h"

VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST * vpoxVhwaCommandCreate(PVPOXMP_DEVEXT pDevExt,
                                                               D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId,
                                                               VPOXVHWACMD_TYPE enmCmd,
                                                               VPOXVHWACMD_LENGTH cbCmd);

void vpoxVhwaCommandFree(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);
int  vpoxVhwaCommandSubmit(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST * pCmd);
void vpoxVhwaCommandSubmitAsynchAndComplete(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);

typedef DECLCALLBACK(void) FNVPOXVHWACMDCOMPLETION(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST * pCmd,
                                                   void *pvContext);
typedef FNVPOXVHWACMDCOMPLETION *PFNVPOXVHWACMDCOMPLETION;

void vpoxVhwaCommandSubmitAsynch(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd,
                                 PFNVPOXVHWACMDCOMPLETION pfnCompletion, void * pContext);
void vpoxVhwaCommandSubmitAsynchByEvent(PVPOXMP_DEVEXT pDevExt, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, RTSEMEVENT hEvent);

#define VPOXVHWA_CMD2LISTENTRY(_pCmd)   ((PVPOXVTLIST_ENTRY)&(_pCmd)->u.pNext)
#define VPOXVHWA_LISTENTRY2CMD(_pEntry) ( (VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *)((uint8_t *)(_pEntry) - RT_UOFFSETOF(VPOXVHWACMD, u.pNext)) )

DECLINLINE(void) vpoxVhwaPutList(VPOXVTLIST *pList, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    vpoxVtListPut(pList, VPOXVHWA_CMD2LISTENTRY(pCmd), VPOXVHWA_CMD2LISTENTRY(pCmd));
}

void vpoxVhwaCompletionListProcess(PVPOXMP_DEVEXT pDevExt, VPOXVTLIST *pList);

int vpoxVhwaEnable(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vpoxVhwaDisable(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
void vpoxVhwaInit(PVPOXMP_DEVEXT pDevExt);
void vpoxVhwaFree(PVPOXMP_DEVEXT pDevExt);

int vpoxVhwaHlpOverlayFlip(PVPOXWDDM_OVERLAY pOverlay, const DXGKARG_FLIPOVERLAY *pFlipInfo);
int vpoxVhwaHlpOverlayUpdate(PVPOXWDDM_OVERLAY pOverlay, const DXGK_OVERLAYINFO *pOverlayInfo);
int vpoxVhwaHlpOverlayDestroy(PVPOXWDDM_OVERLAY pOverlay);
int vpoxVhwaHlpOverlayCreate(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, DXGK_OVERLAYINFO *pOverlayInfo, /* OUT */ PVPOXWDDM_OVERLAY pOverlay);

int vpoxVhwaHlpGetSurfInfo(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pSurf);

BOOLEAN vpoxVhwaHlpOverlayListIsEmpty(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId);
void vpoxVhwaHlpOverlayDstRectUnion(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RECT *pRect);
void vpoxVhwaHlpOverlayDstRectGet(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_OVERLAY pOverlay, RECT *pRect);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVhwa_h */
