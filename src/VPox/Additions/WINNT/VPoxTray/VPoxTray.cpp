/* $Id: VPoxTray.cpp $ */
/** @file
 * VPoxTray - Guest Additions Tray Application
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <package-generated.h>
#include "product-generated.h"

#include "VPoxTray.h"
#include "VPoxTrayMsg.h"
#include "VPoxHelpers.h"
#include "VPoxSeamless.h"
#include "VPoxClipboard.h"
#include "VPoxDisplay.h"
#include "VPoxVRDP.h"
#include "VPoxHostVersion.h"
#ifdef VPOX_WITH_DRAG_AND_DROP
# include "VPoxDnD.h"
#endif
#include "VPoxIPC.h"
#include "VPoxLA.h"
#include <VPoxHook.h>

#include <sddl.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/getopt.h>
#include <iprt/ldr.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/utf16.h>

#ifdef DEBUG
# define LOG_ENABLED
# define LOG_GROUP LOG_GROUP_DEFAULT
#endif
#include <VPox/log.h>

#include <VPox/err.h>

/* Default desktop state tracking */
#include <Wtsapi32.h>

/*
 * St (session [state] tracking) functionality API
 *
 * !!!NOTE: this API is NOT thread-safe!!!
 * it is supposed to be called & used from within the window message handler thread
 * of the window passed to vpoxStInit */
static int vpoxStInit(HWND hWnd);
static void vpoxStTerm(void);
/* @returns true on "IsActiveConsole" state change */
static BOOL vpoxStHandleEvent(WPARAM EventID);
static BOOL vpoxStIsActiveConsole();
static BOOL vpoxStCheckTimer(WPARAM wEvent);

/*
 * Dt (desktop [state] tracking) functionality API
 *
 * !!!NOTE: this API is NOT thread-safe!!!
 * */
static int vpoxDtInit();
static void vpoxDtTerm();
/* @returns true on "IsInputDesktop" state change */
static BOOL vpoxDtHandleEvent();
/* @returns true iff the application (VPoxTray) desktop is input */
static BOOL vpoxDtIsInputDesktop();
static HANDLE vpoxDtGetNotifyEvent();
static BOOL vpoxDtCheckTimer(WPARAM wParam);

/* caps API */
#define VPOXCAPS_ENTRY_IDX_SEAMLESS  0
#define VPOXCAPS_ENTRY_IDX_GRAPHICS  1
#define VPOXCAPS_ENTRY_IDX_COUNT     2

typedef enum VPOXCAPS_ENTRY_FUNCSTATE
{
    /* the cap is unsupported */
    VPOXCAPS_ENTRY_FUNCSTATE_UNSUPPORTED = 0,
    /* the cap is supported */
    VPOXCAPS_ENTRY_FUNCSTATE_SUPPORTED,
    /* the cap functionality is started, it can be disabled however if its AcState is not ACQUIRED */
    VPOXCAPS_ENTRY_FUNCSTATE_STARTED,
} VPOXCAPS_ENTRY_FUNCSTATE;


static void VPoxCapsEntryFuncStateSet(uint32_t iCup, VPOXCAPS_ENTRY_FUNCSTATE enmFuncState);
static int VPoxCapsInit();
static int VPoxCapsReleaseAll();
static void VPoxCapsTerm();
static BOOL VPoxCapsEntryIsAcquired(uint32_t iCap);
static BOOL VPoxCapsEntryIsEnabled(uint32_t iCap);
static BOOL VPoxCapsCheckTimer(WPARAM wParam);
static int VPoxCapsEntryRelease(uint32_t iCap);
static int VPoxCapsEntryAcquire(uint32_t iCap);
static int VPoxCapsAcquireAllSupported();

/* console-related caps API */
static BOOL VPoxConsoleIsAllowed();
static void VPoxConsoleEnable(BOOL fEnable);
static void VPoxConsoleCapSetSupported(uint32_t iCap, BOOL fSupported);

static void VPoxGrapicsSetSupported(BOOL fSupported);


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int vpoxTrayCreateTrayIcon(void);
static LRESULT CALLBACK vpoxToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Global message handler prototypes. */
static int vpoxTrayGlMsgTaskbarCreated(WPARAM lParam, LPARAM wParam);
/*static int vpoxTrayGlMsgShowBalloonMsg(WPARAM lParam, LPARAM wParam);*/

static int VPoxAcquireGuestCaps(uint32_t fOr, uint32_t fNot, bool fCfg);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
int                   g_cVerbosity             = 0;
HANDLE                g_hStopSem;
HANDLE                g_hSeamlessWtNotifyEvent = 0;
HANDLE                g_hSeamlessKmNotifyEvent = 0;
HINSTANCE             g_hInstance              = NULL;
HWND                  g_hwndToolWindow;
NOTIFYICONDATA        g_NotifyIconData;

uint32_t              g_fGuestDisplaysChanged = 0;

static PRTLOGGER      g_pLoggerRelease = NULL;
static uint32_t       g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t       g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t       g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

#ifdef DEBUG_andy
static VPOXSERVICEINFO g_aServices[] =
{
    { &g_SvcDescClipboard,      NIL_RTTHREAD, NULL, false, false, false, false, true }
};
#else
/**
 * The details of the services that has been compiled in.
 */
static VPOXSERVICEINFO g_aServices[] =
{
    { &g_SvcDescDisplay,        NIL_RTTHREAD, NULL, false, false, false, false, true },
#ifdef VPOX_WITH_SHARED_CLIPBOARD
    { &g_SvcDescClipboard,      NIL_RTTHREAD, NULL, false, false, false, false, true },
#endif
    { &g_SvcDescSeamless,       NIL_RTTHREAD, NULL, false, false, false, false, true },
    { &g_SvcDescVRDP,           NIL_RTTHREAD, NULL, false, false, false, false, true },
    { &g_SvcDescIPC,            NIL_RTTHREAD, NULL, false, false, false, false, true },
    { &g_SvcDescLA,             NIL_RTTHREAD, NULL, false, false, false, false, true },
#ifdef VPOX_WITH_DRAG_AND_DROP
    { &g_SvcDescDnD,            NIL_RTTHREAD, NULL, false, false, false, false, true }
#endif
};
#endif

/* The global message table. */
static VPOXGLOBALMESSAGE g_vpoxGlobalMessageTable[] =
{
    /* Windows specific stuff. */
    {
        "TaskbarCreated",
        vpoxTrayGlMsgTaskbarCreated
    },

    /* VPoxTray specific stuff. */
    /** @todo Add new messages here! */

    {
        NULL
    }
};

/**
 * Gets called whenever the Windows main taskbar
 * get (re-)created. Nice to install our tray icon.
 *
 * @return  IPRT status code.
 * @param   wParam
 * @param   lParam
 */
static int vpoxTrayGlMsgTaskbarCreated(WPARAM wParam, LPARAM lParam)
{
    RT_NOREF(wParam, lParam);
    return vpoxTrayCreateTrayIcon();
}

