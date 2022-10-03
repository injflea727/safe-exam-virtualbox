/* $Id: SELMInternal.h $ */
/** @file
 * SELM - Internal header file.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_SELMInternal_h
#define VMM_INCLUDED_SRC_include_SELMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/cdefs.h>
#include <VPox/types.h>
#include <VPox/vmm/stam.h>
#include <VPox/vmm/cpum.h>
#include <VPox/vmm/pgm.h>
#include <VPox/log.h>
#include <iprt/x86.h>



/** @defgroup grp_selm_int   Internals
 * @ingroup grp_selm
 * @internal
 * @{
 */

/** The number of GDTS allocated for our GDT. (full size) */
#define SELM_GDT_ELEMENTS                   8192


/**
 * SELM Data (part of VM)
 *
 * @note This is a very marginal component after kicking raw-mode.
 */
typedef struct SELM
{
#ifdef VPOX_WITH_STATISTICS
    STAMCOUNTER             StatLoadHidSelGst;
    STAMCOUNTER             StatLoadHidSelShw;
#endif
    STAMCOUNTER             StatLoadHidSelReadErrors;
    STAMCOUNTER             StatLoadHidSelGstNoGood;
} SELM, *PSELM;


/** @} */

#endif /* !VMM_INCLUDED_SRC_include_SELMInternal_h */
