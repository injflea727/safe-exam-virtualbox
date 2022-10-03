/* $Id: VPoxDisplay.h $ */
/** @file
 * VPoxSeamless - Display notifications
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDisplay_h
#define GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDisplay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

DWORD VPoxDisplayGetCount();
DWORD VPoxDisplayGetConfig(const DWORD NumDevices, DWORD *pDevPrimaryNum, DWORD *pNumDevices, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes);

DWORD EnableAndResizeDispDev(DEVMODE *paDeviceModes, DISPLAY_DEVICE *paDisplayDevices, DWORD totalDispNum, UINT Id, DWORD aWidth, DWORD aHeight,
                             DWORD aBitsPerPixel, LONG aPosX, LONG aPosY, BOOL fEnabled, BOOL fExtDispSup);

#ifndef VPOX_WITH_WDDM
static bool isVPoxDisplayDriverActive(void);
#else
/* @misha: getVPoxDisplayDriverType is used instead.
 * it seems bad to put static function declaration to header,
 * so it is moved to VPoxDisplay.cpp */
#endif

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDisplay_h */
