/* $Id: VPoxNetFltP-win.h $ */
/** @file
 * VPoxNetFltP-win.h - Bridged Networking Driver, Windows Specific Code.
 * Protocol edge API
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltP_win_h
#define VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltP_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VPOXNETADP
# error "No protocol edge"
#endif
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtRegister(PVPOXNETFLTGLOBALS_PT pGlobalsPt, PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPathStr);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtDeregister(PVPOXNETFLTGLOBALS_PT pGlobalsPt);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtDoUnbinding(PVPOXNETFLTINS pNetFlt, bool bOnUnbind);
DECLHIDDEN(VOID) vpoxNetFltWinPtRequestComplete(NDIS_HANDLE hContext, PNDIS_REQUEST pNdisRequest, NDIS_STATUS Status);
DECLHIDDEN(bool) vpoxNetFltWinPtCloseInterface(PVPOXNETFLTINS pNetFlt, PNDIS_STATUS pStatus);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtDoBinding(PVPOXNETFLTINS pThis, PNDIS_STRING pOurDeviceName, PNDIS_STRING pBindToDeviceName);
#endif /* !VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltP_win_h */
