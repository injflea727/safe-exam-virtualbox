/* $Id: VPoxUSBFilterMgr.h $ */
/** @file
 * VirtualPox Ring-0 USB Filter Manager.
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_VPoxUSBFilterMgr_h
#define VPOX_INCLUDED_SRC_VPoxUSB_VPoxUSBFilterMgr_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/usbfilter.h>

RT_C_DECLS_BEGIN

/** @todo r=bird: VPOXUSBFILTER_CONTEXT isn't following the coding
 *        guildlines. Don't know which clueless dude did this...  */
#if defined(RT_OS_WINDOWS)
typedef struct VPOXUSBFLTCTX *VPOXUSBFILTER_CONTEXT;
#define VPOXUSBFILTER_CONTEXT_NIL NULL
#else
typedef RTPROCESS VPOXUSBFILTER_CONTEXT;
#define VPOXUSBFILTER_CONTEXT_NIL NIL_RTPROCESS
#endif

int     VPoxUSBFilterInit(void);
void    VPoxUSBFilterTerm(void);
void    VPoxUSBFilterRemoveOwner(VPOXUSBFILTER_CONTEXT Owner);
int     VPoxUSBFilterAdd(PCUSBFILTER pFilter, VPOXUSBFILTER_CONTEXT Owner, uintptr_t *puId);
int     VPoxUSBFilterRemove(VPOXUSBFILTER_CONTEXT Owner, uintptr_t uId);
VPOXUSBFILTER_CONTEXT VPoxUSBFilterMatch(PCUSBFILTER pDevice, uintptr_t *puId);
VPOXUSBFILTER_CONTEXT VPoxUSBFilterMatchEx(PCUSBFILTER pDevice, uintptr_t *puId, bool fRemoveFltIfOneShot, bool *pfFilter, bool *pfIsOneShot);
VPOXUSBFILTER_CONTEXT VPoxUSBFilterGetOwner(uintptr_t uId);

RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_VPoxUSBFilterMgr_h */
