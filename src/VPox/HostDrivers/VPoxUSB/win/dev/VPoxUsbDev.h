/* $Id: VPoxUsbDev.h $ */
/** @file
 * VPoxUsbDev.h - USB device.
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_dev_VPoxUsbDev_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_dev_VPoxUsbDev_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxUsbCmn.h"
#include <VPox/cdefs.h>
#include <iprt/assert.h>

typedef struct VPOXUSB_GLOBALS
{
    PDRIVER_OBJECT pDrvObj;
    UNICODE_STRING RegPath;
    VPOXUSBRT_IDC RtIdc;
} VPOXUSB_GLOBALS, *PVPOXUSB_GLOBALS;

extern VPOXUSB_GLOBALS g_VPoxUsbGlobals;

/* pnp state decls */
typedef enum
{
    ENMVPOXUSB_PNPSTATE_UNKNOWN = 0,
    ENMVPOXUSB_PNPSTATE_START_PENDING,
    ENMVPOXUSB_PNPSTATE_STARTED,
    ENMVPOXUSB_PNPSTATE_STOP_PENDING,
    ENMVPOXUSB_PNPSTATE_STOPPED,
    ENMVPOXUSB_PNPSTATE_SURPRISE_REMOVED,
    ENMVPOXUSB_PNPSTATE_REMOVE_PENDING,
    ENMVPOXUSB_PNPSTATE_REMOVED,
    ENMVPOXUSB_PNPSTATE_FORSEDWORD = 0x8fffffff
} ENMVPOXUSB_PNPSTATE;
AssertCompile(sizeof (ENMVPOXUSB_PNPSTATE) == sizeof (uint32_t));

#ifdef VPOX_STRICT
DECLHIDDEN(VOID) vpoxUsbPnPStateGbgChange(ENMVPOXUSB_PNPSTATE enmOld, ENMVPOXUSB_PNPSTATE enmNew);
# define VPOXUSB_PNP_GBG_STATE_CHANGE(_old, _new) vpoxUsbPnPStateGbgChange((_old), (_new))
#else
# define VPOXUSB_PNP_GBG_STATE_CHANGE(_old, _new) do { } while (0)
#endif


typedef struct VPOXUSB_PNPSTATE
{
    /* Current state */
    volatile ENMVPOXUSB_PNPSTATE Curr;
    /* Previous state, used to restore state info on cancell stop device */
    ENMVPOXUSB_PNPSTATE Prev;
} VPOXUSB_PNPSTATE, *PVPOXUSB_PNPSTATE;

typedef struct VPOXUSBDEV_DDISTATE
{
    /* Lock */
    KSPIN_LOCK Lock;
    VPOXDRVTOOL_REF Ref;
    VPOXUSB_PNPSTATE PnPState;
    VPOXUSB_PWRSTATE PwrState;
    /* current dev caps */
    DEVICE_CAPABILITIES DevCaps;
} VPOXUSBDEV_DDISTATE, *PVPOXUSBDEV_DDISTATE;

typedef struct VPOXUSBDEV_EXT
{
    PDEVICE_OBJECT pFDO;
    PDEVICE_OBJECT pPDO;
    PDEVICE_OBJECT pLowerDO;

    VPOXUSBDEV_DDISTATE DdiState;

    uint32_t cHandles;

    VPOXUSB_RT Rt;

} VPOXUSBDEV_EXT, *PVPOXUSBDEV_EXT;

/* pnp state api */
DECLINLINE(ENMVPOXUSB_PNPSTATE) vpoxUsbPnPStateGet(PVPOXUSBDEV_EXT pDevExt)
{
    return (ENMVPOXUSB_PNPSTATE)ASMAtomicUoReadU32((volatile uint32_t*)&pDevExt->DdiState.PnPState.Curr);
}

DECLINLINE(ENMVPOXUSB_PNPSTATE) vpoxUsbPnPStateSet(PVPOXUSBDEV_EXT pDevExt, ENMVPOXUSB_PNPSTATE enmState)
{
    KIRQL Irql;
    ENMVPOXUSB_PNPSTATE enmOldState;
    KeAcquireSpinLock(&pDevExt->DdiState.Lock, &Irql);
    pDevExt->DdiState.PnPState.Prev = (ENMVPOXUSB_PNPSTATE)ASMAtomicUoReadU32((volatile uint32_t*)&pDevExt->DdiState.PnPState.Curr);
    ASMAtomicWriteU32((volatile uint32_t*)&pDevExt->DdiState.PnPState.Curr, (uint32_t)enmState);
    pDevExt->DdiState.PnPState.Curr = enmState;
    enmOldState = pDevExt->DdiState.PnPState.Prev;
    KeReleaseSpinLock(&pDevExt->DdiState.Lock, Irql);
    VPOXUSB_PNP_GBG_STATE_CHANGE(enmOldState, enmState);
    return enmState;
}