static int vpoxTrayCreateTrayIcon(void)
{
    HICON hIcon = LoadIcon(g_hInstance, "IDI_ICON1"); /* see Artwork/win/TemplateR3.rc */
    if (hIcon == NULL)
    {
        DWORD dwErr = GetLastError();
        LogFunc(("Could not load tray icon, error %08X\n", dwErr));
        return RTErrConvertFromWin32(dwErr);
    }

    /* Prepare the system tray icon. */
    RT_ZERO(g_NotifyIconData);
    g_NotifyIconData.cbSize           = NOTIFYICONDATA_V1_SIZE; // sizeof(NOTIFYICONDATA);
    g_NotifyIconData.hWnd             = g_hwndToolWindow;
    g_NotifyIconData.uID              = ID_TRAYICON;
    g_NotifyIconData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_NotifyIconData.uCallbackMessage = WM_VPOXTRAY_TRAY_ICON;
    g_NotifyIconData.hIcon            = hIcon;

    sprintf(g_NotifyIconData.szTip, "%s Guest Additions %d.%d.%dr%d",
            VPOX_PRODUCT, VPOX_VERSION_MAJOR, VPOX_VERSION_MINOR, VPOX_VERSION_BUILD, VPOX_SVN_REV);

    int rc = VINF_SUCCESS;
    if (!Shell_NotifyIcon(NIM_ADD, &g_NotifyIconData))
    {
        DWORD dwErr = GetLastError();
        LogFunc(("Could not create tray icon, error=%ld\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
        RT_ZERO(g_NotifyIconData);
    }

    if (hIcon)
        DestroyIcon(hIcon);
    return rc;
}

static void vpoxTrayRemoveTrayIcon(void)
{
    if (g_NotifyIconData.cbSize > 0)
    {
        /* Remove the system tray icon and refresh system tray. */
        Shell_NotifyIcon(NIM_DELETE, &g_NotifyIconData);
        HWND hTrayWnd = FindWindow("Shell_TrayWnd", NULL); /* We assume we only have one tray atm. */
        if (hTrayWnd)
        {
            HWND hTrayNotifyWnd = FindWindowEx(hTrayWnd, 0, "TrayNotifyWnd", NULL);
            if (hTrayNotifyWnd)
                SendMessage(hTrayNotifyWnd, WM_PAINT, 0, NULL);
        }
        RT_ZERO(g_NotifyIconData);
    }
}

/**
 * The service thread.
 *
 * @returns Whatever the worker function returns.
 * @param   ThreadSelf      My thread handle.
 * @param   pvUser          The service index.
 */
static DECLCALLBACK(int) vpoxTrayServiceThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVPOXSERVICEINFO pSvc = (PVPOXSERVICEINFO)pvUser;
    AssertPtr(pSvc);

#ifndef RT_OS_WINDOWS
    /*
     * Block all signals for this thread. Only the main thread will handle signals.
     */
    sigset_t signalMask;
    sigfillset(&signalMask);
    pthread_sigmask(SIG_BLOCK, &signalMask, NULL);
#endif

    int rc = pSvc->pDesc->pfnWorker(pSvc->pInstance, &pSvc->fShutdown);
    ASMAtomicXchgBool(&pSvc->fShutdown, true);
    RTThreadUserSignal(ThreadSelf);

    LogFunc(("Worker for '%s' ended with %Rrc\n", pSvc->pDesc->pszName, rc));
    return rc;
}

static int vpoxTrayServicesStart(PVPOXSERVICEENV pEnv)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);

    LogRel(("Starting services ...\n"));

    int rc = VINF_SUCCESS;

    for (unsigned i = 0; i < RT_ELEMENTS(g_aServices); i++)
    {
        PVPOXSERVICEINFO pSvc = &g_aServices[i];
        LogRel(("Starting service '%s' ...\n", pSvc->pDesc->pszName));

        pSvc->hThread   = NIL_RTTHREAD;
        pSvc->pInstance = NULL;
        pSvc->fStarted  = false;
        pSvc->fShutdown = false;

        int rc2 = VINF_SUCCESS;

        if (pSvc->pDesc->pfnInit)
            rc2 = pSvc->pDesc->pfnInit(pEnv, &pSvc->pInstance);

        if (RT_FAILURE(rc2))
        {
            switch (rc2)
            {
                case VERR_NOT_SUPPORTED:
                    LogRel(("Service '%s' is not supported on this system\n", pSvc->pDesc->pszName));
                    rc2 = VINF_SUCCESS; /* Keep going. */
                    break;

                case VERR_HGCM_SERVICE_NOT_FOUND:
                    LogRel(("Service '%s' is not available on the host\n", pSvc->pDesc->pszName));
                    rc2 = VINF_SUCCESS; /* Keep going. */
                    break;

                default:
                    LogRel(("Failed to initialize service '%s', rc=%Rrc\n", pSvc->pDesc->pszName, rc2));
                    break;
            }
        }
        else
        {
            if (pSvc->pDesc->pfnWorker)
            {
                rc2 = RTThreadCreate(&pSvc->hThread, vpoxTrayServiceThread, pSvc /* pvUser */,
                                     0 /* Default stack size */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, pSvc->pDesc->pszName);
                if (RT_SUCCESS(rc2))
                {
                    pSvc->fStarted = true;

                    RTThreadUserWait(pSvc->hThread, 30 * 1000 /* Timeout in ms */);
                    if (pSvc->fShutdown)
                    {
                        LogRel(("Service '%s' failed to start!\n", pSvc->pDesc->pszName));
                        rc = VERR_GENERAL_FAILURE;
                    }
                    else
                        LogRel(("Service '%s' started\n", pSvc->pDesc->pszName));
                }
                else
                {
                    LogRel(("Failed to start thread for service '%s': %Rrc\n", rc2));
                    if (pSvc->pDesc->pfnDestroy)
                        pSvc->pDesc->pfnDestroy(pSvc->pInstance);
                }
            }
        }

        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_SUCCESS(rc))
        LogRel(("All services started\n"));
    else
        LogRel(("Services started, but some with errors\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vpoxTrayServicesStop(VPOXSERVICEENV *pEnv)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);

    LogRel2(("Stopping all services ...\n"));

    /*
     * Signal all the services.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aServices); i++)
        ASMAtomicWriteBool(&g_aServices[i].fShutdown, true);

    /*
     * Do the pfnStop callback on all running services.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aServices); i++)
    {
        PVPOXSERVICEINFO pSvc = &g_aServices[i];
        if (   pSvc->fStarted
            && pSvc->pDesc->pfnStop)
        {
            LogRel2(("Calling stop function for service '%s' ...\n", pSvc->pDesc->pszName));
            int rc2 = pSvc->pDesc->pfnStop(pSvc->pInstance);
            if (RT_FAILURE(rc2))
                LogRel(("Failed to stop service '%s': %Rrc\n", pSvc->pDesc->pszName, rc2));
        }
    }

    LogRel2(("All stop functions for services called\n"));

    int rc = VINF_SUCCESS;

    /*
     * Wait for all the service threads to complete.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aServices); i++)
    {
        PVPOXSERVICEINFO pSvc = &g_aServices[i];
        if (!pSvc->fEnabled) /* Only stop services which were started before. */
            continue;

        if (pSvc->hThread != NIL_RTTHREAD)
        {
            LogRel2(("Waiting for service '%s' to stop ...\n", pSvc->pDesc->pszName));
            int rc2 = VINF_SUCCESS;
            for (int j = 0; j < 30; j++) /* Wait 30 seconds in total */
            {
                rc2 = RTThreadWait(pSvc->hThread, 1000 /* Wait 1 second */, NULL);
                if (RT_SUCCESS(rc2))
                    break;
            }
            if (RT_FAILURE(rc2))
            {
                LogRel(("Service '%s' failed to stop (%Rrc)\n", pSvc->pDesc->pszName, rc2));
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }

        if (   pSvc->pDesc->pfnDestroy
            && pSvc->pInstance) /* pInstance might be NULL if initialization of a service failed. */
        {
            LogRel2(("Terminating service '%s' ...\n", pSvc->pDesc->pszName));
            pSvc->pDesc->pfnDestroy(pSvc->pInstance);
        }
    }

    if (RT_SUCCESS(rc))
        LogRel(("All services stopped\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vpoxTrayRegisterGlobalMessages(PVPOXGLOBALMESSAGE pTable)
{
    int rc = VINF_SUCCESS;
    if (pTable == NULL) /* No table to register? Skip. */
        return rc;
    while (   pTable->pszName
           && RT_SUCCESS(rc))
    {
        /* Register global accessible window messages. */
        pTable->uMsgID = RegisterWindowMessage(TEXT(pTable->pszName));
        if (!pTable->uMsgID)
        {
            DWORD dwErr = GetLastError();
            Log(("Registering global message \"%s\" failed, error = %08X\n", dwErr));
            rc = RTErrConvertFromWin32(dwErr);
        }

        /* Advance to next table element. */
        pTable++;
    }
    return rc;
}

static bool vpoxTrayHandleGlobalMessages(PVPOXGLOBALMESSAGE pTable, UINT uMsg,
                                         WPARAM wParam, LPARAM lParam)
{
    if (pTable == NULL)
        return false;
    while (pTable && pTable->pszName)
    {
        if (pTable->uMsgID == uMsg)
        {
            if (pTable->pfnHandler)
                pTable->pfnHandler(wParam, lParam);
            return true;
        }

        /* Advance to next table element. */
        pTable++;
    }
    return false;
}

/**
 * Release logger callback.
 *
 * @return  IPRT status code.
 * @param   pLoggerRelease
 * @param   enmPhase
 * @param   pfnLog
 */
static void vpoxTrayLogHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* Some introductory information. */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            pfnLog(pLoggerRelease,
                   "VPoxTray %s r%s %s (%s %s) release log\n"
                   "Log opened %s\n",
                   RTBldCfgVersion(), RTBldCfgRevisionStr(), VPOX_BUILD_TARGET,
                   __DATE__, __TIME__, szTmp);

            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Version: %s\n", szTmp);
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Service Pack: %s\n", szTmp);

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pLoggerRelease,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VPOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VPOX_PACKAGE_STRING);
            break;
        }

        case RTLOGPHASE_PREROTATE:
            pfnLog(pLoggerRelease, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pLoggerRelease, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pLoggerRelease, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */;
    }
}

/**
 * Creates the default release logger outputting to the specified file.
 *
 * @return  IPRT status code.
 * @param   pszLogFile          Path to log file to use.
 */
static int vpoxTrayLogCreate(const char *pszLogFile)
{
    /* Create release logger (stdout + file). */
    static const char * const s_apszGroups[] = VPOX_LOGGROUP_NAMES;
    static const char s_szEnvVarPfx[] = "VPOXTRAY_RELEASE_LOG";

    RTERRINFOSTATIC ErrInfo;
    int rc = RTLogCreateEx(&g_pLoggerRelease,
                           RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG | RTLOGFLAGS_USECRLF,
                           "all.e", s_szEnvVarPfx, RT_ELEMENTS(s_apszGroups), s_apszGroups, UINT32_MAX,
                           RTLOGDEST_STDOUT,
                           vpoxTrayLogHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                           RTErrInfoInitStatic(&ErrInfo), "%s", pszLogFile ? pszLogFile : "");
    if (RT_SUCCESS(rc))
    {
        /* Register this logger as the release logger. */
        RTLogRelSetDefaultInstance(g_pLoggerRelease);

        /* Register this logger as the _debug_ logger. */
        RTLogSetDefaultInstance(g_pLoggerRelease);

        const char *apszGroups[] = { "all", "guest_dnd" }; /* All groups we want to enable logging for VPoxTray. */
        char        szGroupSettings[_1K];

        szGroupSettings[0] = '\0';

        for (size_t i = 0; i < RT_ELEMENTS(apszGroups); i++)
        {
            if (i > 0)
                rc = RTStrCat(szGroupSettings, sizeof(szGroupSettings), "+");
            if (RT_SUCCESS(rc))
                rc = RTStrCat(szGroupSettings, sizeof(szGroupSettings), apszGroups[i]);
            if (RT_FAILURE(rc))
                break;

            switch (g_cVerbosity)
            {
                case 1:
                    rc = RTStrCat(szGroupSettings, sizeof(szGroupSettings), ".e.l");
                    break;

                case 2:
                    rc = RTStrCat(szGroupSettings, sizeof(szGroupSettings), ".e.l.l2");
                    break;

                case 3:
                    rc = RTStrCat(szGroupSettings, sizeof(szGroupSettings), ".e.l.l2.l3");
                    break;

                case 4:
                    RT_FALL_THROUGH();
                default:
                    rc = RTStrCat(szGroupSettings, sizeof(szGroupSettings), ".e.l.l2.l3.f");
                    break;
            }

            if (RT_FAILURE(rc))
                break;
        }

        LogRel(("Verbose log settings are: %s\n", szGroupSettings));

        if (RT_SUCCESS(rc))
            rc = RTLogGroupSettings(g_pLoggerRelease, szGroupSettings);
        if (RT_FAILURE(rc))
            RTMsgError("Setting log group settings failed, rc=%Rrc\n", rc);

        /* Explicitly flush the log in case of VPOXTRAY_RELEASE_LOG=buffered. */
        RTLogFlush(g_pLoggerRelease);
    }
    else
        VPoxTrayShowError(ErrInfo.szMsg);

    return rc;
}

static void vpoxTrayLogDestroy(void)
{
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
}

/**
 * Displays an error message.
 *
 * @returns RTEXITCODE_FAILURE.
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
RTEXITCODE VPoxTrayShowError(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    AssertPtr(psz);
    LogRel(("Error: %s", psz));

    MessageBox(GetDesktopWindow(), psz, "VPoxTray - Error", MB_OK | MB_ICONERROR);

    RTStrFree(psz);

    return RTEXITCODE_FAILURE;
}

static void vpoxTrayDestroyToolWindow(void)
{
    if (g_hwndToolWindow)
    {
        Log(("Destroying tool window ...\n"));

        /* Destroy the tool window. */
        DestroyWindow(g_hwndToolWindow);
        g_hwndToolWindow = NULL;

        UnregisterClass("VPoxTrayToolWndClass", g_hInstance);
    }
}

