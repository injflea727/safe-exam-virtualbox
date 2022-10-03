/* $Id: VPoxTray.h $ */
/** @file
 * VPoxTray - Guest Additions Tray, Internal Header.
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

#ifndef GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxTray_h
#define GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxTray_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>

#include <tchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <process.h>

#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include <VPox/version.h>
#include <VPox/VPoxGuestLib.h>
#include <VPoxDisplay.h>

#include "VPoxDispIf.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Title of the program to show.
 *  Also shown as part of message boxes. */
#define VPOX_VPOXTRAY_TITLE                     "VPoxTray"

/*
 * Windows messsages.
 */

/**
 * General VPoxTray messages.
 */
#define WM_VPOXTRAY_TRAY_ICON                   WM_APP + 40

/* The tray icon's ID. */
#define ID_TRAYICON                             2000

/*
 * Timer IDs.
 */
#define TIMERID_VPOXTRAY_CHECK_HOSTVERSION      1000
#define TIMERID_VPOXTRAY_CAPS_TIMER             1001
#define TIMERID_VPOXTRAY_DT_TIMER               1002
#define TIMERID_VPOXTRAY_ST_DELAYED_INIT_TIMER  1003


/*********************************************************************************************************************************
*   Common structures                                                                                                            *
*********************************************************************************************************************************/

/**
 * The environment information for services.
 */
typedef struct _VPOXSERVICEENV
{
    /** hInstance of VPoxTray. */
    HINSTANCE hInstance;
    /* Display driver interface, XPDM - WDDM abstraction see VPOXDISPIF** definitions above */
    /** @todo r=andy Argh. Needed by the "display" + "seamless" services (which in turn get called
     *               by the VPoxCaps facility. See #8037. */
    VPOXDISPIF dispIf;
} VPOXSERVICEENV;
/** Pointer to a VPoxTray service env info structure.  */
typedef VPOXSERVICEENV *PVPOXSERVICEENV;
/** Pointer to a const VPoxTray service env info structure.  */
typedef VPOXSERVICEENV const *PCVPOXSERVICEENV;

/**
 * A service descriptor.
 */
typedef struct _VPOXSERVICEDESC
{
    /** The service's name. RTTHREAD_NAME_LEN maximum characters. */
    char           *pszName;
    /** The service description. */
    char           *pszDesc;

    /** Callbacks. */

    /**
     * Initializes a service.
     * @returns VPox status code.
     *          VERR_NOT_SUPPORTED if the service is not supported on this guest system. Logged.
     *          VERR_HGCM_SERVICE_NOT_FOUND if the service is not available on the host system. Logged.
     *          Returning any other error will be considered as a fatal error.
     * @param   pEnv
     * @param   ppInstance      Where to return the thread-specific instance data.
     * @todo r=bird: The pEnv type is WRONG!  Please check all your const pointers.
     */
    DECLCALLBACKMEMBER(int,  pfnInit)   (const PVPOXSERVICEENV pEnv, void **ppInstance);

    /** Called from the worker thread.
     *
     * @returns VPox status code.
     * @retval  VINF_SUCCESS if exitting because *pfShutdown was set.
     * @param   pInstance       Pointer to thread-specific instance data.
     * @param   pfShutdown      Pointer to a per service termination flag to check
     *                          before and after blocking.
     */
    DECLCALLBACKMEMBER(int,  pfnWorker) (void *pInstance, bool volatile *pfShutdown);

    /**
     * Stops a service.
     */
    DECLCALLBACKMEMBER(int,  pfnStop)   (void *pInstance);

    /**
     * Does termination cleanups.
     *
     * @remarks This may be called even if pfnInit hasn't been called!
     */
    DECLCALLBACKMEMBER(void, pfnDestroy)(void *pInstance);
} VPOXSERVICEDESC, *PVPOXSERVICEDESC;


/**
 * The service initialization info and runtime variables.
 */
typedef struct _VPOXSERVICEINFO
{
    /** Pointer to the service descriptor. */
    PVPOXSERVICEDESC pDesc;
    /** Thread handle. */
    RTTHREAD         hThread;
    /** Pointer to service-specific instance data.
     *  Must be free'd by the service itself. */
    void            *pInstance;
    /** Whether Pre-init was called. */
    bool             fPreInited;
    /** Shutdown indicator. */
    bool volatile    fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile    fStopped;
    /** Whether the service was started or not. */
    bool             fStarted;
    /** Whether the service is enabled or not. */
    bool             fEnabled;
} VPOXSERVICEINFO, *PVPOXSERVICEINFO;

/* Globally unique (system wide) message registration. */
typedef struct _VPOXGLOBALMESSAGE
{
    /** Message name. */
    char    *pszName;
    /** Function pointer for handling the message. */
    int      (* pfnHandler)          (WPARAM wParam, LPARAM lParam);

    /* Variables. */

    /** Message ID;
     *  to be filled in when registering the actual message. */
    UINT     uMsgID;
} VPOXGLOBALMESSAGE, *PVPOXGLOBALMESSAGE;


/*********************************************************************************************************************************
*   Externals                                                                                                                    *
*********************************************************************************************************************************/
extern VPOXSERVICEDESC g_SvcDescDisplay;
#ifdef VPOX_WITH_SHARED_CLIPBOARD
extern VPOXSERVICEDESC g_SvcDescClipboard;
#endif
extern VPOXSERVICEDESC g_SvcDescSeamless;
extern VPOXSERVICEDESC g_SvcDescVRDP;
extern VPOXSERVICEDESC g_SvcDescIPC;
extern VPOXSERVICEDESC g_SvcDescLA;
#ifdef VPOX_WITH_DRAG_AND_DROP
extern VPOXSERVICEDESC g_SvcDescDnD;
#endif

extern int          g_cVerbosity;
extern HINSTANCE    g_hInstance;
extern HWND         g_hwndToolWindow;
extern uint32_t     g_fGuestDisplaysChanged;

RTEXITCODE VPoxTrayShowError(const char *pszFormat, ...);

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxTray_h */