DECLINLINE(ENMVPOXUSB_PNPSTATE) vpoxUsbPnPStateRestore(PVPOXUSBDEV_EXT pDevExt)
{
    ENMVPOXUSB_PNPSTATE enmNewState, enmOldState;
    KIRQL Irql;
    KeAcquireSpinLock(&pDevExt->DdiState.Lock, &Irql);
    enmOldState = pDevExt->DdiState.PnPState.Curr;
    enmNewState = pDevExt->DdiState.PnPState.Prev;
    ASMAtomicWriteU32((volatile uint32_t*)&pDevExt->DdiState.PnPState.Curr, (uint32_t)pDevExt->DdiState.PnPState.Prev);
    KeReleaseSpinLock(&pDevExt->DdiState.Lock, Irql);
    VPOXUSB_PNP_GBG_STATE_CHANGE(enmOldState, enmNewState);
    Assert(enmNewState == ENMVPOXUSB_PNPSTATE_STARTED);
    Assert(enmOldState == ENMVPOXUSB_PNPSTATE_STOP_PENDING
            || enmOldState == ENMVPOXUSB_PNPSTATE_REMOVE_PENDING);
    return enmNewState;
}

DECLINLINE(VOID) vpoxUsbPnPStateInit(PVPOXUSBDEV_EXT pDevExt)
{
    pDevExt->DdiState.PnPState.Curr = pDevExt->DdiState.PnPState.Prev = ENMVPOXUSB_PNPSTATE_START_PENDING;
}

DECLINLINE(VOID) vpoxUsbDdiStateInit(PVPOXUSBDEV_EXT pDevExt)
{
    KeInitializeSpinLock(&pDevExt->DdiState.Lock);
    VPoxDrvToolRefInit(&pDevExt->DdiState.Ref);
    vpoxUsbPwrStateInit(pDevExt);
    vpoxUsbPnPStateInit(pDevExt);
}

DECLINLINE(bool) vpoxUsbDdiStateRetainIfStarted(PVPOXUSBDEV_EXT pDevExt)
{
    KIRQL oldIrql;
    bool bRetained = true;
    KeAcquireSpinLock(&pDevExt->DdiState.Lock, &oldIrql);
    if (vpoxUsbPnPStateGet(pDevExt) == ENMVPOXUSB_PNPSTATE_STARTED)
    {
        VPoxDrvToolRefRetain(&pDevExt->DdiState.Ref);
    }
    else
    {
        bRetained = false;
    }
    KeReleaseSpinLock(&pDevExt->DdiState.Lock, oldIrql);
    return bRetained;
}

/* if device is removed - does nothing and returns zero,
 * otherwise increments a ref counter and returns the current pnp state
 * NOTE: never returns ENMVPOXUSB_PNPSTATE_REMOVED
 * */
DECLINLINE(ENMVPOXUSB_PNPSTATE) vpoxUsbDdiStateRetainIfNotRemoved(PVPOXUSBDEV_EXT pDevExt)
{
    KIRQL oldIrql;
    ENMVPOXUSB_PNPSTATE enmState;
    KeAcquireSpinLock(&pDevExt->DdiState.Lock, &oldIrql);
    enmState = vpoxUsbPnPStateGet(pDevExt);
    if (enmState != ENMVPOXUSB_PNPSTATE_REMOVED)
    {
        VPoxDrvToolRefRetain(&pDevExt->DdiState.Ref);
    }
    KeReleaseSpinLock(&pDevExt->DdiState.Lock, oldIrql);
    return enmState != ENMVPOXUSB_PNPSTATE_REMOVED ? enmState : (ENMVPOXUSB_PNPSTATE)0;
}

DECLINLINE(uint32_t) vpoxUsbDdiStateRetain(PVPOXUSBDEV_EXT pDevExt)
{
    return VPoxDrvToolRefRetain(&pDevExt->DdiState.Ref);
}

DECLINLINE(uint32_t) vpoxUsbDdiStateRelease(PVPOXUSBDEV_EXT pDevExt)
{
    return VPoxDrvToolRefRelease(&pDevExt->DdiState.Ref);
}

DECLINLINE(VOID) vpoxUsbDdiStateReleaseAndWaitCompleted(PVPOXUSBDEV_EXT pDevExt)
{
    VPoxDrvToolRefRelease(&pDevExt->DdiState.Ref);
    VPoxDrvToolRefWaitEqual(&pDevExt->DdiState.Ref, 1);
}

DECLINLINE(VOID) vpoxUsbDdiStateReleaseAndWaitRemoved(PVPOXUSBDEV_EXT pDevExt)
{
    VPoxDrvToolRefRelease(&pDevExt->DdiState.Ref);
    VPoxDrvToolRefWaitEqual(&pDevExt->DdiState.Ref, 0);
}

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_dev_VPoxUsbDev_h */
