/* $Id: VPoxUsbMon.h $ */
/** @file
 * VPox USB Monitor
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbMon_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbMon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/cdefs.h>
#include <VPox/types.h>
#include <iprt/assert.h>
#include <VPox/sup.h>
#include <iprt/asm.h>
#include <VPox/log.h>

#ifdef DEBUG
/* disables filters */
//#define VPOXUSBMON_DBG_NO_FILTERS
/* disables pnp hooking */
//#define VPOXUSBMON_DBG_NO_PNPHOOK
#endif

#include "../../../win/VPoxDbgLog.h"
#include "../cmn/VPoxDrvTool.h"
#include "../cmn/VPoxUsbTool.h"

#include "VPoxUsbHook.h"
#include "VPoxUsbFlt.h"

PVOID VPoxUsbMonMemAlloc(SIZE_T cbBytes);
PVOID VPoxUsbMonMemAllocZ(SIZE_T cbBytes);
VOID VPoxUsbMonMemFree(PVOID pvMem);

NTSTATUS VPoxUsbMonGetDescriptor(PDEVICE_OBJECT pDevObj, void *buffer, int size, int type, int index, int language_id);
NTSTATUS VPoxUsbMonQueryBusRelations(PDEVICE_OBJECT pDevObj, PFILE_OBJECT pFileObj, PDEVICE_RELATIONS *pDevRelations);

void vpoxUsbDbgPrintUnicodeString(PUNICODE_STRING pUnicodeString);

typedef DECLCALLBACK(BOOLEAN) FNVPOXUSBMONDEVWALKER(PFILE_OBJECT pHubFile, PDEVICE_OBJECT pHubDo, PVOID pvContext);
typedef FNVPOXUSBMONDEVWALKER *PFNVPOXUSBMONDEVWALKER;

VOID vpoxUsbMonHubDevWalk(PFNVPOXUSBMONDEVWALKER pfnWalker, PVOID pvWalker);

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbMon_h */