static int vpoxTrayCreateToolWindow(void)
{
    DWORD dwErr = ERROR_SUCCESS;

    /* Create a custom window class. */
    WNDCLASSEX wc = { 0 };
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = (WNDPROC)vpoxToolWndProc;
    wc.hInstance     = g_hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "VPoxTrayToolWndClass";

    if (!RegisterClassEx(&wc))
    {
        dwErr = GetLastError();
        Log(("Registering invisible tool window failed, error = %08X\n", dwErr));
    }
    else
    {
        /*
         * Create our (invisible) tool window.
         * Note: The window name ("VPoxTrayToolWnd") and class ("VPoxTrayToolWndClass") is
         * needed for posting globally registered messages to VPoxTray and must not be
         * changed! Otherwise things get broken!
         *
         */
        g_hwndToolWindow = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                         "VPoxTrayToolWndClass", "VPoxTrayToolWnd",
                                         WS_POPUPWINDOW,
                                         -200, -200, 100, 100, NULL, NULL, g_hInstance, NULL);
        if (!g_hwndToolWindow)
        {
            dwErr = GetLastError();
            Log(("Creating invisible tool window failed, error = %08X\n", dwErr));
        }
        else
        {
            /* Reload the cursor(s). */
            hlpReloadCursor();

            Log(("Invisible tool window handle = %p\n", g_hwndToolWindow));
        }
    }

    if (dwErr != ERROR_SUCCESS)
         vpoxTrayDestroyToolWindow();
    return RTErrConvertFromWin32(dwErr);
}

