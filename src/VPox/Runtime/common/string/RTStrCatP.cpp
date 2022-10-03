/* $Id: RTStrCatP.cpp $ */
/** @file
 * IPRT - RTStrCat.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#include <iprt/string.h>
#include "internal/iprt.h"

#include <iprt/errcore.h>


RTDECL(int) RTStrCatP(char **ppszDst, size_t *pcbDst, const char *pszSrc)
{
    /*
     * Advance past the current string in the output buffer and turn this into
     * a copy operation.
     */
    char   *pszDstOrg = *ppszDst;
    size_t  cbDst     = *pcbDst;
    char   *pszDst    = RTStrEnd(pszDstOrg, cbDst);
    AssertReturn(pszDst, VERR_INVALID_PARAMETER);
    *ppszDst = pszDst;
    *pcbDst  = cbDst - (pszDst - pszDstOrg);

    return RTStrCopyP(ppszDst, pcbDst, pszSrc);
}
RT_EXPORT_SYMBOL(RTStrCatP);

