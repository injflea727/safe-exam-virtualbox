/* $Id: VPoxUsbFlt.h $ */
/** @file
 * VPox USB Monitor Device Filtering functionality
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbFlt_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbFlt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxUsbMon.h"
#include <VPoxUSBFilterMgr.h>

#include <VPox/usblib-win.h>

typedef struct VPOXUSBFLTCTX
{
    LIST_ENTRY ListEntry;
    RTPROCESS Process;          // Purely informational, no function?
    uint32_t cActiveFilters;
    BOOLEAN bRemoved;           // For debugging only?
} VPOXUSBFLTCTX, *PVPOXUSBFLTCTX;

NTSTATUS VPoxUsbFltInit();
NTSTATUS VPoxUsbFltTerm();
NTSTATUS VPoxUsbFltCreate(PVPOXUSBFLTCTX pContext);
NTSTATUS VPoxUsbFltClose(PVPOXUSBFLTCTX pContext);
int VPoxUsbFltAdd(PVPOXUSBFLTCTX pContext, PUSBFILTER pFilter, uintptr_t *pId);
int VPoxUsbFltRemove(PVPOXUSBFLTCTX pContext, uintptr_t uId);
NTSTATUS VPoxUsbFltFilterCheck(PVPOXUSBFLTCTX pContext);

NTSTATUS VPoxUsbFltGetDevice(PVPOXUSBFLTCTX pContext, HVPOXUSBDEVUSR hDevice, PUSBSUP_GETDEV_MON pInfo);

typedef void* HVPOXUSBFLTDEV;
HVPOXUSBFLTDEV VPoxUsbFltProxyStarted(PDEVICE_OBJECT pPdo);
void VPoxUsbFltProxyStopped(HVPOXUSBFLTDEV hDev);

NTSTATUS VPoxUsbFltPdoAdd(PDEVICE_OBJECT pPdo, BOOLEAN *pbFiltered);
NTSTATUS VPoxUsbFltPdoRemove(PDEVICE_OBJECT pPdo);
BOOLEAN VPoxUsbFltPdoIsFiltered(PDEVICE_OBJECT pPdo);

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbFlt_h */