static int vpoxTraySetupSeamless(void)
{
    /* We need to setup a security descriptor to allow other processes modify access to the seamless notification event semaphore. */
    SECURITY_ATTRIBUTES     SecAttr;
    DWORD                   dwErr = ERROR_SUCCESS;
    char                    secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    BOOL                    fRC;

    SecAttr.nLength              = sizeof(SecAttr);
    SecAttr.bInheritHandle       = FALSE;
    SecAttr.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(SecAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    fRC = SetSecurityDescriptorDacl(SecAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
    if (!fRC)
    {
        dwErr = GetLastError();
        Log(("SetSecurityDescriptorDacl failed with last error = %08X\n", dwErr));
    }
    else
    {
        /* For Vista and up we need to change the integrity of the security descriptor, too. */
        uint64_t const uNtVersion = RTSystemGetNtVersion();
        if (uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(6, 0, 0))
        {
            BOOL (WINAPI * pfnConvertStringSecurityDescriptorToSecurityDescriptorA)(LPCSTR StringSecurityDescriptor, DWORD StringSDRevision, PSECURITY_DESCRIPTOR  *SecurityDescriptor, PULONG  SecurityDescriptorSize);
            *(void **)&pfnConvertStringSecurityDescriptorToSecurityDescriptorA =
                RTLdrGetSystemSymbol("advapi32.dll", "ConvertStringSecurityDescriptorToSecurityDescriptorA");
            Log(("pfnConvertStringSecurityDescriptorToSecurityDescriptorA = %x\n", pfnConvertStringSecurityDescriptorToSecurityDescriptorA));
            if (pfnConvertStringSecurityDescriptorToSecurityDescriptorA)
            {
                PSECURITY_DESCRIPTOR    pSD;
                PACL                    pSacl          = NULL;
                BOOL                    fSaclPresent   = FALSE;
                BOOL                    fSaclDefaulted = FALSE;

                fRC = pfnConvertStringSecurityDescriptorToSecurityDescriptorA("S:(ML;;NW;;;LW)", /* this means "low integrity" */
                                                                              SDDL_REVISION_1, &pSD, NULL);
                if (!fRC)
                {
                    dwErr = GetLastError();
                    Log(("ConvertStringSecurityDescriptorToSecurityDescriptorA failed with last error = %08X\n", dwErr));
                }
                else
                {
                    fRC = GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted);
                    if (!fRC)
                    {
                        dwErr = GetLastError();
                        Log(("GetSecurityDescriptorSacl failed with last error = %08X\n", dwErr));
                    }
                    else
                    {
                        fRC = SetSecurityDescriptorSacl(SecAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
                        if (!fRC)
                        {
                            dwErr = GetLastError();
                            Log(("SetSecurityDescriptorSacl failed with last error = %08X\n", dwErr));
                        }
                    }
                }
            }
        }

        if (   dwErr == ERROR_SUCCESS
            && uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(5, 0, 0)) /* Only for W2K and up ... */
        {
            g_hSeamlessWtNotifyEvent = CreateEvent(&SecAttr, FALSE, FALSE, VPOXHOOK_GLOBAL_WT_EVENT_NAME);
            if (g_hSeamlessWtNotifyEvent == NULL)
            {
                dwErr = GetLastError();
                Log(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            }

            g_hSeamlessKmNotifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (g_hSeamlessKmNotifyEvent == NULL)
            {
                dwErr = GetLastError();
                Log(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            }
        }
    }
    return RTErrConvertFromWin32(dwErr);
}

static void vpoxTrayShutdownSeamless(void)
{
    if (g_hSeamlessWtNotifyEvent)
    {
        CloseHandle(g_hSeamlessWtNotifyEvent);
        g_hSeamlessWtNotifyEvent = NULL;
    }

    if (g_hSeamlessKmNotifyEvent)
    {
        CloseHandle(g_hSeamlessKmNotifyEvent);
        g_hSeamlessKmNotifyEvent = NULL;
    }
}

static void VPoxTrayCheckDt()
{
    BOOL fOldAllowedState = VPoxConsoleIsAllowed();
    if (vpoxDtHandleEvent())
    {
        if (!VPoxConsoleIsAllowed() != !fOldAllowedState)
            VPoxConsoleEnable(!fOldAllowedState);
    }
}

static int vpoxTrayServiceMain(void)
{
    int rc = VINF_SUCCESS;
    LogFunc(("Entering vpoxTrayServiceMain\n"));

    g_hStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_hStopSem == NULL)
    {
        rc = RTErrConvertFromWin32(GetLastError());
        LogFunc(("CreateEvent for stopping VPoxTray failed, rc=%Rrc\n", rc));
    }
    else
    {
        /*
         * Start services listed in the vpoxServiceTable.
         */
        VPOXSERVICEENV svcEnv;
        svcEnv.hInstance = g_hInstance;

        /* Initializes disp-if to default (XPDM) mode. */
        VPoxDispIfInit(&svcEnv.dispIf); /* Cannot fail atm. */
    #ifdef VPOX_WITH_WDDM
        /*
         * For now the display mode will be adjusted to WDDM mode if needed
         * on display service initialization when it detects the display driver type.
         */
    #endif

        /* Finally start all the built-in services! */
        rc = vpoxTrayServicesStart(&svcEnv);
        if (RT_FAILURE(rc))
        {
            /* Terminate service if something went wrong. */
            vpoxTrayServicesStop(&svcEnv);
        }
        else
        {
            uint64_t const uNtVersion = RTSystemGetNtVersion();
            rc = vpoxTrayCreateTrayIcon();
            if (   RT_SUCCESS(rc)
                && uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(5, 0, 0)) /* Only for W2K and up ... */
            {
                /* We're ready to create the tooltip balloon.
                   Check in 10 seconds (@todo make seconds configurable) ... */
                SetTimer(g_hwndToolWindow,
                         TIMERID_VPOXTRAY_CHECK_HOSTVERSION,
                         10 * 1000, /* 10 seconds */
                         NULL       /* No timerproc */);
            }

            if (RT_SUCCESS(rc))
            {
                /* Report the host that we're up and running! */
                hlpReportStatus(VPoxGuestFacilityStatus_Active);
            }

            if (RT_SUCCESS(rc))
            {
                /* Boost thread priority to make sure we wake up early for seamless window notifications
                 * (not sure if it actually makes any difference though). */
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

                /*
                 * Main execution loop
                 * Wait for the stop semaphore to be posted or a window event to arrive
                 */

                HANDLE hWaitEvent[4] = {0};
                DWORD dwEventCount = 0;

                hWaitEvent[dwEventCount++] = g_hStopSem;

                /* Check if seamless mode is not active and add seamless event to the list */
                if (0 != g_hSeamlessWtNotifyEvent)
                {
                    hWaitEvent[dwEventCount++] = g_hSeamlessWtNotifyEvent;
                }

                if (0 != g_hSeamlessKmNotifyEvent)
                {
                    hWaitEvent[dwEventCount++] = g_hSeamlessKmNotifyEvent;
                }

                if (0 != vpoxDtGetNotifyEvent())
                {
                    hWaitEvent[dwEventCount++] = vpoxDtGetNotifyEvent();
                }

                LogFlowFunc(("Number of events to wait in main loop: %ld\n", dwEventCount));
                while (true)
                {
                    DWORD waitResult = MsgWaitForMultipleObjectsEx(dwEventCount, hWaitEvent, 500, QS_ALLINPUT, 0);
                    waitResult = waitResult - WAIT_OBJECT_0;

                    /* Only enable for message debugging, lots of traffic! */
                    //Log(("Wait result  = %ld\n", waitResult));

                    if (waitResult == 0)
                    {
                        LogFunc(("Event 'Exit' triggered\n"));
                        /* exit */
                        break;
                    }
                    else
                    {
                        BOOL fHandled = FALSE;
                        if (waitResult < RT_ELEMENTS(hWaitEvent))
                        {
                            if (hWaitEvent[waitResult])
                            {
                                if (hWaitEvent[waitResult] == g_hSeamlessWtNotifyEvent)
                                {
                                    LogFunc(("Event 'Seamless' triggered\n"));

                                    /* seamless window notification */
                                    VPoxSeamlessCheckWindows(false);
                                    fHandled = TRUE;
                                }
                                else if (hWaitEvent[waitResult] == g_hSeamlessKmNotifyEvent)
                                {
                                    LogFunc(("Event 'Km Seamless' triggered\n"));

                                    /* seamless window notification */
                                    VPoxSeamlessCheckWindows(true);
                                    fHandled = TRUE;
                                }
                                else if (hWaitEvent[waitResult] == vpoxDtGetNotifyEvent())
                                {
                                    LogFunc(("Event 'Dt' triggered\n"));
                                    VPoxTrayCheckDt();
                                    fHandled = TRUE;
                                }
                            }
                        }

                        if (!fHandled)
                        {
                            /* timeout or a window message, handle it */
                            MSG msg;
                            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                            {
#ifdef DEBUG_andy
                                LogFlowFunc(("PeekMessage %u\n", msg.message));
#endif
                                if (msg.message == WM_QUIT)
                                {
                                    LogFunc(("Terminating ...\n"));
                                    SetEvent(g_hStopSem);
                                }
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        }
                    }
                }
                LogFunc(("Returned from main loop, exiting ...\n"));
            }
            LogFunc(("Waiting for services to stop ...\n"));
            vpoxTrayServicesStop(&svcEnv);
        } /* Services started */
        CloseHandle(g_hStopSem);
    } /* Stop event created */

    vpoxTrayRemoveTrayIcon();

    LogFunc(("Leaving with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Main function
 */
int mymain(int cArgs, char **papszArgs)
{
    int rc = RTR3InitExe(cArgs, &papszArgs, RTR3INIT_FLAGS_STANDALONE_APP);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse the top level arguments until we find a command.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--help",             'h',                         RTGETOPT_REQ_NOTHING },
        { "-help",              'h',                         RTGETOPT_REQ_NOTHING },
        { "/help",              'h',                         RTGETOPT_REQ_NOTHING },
        { "/?",                 'h',                         RTGETOPT_REQ_NOTHING },
        { "--logfile",          'l',                         RTGETOPT_REQ_STRING  },
        { "--verbose",          'v',                         RTGETOPT_REQ_NOTHING },
        { "--version",          'V',                         RTGETOPT_REQ_NOTHING },
    };

    char szLogFile[RTPATH_MAX] = {0};

    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOptInit failed: %Rrc\n", rc);

    int ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (ch)
        {
            case 'h':
                hlpShowMessageBox(VPOX_PRODUCT " - " VPOX_VPOXTRAY_TITLE,
                                  MB_ICONINFORMATION,
                     "-- " VPOX_PRODUCT " %s v%u.%u.%ur%u --\n\n"
                     "Copyright (C) 2009-" VPOX_C_YEAR " " VPOX_VENDOR "\n"
                     "All rights reserved.\n\n"
                     "Command Line Parameters:\n\n"
                     "-l, --logfile <file>\n"
                     "    Enables logging to a file\n"
                     "-v, --verbose\n"
                     "    Increases verbosity\n"
                     "-V, --version\n"
                     "   Displays version number and exit\n"
                     "-?, -h, --help\n"
                     "   Displays this help text and exit\n"
                     "\n"
                     "Examples:\n"
                     "  %s -vvv\n",
                     VPOX_VPOXTRAY_TITLE, VPOX_VERSION_MAJOR, VPOX_VERSION_MINOR, VPOX_VERSION_BUILD, VPOX_SVN_REV,
                     papszArgs[0], papszArgs[0]);
                return RTEXITCODE_SUCCESS;

            case 'l':
                if (*ValueUnion.psz == '\0')
                    szLogFile[0] = '\0';
                else
                {
                    rc = RTPathAbs(ValueUnion.psz, szLogFile, sizeof(szLogFile));
                    if (RT_FAILURE(rc))
                        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAbs failed on log file path: %Rrc (%s)",
                                              rc, ValueUnion.psz);
                }
                break;

            case 'v':
                g_cVerbosity++;
                break;

            case 'V':
                hlpShowMessageBox(VPOX_VPOXTRAY_TITLE, MB_ICONINFORMATION,
                                  "Version: %u.%u.%ur%u",
                                  VPOX_VERSION_MAJOR, VPOX_VERSION_MINOR, VPOX_VERSION_BUILD, VPOX_SVN_REV);
                return RTEXITCODE_SUCCESS;

            default:
                rc = RTGetOptPrintError(ch, &ValueUnion);
                break;
        }
    }

    /* Note: Do not use a global namespace ("Global\\") for mutex name here,
     * will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex(NULL, FALSE, VPOX_VPOXTRAY_TITLE);
    if (   hMutexAppRunning != NULL
        && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        /* VPoxTray already running? Bail out. */
        CloseHandle (hMutexAppRunning);
        hMutexAppRunning = NULL;
        return RTEXITCODE_SUCCESS;
    }

    rc = vpoxTrayLogCreate(szLogFile[0] ? szLogFile : NULL);
    if (RT_SUCCESS(rc))
    {
        LogRel(("Verbosity level: %d\n", g_cVerbosity));

        rc = VbglR3Init();
        if (RT_SUCCESS(rc))
        {
            /* Log the major windows NT version: */
            uint64_t const uNtVersion = RTSystemGetNtVersion();
            LogRel(("Windows version %u.%u build %u (uNtVersion=%#RX64)\n", RTSYSTEM_NT_VERSION_GET_MAJOR(uNtVersion),
                    RTSYSTEM_NT_VERSION_GET_MINOR(uNtVersion), RTSYSTEM_NT_VERSION_GET_BUILD(uNtVersion), uNtVersion ));

            /* Set the instance handle. */
            hlpReportStatus(VPoxGuestFacilityStatus_Init);
            rc = vpoxTrayCreateToolWindow();
            if (RT_SUCCESS(rc))
            {
                VPoxCapsInit();

                rc = vpoxStInit(g_hwndToolWindow);
                if (!RT_SUCCESS(rc))
                {
                    LogFlowFunc(("vpoxStInit failed, rc=%Rrc\n", rc));
                    /* ignore the St Init failure. this can happen for < XP win that do not support WTS API
                     * in that case the session is treated as active connected to the physical console
                     * (i.e. fallback to the old behavior that was before introduction of VPoxSt) */
                    Assert(vpoxStIsActiveConsole());
                }

                rc = vpoxDtInit();
                if (!RT_SUCCESS(rc))
                {
                    LogFlowFunc(("vpoxDtInit failed, rc=%Rrc\n", rc));
                    /* ignore the Dt Init failure. this can happen for < XP win that do not support WTS API
                     * in that case the session is treated as active connected to the physical console
                     * (i.e. fallback to the old behavior that was before introduction of VPoxSt) */
                    Assert(vpoxDtIsInputDesktop());
                }

                rc = VPoxAcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_SEAMLESS | VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, true);
                if (!RT_SUCCESS(rc))
                    LogFlowFunc(("VPoxAcquireGuestCaps failed with rc=%Rrc, ignoring ...\n", rc));

                rc = vpoxTraySetupSeamless(); /** @todo r=andy Do we really want to be this critical for the whole application? */
                if (RT_SUCCESS(rc))
                {
                    rc = vpoxTrayServiceMain();
                    if (RT_SUCCESS(rc))
                        hlpReportStatus(VPoxGuestFacilityStatus_Terminating);
                    vpoxTrayShutdownSeamless();
                }

                /* it should be safe to call vpoxDtTerm even if vpoxStInit above failed */
                vpoxDtTerm();

                /* it should be safe to call vpoxStTerm even if vpoxStInit above failed */
                vpoxStTerm();

                VPoxCapsTerm();

                vpoxTrayDestroyToolWindow();
            }
            if (RT_SUCCESS(rc))
                hlpReportStatus(VPoxGuestFacilityStatus_Terminated);
            else
            {
                LogRel(("Error while starting, rc=%Rrc\n", rc));
                hlpReportStatus(VPoxGuestFacilityStatus_Failed);
            }

            LogRel(("Ended\n"));
            VbglR3Term();
        }
        else
            LogRel(("VbglR3Init failed: %Rrc\n", rc));
    }

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL)
    {
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    vpoxTrayLogDestroy();

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    RT_NOREF(hPrevInstance, lpCmdLine, nCmdShow);

    g_hInstance = hInstance;

    return mymain(__argc, __argv);
}

/**
 * Window procedure for our main tool window.
 */
static LRESULT CALLBACK vpoxToolWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LogFlowFunc(("hWnd=%p, uMsg=%u\n", hWnd, uMsg));

    switch (uMsg)
    {
        case WM_CREATE:
        {
            LogFunc(("Tool window created\n"));

            int rc = vpoxTrayRegisterGlobalMessages(&g_vpoxGlobalMessageTable[0]);
            if (RT_FAILURE(rc))
                LogFunc(("Error registering global window messages, rc=%Rrc\n", rc));
            return 0;
        }

        case WM_CLOSE:
            return 0;

        case WM_DESTROY:
        {
            LogFunc(("Tool window destroyed\n"));
            KillTimer(g_hwndToolWindow, TIMERID_VPOXTRAY_CHECK_HOSTVERSION);
            return 0;
        }

        case WM_TIMER:
        {
            if (VPoxCapsCheckTimer(wParam))
                return 0;
            if (vpoxDtCheckTimer(wParam))
                return 0;
            if (vpoxStCheckTimer(wParam))
                return 0;

            switch (wParam)
            {
                case TIMERID_VPOXTRAY_CHECK_HOSTVERSION:
                    if (RT_SUCCESS(VPoxCheckHostVersion()))
                    {
                        /* After successful run we don't need to check again. */
                        KillTimer(g_hwndToolWindow, TIMERID_VPOXTRAY_CHECK_HOSTVERSION);
                    }
                    return 0;

                default:
                    break;
            }

            break; /* Make sure other timers get processed the usual way! */
        }

        case WM_VPOXTRAY_TRAY_ICON:
        {
            switch (LOWORD(lParam))
            {
                case WM_LBUTTONDBLCLK:
                    break;
                case WM_RBUTTONDOWN:
                {
                    if (!g_cVerbosity) /* Don't show menu when running in non-verbose mode. */
                        break;

                    POINT lpCursor;
                    if (GetCursorPos(&lpCursor))
                    {
                        HMENU hContextMenu = CreatePopupMenu();
                        if (hContextMenu)
                        {
                            UINT_PTR uMenuItem = 9999;
                            UINT     fMenuItem = MF_BYPOSITION | MF_STRING;
                            if (InsertMenuW(hContextMenu, UINT_MAX, fMenuItem, uMenuItem, L"Exit"))
                            {
                                SetForegroundWindow(hWnd);

                                const bool fBlockWhileTracking = true;

                                UINT fTrack = TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN;

                                if (fBlockWhileTracking)
                                    fTrack |= TPM_RETURNCMD | TPM_NONOTIFY;

                                UINT uMsg = TrackPopupMenu(hContextMenu, fTrack, lpCursor.x, lpCursor.y, 0, hWnd, NULL);
                                if (   uMsg
                                    && fBlockWhileTracking)
                                {
                                    if (uMsg == uMenuItem)
                                        PostMessage(g_hwndToolWindow, WM_QUIT, 0, 0);
                                }
                                else if (!uMsg)
                                    LogFlowFunc(("Tracking popup menu failed with %ld\n", GetLastError()));
                            }

                            DestroyMenu(hContextMenu);
                        }
                    }
                    break;
                }
            }
            return 0;
        }

        case WM_VPOX_SEAMLESS_ENABLE:
        {
            VPoxCapsEntryFuncStateSet(VPOXCAPS_ENTRY_IDX_SEAMLESS, VPOXCAPS_ENTRY_FUNCSTATE_STARTED);
            if (VPoxCapsEntryIsEnabled(VPOXCAPS_ENTRY_IDX_SEAMLESS))
                VPoxSeamlessCheckWindows(true);
            return 0;
        }

        case WM_VPOX_SEAMLESS_DISABLE:
        {
            VPoxCapsEntryFuncStateSet(VPOXCAPS_ENTRY_IDX_SEAMLESS, VPOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);
            return 0;
        }

        case WM_DISPLAYCHANGE:
            ASMAtomicUoWriteU32(&g_fGuestDisplaysChanged, 1);
            // No break or return is intentional here.
        case WM_VPOX_SEAMLESS_UPDATE:
        {
            if (VPoxCapsEntryIsEnabled(VPOXCAPS_ENTRY_IDX_SEAMLESS))
                VPoxSeamlessCheckWindows(true);
            return 0;
        }

        case WM_VPOX_GRAPHICS_SUPPORTED:
        {
            VPoxGrapicsSetSupported(TRUE);
            return 0;
        }

        case WM_VPOX_GRAPHICS_UNSUPPORTED:
        {
            VPoxGrapicsSetSupported(FALSE);
            return 0;
        }

        case WM_WTSSESSION_CHANGE:
        {
            BOOL fOldAllowedState = VPoxConsoleIsAllowed();
            if (vpoxStHandleEvent(wParam))
            {
                if (!VPoxConsoleIsAllowed() != !fOldAllowedState)
                    VPoxConsoleEnable(!fOldAllowedState);
            }
            return 0;
        }

        default:
        {
            /* Handle all globally registered window messages. */
            if (vpoxTrayHandleGlobalMessages(&g_vpoxGlobalMessageTable[0], uMsg,
                                             wParam, lParam))
            {
                return 0; /* We handled the message. @todo Add return value!*/
            }
            break; /* We did not handle the message, dispatch to DefWndProc. */
        }
    }

    /* Only if message was *not* handled by our switch above, dispatch to DefWindowProc. */
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/* St (session [state] tracking) functionality API impl */

typedef struct VPOXST
{
    HWND hWTSAPIWnd;
    RTLDRMOD hLdrModWTSAPI32;
    BOOL fIsConsole;
    WTS_CONNECTSTATE_CLASS enmConnectState;
    UINT_PTR idDelayedInitTimer;
    BOOL (WINAPI * pfnWTSRegisterSessionNotification)(HWND hWnd, DWORD dwFlags);
    BOOL (WINAPI * pfnWTSUnRegisterSessionNotification)(HWND hWnd);
    BOOL (WINAPI * pfnWTSQuerySessionInformationA)(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPTSTR *ppBuffer, DWORD *pBytesReturned);
} VPOXST;

static VPOXST gVPoxSt;

static int vpoxStCheckState()
{
    int rc = VINF_SUCCESS;
    WTS_CONNECTSTATE_CLASS *penmConnectState = NULL;
    USHORT *pProtocolType = NULL;
    DWORD cbBuf = 0;
    if (gVPoxSt.pfnWTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSConnectState,
                                               (LPTSTR *)&penmConnectState, &cbBuf))
    {
        if (gVPoxSt.pfnWTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSClientProtocolType,
                                                   (LPTSTR *)&pProtocolType, &cbBuf))
        {
            gVPoxSt.fIsConsole = (*pProtocolType == 0);
            gVPoxSt.enmConnectState = *penmConnectState;
            return VINF_SUCCESS;
        }

        DWORD dwErr = GetLastError();
        LogFlowFunc(("WTSQuerySessionInformationA WTSClientProtocolType failed, error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }
    else
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("WTSQuerySessionInformationA WTSConnectState failed, error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }

    /* failure branch, set to "console-active" state */
    gVPoxSt.fIsConsole = TRUE;
    gVPoxSt.enmConnectState = WTSActive;

    return rc;
}

static int vpoxStInit(HWND hWnd)
{
    RT_ZERO(gVPoxSt);
    int rc = RTLdrLoadSystem("WTSAPI32.DLL", false /*fNoUnload*/, &gVPoxSt.hLdrModWTSAPI32);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(gVPoxSt.hLdrModWTSAPI32, "WTSRegisterSessionNotification",
                            (void **)&gVPoxSt.pfnWTSRegisterSessionNotification);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(gVPoxSt.hLdrModWTSAPI32, "WTSUnRegisterSessionNotification",
                                (void **)&gVPoxSt.pfnWTSUnRegisterSessionNotification);
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrGetSymbol(gVPoxSt.hLdrModWTSAPI32, "WTSQuerySessionInformationA",
                                    (void **)&gVPoxSt.pfnWTSQuerySessionInformationA);
                if (RT_FAILURE(rc))
                    LogFlowFunc(("WTSQuerySessionInformationA not found\n"));
            }
            else
                LogFlowFunc(("WTSUnRegisterSessionNotification not found\n"));
        }
        else
            LogFlowFunc(("WTSRegisterSessionNotification not found\n"));
        if (RT_SUCCESS(rc))
        {
            gVPoxSt.hWTSAPIWnd = hWnd;
            if (gVPoxSt.pfnWTSRegisterSessionNotification(gVPoxSt.hWTSAPIWnd, NOTIFY_FOR_THIS_SESSION))
                vpoxStCheckState();
            else
            {
                DWORD dwErr = GetLastError();
                LogFlowFunc(("WTSRegisterSessionNotification failed, error = %08X\n", dwErr));
                if (dwErr == RPC_S_INVALID_BINDING)
                {
                    gVPoxSt.idDelayedInitTimer = SetTimer(gVPoxSt.hWTSAPIWnd, TIMERID_VPOXTRAY_ST_DELAYED_INIT_TIMER,
                                                          2000, (TIMERPROC)NULL);
                    gVPoxSt.fIsConsole = TRUE;
                    gVPoxSt.enmConnectState = WTSActive;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = RTErrConvertFromWin32(dwErr);
            }

            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
        }

        RTLdrClose(gVPoxSt.hLdrModWTSAPI32);
    }
    else
        LogFlowFunc(("WTSAPI32 load failed, rc = %Rrc\n", rc));

    RT_ZERO(gVPoxSt);
    gVPoxSt.fIsConsole = TRUE;
    gVPoxSt.enmConnectState = WTSActive;
    return rc;
}

static void vpoxStTerm(void)
{
    if (!gVPoxSt.hWTSAPIWnd)
    {
        LogFlowFunc(("vpoxStTerm called for non-initialized St\n"));
        return;
    }

    if (gVPoxSt.idDelayedInitTimer)
    {
        /* notification is not registered, just kill timer */
        KillTimer(gVPoxSt.hWTSAPIWnd, gVPoxSt.idDelayedInitTimer);
        gVPoxSt.idDelayedInitTimer = 0;
    }
    else
    {
        if (!gVPoxSt.pfnWTSUnRegisterSessionNotification(gVPoxSt.hWTSAPIWnd))
        {
            LogFlowFunc(("WTSAPI32 load failed, error = %08X\n", GetLastError()));
        }
    }

    RTLdrClose(gVPoxSt.hLdrModWTSAPI32);
    RT_ZERO(gVPoxSt);
}

#define VPOXST_DBG_MAKECASE(_val) case _val: return #_val;

static const char* vpoxStDbgGetString(DWORD val)
{
    switch (val)
    {
        VPOXST_DBG_MAKECASE(WTS_CONSOLE_CONNECT);
        VPOXST_DBG_MAKECASE(WTS_CONSOLE_DISCONNECT);
        VPOXST_DBG_MAKECASE(WTS_REMOTE_CONNECT);
        VPOXST_DBG_MAKECASE(WTS_REMOTE_DISCONNECT);
        VPOXST_DBG_MAKECASE(WTS_SESSION_LOGON);
        VPOXST_DBG_MAKECASE(WTS_SESSION_LOGOFF);
        VPOXST_DBG_MAKECASE(WTS_SESSION_LOCK);
        VPOXST_DBG_MAKECASE(WTS_SESSION_UNLOCK);
        VPOXST_DBG_MAKECASE(WTS_SESSION_REMOTE_CONTROL);
        default:
            LogFlowFunc(("invalid WTS state %d\n", val));
            return "Unknown";
    }
}

static BOOL vpoxStCheckTimer(WPARAM wEvent)
{
    if (wEvent != gVPoxSt.idDelayedInitTimer)
        return FALSE;

    if (gVPoxSt.pfnWTSRegisterSessionNotification(gVPoxSt.hWTSAPIWnd, NOTIFY_FOR_THIS_SESSION))
    {
        KillTimer(gVPoxSt.hWTSAPIWnd, gVPoxSt.idDelayedInitTimer);
        gVPoxSt.idDelayedInitTimer = 0;
        vpoxStCheckState();
    }
    else
    {
        LogFlowFunc(("timer WTSRegisterSessionNotification failed, error = %08X\n", GetLastError()));
        Assert(gVPoxSt.fIsConsole == TRUE);
        Assert(gVPoxSt.enmConnectState == WTSActive);
    }

    return TRUE;
}


static BOOL vpoxStHandleEvent(WPARAM wEvent)
{
    RT_NOREF(wEvent);
    LogFlowFunc(("WTS Event: %s\n", vpoxStDbgGetString(wEvent)));
    BOOL fOldIsActiveConsole = vpoxStIsActiveConsole();

    vpoxStCheckState();

    return !vpoxStIsActiveConsole() != !fOldIsActiveConsole;
}

static BOOL vpoxStIsActiveConsole(void)
{
    return (gVPoxSt.enmConnectState == WTSActive && gVPoxSt.fIsConsole);
}

/*
 * Dt (desktop [state] tracking) functionality API impl
 *
 * !!!NOTE: this API is NOT thread-safe!!!
 * */

typedef struct VPOXDT
{
    HANDLE hNotifyEvent;
    BOOL fIsInputDesktop;
    UINT_PTR idTimer;
    RTLDRMOD hLdrModHook;
    BOOL (* pfnVPoxHookInstallActiveDesktopTracker)(HMODULE hDll);
    BOOL (* pfnVPoxHookRemoveActiveDesktopTracker)();
    HDESK (WINAPI * pfnGetThreadDesktop)(DWORD dwThreadId);
    HDESK (WINAPI * pfnOpenInputDesktop)(DWORD dwFlags, BOOL fInherit, ACCESS_MASK dwDesiredAccess);
    BOOL (WINAPI * pfnCloseDesktop)(HDESK hDesktop);
} VPOXDT;

static VPOXDT gVPoxDt;

static BOOL vpoxDtCalculateIsInputDesktop()
{
    BOOL fIsInputDt = FALSE;
    HDESK hInput = gVPoxDt.pfnOpenInputDesktop(0, FALSE, DESKTOP_CREATEWINDOW);
    if (hInput)
    {
//        DWORD dwThreadId = GetCurrentThreadId();
//        HDESK hThreadDt = gVPoxDt.pfnGetThreadDesktop(dwThreadId);
//        if (hThreadDt)
//        {
            fIsInputDt = TRUE;
//        }
//        else
//        {
//            DWORD dwErr = GetLastError();
//            LogFlowFunc(("pfnGetThreadDesktop for Seamless failed, last error = %08X\n", dwErr));
//        }

        gVPoxDt.pfnCloseDesktop(hInput);
    }
    else
    {
//        DWORD dwErr = GetLastError();
//        LogFlowFunc(("pfnOpenInputDesktop for Seamless failed, last error = %08X\n", dwErr));
    }
    return fIsInputDt;
}

static BOOL vpoxDtCheckTimer(WPARAM wParam)
{
    if (wParam != gVPoxDt.idTimer)
        return FALSE;

    VPoxTrayCheckDt();

    return TRUE;
}

static int vpoxDtInit()
{
    RT_ZERO(gVPoxDt);

    int rc;
    gVPoxDt.hNotifyEvent = CreateEvent(NULL, FALSE, FALSE, VPOXHOOK_GLOBAL_DT_EVENT_NAME);
    if (gVPoxDt.hNotifyEvent != NULL)
    {
        /* Load the hook dll and resolve the necessary entry points. */
        rc = RTLdrLoadAppPriv(VPOXHOOK_DLL_NAME, &gVPoxDt.hLdrModHook);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(gVPoxDt.hLdrModHook, "VPoxHookInstallActiveDesktopTracker",
                                (void **)&gVPoxDt.pfnVPoxHookInstallActiveDesktopTracker);
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrGetSymbol(gVPoxDt.hLdrModHook, "VPoxHookRemoveActiveDesktopTracker",
                                    (void **)&gVPoxDt.pfnVPoxHookRemoveActiveDesktopTracker);
                if (RT_FAILURE(rc))
                    LogFlowFunc(("VPoxHookRemoveActiveDesktopTracker not found\n"));
            }
            else
                LogFlowFunc(("VPoxHookInstallActiveDesktopTracker not found\n"));
            if (RT_SUCCESS(rc))
            {
                /* Try get the system APIs we need. */
                *(void **)&gVPoxDt.pfnGetThreadDesktop = RTLdrGetSystemSymbol("user32.dll", "GetThreadDesktop");
                if (!gVPoxDt.pfnGetThreadDesktop)
                {
                    LogFlowFunc(("GetThreadDesktop not found\n"));
                    rc = VERR_NOT_SUPPORTED;
                }

                *(void **)&gVPoxDt.pfnOpenInputDesktop = RTLdrGetSystemSymbol("user32.dll", "OpenInputDesktop");
                if (!gVPoxDt.pfnOpenInputDesktop)
                {
                    LogFlowFunc(("OpenInputDesktop not found\n"));
                    rc = VERR_NOT_SUPPORTED;
                }

                *(void **)&gVPoxDt.pfnCloseDesktop = RTLdrGetSystemSymbol("user32.dll", "CloseDesktop");
                if (!gVPoxDt.pfnCloseDesktop)
                {
                    LogFlowFunc(("CloseDesktop not found\n"));
                    rc = VERR_NOT_SUPPORTED;
                }

                if (RT_SUCCESS(rc))
                {
                    BOOL fRc = FALSE;
                    /* For Vista and up we need to change the integrity of the security descriptor, too. */
                    uint64_t const uNtVersion = RTSystemGetNtVersion();
                    if (uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(6, 0, 0))
                    {
                        HMODULE hModHook = (HMODULE)RTLdrGetNativeHandle(gVPoxDt.hLdrModHook);
                        Assert((uintptr_t)hModHook != ~(uintptr_t)0);
                        fRc = gVPoxDt.pfnVPoxHookInstallActiveDesktopTracker(hModHook);
                        if (!fRc)
                            LogFlowFunc(("pfnVPoxHookInstallActiveDesktopTracker failed, last error = %08X\n", GetLastError()));
                    }

                    if (!fRc)
                    {
                        gVPoxDt.idTimer = SetTimer(g_hwndToolWindow, TIMERID_VPOXTRAY_DT_TIMER, 500, (TIMERPROC)NULL);
                        if (!gVPoxDt.idTimer)
                        {
                            DWORD dwErr = GetLastError();
                            LogFlowFunc(("SetTimer error %08X\n", dwErr));
                            rc = RTErrConvertFromWin32(dwErr);
                        }
                    }

                    if (RT_SUCCESS(rc))
                    {
                        gVPoxDt.fIsInputDesktop = vpoxDtCalculateIsInputDesktop();
                        return VINF_SUCCESS;
                    }
                }
            }

            RTLdrClose(gVPoxDt.hLdrModHook);
        }
        else
        {
            DWORD dwErr = GetLastError();
            LogFlowFunc(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            rc = RTErrConvertFromWin32(dwErr);
        }

        CloseHandle(gVPoxDt.hNotifyEvent);
    }
    else
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }


    RT_ZERO(gVPoxDt);
    gVPoxDt.fIsInputDesktop = TRUE;

    return rc;
}

