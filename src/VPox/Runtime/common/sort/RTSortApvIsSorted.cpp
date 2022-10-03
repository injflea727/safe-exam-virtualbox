/* $Id: RTSortApvIsSorted.cpp $ */
/** @file
 * IPRT - RTSortApvIsSorted.
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
#include <iprt/sort.h>
#include "internal/iprt.h"


RTDECL(bool) RTSortApvIsSorted(void const * const *papvArray, size_t cElements, PFNRTSORTCMP pfnCmp, void *pvUser)
{
    if (cElements >= 2)
    {
        for (size_t i = 0; i < cElements - 1; i++)
        {
            if (pfnCmp(papvArray[i], papvArray[i + 1], pvUser) > 0)
                return false;
        }
    }
    return true;
}
RT_EXPORT_SYMBOL(RTSortIsSorted);
