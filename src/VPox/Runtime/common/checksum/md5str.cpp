/* $Id: md5str.cpp $ */
/** @file
 * IPRT - MD5 string functions.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/md5.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>


RTDECL(int) RTMd5ToString(uint8_t const pabDigest[RTMD5_HASH_SIZE], char *pszDigest, size_t cchDigest)
{
    return RTStrPrintHexBytes(pszDigest, cchDigest, &pabDigest[0], RTMD5_HASH_SIZE, 0 /*fFlags*/);
}


RTDECL(int) RTMd5FromString(char const *pszDigest, uint8_t pabDigest[RTMD5_HASH_SIZE])
{
    return RTStrConvertHexBytes(RTStrStripL(pszDigest), &pabDigest[0], RTMD5_HASH_SIZE, 0 /*fFlags*/);
}