static void vpoxDtTerm()
{
    if (!gVPoxDt.hLdrModHook)
        return;

    gVPoxDt.pfnVPoxHookRemoveActiveDesktopTracker();

    RTLdrClose(gVPoxDt.hLdrModHook);
    CloseHandle(gVPoxDt.hNotifyEvent);

    RT_ZERO(gVPoxDt);
}
/* @returns true on "IsInputDesktop" state change */
static BOOL vpoxDtHandleEvent()
{
    BOOL fIsInputDesktop = gVPoxDt.fIsInputDesktop;
    gVPoxDt.fIsInputDesktop = vpoxDtCalculateIsInputDesktop();
    return !fIsInputDesktop != !gVPoxDt.fIsInputDesktop;
}

static HANDLE vpoxDtGetNotifyEvent()
{
    return gVPoxDt.hNotifyEvent;
}

/* @returns true iff the application (VPoxTray) desktop is input */
static BOOL vpoxDtIsInputDesktop()
{
    return gVPoxDt.fIsInputDesktop;
}

/* we need to perform Acquire/Release using the file handled we use for rewuesting events from VPoxGuest
 * otherwise Acquisition mechanism will treat us as different client and will not propagate necessary requests
 * */
static int VPoxAcquireGuestCaps(uint32_t fOr, uint32_t fNot, bool fCfg)
{
    Log(("VPoxAcquireGuestCaps or(0x%x), not(0x%x), cfx(%d)\n", fOr, fNot, fCfg));
    int rc = VbglR3AcquireGuestCaps(fOr, fNot, fCfg);
    if (RT_FAILURE(rc))
        LogFlowFunc(("VPOXGUEST_IOCTL_GUEST_CAPS_ACQUIRE failed: %Rrc\n", rc));
    return rc;
}

