/* $Id: VPoxDebugLib.h $ */
/** @file
 * VPoxDebugLib.h - Debug and logging routines implemented by VPoxDebugLib.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VPoxPkg_VPoxDebugLib_h
#define ___VPoxPkg_VPoxDebugLib_h

#include <Uefi/UefiBaseType.h>
#include "VPoxPkg.h"

size_t VPoxPrintChar(int ch);
size_t VPoxPrintGuid(CONST EFI_GUID *pGuid);
size_t VPoxPrintHex(UINT64 uValue, size_t cbType);
size_t VPoxPrintHexDump(const void *pv, size_t cb);
size_t VPoxPrintString(const char *pszString);

#endif

