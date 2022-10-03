/* $Id: VPoxMPVModes.h $ */
/** @file
 * VPox WDDM Miniport driver
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVModes_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVModes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

//#include "../../common/VPoxVideoTools.h"

#include "VPoxMPSa.h"

#define _CR_TYPECAST(_Type, _pVal) ((_Type*)((void*)(_pVal)))

DECLINLINE(uint64_t) vpoxRSize2U64(RTRECTSIZE size) { return *_CR_TYPECAST(uint64_t, &(size)); }
DECLINLINE(RTRECTSIZE) vpoxU642RSize2(uint64_t size) { return *_CR_TYPECAST(RTRECTSIZE, &(size)); }

#define CR_RSIZE2U64 vpoxRSize2U64
#define CR_U642RSIZE vpoxU642RSize2

int VPoxWddmVModesInit(PVPOXMP_DEVEXT pExt);
void VPoxWddmVModesCleanup();
const CR_SORTARRAY* VPoxWddmVModesGet(PVPOXMP_DEVEXT pExt, uint32_t u32Target);
int VPoxWddmVModesRemove(PVPOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution);
int VPoxWddmVModesAdd(PVPOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution, BOOLEAN fTrancient);

NTSTATUS VPoxWddmChildStatusReportReconnected(PVPOXMP_DEVEXT pDevExt, uint32_t iChild);
NTSTATUS VPoxWddmChildStatusConnect(PVPOXMP_DEVEXT pDevExt, uint32_t iChild, BOOLEAN fConnect);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVModes_h */
