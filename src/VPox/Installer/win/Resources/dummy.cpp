/* $Id: dummy.cpp $ */
/** @file
 * dummy.cpp - file to make kBuild happy when building a resource only DLL.
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

#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN

DECLEXPORT(void) VPoxResDummy(void)
{
}

RT_C_DECLS_END
