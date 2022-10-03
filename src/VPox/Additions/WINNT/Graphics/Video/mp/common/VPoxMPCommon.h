/* $Id: VPoxMPCommon.h $ */
/** @file
 * VPox Miniport common functions used by XPDM/WDDM drivers
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_common_VPoxMPCommon_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_common_VPoxMPCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxMPDevExt.h"

RT_C_DECLS_BEGIN

int VPoxMPCmnMapAdapterMemory(PVPOXMP_COMMON pCommon, void **ppv, uint32_t ulOffset, uint32_t ulSize);
void VPoxMPCmnUnmapAdapterMemory(PVPOXMP_COMMON pCommon, void **ppv);

typedef bool(*PFNVIDEOIRQSYNC)(void *);
bool VPoxMPCmnSyncToVideoIRQ(PVPOXMP_COMMON pCommon, PFNVIDEOIRQSYNC pfnSync, void *pvUser);

/* Video modes related */
#ifdef VPOX_XPDM_MINIPORT
void VPoxMPCmnInitCustomVideoModes(PVPOXMP_DEVEXT pExt);
VIDEO_MODE_INFORMATION* VPoxMPCmnGetCustomVideoModeInfo(ULONG ulIndex);
VIDEO_MODE_INFORMATION* VPoxMPCmnGetVideoModeInfo(PVPOXMP_DEVEXT pExt, ULONG ulIndex);
VIDEO_MODE_INFORMATION* VPoxMPXpdmCurrentVideoMode(PVPOXMP_DEVEXT pExt);
ULONG VPoxMPXpdmGetVideoModesCount(PVPOXMP_DEVEXT pExt);
void VPoxMPXpdmBuildVideoModesTable(PVPOXMP_DEVEXT pExt);
#endif

/* Registry access */
#ifdef VPOX_XPDM_MINIPORT
typedef PVPOXMP_DEVEXT VPOXMPCMNREGISTRY;
#else
typedef HANDLE VPOXMPCMNREGISTRY;
#endif

VP_STATUS VPoxMPCmnRegInit(IN PVPOXMP_DEVEXT pExt, OUT VPOXMPCMNREGISTRY *pReg);
VP_STATUS VPoxMPCmnRegFini(IN VPOXMPCMNREGISTRY Reg);
VP_STATUS VPoxMPCmnRegSetDword(IN VPOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t Val);
VP_STATUS VPoxMPCmnRegQueryDword(IN VPOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t *pVal);

/* Pointer related */
bool VPoxMPCmnUpdatePointerShape(PVPOXMP_COMMON pCommon, PVIDEO_POINTER_ATTRIBUTES pAttrs, uint32_t cbLength);

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_common_VPoxMPCommon_h */
