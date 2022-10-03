/* $Id: VPoxServiceInternal.h $ */
/** @file
 * VPoxService - Guest Additions Services.
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
 */

#ifndef GA_INCLUDED_SRC_common_VPoxService_VPoxServiceInternal_h
#define GA_INCLUDED_SRC_common_VPoxService_VPoxServiceInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <stdio.h>
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
# include <process.h> /* Needed for file version information. */
#endif

#include <iprt/list.h>
#include <iprt/critsect.h>
#include <iprt/path.h> /* RTPATH_MAX */
#include <iprt/stdarg.h>

#include <VPox/VPoxGuestLib.h>
#include <VPox/HostServices/GuestControlSvc.h>


#if !defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
/** Special argv[1] value that indicates that argv is UTF-8.
 * This causes RTR3Init to be called with RTR3INIT_FLAGS_UTF8_ARGV and helps
 * work around potential issues caused by a user's locale config not being
 * UTF-8.  See @bugref{10153}.
 *
 * @note We don't need this on windows and it would be harmful to enable it
 *       as the argc/argv vs __argc/__argv comparison would fail and we would
 *       not use the unicode command line to create a UTF-8 argv.  Since the
 *       original argv is ANSI, it may be missing codepoints not present in
 *       the ANSI code page of the process. */
# define VPOXSERVICE_ARG1_UTF8_ARGV         "--utf8-argv"
#endif
/** RTProcCreateEx flags corresponding to VPOXSERVICE_ARG1_UTF8_ARGV. */
#ifdef VPOXSERVICE_ARG1_UTF8_ARGV
# define VPOXSERVICE_PROC_F_UTF8_ARGV       RTPROC_FLAGS_UTF8_ARGV
#else
# define VPOXSERVICE_PROC_F_UTF8_ARGV       0
#endif


/**
 * A service descriptor.
 */
typedef struct
{
    /** The short service name. */
    const char *pszName;
    /** The longer service name. */
    const char *pszDescription;
    /** The usage options stuff for the --help screen. */
    const char *pszUsage;
    /** The option descriptions for the --help screen. */
    const char *pszOptions;

    /**
     * Called before parsing arguments.
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnPreInit)(void);

    /**
     * Tries to parse the given command line option.
     *
     * @returns 0 if we parsed, -1 if it didn't and anything else means exit.
     * @param   ppszShort   If not NULL it points to the short option iterator. a short argument.
     *                      If NULL examine argv[*pi].
     * @param   argc        The argument count.
     * @param   argv        The argument vector.
     * @param   pi          The argument vector index. Update if any value(s) are eaten.
     */
    DECLCALLBACKMEMBER(int, pfnOption)(const char **ppszShort, int argc, char **argv, int *pi);

    /**
     * Called before parsing arguments.
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit)(void);

    /** Called from the worker thread.
     *
     * @returns VPox status code.
     * @retval  VINF_SUCCESS if exitting because *pfShutdown was set.
     * @param   pfShutdown      Pointer to a per service termination flag to check
     *                          before and after blocking.
     */
    DECLCALLBACKMEMBER(int, pfnWorker)(bool volatile *pfShutdown);

    /**
     * Stops a service.
     */
    DECLCALLBACKMEMBER(void, pfnStop)(void);

    /**
     * Does termination cleanups.
     *
     * @remarks This may be called even if pfnInit hasn't been called!
     */
    DECLCALLBACKMEMBER(void, pfnTerm)(void);
} VPOXSERVICE;
/** Pointer to a VPOXSERVICE. */
typedef VPOXSERVICE *PVPOXSERVICE;
/** Pointer to a const VPOXSERVICE. */
typedef VPOXSERVICE const *PCVPOXSERVICE;