typedef enum VPOXCAPS_ENTRY_ACSTATE
{
    /* the given cap is released */
    VPOXCAPS_ENTRY_ACSTATE_RELEASED = 0,
    /* the given cap acquisition is in progress */
    VPOXCAPS_ENTRY_ACSTATE_ACQUIRING,
    /* the given cap is acquired */
    VPOXCAPS_ENTRY_ACSTATE_ACQUIRED
} VPOXCAPS_ENTRY_ACSTATE;


struct VPOXCAPS_ENTRY;
struct VPOXCAPS;

typedef DECLCALLBACKPTR(void, PFNVPOXCAPS_ENTRY_ON_ENABLE,(struct VPOXCAPS *pConsole, struct VPOXCAPS_ENTRY *pCap, BOOL fEnabled));

typedef struct VPOXCAPS_ENTRY
{
    uint32_t fCap;
    uint32_t iCap;
    VPOXCAPS_ENTRY_FUNCSTATE enmFuncState;
    VPOXCAPS_ENTRY_ACSTATE enmAcState;
    PFNVPOXCAPS_ENTRY_ON_ENABLE pfnOnEnable;
} VPOXCAPS_ENTRY;


typedef struct VPOXCAPS
{
    UINT_PTR idTimer;
    VPOXCAPS_ENTRY aCaps[VPOXCAPS_ENTRY_IDX_COUNT];
} VPOXCAPS;

