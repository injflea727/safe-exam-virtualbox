/* $Id: VPoxHook.h $ */
/** @file
 * VPoxHook -- Global windows hook dll.
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

#ifndef GA_INCLUDED_WINNT_VPoxHook_h
#define GA_INCLUDED_WINNT_VPoxHook_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* custom messages as we must install the hook from the main thread */
/** @todo r=andy Use WM_APP + n offsets here! */
#define WM_VPOX_SEAMLESS_ENABLE                     0x2001
#define WM_VPOX_SEAMLESS_DISABLE                    0x2002
#define WM_VPOX_SEAMLESS_UPDATE                     0x2003
#define WM_VPOX_GRAPHICS_SUPPORTED                  0x2004
#define WM_VPOX_GRAPHICS_UNSUPPORTED                0x2005


#define VPOXHOOK_DLL_NAME              "VPoxHook.dll"
#define VPOXHOOK_GLOBAL_DT_EVENT_NAME  "Local\\VPoxHookDtNotifyEvent"
#define VPOXHOOK_GLOBAL_WT_EVENT_NAME  "Local\\VPoxHookWtNotifyEvent"

BOOL VPoxHookInstallActiveDesktopTracker(HMODULE hDll);
BOOL VPoxHookRemoveActiveDesktopTracker();

BOOL VPoxHookInstallWindowTracker(HMODULE hDll);
BOOL VPoxHookRemoveWindowTracker();

#endif /* !GA_INCLUDED_WINNT_VPoxHook_h */

