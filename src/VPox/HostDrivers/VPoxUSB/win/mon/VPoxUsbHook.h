/* $Id: VPoxUsbHook.h $ */
/** @file
 * Driver Dispatch Table Hooking API impl
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbHook_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbHook_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxUsbMon.h"

typedef struct VPOXUSBHOOK_ENTRY
{
    LIST_ENTRY RequestList;
    KSPIN_LOCK Lock;
    BOOLEAN fIsInstalled;
    PDRIVER_DISPATCH pfnOldHandler;
    VPOXDRVTOOL_REF HookRef;
    PDRIVER_OBJECT pDrvObj;
    UCHAR iMjFunction;
    PDRIVER_DISPATCH pfnHook;
} VPOXUSBHOOK_ENTRY, *PVPOXUSBHOOK_ENTRY;

typedef struct VPOXUSBHOOK_REQUEST
{
    LIST_ENTRY ListEntry;
    PVPOXUSBHOOK_ENTRY pHook;
    IO_STACK_LOCATION OldLocation;
    PDEVICE_OBJECT pDevObj;
    PIRP pIrp;
    BOOLEAN bCompletionStopped;
} VPOXUSBHOOK_REQUEST, *PVPOXUSBHOOK_REQUEST;

DECLINLINE(BOOLEAN) VPoxUsbHookRetain(PVPOXUSBHOOK_ENTRY pHook)
{
    KIRQL Irql;
    KeAcquireSpinLock(&pHook->Lock, &Irql);
    if (!pHook->fIsInstalled)
    {
        KeReleaseSpinLock(&pHook->Lock, Irql);
        return FALSE;
    }

    VPoxDrvToolRefRetain(&pHook->HookRef);
    KeReleaseSpinLock(&pHook->Lock, Irql);
    return TRUE;
}

DECLINLINE(VOID) VPoxUsbHookRelease(PVPOXUSBHOOK_ENTRY pHook)
{
    VPoxDrvToolRefRelease(&pHook->HookRef);
}

VOID VPoxUsbHookInit(PVPOXUSBHOOK_ENTRY pHook, PDRIVER_OBJECT pDrvObj, UCHAR iMjFunction, PDRIVER_DISPATCH pfnHook);
NTSTATUS VPoxUsbHookInstall(PVPOXUSBHOOK_ENTRY pHook);
NTSTATUS VPoxUsbHookUninstall(PVPOXUSBHOOK_ENTRY pHook);
BOOLEAN VPoxUsbHookIsInstalled(PVPOXUSBHOOK_ENTRY pHook);
NTSTATUS VPoxUsbHookRequestPassDownHookCompletion(PVPOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp, PIO_COMPLETION_ROUTINE pfnCompletion, PVPOXUSBHOOK_REQUEST pRequest);
NTSTATUS VPoxUsbHookRequestPassDownHookSkip(PVPOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS VPoxUsbHookRequestMoreProcessingRequired(PVPOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVPOXUSBHOOK_REQUEST pRequest);
NTSTATUS VPoxUsbHookRequestComplete(PVPOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVPOXUSBHOOK_REQUEST pRequest);
VOID VPoxUsbHookVerifyCompletion(PVPOXUSBHOOK_ENTRY pHook, PVPOXUSBHOOK_REQUEST pRequest, PIRP pIrp);

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_mon_VPoxUsbHook_h */