static VPOXCAPS gVPoxCaps;

static DECLCALLBACK(void) vpoxCapsOnEnableSeamless(struct VPOXCAPS *pConsole, struct VPOXCAPS_ENTRY *pCap, BOOL fEnabled)
{
    RT_NOREF(pConsole, pCap);
    if (fEnabled)
    {
        Log(("vpoxCapsOnEnableSeamless: ENABLED\n"));
        Assert(pCap->enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED);
        Assert(pCap->enmFuncState == VPOXCAPS_ENTRY_FUNCSTATE_STARTED);
        VPoxSeamlessEnable();
    }
    else
    {
        Log(("vpoxCapsOnEnableSeamless: DISABLED\n"));
        Assert(pCap->enmAcState != VPOXCAPS_ENTRY_ACSTATE_ACQUIRED || pCap->enmFuncState != VPOXCAPS_ENTRY_FUNCSTATE_STARTED);
        VPoxSeamlessDisable();
    }
}

static void vpoxCapsEntryAcStateSet(VPOXCAPS_ENTRY *pCap, VPOXCAPS_ENTRY_ACSTATE enmAcState)
{
    VPOXCAPS *pConsole = &gVPoxCaps;

    Log(("vpoxCapsEntryAcStateSet: new state enmAcState(%d); pCap: fCap(%d), iCap(%d), enmFuncState(%d), enmAcState(%d)\n",
            enmAcState, pCap->fCap, pCap->iCap, pCap->enmFuncState, pCap->enmAcState));

    if (pCap->enmAcState == enmAcState)
        return;

    VPOXCAPS_ENTRY_ACSTATE enmOldAcState = pCap->enmAcState;
    pCap->enmAcState = enmAcState;

    if (enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED)
    {
        if (pCap->enmFuncState == VPOXCAPS_ENTRY_FUNCSTATE_STARTED)
        {
            if (pCap->pfnOnEnable)
                pCap->pfnOnEnable(pConsole, pCap, TRUE);
        }
    }
    else if (enmOldAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED && pCap->enmFuncState == VPOXCAPS_ENTRY_FUNCSTATE_STARTED)
    {
        if (pCap->pfnOnEnable)
            pCap->pfnOnEnable(pConsole, pCap, FALSE);
    }
}

static void vpoxCapsEntryFuncStateSet(VPOXCAPS_ENTRY *pCap, VPOXCAPS_ENTRY_FUNCSTATE enmFuncState)
{
    VPOXCAPS *pConsole = &gVPoxCaps;

    Log(("vpoxCapsEntryFuncStateSet: new state enmAcState(%d); pCap: fCap(%d), iCap(%d), enmFuncState(%d), enmAcState(%d)\n",
            enmFuncState, pCap->fCap, pCap->iCap, pCap->enmFuncState, pCap->enmAcState));

    if (pCap->enmFuncState == enmFuncState)
        return;

    VPOXCAPS_ENTRY_FUNCSTATE enmOldFuncState = pCap->enmFuncState;

    pCap->enmFuncState = enmFuncState;

    if (enmFuncState == VPOXCAPS_ENTRY_FUNCSTATE_STARTED)
    {
        Assert(enmOldFuncState == VPOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);
        if (pCap->enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED)
        {
            if (pCap->pfnOnEnable)
                pCap->pfnOnEnable(pConsole, pCap, TRUE);
        }
    }
    else if (pCap->enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED && enmOldFuncState == VPOXCAPS_ENTRY_FUNCSTATE_STARTED)
    {
        if (pCap->pfnOnEnable)
            pCap->pfnOnEnable(pConsole, pCap, FALSE);
    }
}

