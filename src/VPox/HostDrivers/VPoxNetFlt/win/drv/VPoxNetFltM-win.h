/* $Id: VPoxNetFltM-win.h $ */
/** @file
 * VPoxNetFltM-win.h - Bridged Networking Driver, Windows Specific Code - Miniport edge API
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltM_win_h
#define VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltM_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinMpRegister(PVPOXNETFLTGLOBALS_MP pGlobalsMp, PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPathStr);
DECLHIDDEN(VOID) vpoxNetFltWinMpDeregister(PVPOXNETFLTGLOBALS_MP pGlobalsMp);
DECLHIDDEN(VOID) vpoxNetFltWinMpReturnPacket(IN NDIS_HANDLE hMiniportAdapterContext, IN PNDIS_PACKET pPacket);

#ifdef VPOXNETADP
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinMpDoInitialization(PVPOXNETFLTINS pThis, NDIS_HANDLE hMiniportAdapter, NDIS_HANDLE hWrapperConfigurationContext);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinMpDoDeinitialization(PVPOXNETFLTINS pThis);

#else

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinMpInitializeDevideInstance(PVPOXNETFLTINS pThis);
DECLHIDDEN(bool) vpoxNetFltWinMpDeInitializeDeviceInstance(PVPOXNETFLTINS pThis, PNDIS_STATUS pStatus);

DECLINLINE(VOID) vpoxNetFltWinMpRequestStateComplete(PVPOXNETFLTINS pNetFlt)
{
    RTSpinlockAcquire(pNetFlt->hSpinlock);
    pNetFlt->u.s.WinIf.StateFlags.fRequestInfo = 0;
    RTSpinlockRelease(pNetFlt->hSpinlock);
}

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinMpRequestPost(PVPOXNETFLTINS pNetFlt);
#endif

#endif /* !VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltM_win_h */

