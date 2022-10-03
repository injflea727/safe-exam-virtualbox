/* $Id: VPoxMPInternal.h $ */
/** @file
 * VPox XPDM Miniport internal header
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
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_xpdm_VPoxMPInternal_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_xpdm_VPoxMPInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "common/VPoxMPUtils.h"
#include "common/VPoxMPDevExt.h"
#include "common/xpdm/VPoxVideoIOCTL.h"

RT_C_DECLS_BEGIN
ULONG DriverEntry(IN PVOID Context1, IN PVOID Context2);
RT_C_DECLS_END

/* ==================== Misc ==================== */
void VPoxSetupVideoPortAPI(PVPOXMP_DEVEXT pExt, PVIDEO_PORT_CONFIG_INFO pConfigInfo);
void VPoxCreateDisplays(PVPOXMP_DEVEXT pExt, PVIDEO_PORT_CONFIG_INFO pConfigInfo);
int VPoxVbvaEnable(PVPOXMP_DEVEXT pExt, BOOLEAN bEnable, VBVAENABLERESULT *pResult);
DECLCALLBACK(void) VPoxMPHGSMIHostCmdCompleteCB(HVPOXVIDEOHGSMI hHGSMI, struct VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);
DECLCALLBACK(int) VPoxMPHGSMIHostCmdRequestCB(HVPOXVIDEOHGSMI hHGSMI, uint8_t u8Channel, uint32_t iDisplay,
                                              struct VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST **ppCmd);
int VPoxVbvaChannelDisplayEnable(PVPOXMP_COMMON pCommon, int iDisplay, uint8_t u8Channel);

/* ==================== System VRP's handlers ==================== */
BOOLEAN VPoxMPSetCurrentMode(PVPOXMP_DEVEXT pExt, PVIDEO_MODE pMode, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPResetDevice(PVPOXMP_DEVEXT pExt, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPMapVideoMemory(PVPOXMP_DEVEXT pExt, PVIDEO_MEMORY pRequestedAddress,
                             PVIDEO_MEMORY_INFORMATION pMapInfo, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPUnmapVideoMemory(PVPOXMP_DEVEXT pExt, PVIDEO_MEMORY VideoMemory, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPShareVideoMemory(PVPOXMP_DEVEXT pExt, PVIDEO_SHARE_MEMORY pShareMem,
                               PVIDEO_SHARE_MEMORY_INFORMATION pShareMemInfo, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPUnshareVideoMemory(PVPOXMP_DEVEXT pExt, PVIDEO_SHARE_MEMORY pMem, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPQueryCurrentMode(PVPOXMP_DEVEXT pExt, PVIDEO_MODE_INFORMATION pModeInfo, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPQueryNumAvailModes(PVPOXMP_DEVEXT pExt, PVIDEO_NUM_MODES pNumModes, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPQueryAvailModes(PVPOXMP_DEVEXT pExt, PVIDEO_MODE_INFORMATION pModes, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPSetColorRegisters(PVPOXMP_DEVEXT pExt, PVIDEO_CLUT pClut, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPSetPointerAttr(PVPOXMP_DEVEXT pExt, PVIDEO_POINTER_ATTRIBUTES pPointerAttrs, uint32_t cbLen, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPEnablePointer(PVPOXMP_DEVEXT pExt, BOOLEAN bEnable, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPQueryPointerPosition(PVPOXMP_DEVEXT pExt, PVIDEO_POINTER_POSITION pPos, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPQueryPointerCapabilities(PVPOXMP_DEVEXT pExt, PVIDEO_POINTER_CAPABILITIES pCaps, PSTATUS_BLOCK pStatus);

/* ==================== VirtualPox VRP's handlers ==================== */
BOOLEAN VPoxMPVBVAEnable(PVPOXMP_DEVEXT pExt, BOOLEAN bEnable, VBVAENABLERESULT *pResult, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPSetVisibleRegion(uint32_t cRects, RTRECT *pRects, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPHGSMIQueryPortProcs(PVPOXMP_DEVEXT pExt, HGSMIQUERYCPORTPROCS *pProcs, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPHGSMIQueryCallbacks(PVPOXMP_DEVEXT pExt, HGSMIQUERYCALLBACKS *pCallbacks, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPQueryHgsmiInfo(PVPOXMP_DEVEXT pExt, QUERYHGSMIRESULT *pResult, PSTATUS_BLOCK pStatus);
BOOLEAN VPoxMPHgsmiHandlerEnable(PVPOXMP_DEVEXT pExt, HGSMIHANDLERENABLE *pChannel, PSTATUS_BLOCK pStatus);
#ifdef VPOX_WITH_VIDEOHWACCEL
BOOLEAN VPoxMPVhwaQueryInfo(PVPOXMP_DEVEXT pExt, VHWAQUERYINFO *pInfo, PSTATUS_BLOCK pStatus);
#endif
BOOLEAN VPoxMPQueryRegistryFlags(PVPOXMP_DEVEXT pExt, ULONG *pulFlags, PSTATUS_BLOCK pStatus);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_xpdm_VPoxMPInternal_h */