static void VPoxCapsEntryFuncStateSet(uint32_t iCup, VPOXCAPS_ENTRY_FUNCSTATE enmFuncState)
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    VPOXCAPS_ENTRY *pCap = &pConsole->aCaps[iCup];
    vpoxCapsEntryFuncStateSet(pCap, enmFuncState);
}

static int VPoxCapsInit()
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    memset(pConsole, 0, sizeof (*pConsole));
    pConsole->aCaps[VPOXCAPS_ENTRY_IDX_SEAMLESS].fCap = VMMDEV_GUEST_SUPPORTS_SEAMLESS;
    pConsole->aCaps[VPOXCAPS_ENTRY_IDX_SEAMLESS].iCap = VPOXCAPS_ENTRY_IDX_SEAMLESS;
    pConsole->aCaps[VPOXCAPS_ENTRY_IDX_SEAMLESS].pfnOnEnable = vpoxCapsOnEnableSeamless;
    pConsole->aCaps[VPOXCAPS_ENTRY_IDX_GRAPHICS].fCap = VMMDEV_GUEST_SUPPORTS_GRAPHICS;
    pConsole->aCaps[VPOXCAPS_ENTRY_IDX_GRAPHICS].iCap = VPOXCAPS_ENTRY_IDX_GRAPHICS;
    return VINF_SUCCESS;
}

static int VPoxCapsReleaseAll()
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    Log(("VPoxCapsReleaseAll\n"));
    int rc = VPoxAcquireGuestCaps(0, VMMDEV_GUEST_SUPPORTS_SEAMLESS | VMMDEV_GUEST_SUPPORTS_GRAPHICS, false);
    if (!RT_SUCCESS(rc))
    {
        LogFlowFunc(("vpoxCapsEntryReleaseAll VPoxAcquireGuestCaps failed rc %d\n", rc));
        return rc;
    }

    if (pConsole->idTimer)
    {
        Log(("killing console timer\n"));
        KillTimer(g_hwndToolWindow, pConsole->idTimer);
        pConsole->idTimer = 0;
    }

    for (int i = 0; i < RT_ELEMENTS(pConsole->aCaps); ++i)
    {
        vpoxCapsEntryAcStateSet(&pConsole->aCaps[i], VPOXCAPS_ENTRY_ACSTATE_RELEASED);
    }

    return rc;
}

static void VPoxCapsTerm()
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    VPoxCapsReleaseAll();
    memset(pConsole, 0, sizeof (*pConsole));
}

static BOOL VPoxCapsEntryIsAcquired(uint32_t iCap)
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    return pConsole->aCaps[iCap].enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED;
}

static BOOL VPoxCapsEntryIsEnabled(uint32_t iCap)
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    return pConsole->aCaps[iCap].enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED
            && pConsole->aCaps[iCap].enmFuncState == VPOXCAPS_ENTRY_FUNCSTATE_STARTED;
}

static BOOL VPoxCapsCheckTimer(WPARAM wParam)
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    if (wParam != pConsole->idTimer)
        return FALSE;

    uint32_t u32AcquiredCaps = 0;
    BOOL fNeedNewTimer = FALSE;

    for (int i = 0; i < RT_ELEMENTS(pConsole->aCaps); ++i)
    {
        VPOXCAPS_ENTRY *pCap = &pConsole->aCaps[i];
        if (pCap->enmAcState != VPOXCAPS_ENTRY_ACSTATE_ACQUIRING)
            continue;

        int rc = VPoxAcquireGuestCaps(pCap->fCap, 0, false);
        if (RT_SUCCESS(rc))
        {
            vpoxCapsEntryAcStateSet(&pConsole->aCaps[i], VPOXCAPS_ENTRY_ACSTATE_ACQUIRED);
            u32AcquiredCaps |= pCap->fCap;
        }
        else
        {
            Assert(rc == VERR_RESOURCE_BUSY);
            fNeedNewTimer = TRUE;
        }
    }

    if (!fNeedNewTimer)
    {
        KillTimer(g_hwndToolWindow, pConsole->idTimer);
        /* cleanup timer data */
        pConsole->idTimer = 0;
    }

    return TRUE;
}

static int VPoxCapsEntryRelease(uint32_t iCap)
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    VPOXCAPS_ENTRY *pCap = &pConsole->aCaps[iCap];
    if (pCap->enmAcState == VPOXCAPS_ENTRY_ACSTATE_RELEASED)
    {
        LogFlowFunc(("invalid cap[%d] state[%d] on release\n", iCap, pCap->enmAcState));
        return VERR_INVALID_STATE;
    }

    if (pCap->enmAcState == VPOXCAPS_ENTRY_ACSTATE_ACQUIRED)
    {
        int rc = VPoxAcquireGuestCaps(0, pCap->fCap, false);
        AssertRC(rc);
    }

    vpoxCapsEntryAcStateSet(pCap, VPOXCAPS_ENTRY_ACSTATE_RELEASED);

    return VINF_SUCCESS;
}

static int VPoxCapsEntryAcquire(uint32_t iCap)
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    Assert(VPoxConsoleIsAllowed());
    VPOXCAPS_ENTRY *pCap = &pConsole->aCaps[iCap];
    Log(("VPoxCapsEntryAcquire %d\n", iCap));
    if (pCap->enmAcState != VPOXCAPS_ENTRY_ACSTATE_RELEASED)
    {
        LogFlowFunc(("invalid cap[%d] state[%d] on acquire\n", iCap, pCap->enmAcState));
        return VERR_INVALID_STATE;
    }

    vpoxCapsEntryAcStateSet(pCap, VPOXCAPS_ENTRY_ACSTATE_ACQUIRING);
    int rc = VPoxAcquireGuestCaps(pCap->fCap, 0, false);
    if (RT_SUCCESS(rc))
    {
        vpoxCapsEntryAcStateSet(pCap, VPOXCAPS_ENTRY_ACSTATE_ACQUIRED);
        return VINF_SUCCESS;
    }

    if (rc != VERR_RESOURCE_BUSY)
    {
        LogFlowFunc(("vpoxCapsEntryReleaseAll VPoxAcquireGuestCaps failed rc %d\n", rc));
        return rc;
    }

    LogFlowFunc(("iCap %d is busy!\n", iCap));

    /* the cap was busy, most likely it is still used by other VPoxTray instance running in another session,
     * queue the retry timer */
    if (!pConsole->idTimer)
    {
        pConsole->idTimer = SetTimer(g_hwndToolWindow, TIMERID_VPOXTRAY_CAPS_TIMER, 100, (TIMERPROC)NULL);
        if (!pConsole->idTimer)
        {
            DWORD dwErr = GetLastError();
            LogFlowFunc(("SetTimer error %08X\n", dwErr));
            return RTErrConvertFromWin32(dwErr);
        }
    }

    return rc;
}

static int VPoxCapsAcquireAllSupported()
{
    VPOXCAPS *pConsole = &gVPoxCaps;
    Log(("VPoxCapsAcquireAllSupported\n"));
    for (int i = 0; i < RT_ELEMENTS(pConsole->aCaps); ++i)
    {
        if (pConsole->aCaps[i].enmFuncState >= VPOXCAPS_ENTRY_FUNCSTATE_SUPPORTED)
        {
            Log(("VPoxCapsAcquireAllSupported acquiring cap %d, state %d\n", i, pConsole->aCaps[i].enmFuncState));
            VPoxCapsEntryAcquire(i);
        }
        else
        {
            LogFlowFunc(("VPoxCapsAcquireAllSupported: WARN: cap %d not supported, state %d\n", i, pConsole->aCaps[i].enmFuncState));
        }
    }
    return VINF_SUCCESS;
}

static BOOL VPoxConsoleIsAllowed()
{
    return vpoxDtIsInputDesktop() && vpoxStIsActiveConsole();
}

static void VPoxConsoleEnable(BOOL fEnable)
{
    if (fEnable)
        VPoxCapsAcquireAllSupported();
    else
        VPoxCapsReleaseAll();
}

static void VPoxConsoleCapSetSupported(uint32_t iCap, BOOL fSupported)
{
    if (fSupported)
    {
        VPoxCapsEntryFuncStateSet(iCap, VPOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);

        if (VPoxConsoleIsAllowed())
            VPoxCapsEntryAcquire(iCap);
    }
    else
    {
        VPoxCapsEntryFuncStateSet(iCap, VPOXCAPS_ENTRY_FUNCSTATE_UNSUPPORTED);

        VPoxCapsEntryRelease(iCap);
    }
}

void VPoxSeamlessSetSupported(BOOL fSupported)
{
    VPoxConsoleCapSetSupported(VPOXCAPS_ENTRY_IDX_SEAMLESS, fSupported);
}

static void VPoxGrapicsSetSupported(BOOL fSupported)
{
    VPoxConsoleCapSetSupported(VPOXCAPS_ENTRY_IDX_GRAPHICS, fSupported);
}
