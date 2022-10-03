/* $Id: VPoxDispDDraw.h $ */
/** @file
 * VPox XPDM Display driver, direct draw callbacks
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispDDraw_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispDDraw_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <winddi.h>

DWORD APIENTRY VPoxDispDDCanCreateSurface(PDD_CANCREATESURFACEDATA lpCanCreateSurface);
DWORD APIENTRY VPoxDispDDCreateSurface(PDD_CREATESURFACEDATA lpCreateSurface);
DWORD APIENTRY VPoxDispDDDestroySurface(PDD_DESTROYSURFACEDATA lpDestroySurface);
DWORD APIENTRY VPoxDispDDLock(PDD_LOCKDATA lpLock);
DWORD APIENTRY VPoxDispDDUnlock(PDD_UNLOCKDATA lpUnlock);
DWORD APIENTRY VPoxDispDDMapMemory(PDD_MAPMEMORYDATA lpMapMemory);

#ifdef VPOX_WITH_VIDEOHWACCEL
int VPoxDispVHWAUpdateDDHalInfo(PVPOXDISPDEV pDev, DD_HALINFO *pHalInfo);

DWORD APIENTRY VPoxDispDDGetDriverInfo(DD_GETDRIVERINFODATA *lpData);
DWORD APIENTRY VPoxDispDDSetColorKey(PDD_SETCOLORKEYDATA lpSetColorKey);
DWORD APIENTRY VPoxDispDDAddAttachedSurface(PDD_ADDATTACHEDSURFACEDATA lpAddAttachedSurface);
DWORD APIENTRY VPoxDispDDBlt(PDD_BLTDATA lpBlt);
DWORD APIENTRY VPoxDispDDFlip(PDD_FLIPDATA lpFlip);
DWORD APIENTRY VPoxDispDDGetBltStatus(PDD_GETBLTSTATUSDATA lpGetBltStatus);
DWORD APIENTRY VPoxDispDDGetFlipStatus(PDD_GETFLIPSTATUSDATA lpGetFlipStatus);
DWORD APIENTRY VPoxDispDDSetOverlayPosition(PDD_SETOVERLAYPOSITIONDATA lpSetOverlayPosition);
DWORD APIENTRY VPoxDispDDUpdateOverlay(PDD_UPDATEOVERLAYDATA lpUpdateOverlay);
#endif

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispDDraw_h */
