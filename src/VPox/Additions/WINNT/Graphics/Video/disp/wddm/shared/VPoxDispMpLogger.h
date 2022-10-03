/* $Id: VPoxDispMpLogger.h $ */
/** @file
 * VPox WDDM Display backdoor logger API
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* We're unable to use standard r3 vbgl-based backdoor logging API because win8 Metro apps
 * can not do CreateFile/Read/Write by default
 * this is why we use miniport escape functionality to issue backdoor log string to the miniport
 * and submit it to host via standard r0 backdoor logging api accordingly */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_shared_VPoxDispMpLogger_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_shared_VPoxDispMpLogger_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

/*enable this in case we include this in a static lib*/
# define VPOXDISPMPLOGGER_DECL(a_Type) a_Type RTCALL

RT_C_DECLS_BEGIN

VPOXDISPMPLOGGER_DECL(int) VPoxDispMpLoggerInit(void);

VPOXDISPMPLOGGER_DECL(int) VPoxDispMpLoggerTerm(void);

VPOXDISPMPLOGGER_DECL(void) VPoxDispMpLoggerLog(const char *pszString);

VPOXDISPMPLOGGER_DECL(void) VPoxDispMpLoggerLogF(const char *pszString, ...);

DECLCALLBACK(void) VPoxWddmUmLog(const char *pszString);

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_shared_VPoxDispMpLogger_h */

