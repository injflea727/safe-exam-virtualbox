/* $Id: VPoxHlp.h $ */
/** @file
 * VPox Qt GUI - Declaration of OS/2-specific helpers that require to reside in a DLL.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_platform_os2_VPoxHlp_h
#define FEQT_INCLUDED_SRC_platform_os2_VPoxHlp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

#ifdef IN_VPOXHLP
# define VPOXHLPDECL(type) DECLEXPORT(type) RTCALL
#else
# define VPOXHLPDECL(type) DECLIMPORT(type) RTCALL
#endif

VPOXHLPDECL(bool) VPoxHlpInstallKbdHook (HAB aHab, HWND aHwnd,
                                           unsigned long aMsg);

VPOXHLPDECL(bool) VPoxHlpUninstallKbdHook (HAB aHab, HWND aHwnd,
                                           unsigned long aMsg);

#endif /* !FEQT_INCLUDED_SRC_platform_os2_VPoxHlp_h */

