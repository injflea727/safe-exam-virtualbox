/* $Id: VPoxDrvCfg-win.h $ */
/** @file
 * Windows Driver Manipulation API.
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

#ifndef VPOX_INCLUDED_VPoxDrvCfg_win_h
#define VPOX_INCLUDED_VPoxDrvCfg_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>
#include <VPox/cdefs.h>

RT_C_DECLS_BEGIN

#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VPOXDRVCFG
#  define VPOXDRVCFG_DECL(a_Type) DECLEXPORT(a_Type)
# else
#  define VPOXDRVCFG_DECL(a_Type) DECLIMPORT(a_Type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VPOXDRVCFG_DECL(a_Type) a_Type VPOXCALL
#endif

typedef enum
{
    VPOXDRVCFG_LOG_SEVERITY_FLOW = 1,
    VPOXDRVCFG_LOG_SEVERITY_REGULAR,
    VPOXDRVCFG_LOG_SEVERITY_REL
} VPOXDRVCFG_LOG_SEVERITY;

typedef DECLCALLBACK(void) FNVPOXDRVCFG_LOG(VPOXDRVCFG_LOG_SEVERITY enmSeverity, char *pszMsg, void *pvContext);
typedef FNVPOXDRVCFG_LOG *PFNVPOXDRVCFG_LOG;

VPOXDRVCFG_DECL(void) VPoxDrvCfgLoggerSet(PFNVPOXDRVCFG_LOG pfnLog, void *pvLog);

typedef DECLCALLBACK(void) FNVPOXDRVCFG_PANIC(void * pvPanic);
typedef FNVPOXDRVCFG_PANIC *PFNVPOXDRVCFG_PANIC;
VPOXDRVCFG_DECL(void) VPoxDrvCfgPanicSet(PFNVPOXDRVCFG_PANIC pfnPanic, void *pvPanic);

/* Driver package API*/
VPOXDRVCFG_DECL(HRESULT) VPoxDrvCfgInfInstall(IN LPCWSTR lpszInfPath);
VPOXDRVCFG_DECL(HRESULT) VPoxDrvCfgInfUninstall(IN LPCWSTR lpszInfPath, IN DWORD fFlags);
VPOXDRVCFG_DECL(HRESULT) VPoxDrvCfgInfUninstallAllSetupDi(IN const GUID * pGuidClass, IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD fFlags);
VPOXDRVCFG_DECL(HRESULT) VPoxDrvCfgInfUninstallAllF(IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD fFlags);

/* Service API */
VPOXDRVCFG_DECL(HRESULT) VPoxDrvCfgSvcStart(LPCWSTR lpszSvcName);

HRESULT VPoxDrvCfgDrvUpdate(LPCWSTR pcszwHwId, LPCWSTR pcsxwInf, BOOL *pbRebootRequired);

RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_VPoxDrvCfg_win_h */