/* Default call-backs for services which do not need special behaviour. */
DECLCALLBACK(int)  VGSvcDefaultPreInit(void);
DECLCALLBACK(int)  VGSvcDefaultOption(const char **ppszShort, int argc, char **argv, int *pi);
DECLCALLBACK(int)  VGSvcDefaultInit(void);
DECLCALLBACK(void) VGSvcDefaultTerm(void);

/** The service name.
 * @note Used on windows to name the service as well as the global mutex. */
#define VPOXSERVICE_NAME            "VPoxService"

#ifdef RT_OS_WINDOWS
/** The friendly service name. */
# define VPOXSERVICE_FRIENDLY_NAME  "VirtualPox Guest Additions Service"
/** The service description (only W2K+ atm) */
# define VPOXSERVICE_DESCRIPTION    "Manages VM runtime information, time synchronization, guest control execution and miscellaneous utilities for guest operating systems."
/** The following constant may be defined by including NtStatus.h. */
# define STATUS_SUCCESS             ((NTSTATUS)0x00000000L)
#endif /* RT_OS_WINDOWS */

#ifdef VPOX_WITH_GUEST_PROPS
/**
 * A guest property cache.
 */
typedef struct VPOXSERVICEVEPROPCACHE
{
    /** The client ID for HGCM communication. */
    uint32_t        uClientID;
    /** Head in a list of VPOXSERVICEVEPROPCACHEENTRY nodes. */
    RTLISTANCHOR    NodeHead;
    /** Critical section for thread-safe use. */
    RTCRITSECT      CritSect;
} VPOXSERVICEVEPROPCACHE;
/** Pointer to a guest property cache. */
typedef VPOXSERVICEVEPROPCACHE *PVPOXSERVICEVEPROPCACHE;

/**
 * An entry in the property cache (VPOXSERVICEVEPROPCACHE).
 */
typedef struct VPOXSERVICEVEPROPCACHEENTRY
{
    /** Node to successor.
     * @todo r=bird: This is not really the node to the successor, but
     *       rather the OUR node in the list.  If it helps, remember that
     *       its a doubly linked list. */
    RTLISTNODE  NodeSucc;
    /** Name (and full path) of guest property. */
    char       *pszName;
    /** The last value stored (for reference). */
    char       *pszValue;
    /** Reset value to write if property is temporary.  If NULL, it will be
     *  deleted. */
    char       *pszValueReset;
    /** Flags. */
    uint32_t    fFlags;
} VPOXSERVICEVEPROPCACHEENTRY;
/** Pointer to a cached guest property. */
typedef VPOXSERVICEVEPROPCACHEENTRY *PVPOXSERVICEVEPROPCACHEENTRY;

#endif /* VPOX_WITH_GUEST_PROPS */

RT_C_DECLS_BEGIN

extern char        *g_pszProgName;
extern unsigned     g_cVerbosity;
extern char         g_szLogFile[RTPATH_MAX + 128];
extern uint32_t     g_DefaultInterval;
extern VPOXSERVICE  g_TimeSync;
#ifdef VPOX_WITH_VPOXSERVICE_CLIPBOARD
extern VPOXSERVICE  g_Clipboard;
#endif
extern VPOXSERVICE  g_Control;
extern VPOXSERVICE  g_VMInfo;
extern VPOXSERVICE  g_CpuHotPlug;
#ifdef VPOX_WITH_VPOXSERVICE_MANAGEMENT
extern VPOXSERVICE  g_MemBalloon;
extern VPOXSERVICE  g_VMStatistics;
#endif
#ifdef VPOX_WITH_VPOXSERVICE_PAGE_SHARING
extern VPOXSERVICE  g_PageSharing;
#endif
#ifdef VPOX_WITH_SHARED_FOLDERS
extern VPOXSERVICE  g_AutoMount;
#endif
#ifdef DEBUG
extern RTCRITSECT   g_csLog; /* For guest process stdout dumping. */
#endif

