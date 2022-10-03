/* $Id: VPoxNetCfg-win.h $ */
/** @file
 * Network Configuration API for Windows platforms.
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

#ifndef VPOX_INCLUDED_VPoxNetCfg_win_h
#define VPOX_INCLUDED_VPoxNetCfg_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*
 * Defining VPOXNETCFG_DELAYEDRENAME postpones renaming of host-only adapter
 * connection during adapter creation after it has been assigned with an
 * IP address. This hopefully prevents collisions that may happen when we
 * attempt to rename a connection too early, while its configuration is
 * still being 'committed' by the network setup engine.
 */
#define VPOXNETCFG_DELAYEDRENAME

#include <iprt/win/winsock2.h>
#include <iprt/win/windows.h>
#include <Netcfgn.h>
#include <iprt/win/Setupapi.h>
#include <VPox/cdefs.h>
#include <iprt/types.h>

/** @defgroup grp_vpoxnetcfgwin     The Windows Network Configration Library
 * @{ */

/** @def VPOXNETCFGWIN_DECL
 * The usual declaration wrapper.
 */
#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VPOXDDU
#  define VPOXNETCFGWIN_DECL(a_Type) DECLEXPORT(a_Type)
# else
#  define VPOXNETCFGWIN_DECL(a_Type) DECLIMPORT(a_Type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VPOXNETCFGWIN_DECL(a_Type) a_Type VPOXCALL
#endif

RT_C_DECLS_BEGIN

VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinQueryINetCfg(OUT INetCfg **ppNetCfg,
                          IN BOOL fGetWriteLock,
                          IN LPCWSTR pszwClientDescription,
                          IN DWORD cmsTimeout,
                          OUT LPWSTR *ppszwClientDescription);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinReleaseINetCfg(IN INetCfg *pNetCfg, IN BOOL fHasWriteLock);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinGetComponentByGuid(IN INetCfg *pNc, IN const GUID *pguidClass,
                                                            IN const GUID * pComponentGuid, OUT INetCfgComponent **ppncc);

VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinNetFltInstall(IN INetCfg *pNc, IN LPCWSTR const * apInfFullPaths, IN UINT cInfFullPaths);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinNetFltUninstall(IN INetCfg *pNc);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinNetLwfInstall(IN INetCfg *pNc, IN LPCWSTR const pInfFullPath);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinNetLwfUninstall(IN INetCfg *pNc);

VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinNetAdpUninstall(IN INetCfg *pNc, IN LPCWSTR pwszId);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinNetAdpInstall(IN INetCfg *pNc,IN LPCWSTR const pInfFullPath);

#ifndef VPOXNETCFG_DELAYEDRENAME
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinCreateHostOnlyNetworkInterface(IN LPCWSTR pInfPath, IN bool bIsInfPathFile, IN BSTR bstrDesiredName,
                                                                        OUT GUID *pGuid, OUT BSTR *lppszName,
                                                                        OUT BSTR *pErrMsg);
#else /* VPOXNETCFG_DELAYEDRENAME */
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinCreateHostOnlyNetworkInterface(IN LPCWSTR pInfPath, IN bool bIsInfPathFile, IN BSTR bstrDesiredName,
                                                                        OUT GUID *pGuid, OUT BSTR *lppszId,
                                                                        OUT BSTR *pErrMsg);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinRenameHostOnlyConnection(IN const GUID *pGuid, IN LPCWSTR pszId,  OUT BSTR *pDevName);
#endif /* VPOXNETCFG_DELAYEDRENAME */
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinUpdateHostOnlyNetworkInterface(LPCWSTR pcsxwInf, BOOL *pbRebootRequired, LPCWSTR pcsxwId);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinRemoveHostOnlyNetworkInterface(IN const GUID *pGUID, OUT BSTR *pErrMsg);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinRemoveAllNetDevicesOfId(IN LPCWSTR lpszPnPId);

typedef enum
{
    VPOXNECTFGWINPROPCHANGE_TYPE_UNDEFINED = 0,
    VPOXNECTFGWINPROPCHANGE_TYPE_DISABLE,
    VPOXNECTFGWINPROPCHANGE_TYPE_ENABLE
} VPOXNECTFGWINPROPCHANGE_TYPE;

VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinPropChangeAllNetDevicesOfId(IN LPCWSTR lpszPnPId, VPOXNECTFGWINPROPCHANGE_TYPE enmPcType);

VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinGenHostOnlyNetworkNetworkIp(OUT PULONG pNetIp, OUT PULONG pNetMask);

typedef struct ADAPTER_SETTINGS
{
    ULONG ip;
    ULONG mask;
    BOOL bDhcp;
} ADAPTER_SETTINGS, *PADAPTER_SETTINGS; /**< I'm not prefixed */

VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinEnableStaticIpConfig(IN const GUID *pGuid, IN ULONG ip, IN ULONG mask);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinGetAdapterSettings(IN const GUID * pGuid, OUT PADAPTER_SETTINGS pSettings);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinEnableDynamicIpConfig(IN const GUID *pGuid);
VPOXNETCFGWIN_DECL(HRESULT) VPoxNetCfgWinDhcpRediscover(IN const GUID *pGuid);


typedef VOID (*LOG_ROUTINE)(LPCSTR szString); /**< I'm not prefixed. */
VPOXNETCFGWIN_DECL(VOID) VPoxNetCfgWinSetLogging(IN LOG_ROUTINE pfnLog);

RT_C_DECLS_END

/** @} */

#endif /* !VPOX_INCLUDED_VPoxNetCfg_win_h */

