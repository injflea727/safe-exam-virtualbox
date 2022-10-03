/* $Id: bs3-cmn-RegCtxSetGrpSegFromFlat.c $ */
/** @file
 * BS3Kit - Bs3RegCtxSetGrpSegFromFlat
 */

/*
 * Copyright (C) 2007-2020 Oracle Corporation
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
#include "bs3kit-template-header.h"


#undef Bs3RegCtxSetGrpSegFromFlat
BS3_CMN_DEF(void, Bs3RegCtxSetGrpSegFromFlat,(PBS3REGCTX pRegCtx, PBS3REG pGpr, PRTSEL pSel, RTCCUINTXREG uFlat))
{
    if (BS3_MODE_IS_16BIT_CODE(pRegCtx->bMode))
    {
        uint32_t uFar1616;
        if (BS3_MODE_IS_RM_OR_V86(pRegCtx->bMode))
            uFar1616 = Bs3SelFlatDataToRealMode(uFlat);
        else
            uFar1616 = Bs3SelFlatDataToProtFar16(uFlat);
        pGpr->u  = uFar1616 & UINT16_MAX;
        *pSel    = uFar1616 >> 16;
    }
    else
    {
        pGpr->u  = uFlat;
        if (BS3_MODE_IS_32BIT_CODE(pRegCtx->bMode))
            *pSel = BS3_SEL_R0_DS32;
        else
            *pSel = BS3_SEL_R0_DS64;
    }

    /* Adjust CS to the right ring, if not ring-0 or V86 context. */
    if (   pRegCtx->bCpl != 0
        && !BS3_MODE_IS_RM_OR_V86(pRegCtx->bMode))
    {
        if (BS3_SEL_IS_IN_R0_RANGE(*pSel))
            *pSel += (uint16_t)pRegCtx->bCpl << BS3_SEL_RING_SHIFT;
        *pSel |= pRegCtx->bCpl;
    }
}

