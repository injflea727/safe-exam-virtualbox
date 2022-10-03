/* $Id: VPoxDispMini.h $ */
/** @file
 * VPox XPDM Display driver, helper functions which interacts with our miniport driver
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispMini_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispMini_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxDisp.h"

int VPoxDispMPGetVideoModes(HANDLE hDriver, PVIDEO_MODE_INFORMATION *ppModesTable, ULONG *cModes);
int VPoxDispMPGetPointerCaps(HANDLE hDriver, PVIDEO_POINTER_CAPABILITIES pCaps);
int VPoxDispMPSetCurrentMode(HANDLE hDriver, ULONG ulMode);
int VPoxDispMPMapMemory(PVPOXDISPDEV pDev, PVIDEO_MEMORY_INFORMATION pMemInfo);
int VPoxDispMPUnmapMemory(PVPOXDISPDEV pDev);
int VPoxDispMPQueryHGSMIInfo(HANDLE hDriver, QUERYHGSMIRESULT *pInfo);
int VPoxDispMPQueryHGSMICallbacks(HANDLE hDriver, HGSMIQUERYCALLBACKS *pCallbacks);
int VPoxDispMPHGSMIQueryPortProcs(HANDLE hDriver, HGSMIQUERYCPORTPROCS *pPortProcs);
#ifdef VPOX_WITH_VIDEOHWACCEL
int VPoxDispMPVHWAQueryInfo(HANDLE hDriver, VHWAQUERYINFO *pInfo);
#endif
int VPoxDispMPSetColorRegisters(HANDLE hDriver, PVIDEO_CLUT pClut, DWORD cbClut);
int VPoxDispMPDisablePointer(HANDLE hDriver);
int VPoxDispMPSetPointerPosition(HANDLE hDriver, PVIDEO_POINTER_POSITION pPos);
int VPoxDispMPSetPointerAttrs(PVPOXDISPDEV pDev);
int VPoxDispMPSetVisibleRegion(HANDLE hDriver, PRTRECT pRects, DWORD cRects);
int VPoxDispMPResetDevice(HANDLE hDriver);
int VPoxDispMPShareVideoMemory(HANDLE hDriver, PVIDEO_SHARE_MEMORY pSMem, PVIDEO_SHARE_MEMORY_INFORMATION pSMemInfo);
int VPoxDispMPUnshareVideoMemory(HANDLE hDriver, PVIDEO_SHARE_MEMORY pSMem);
int VPoxDispMPQueryRegistryFlags(HANDLE hDriver, ULONG *pulFlags);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispMini_h */
