/* $Id: VPoxCommon.h $ */
/** @file
 * VPoxCommon - Misc helper routines for install helper.
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

#ifndef VPOX_INCLUDED_SRC_InstallHelper_VPoxCommon_h
#define VPOX_INCLUDED_SRC_InstallHelper_VPoxCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if (_MSC_VER < 1400) /* Provide _stprintf_s to VC < 8.0. */
int swprintf_s(WCHAR *buffer, size_t cbBuffer, const WCHAR *format, ...);
#endif

UINT VPoxGetProperty(MSIHANDLE a_hModule, WCHAR *a_pwszName, WCHAR *a_pwszValue, DWORD a_dwSize);
UINT VPoxSetProperty(MSIHANDLE a_hModule, WCHAR *a_pwszName, WCHAR *a_pwszValue);

#endif /* !VPOX_INCLUDED_SRC_InstallHelper_VPoxCommon_h */

