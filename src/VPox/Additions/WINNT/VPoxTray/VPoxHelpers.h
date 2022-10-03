/* $Id: VPoxHelpers.h $ */
/** @file
 * helpers - Guest Additions Service helper functions header.
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

#ifndef GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxHelpers_h
#define GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxHelpers_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

// #define DEBUG_DISPLAY_CHANGE

/** @todo r=andy WTF? Remove this!! */
#ifdef DEBUG_DISPLAY_CHANGE
#   define DDCLOG(a) Log(a)
#else
#   define DDCLOG(a) do {} while (0)
#endif /* !DEBUG_DISPLAY_CHANGE */

extern int  hlpReportStatus(VPoxGuestFacilityStatus statusCurrent);
extern void hlpReloadCursor(void);
extern void hlpResizeRect(RECTL *paRects, unsigned nRects, unsigned uPrimary, unsigned uResized, int iNewWidth, int iNewHeight, int iNewPosX, int iNewPosY);
extern int  hlpShowBalloonTip(HINSTANCE hInst, HWND hWnd, UINT uID, const char *pszMsg, const char *pszTitle, UINT uTimeout, DWORD dwInfoFlags);
extern void hlpShowMessageBox(const char *pszTitle, UINT uStyle, const char *pszFmt, ...);

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxHelpers_h */

