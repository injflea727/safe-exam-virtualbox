/* $Id: PasswordInput.h $ */
/** @file
 * Frontend shared bits - Password file and console input helpers.
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

#ifndef VPOX_INCLUDED_SRC_Common_PasswordInput_h
#define VPOX_INCLUDED_SRC_Common_PasswordInput_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/com/com.h>
#include <VPox/com/ptr.h>
#include <VPox/com/string.h>
#include <VPox/com/VirtualPox.h>


RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd);
RTEXITCODE readPasswordFromConsole(com::Utf8Str *pPassword, const char *pszPrompt, ...);
RTEXITCODE settingsPasswordFile(ComPtr<IVirtualPox> virtualPox, const char *pszFilename);

#endif /* !VPOX_INCLUDED_SRC_Common_PasswordInput_h */