extern RTEXITCODE               VGSvcSyntax(const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(1, 2);
extern RTEXITCODE               VGSvcError(const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(1, 2);
extern void                     VGSvcVerbose(unsigned iLevel, const char *pszFormat, ...)  RT_IPRT_FORMAT_ATTR(2, 3);
extern int                      VGSvcLogCreate(const char *pszLogFile);
extern void                     VGSvcLogV(const char *pszFormat, va_list va) RT_IPRT_FORMAT_ATTR(1, 0);
extern void                     VGSvcLogDestroy(void);
extern int                      VGSvcArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32,
                                               uint32_t u32Min, uint32_t u32Max);

/* Exposing the following bits because of windows: */
extern int                      VGSvcStartServices(void);
extern int                      VGSvcStopServices(void);
extern void                     VGSvcMainWait(void);
extern int                      VGSvcReportStatus(VPoxGuestFacilityStatus enmStatus);
#ifdef RT_OS_WINDOWS
extern void                     VGSvcWinResolveApis(void);
extern RTEXITCODE               VGSvcWinInstall(void);
extern RTEXITCODE               VGSvcWinUninstall(void);
extern RTEXITCODE               VGSvcWinEnterCtrlDispatcher(void);
extern void                     VGSvcWinSetStopPendingStatus(uint32_t uCheckPoint);
# ifdef TH32CS_SNAPHEAPLIST
extern decltype(CreateToolhelp32Snapshot)      *g_pfnCreateToolhelp32Snapshot;
extern decltype(Process32First)                *g_pfnProcess32First;
extern decltype(Process32Next)                 *g_pfnProcess32Next;
extern decltype(Module32First)                 *g_pfnModule32First;
extern decltype(Module32Next)                  *g_pfnModule32Next;
# endif
extern decltype(GetSystemTimeAdjustment)       *g_pfnGetSystemTimeAdjustment;
extern decltype(SetSystemTimeAdjustment)       *g_pfnSetSystemTimeAdjustment;
# ifdef IPRT_INCLUDED_nt_nt_h
extern decltype(ZwQuerySystemInformation)      *g_pfnZwQuerySystemInformation;
# endif
extern ULONG (WINAPI *g_pfnGetAdaptersInfo)(struct _IP_ADAPTER_INFO *, PULONG);
#ifdef WINSOCK_VERSION
extern decltype(WSAStartup)                    *g_pfnWSAStartup;
extern decltype(WSACleanup)                    *g_pfnWSACleanup;
extern decltype(WSASocketA)                    *g_pfnWSASocketA;
extern decltype(WSAIoctl)                      *g_pfnWSAIoctl;
extern decltype(WSAGetLastError)               *g_pfnWSAGetLastError;
extern decltype(closesocket)                   *g_pfnclosesocket;
extern decltype(inet_ntoa)                     *g_pfninet_ntoa;
# endif /* WINSOCK_VERSION */

#ifdef SE_INTERACTIVE_LOGON_NAME
extern decltype(LsaNtStatusToWinError)         *g_pfnLsaNtStatusToWinError;
#endif

# ifdef VPOX_WITH_GUEST_PROPS
extern int                      VGSvcVMInfoWinWriteUsers(PVPOXSERVICEVEPROPCACHE pCache, char **ppszUserList, uint32_t *pcUsersInList);
extern int                      VGSvcVMInfoWinGetComponentVersions(uint32_t uClientID);
# endif /* VPOX_WITH_GUEST_PROPS */

#endif /* RT_OS_WINDOWS */

#ifdef VPOX_WITH_VPOXSERVICE_MANAGEMENT
extern uint32_t                 VGSvcBalloonQueryPages(uint32_t cbPage);
#endif
#if defined(VPOX_WITH_VPOXSERVICE_PAGE_SHARING)
extern RTEXITCODE               VGSvcPageSharingWorkerChild(void);
#endif
extern int                      VGSvcVMInfoSignal(void);

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_common_VPoxService_VPoxServiceInternal_h */

