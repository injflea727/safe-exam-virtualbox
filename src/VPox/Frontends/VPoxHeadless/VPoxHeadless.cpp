/* $Id: VPoxHeadless.cpp $ */
/** @file
 * VPoxHeadless - The VirtualPox Headless frontend for running VMs on servers.
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

#include <VPox/com/com.h>
#include <VPox/com/string.h>
#include <VPox/com/array.h>
#include <VPox/com/Guid.h>
#include <VPox/com/ErrorInfo.h>
#include <VPox/com/errorprint.h>
#include <VPox/com/NativeEventQueue.h>

#include <VPox/com/VirtualPox.h>
#include <VPox/com/listeners.h>

using namespace com;

#define LOG_GROUP LOG_GROUP_GUI

#include <VPox/log.h>
#include <VPox/version.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/semaphore.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/ldr.h>
#include <iprt/getopt.h>
#include <iprt/env.h>
#include <VPox/err.h>
#include <VPoxVideo.h>

#ifdef VPOX_WITH_RECORDING
# include <cstdlib>
# include <cerrno>
# include <iprt/process.h>
#endif

#ifdef RT_OS_DARWIN
# include <iprt/asm.h>
# include <dlfcn.h>
# include <sys/mman.h>
#endif

#if !defined(RT_OS_WINDOWS)
#include <signal.h>
static void HandleSignal(int sig);
#endif

#include "PasswordInput.h"

////////////////////////////////////////////////////////////////////////////////

#define LogError(m,rc) \
    do { \
        Log(("VPoxHeadless: ERROR: " m " [rc=0x%08X]\n", rc)); \
        RTPrintf("%s\n", m); \
    } while (0)

////////////////////////////////////////////////////////////////////////////////

/* global weak references (for event handlers) */
static IConsole *gConsole = NULL;
static NativeEventQueue *gEventQ = NULL;

/* keep this handy for messages */
static com::Utf8Str g_strVMName;
static com::Utf8Str g_strVMUUID;

/* flag whether frontend should terminate */
static volatile bool g_fTerminateFE = false;

////////////////////////////////////////////////////////////////////////////////

/**
 *  Handler for VirtualPoxClient events.
 */
class VirtualPoxClientEventListener
{
public:
    VirtualPoxClientEventListener()
    {
    }

    virtual ~VirtualPoxClientEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VPoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VPoxEventType_OnVPoxSVCAvailabilityChanged:
            {
                ComPtr<IVPoxSVCAvailabilityChangedEvent> pVSACEv = aEvent;
                Assert(pVSACEv);
                BOOL fAvailable = FALSE;
                pVSACEv->COMGETTER(Available)(&fAvailable);
                if (!fAvailable)
                {
                    LogRel(("VPoxHeadless: VPoxSVC became unavailable, exiting.\n"));
                    RTPrintf("VPoxSVC became unavailable, exiting.\n");
                    /* Terminate the VM as cleanly as possible given that VPoxSVC
                     * is no longer present. */
                    g_fTerminateFE = true;
                    gEventQ->interruptEventQueueProcessing();
                }
                break;
            }
            default:
                AssertFailed();
        }

        return S_OK;
    }

private:
};

/**
 *  Handler for machine events.
 */
class ConsoleEventListener
{
public:
    ConsoleEventListener() :
        mLastVRDEPort(-1),
        m_fIgnorePowerOffEvents(false),
        m_fNoLoggedInUsers(true)
    {
    }

    virtual ~ConsoleEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VPoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VPoxEventType_OnMouseCapabilityChanged:
            {

                ComPtr<IMouseCapabilityChangedEvent> mccev = aEvent;
                Assert(!mccev.isNull());

                BOOL fSupportsAbsolute = false;
                mccev->COMGETTER(SupportsAbsolute)(&fSupportsAbsolute);

                /* Emit absolute mouse event to actually enable the host mouse cursor. */
                if (fSupportsAbsolute && gConsole)
                {
                    ComPtr<IMouse> mouse;
                    gConsole->COMGETTER(Mouse)(mouse.asOutParam());
                    if (mouse)
                    {
                        mouse->PutMouseEventAbsolute(-1, -1, 0, 0 /* Horizontal wheel */, 0);
                    }
                }
                break;
            }
            case VPoxEventType_OnStateChanged:
            {
                ComPtr<IStateChangedEvent> scev = aEvent;
                Assert(scev);

                MachineState_T machineState;
                scev->COMGETTER(State)(&machineState);

                /* Terminate any event wait operation if the machine has been
                 * PoweredDown/Saved/Aborted. */
                if (machineState < MachineState_Running && !m_fIgnorePowerOffEvents)
                {
                    g_fTerminateFE = true;
                    gEventQ->interruptEventQueueProcessing();
                }

                break;
            }
            case VPoxEventType_OnVRDEServerInfoChanged:
            {
                ComPtr<IVRDEServerInfoChangedEvent> rdicev = aEvent;
                Assert(rdicev);

                if (gConsole)
                {
                    ComPtr<IVRDEServerInfo> info;
                    gConsole->COMGETTER(VRDEServerInfo)(info.asOutParam());
                    if (info)
                    {
                        LONG port;
                        info->COMGETTER(Port)(&port);
                        if (port != mLastVRDEPort)
                        {
                            if (port == -1)
                                RTPrintf("VRDE server is inactive.\n");
                            else if (port == 0)
                                RTPrintf("VRDE server failed to start.\n");
                            else
                                RTPrintf("VRDE server is listening on port %d.\n", port);

                            mLastVRDEPort = port;
                        }
                    }
                }
                break;
            }
            case VPoxEventType_OnCanShowWindow:
            {
                ComPtr<ICanShowWindowEvent> cswev = aEvent;
                Assert(cswev);
                cswev->AddVeto(NULL);
                break;
            }
            case VPoxEventType_OnShowWindow:
            {
                ComPtr<IShowWindowEvent> swev = aEvent;
                Assert(swev);
                /* Ignore the event, WinId is either still zero or some other listener assigned it. */
                NOREF(swev); /* swev->COMSETTER(WinId)(0); */
                break;
            }
            case VPoxEventType_OnGuestPropertyChanged:
            {
                ComPtr<IGuestPropertyChangedEvent> pChangedEvent = aEvent;
                Assert(pChangedEvent);

                HRESULT hrc;

                ComPtr <IMachine> pMachine;
                if (gConsole)
                {
                    hrc = gConsole->COMGETTER(Machine)(pMachine.asOutParam());
                    if (FAILED(hrc) || !pMachine)
                        hrc = VPOX_E_OBJECT_NOT_FOUND;
                }
                else
                    hrc = VPOX_E_INVALID_VM_STATE;

                if (SUCCEEDED(hrc))
                {
                    Bstr strKey;
                    hrc = pChangedEvent->COMGETTER(Name)(strKey.asOutParam());
                    AssertComRC(hrc);

                    Bstr strValue;
                    hrc = pChangedEvent->COMGETTER(Value)(strValue.asOutParam());
                    AssertComRC(hrc);

                    Utf8Str utf8Key = strKey;
                    Utf8Str utf8Value = strValue;
                    LogRelFlow(("Guest property \"%s\" has been changed to \"%s\"\n",
                                utf8Key.c_str(), utf8Value.c_str()));

                    if (utf8Key.equals("/VirtualPox/GuestInfo/OS/NoLoggedInUsers"))
                    {
                        LogRelFlow(("Guest indicates that there %s logged in users\n",
                                    utf8Value.equals("true") ? "are no" : "are"));

                        /* Check if this is our machine and the "disconnect on logout feature" is enabled. */
                        BOOL fProcessDisconnectOnGuestLogout = FALSE;

                        /* Does the machine handle VRDP disconnects? */
                        Bstr strDiscon;
                        hrc = pMachine->GetExtraData(Bstr("VRDP/DisconnectOnGuestLogout").raw(),
                                                    strDiscon.asOutParam());
                        if (SUCCEEDED(hrc))
                        {
                            Utf8Str utf8Discon = strDiscon;
                            fProcessDisconnectOnGuestLogout = utf8Discon.equals("1")
                                                            ? TRUE : FALSE;
                        }

                        LogRelFlow(("VRDE: hrc=%Rhrc: Host %s disconnecting clients (current host state known: %s)\n",
                                    hrc, fProcessDisconnectOnGuestLogout ? "will handle" : "does not handle",
                                    m_fNoLoggedInUsers ? "No users logged in" : "Users logged in"));

                        if (fProcessDisconnectOnGuestLogout)
                        {
                            bool fDropConnection = false;
                            if (!m_fNoLoggedInUsers) /* Only if the property really changes. */
                            {
                                if (   utf8Value == "true"
                                    /* Guest property got deleted due to reset,
                                     * so it has no value anymore. */
                                    || utf8Value.isEmpty())
                                {
                                    m_fNoLoggedInUsers = true;
                                    fDropConnection = true;
                                }
                            }
                            else if (utf8Value == "false")
                                m_fNoLoggedInUsers = false;
                            /* Guest property got deleted due to reset,
                             * take the shortcut without touching the m_fNoLoggedInUsers
                             * state. */
                            else if (utf8Value.isEmpty())
                                fDropConnection = true;

                            LogRelFlow(("VRDE: szNoLoggedInUsers=%s, m_fNoLoggedInUsers=%RTbool, fDropConnection=%RTbool\n",
                                        utf8Value.c_str(), m_fNoLoggedInUsers, fDropConnection));

                            if (fDropConnection)
                            {
                                /* If there is a connection, drop it. */
                                ComPtr<IVRDEServerInfo> info;
                                hrc = gConsole->COMGETTER(VRDEServerInfo)(info.asOutParam());
                                if (SUCCEEDED(hrc) && info)
                                {
                                    ULONG cClients = 0;
                                    hrc = info->COMGETTER(NumberOfClients)(&cClients);

                                    LogRelFlow(("VRDE: connected clients=%RU32\n", cClients));
                                    if (SUCCEEDED(hrc) && cClients > 0)
                                    {
                                        ComPtr <IVRDEServer> vrdeServer;
                                        hrc = pMachine->COMGETTER(VRDEServer)(vrdeServer.asOutParam());
                                        if (SUCCEEDED(hrc) && vrdeServer)
                                        {
                                            LogRel(("VRDE: the guest user has logged out, disconnecting remote clients.\n"));
                                            hrc = vrdeServer->COMSETTER(Enabled)(FALSE);
                                            AssertComRC(hrc);
                                            HRESULT hrc2 = vrdeServer->COMSETTER(Enabled)(TRUE);
                                            if (SUCCEEDED(hrc))
                                                hrc = hrc2;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (FAILED(hrc))
                        LogRelFlow(("VRDE: returned error=%Rhrc\n", hrc));
                }

                break;
            }

            default:
                AssertFailed();
        }
        return S_OK;
    }

    void ignorePowerOffEvents(bool fIgnore)
    {
        m_fIgnorePowerOffEvents = fIgnore;
    }

private:

    long mLastVRDEPort;
    bool m_fIgnorePowerOffEvents;
    bool m_fNoLoggedInUsers;
};

typedef ListenerImpl<VirtualPoxClientEventListener> VirtualPoxClientEventListenerImpl;
typedef ListenerImpl<ConsoleEventListener> ConsoleEventListenerImpl;

VPOX_LISTENER_DECLARE(VirtualPoxClientEventListenerImpl)
VPOX_LISTENER_DECLARE(ConsoleEventListenerImpl)

#if !defined(RT_OS_WINDOWS)
static void
HandleSignal(int sig)
{
    RT_NOREF(sig);
    LogRel(("VPoxHeadless: received singal %d\n", sig));
    g_fTerminateFE = true;
}
#endif /* !RT_OS_WINDOWS */

////////////////////////////////////////////////////////////////////////////////

static void show_usage()
{
    RTPrintf("Usage:\n"
             "   -s, -startvm, --startvm <name|uuid>   Start given VM (required argument)\n"
             "   -v, -vrde, --vrde on|off|config       Enable or disable the VRDE server\n"
             "                                           or don't change the setting (default)\n"
             "   -e, -vrdeproperty, --vrdeproperty <name=[value]> Set a VRDE property:\n"
             "                                     \"TCP/Ports\" - comma-separated list of\n"
             "                                       ports the VRDE server can bind to; dash\n"
             "                                       between two port numbers specifies range\n"
             "                                     \"TCP/Address\" - interface IP the VRDE\n"
             "                                       server will bind to\n"
             "   --settingspw <pw>                 Specify the settings password\n"
             "   --settingspwfile <file>           Specify a file containing the\n"
             "                                       settings password\n"
             "   -start-paused, --start-paused     Start the VM in paused state\n"
#ifdef VPOX_WITH_RECORDING
             "   -c, -record, --record             Record the VM screen output to a file\n"
             "   -w, --videowidth                  Video frame width when recording\n"
             "   -h, --videoheight                 Video frame height when recording\n"
             "   -r, --videobitrate                Recording bit rate when recording\n"
             "   -f, --filename                    File name when recording. The codec used\n"
             "                                     will be chosen based on file extension\n"
#endif
             "\n");
}

#ifdef VPOX_WITH_RECORDING
/**
 * Parse the environment for variables which can influence the VIDEOREC settings.
 * purely for backwards compatibility.
 * @param pulFrameWidth may be updated with a desired frame width
 * @param pulFrameHeight may be updated with a desired frame height
 * @param pulBitRate may be updated with a desired bit rate
 * @param ppszFilename may be updated with a desired file name
 */
static void parse_environ(uint32_t *pulFrameWidth, uint32_t *pulFrameHeight,
                          uint32_t *pulBitRate, const char **ppszFilename)
{
    const char *pszEnvTemp;
/** @todo r=bird: This isn't up to scratch. The life time of an RTEnvGet
 *        return value is only up to the next RTEnv*, *getenv, *putenv,
 *        setenv call in _any_ process in the system and the it has known and
 *        documented code page issues.
 *
 *        Use RTEnvGetEx instead! */
    if ((pszEnvTemp = RTEnvGet("VPOX_RECORDWIDTH")) != 0)
    {
        errno = 0;
        unsigned long ulFrameWidth = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
            LogError("VPoxHeadless: ERROR: invalid VPOX_RECORDWIDTH environment variable", 0);
        else
            *pulFrameWidth = ulFrameWidth;
    }
    if ((pszEnvTemp = RTEnvGet("VPOX_RECORDHEIGHT")) != 0)
    {
        errno = 0;
        unsigned long ulFrameHeight = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
            LogError("VPoxHeadless: ERROR: invalid VPOX_RECORDHEIGHT environment variable", 0);
        else
            *pulFrameHeight = ulFrameHeight;
    }
    if ((pszEnvTemp = RTEnvGet("VPOX_RECORDBITRATE")) != 0)
    {
        errno = 0;
        unsigned long ulBitRate = strtoul(pszEnvTemp, 0, 10);
        if (errno != 0)
            LogError("VPoxHeadless: ERROR: invalid VPOX_RECORDBITRATE environment variable", 0);
        else
            *pulBitRate = ulBitRate;
    }
    if ((pszEnvTemp = RTEnvGet("VPOX_RECORDFILE")) != 0)
        *ppszFilename = pszEnvTemp;
}
#endif /* VPOX_WITH_RECORDING defined */


#ifdef RT_OS_WINDOWS

#define MAIN_WND_CLASS L"VirtualPox Headless Interface"

HINSTANCE g_hInstance = NULL;
HWND g_hWindow = NULL;
RTSEMEVENT g_hCanQuit;

static DECLCALLBACK(int) windowsMessageMonitor(RTTHREAD ThreadSelf, void *pvUser);
static int createWindow();
static LRESULT CALLBACK WinMainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void destroyWindow();


static DECLCALLBACK(int)
windowsMessageMonitor(RTTHREAD ThreadSelf, void *pvUser)
{
    RT_NOREF(ThreadSelf, pvUser);
    int rc;

    rc = createWindow();
    if (RT_FAILURE(rc))
        return rc;

    RTSemEventCreate(&g_hCanQuit);

    MSG msg;
    BOOL b;
    while ((b = ::GetMessage(&msg, 0, 0, 0)) > 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    if (b < 0)
        LogRel(("VPoxHeadless: GetMessage failed\n"));

    destroyWindow();
    return VINF_SUCCESS;
}


static int
createWindow()
{
    /* program instance handle */
    g_hInstance = (HINSTANCE)::GetModuleHandle(NULL);
    if (g_hInstance == NULL)
    {
        LogRel(("VPoxHeadless: failed to obtain module handle\n"));
        return VERR_GENERAL_FAILURE;
    }

    /* window class */
    WNDCLASS wc;
    RT_ZERO(wc);

    wc.style = CS_NOCLOSE;
    wc.lpfnWndProc = WinMainWndProc;
    wc.hInstance = g_hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszClassName = MAIN_WND_CLASS;

    ATOM atomWindowClass = ::RegisterClass(&wc);
    if (atomWindowClass == 0)
    {
        LogRel(("VPoxHeadless: failed to register window class\n"));
        return VERR_GENERAL_FAILURE;
    }

    /* secret window, secret garden */
    g_hWindow = ::CreateWindowEx(0, MAIN_WND_CLASS, MAIN_WND_CLASS, 0,
                                 0, 0, 1, 1, NULL, NULL, g_hInstance, NULL);
    if (g_hWindow == NULL)
    {
        LogRel(("VPoxHeadless: failed to create window\n"));
        return VERR_GENERAL_FAILURE;
    }

    return VINF_SUCCESS;
}


static void
destroyWindow()
{
    if (g_hWindow == NULL)
        return;

    ::DestroyWindow(g_hWindow);
    g_hWindow = NULL;

    if (g_hInstance == NULL)
        return;

    ::UnregisterClass(MAIN_WND_CLASS, g_hInstance);
    g_hInstance = NULL;
}


static LRESULT CALLBACK
WinMainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int rc;

    LRESULT lResult = 0;
    switch (msg)
    {
        case WM_QUERYENDSESSION:
            LogRel(("VPoxHeadless: WM_QUERYENDSESSION:%s%s%s%s (0x%08lx)\n",
                    lParam == 0                  ? " shutdown" : "",
                    lParam & ENDSESSION_CRITICAL ? " critical" : "",
                    lParam & ENDSESSION_LOGOFF   ? " logoff"   : "",
                    lParam & ENDSESSION_CLOSEAPP ? " close"    : "",
                    (unsigned long)lParam));

            /* do not block windows session termination */
            lResult = TRUE;
            break;

        case WM_ENDSESSION:
            lResult = 0;
            LogRel(("WM_ENDSESSION:%s%s%s%s%s (%s/0x%08lx)\n",
                    lParam == 0                  ? " shutdown"  : "",
                    lParam & ENDSESSION_CRITICAL ? " critical"  : "",
                    lParam & ENDSESSION_LOGOFF   ? " logoff"    : "",
                    lParam & ENDSESSION_CLOSEAPP ? " close"     : "",
                    wParam == FALSE              ? " cancelled" : "",
                    wParam ? "TRUE" : "FALSE",
                    (unsigned long)lParam));
            if (wParam == FALSE)
                break;

            /* tell the user what we are doing */
            ::ShutdownBlockReasonCreate(hwnd,
                com::BstrFmt("%s saving state",
                             g_strVMName.c_str()).raw());

            /* tell the VM to save state/power off */
            g_fTerminateFE = true;
            gEventQ->interruptEventQueueProcessing();

            if (g_hCanQuit != NIL_RTSEMEVENT)
            {
                LogRel(("VPoxHeadless: WM_ENDSESSION: waiting for VM termination...\n"));

                rc = RTSemEventWait(g_hCanQuit, RT_INDEFINITE_WAIT);
                if (RT_SUCCESS(rc))
                    LogRel(("VPoxHeadless: WM_ENDSESSION: done\n"));
                else
                    LogRel(("VPoxHeadless: WM_ENDSESSION: failed to wait for VM termination: %Rrc\n", rc));
            }
            else
            {
                LogRel(("VPoxHeadless: WM_ENDSESSION: cannot wait for VM termination\n"));
            }
            break;

        default:
            lResult = ::DefWindowProc(hwnd, msg, wParam, lParam);
            break;
    }
    return lResult;
}


static const char * const ctrl_event_names[] = {
    "CTRL_C_EVENT",
    "CTRL_BREAK_EVENT",
    "CTRL_CLOSE_EVENT",
    /* reserved, not used */
    "<console control event 3>",
    "<console control event 4>",
    /* not sent to processes that load gdi32.dll or user32.dll */
    "CTRL_LOGOFF_EVENT",
    "CTRL_SHUTDOWN_EVENT",
};


BOOL WINAPI
ConsoleCtrlHandler(DWORD dwCtrlType) /* RT_NOTHROW_DEF */
{
    const char *signame;
    char namebuf[48];
    int rc;

    if (dwCtrlType < RT_ELEMENTS(ctrl_event_names))
        signame = ctrl_event_names[dwCtrlType];
    else
    {
        /* should not happen, but be prepared */
        RTStrPrintf(namebuf, sizeof(namebuf),
                    "<console control event %lu>", (unsigned long)dwCtrlType);
        signame = namebuf;
    }
    LogRel(("VPoxHeadless: got %s\n", signame));
    RTMsgInfo("Got %s\n", signame);
    RTMsgInfo("");

    /* tell the VM to save state/power off */
    g_fTerminateFE = true;
    gEventQ->interruptEventQueueProcessing();

    /*
     * We don't need to wait for Ctrl-C / Ctrl-Break, but we must wait
     * for Close, or we will be killed before the VM is saved.
     */
    if (g_hCanQuit != NIL_RTSEMEVENT)
    {
        LogRel(("VPoxHeadless: waiting for VM termination...\n"));

        rc = RTSemEventWait(g_hCanQuit, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            LogRel(("VPoxHeadless: Failed to wait for VM termination: %Rrc\n", rc));
    }

    /* tell the system we handled it */
    LogRel(("VPoxHeadless: ConsoleCtrlHandler: return\n"));
    return TRUE;
}
#endif /* RT_OS_WINDOWS */


/*
 * Simplified version of showProgress() borrowed from VPoxManage.
 * Note that machine power up/down operations are not cancelable, so
 * we don't bother checking for signals.
 */
HRESULT
showProgress(const ComPtr<IProgress> &progress)
{
    BOOL fCompleted = FALSE;
    ULONG ulLastPercent = 0;
    ULONG ulCurrentPercent = 0;
    HRESULT hrc;

    com::Bstr bstrDescription;
    hrc = progress->COMGETTER(Description(bstrDescription.asOutParam()));
    if (FAILED(hrc))
    {
        RTStrmPrintf(g_pStdErr, "Failed to get progress description: %Rhrc\n", hrc);
        return hrc;
    }

    RTStrmPrintf(g_pStdErr, "%ls: ", bstrDescription.raw());
    RTStrmFlush(g_pStdErr);

    hrc = progress->COMGETTER(Completed(&fCompleted));
    while (SUCCEEDED(hrc))
    {
        progress->COMGETTER(Percent(&ulCurrentPercent));

        /* did we cross a 10% mark? */
        if (ulCurrentPercent / 10  >  ulLastPercent / 10)
        {
            /* make sure to also print out missed steps */
            for (ULONG curVal = (ulLastPercent / 10) * 10 + 10; curVal <= (ulCurrentPercent / 10) * 10; curVal += 10)
            {
                if (curVal < 100)
                {
                    RTStrmPrintf(g_pStdErr, "%u%%...", curVal);
                    RTStrmFlush(g_pStdErr);
                }
            }
            ulLastPercent = (ulCurrentPercent / 10) * 10;
        }

        if (fCompleted)
            break;

        gEventQ->processEventQueue(500);
        hrc = progress->COMGETTER(Completed(&fCompleted));
    }

    /* complete the line. */
    LONG iRc = E_FAIL;
    hrc = progress->COMGETTER(ResultCode)(&iRc);
    if (SUCCEEDED(hrc))
    {
        if (SUCCEEDED(iRc))
            RTStrmPrintf(g_pStdErr, "100%%\n");
#if 0
        else if (g_fCanceled)
            RTStrmPrintf(g_pStdErr, "CANCELED\n");
#endif
        else
        {
            RTStrmPrintf(g_pStdErr, "\n");
            RTStrmPrintf(g_pStdErr, "Operation failed: %Rhrc\n", iRc);
        }
        hrc = iRc;
    }
    else
    {
        RTStrmPrintf(g_pStdErr, "\n");
        RTStrmPrintf(g_pStdErr, "Failed to obtain operation result: %Rhrc\n", hrc);
    }
    RTStrmFlush(g_pStdErr);
    return hrc;
}


/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    RT_NOREF(envp);
    const char *vrdePort = NULL;
    const char *vrdeAddress = NULL;
    const char *vrdeEnabled = NULL;
    unsigned cVRDEProperties = 0;
    const char *aVRDEProperties[16];
    unsigned fRawR0 = ~0U;
    unsigned fRawR3 = ~0U;
    unsigned fPATM  = ~0U;
    unsigned fCSAM  = ~0U;
    unsigned fPaused = 0;
#ifdef VPOX_WITH_RECORDING
    bool fRecordEnabled = false;
    uint32_t ulRecordVideoWidth = 800;
    uint32_t ulRecordVideoHeight = 600;
    uint32_t ulRecordVideoRate = 300000;
    char szRecordFilename[RTPATH_MAX];
    const char *pszRecordFilenameTemplate = "VPox-%d.webm"; /* .webm container by default. */
#endif /* VPOX_WITH_RECORDING */
#ifdef RT_OS_WINDOWS
    ATL::CComModule _Module; /* Required internally by ATL (constructor records instance in global variable). */
#endif

    LogFlow(("VPoxHeadless STARTED.\n"));
    RTPrintf(VPOX_PRODUCT " Headless Interface " VPOX_VERSION_STRING "\n"
             "(C) 2008-" VPOX_C_YEAR " " VPOX_VENDOR "\n"
             "All rights reserved.\n\n");

#ifdef VPOX_WITH_RECORDING
    /* Parse the environment */
    parse_environ(&ulRecordVideoWidth, &ulRecordVideoHeight, &ulRecordVideoRate, &pszRecordFilenameTemplate);
#endif

    enum eHeadlessOptions
    {
        OPT_RAW_R0 = 0x100,
        OPT_NO_RAW_R0,
        OPT_RAW_R3,
        OPT_NO_RAW_R3,
        OPT_PATM,
        OPT_NO_PATM,
        OPT_CSAM,
        OPT_NO_CSAM,
        OPT_SETTINGSPW,
        OPT_SETTINGSPW_FILE,
        OPT_COMMENT,
        OPT_PAUSED
    };

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "-startvm", 's', RTGETOPT_REQ_STRING },
        { "--startvm", 's', RTGETOPT_REQ_STRING },
        { "-vrdpport", 'p', RTGETOPT_REQ_STRING },     /* VRDE: deprecated. */
        { "--vrdpport", 'p', RTGETOPT_REQ_STRING },    /* VRDE: deprecated. */
        { "-vrdpaddress", 'a', RTGETOPT_REQ_STRING },  /* VRDE: deprecated. */
        { "--vrdpaddress", 'a', RTGETOPT_REQ_STRING }, /* VRDE: deprecated. */
        { "-vrdp", 'v', RTGETOPT_REQ_STRING },         /* VRDE: deprecated. */
        { "--vrdp", 'v', RTGETOPT_REQ_STRING },        /* VRDE: deprecated. */
        { "-vrde", 'v', RTGETOPT_REQ_STRING },
        { "--vrde", 'v', RTGETOPT_REQ_STRING },
        { "-vrdeproperty", 'e', RTGETOPT_REQ_STRING },
        { "--vrdeproperty", 'e', RTGETOPT_REQ_STRING },
        { "-rawr0", OPT_RAW_R0, 0 },
        { "--rawr0", OPT_RAW_R0, 0 },
        { "-norawr0", OPT_NO_RAW_R0, 0 },
        { "--norawr0", OPT_NO_RAW_R0, 0 },
        { "-rawr3", OPT_RAW_R3, 0 },
        { "--rawr3", OPT_RAW_R3, 0 },
        { "-norawr3", OPT_NO_RAW_R3, 0 },
        { "--norawr3", OPT_NO_RAW_R3, 0 },
        { "-patm", OPT_PATM, 0 },
        { "--patm", OPT_PATM, 0 },
        { "-nopatm", OPT_NO_PATM, 0 },
        { "--nopatm", OPT_NO_PATM, 0 },
        { "-csam", OPT_CSAM, 0 },
        { "--csam", OPT_CSAM, 0 },
        { "-nocsam", OPT_NO_CSAM, 0 },
        { "--nocsam", OPT_NO_CSAM, 0 },
        { "--settingspw", OPT_SETTINGSPW, RTGETOPT_REQ_STRING },
        { "--settingspwfile", OPT_SETTINGSPW_FILE, RTGETOPT_REQ_STRING },
#ifdef VPOX_WITH_RECORDING
        { "-record", 'c', 0 },
        { "--record", 'c', 0 },
        { "--videowidth", 'w', RTGETOPT_REQ_UINT32 },
        { "--videoheight", 'h', RTGETOPT_REQ_UINT32 }, /* great choice of short option! */
        { "--videorate", 'r', RTGETOPT_REQ_UINT32 },
        { "--filename", 'f', RTGETOPT_REQ_STRING },
#endif /* VPOX_WITH_RECORDING defined */
        { "-comment", OPT_COMMENT, RTGETOPT_REQ_STRING },
        { "--comment", OPT_COMMENT, RTGETOPT_REQ_STRING },
        { "-start-paused", OPT_PAUSED, 0 },
        { "--start-paused", OPT_PAUSED, 0 }
    };

    const char *pcszNameOrUUID = NULL;

    // parse the command line
    int ch;
    const char *pcszSettingsPw = NULL;
    const char *pcszSettingsPwFile = NULL;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch(ch)
        {
            case 's':
                pcszNameOrUUID = ValueUnion.psz;
                break;
            case 'p':
                RTPrintf("Warning: '-p' or '-vrdpport' are deprecated. Use '-e \"TCP/Ports=%s\"'\n", ValueUnion.psz);
                vrdePort = ValueUnion.psz;
                break;
            case 'a':
                RTPrintf("Warning: '-a' or '-vrdpaddress' are deprecated. Use '-e \"TCP/Address=%s\"'\n", ValueUnion.psz);
                vrdeAddress = ValueUnion.psz;
                break;
            case 'v':
                vrdeEnabled = ValueUnion.psz;
                break;
            case 'e':
                if (cVRDEProperties < RT_ELEMENTS(aVRDEProperties))
                    aVRDEProperties[cVRDEProperties++] = ValueUnion.psz;
                else
                     RTPrintf("Warning: too many VRDE properties. Ignored: '%s'\n", ValueUnion.psz);
                break;
            case OPT_RAW_R0:
                fRawR0 = true;
                break;
            case OPT_NO_RAW_R0:
                fRawR0 = false;
                break;
            case OPT_RAW_R3:
                fRawR3 = true;
                break;
            case OPT_NO_RAW_R3:
                fRawR3 = false;
                break;
            case OPT_PATM:
                fPATM = true;
                break;
            case OPT_NO_PATM:
                fPATM = false;
                break;
            case OPT_CSAM:
                fCSAM = true;
                break;
            case OPT_NO_CSAM:
                fCSAM = false;
                break;
            case OPT_SETTINGSPW:
                pcszSettingsPw = ValueUnion.psz;
                break;
            case OPT_SETTINGSPW_FILE:
                pcszSettingsPwFile = ValueUnion.psz;
                break;
            case OPT_PAUSED:
                fPaused = true;
                break;
#ifdef VPOX_WITH_RECORDING
            case 'c':
                fRecordEnabled = true;
                break;
            case 'w':
                ulRecordVideoWidth = ValueUnion.u32;
                break;
            case 'r':
                ulRecordVideoRate = ValueUnion.u32;
                break;
            case 'f':
                pszRecordFilenameTemplate = ValueUnion.psz;
                break;
#endif /* VPOX_WITH_RECORDING defined */
            case 'h':
#ifdef VPOX_WITH_RECORDING
                if ((GetState.pDef->fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING)
                {
                    ulRecordVideoHeight = ValueUnion.u32;
                    break;
                }
#endif
                show_usage();
                return 0;
            case OPT_COMMENT:
                /* nothing to do */
                break;
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;
            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                show_usage();
                return ch;
        }
    }

#ifdef VPOX_WITH_RECORDING
    if (ulRecordVideoWidth < 512 || ulRecordVideoWidth > 2048 || ulRecordVideoWidth % 2)
    {
        LogError("VPoxHeadless: ERROR: please specify an even video frame width between 512 and 2048", 0);
        return 1;
    }
    if (ulRecordVideoHeight < 384 || ulRecordVideoHeight > 1536 || ulRecordVideoHeight % 2)
    {
        LogError("VPoxHeadless: ERROR: please specify an even video frame height between 384 and 1536", 0);
        return 1;
    }
    if (ulRecordVideoRate < 300000 || ulRecordVideoRate > 1000000)
    {
        LogError("VPoxHeadless: ERROR: please specify an even video bitrate between 300000 and 1000000", 0);
        return 1;
    }
    /* Make sure we only have %d or %u (or none) in the file name specified */
    char *pcPercent = (char*)strchr(pszRecordFilenameTemplate, '%');
    if (pcPercent != 0 && *(pcPercent + 1) != 'd' && *(pcPercent + 1) != 'u')
    {
        LogError("VPoxHeadless: ERROR: Only %%d and %%u are allowed in the recording file name.", -1);
        return 1;
    }
    /* And no more than one % in the name */
    if (pcPercent != 0 && strchr(pcPercent + 1, '%') != 0)
    {
        LogError("VPoxHeadless: ERROR: Only one format modifier is allowed in the recording file name.", -1);
        return 1;
    }
    RTStrPrintf(&szRecordFilename[0], RTPATH_MAX, pszRecordFilenameTemplate, RTProcSelf());
#endif /* defined VPOX_WITH_RECORDING */

    if (!pcszNameOrUUID)
    {
        show_usage();
        return 1;
    }

    HRESULT rc;
    int irc;

    rc = com::Initialize();
#ifdef VPOX_WITH_XPCOM
    if (rc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVPoxUserHomeDirectory(szHome, sizeof(szHome));
        RTPrintf("Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
        return 1;
    }
#endif
    if (FAILED(rc))
    {
        RTPrintf("VPoxHeadless: ERROR: failed to initialize COM!\n");
        return 1;
    }

    ComPtr<IVirtualPoxClient> pVirtualPoxClient;
    ComPtr<IVirtualPox> virtualPox;
    ComPtr<ISession> session;
    ComPtr<IMachine> machine;
    bool fSessionOpened = false;
    ComPtr<IEventListener> vpoxClientListener;
    ComPtr<IEventListener> vpoxListener;
    ComObjPtr<ConsoleEventListenerImpl> consoleListener;

    do
    {
        rc = pVirtualPoxClient.createInprocObject(CLSID_VirtualPoxClient);
        if (FAILED(rc))
        {
            RTPrintf("VPoxHeadless: ERROR: failed to create the VirtualPoxClient object!\n");
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
            {
                com::GluePrintRCMessage(rc);
                RTPrintf("Most likely, the VirtualPox COM server is not running or failed to start.\n");
            }
            else
                GluePrintErrorInfo(info);
            break;
        }

        rc = pVirtualPoxClient->COMGETTER(VirtualPox)(virtualPox.asOutParam());
        if (FAILED(rc))
        {
            RTPrintf("Failed to get VirtualPox object (rc=%Rhrc)!\n", rc);
            break;
        }
        rc = pVirtualPoxClient->COMGETTER(Session)(session.asOutParam());
        if (FAILED(rc))
        {
            RTPrintf("Failed to get session object (rc=%Rhrc)!\n", rc);
            break;
        }

        if (pcszSettingsPw)
        {
            CHECK_ERROR(virtualPox, SetSettingsSecret(Bstr(pcszSettingsPw).raw()));
            if (FAILED(rc))
                break;
        }
        else if (pcszSettingsPwFile)
        {
            int rcExit = settingsPasswordFile(virtualPox, pcszSettingsPwFile);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;
        }

        ComPtr<IMachine> m;

        rc = virtualPox->FindMachine(Bstr(pcszNameOrUUID).raw(), m.asOutParam());
        if (FAILED(rc))
        {
            LogError("Invalid machine name or UUID!\n", rc);
            break;
        }

        Bstr bstrVMId;
        rc = m->COMGETTER(Id)(bstrVMId.asOutParam());
        AssertComRC(rc);
        if (FAILED(rc))
            break;
        g_strVMUUID = bstrVMId;

        Bstr bstrVMName;
        rc = m->COMGETTER(Name)(bstrVMName.asOutParam());
        AssertComRC(rc);
        if (FAILED(rc))
            break;
        g_strVMName = bstrVMName;

        Log(("VPoxHeadless: Opening a session with machine (id={%s})...\n",
             g_strVMUUID.c_str()));

        // set session name
        CHECK_ERROR_BREAK(session, COMSETTER(Name)(Bstr("headless").raw()));
        // open a session
        CHECK_ERROR_BREAK(m, LockMachine(session, LockType_VM));
        fSessionOpened = true;

        /* get the console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(session, COMGETTER(Console)(console.asOutParam()));

        /* get the mutable machine */
        CHECK_ERROR_BREAK(console, COMGETTER(Machine)(machine.asOutParam()));

        ComPtr<IDisplay> display;
        CHECK_ERROR_BREAK(console, COMGETTER(Display)(display.asOutParam()));

#ifdef VPOX_WITH_RECORDING
        if (fRecordEnabled)
        {
            ComPtr<IRecordingSettings> recordingSettings;
            CHECK_ERROR_BREAK(machine, COMGETTER(RecordingSettings)(recordingSettings.asOutParam()));
            CHECK_ERROR_BREAK(recordingSettings, COMSETTER(Enabled)(TRUE));

            SafeIfaceArray <IRecordingScreenSettings> saRecordScreenScreens;
            CHECK_ERROR_BREAK(recordingSettings, COMGETTER(Screens)(ComSafeArrayAsOutParam(saRecordScreenScreens)));

            /* Note: For now all screens have the same configuration. */
            for (size_t i = 0; i < saRecordScreenScreens.size(); ++i)
            {
                CHECK_ERROR_BREAK(saRecordScreenScreens[i], COMSETTER(Enabled)(TRUE));
                CHECK_ERROR_BREAK(saRecordScreenScreens[i], COMSETTER(Filename)(Bstr(szRecordFilename).raw()));
                CHECK_ERROR_BREAK(saRecordScreenScreens[i], COMSETTER(VideoWidth)(ulRecordVideoWidth));
                CHECK_ERROR_BREAK(saRecordScreenScreens[i], COMSETTER(VideoHeight)(ulRecordVideoHeight));
                CHECK_ERROR_BREAK(saRecordScreenScreens[i], COMSETTER(VideoRate)(ulRecordVideoRate));
            }
        }
#endif /* defined(VPOX_WITH_RECORDING) */

        /* get the machine debugger (isn't necessarily available) */
        ComPtr <IMachineDebugger> machineDebugger;
        console->COMGETTER(Debugger)(machineDebugger.asOutParam());
        if (machineDebugger)
        {
            Log(("Machine debugger available!\n"));
        }

        if (fRawR0 != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%srawr0 cannot be executed!\n", fRawR0 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(RecompileSupervisor)(!fRawR0);
        }
        if (fRawR3 != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%srawr3 cannot be executed!\n", fRawR3 ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(RecompileUser)(!fRawR3);
        }
        if (fPATM != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%spatm cannot be executed!\n", fPATM ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(PATMEnabled)(fPATM);
        }
        if (fCSAM != ~0U)
        {
            if (!machineDebugger)
            {
                RTPrintf("Error: No debugger object; -%scsam cannot be executed!\n", fCSAM ? "" : "no");
                break;
            }
            machineDebugger->COMSETTER(CSAMEnabled)(fCSAM);
        }

        /* initialize global references */
        gConsole = console;
        gEventQ = com::NativeEventQueue::getMainEventQueue();

        /* VirtualPoxClient events registration. */
        {
            ComPtr<IEventSource> pES;
            CHECK_ERROR(pVirtualPoxClient, COMGETTER(EventSource)(pES.asOutParam()));
            ComObjPtr<VirtualPoxClientEventListenerImpl> listener;
            listener.createObject();
            listener->init(new VirtualPoxClientEventListener());
            vpoxClientListener = listener;
            com::SafeArray<VPoxEventType_T> eventTypes;
            eventTypes.push_back(VPoxEventType_OnVPoxSVCAvailabilityChanged);
            CHECK_ERROR(pES, RegisterListener(vpoxClientListener, ComSafeArrayAsInParam(eventTypes), true));
        }

        /* Console events registration. */
        {
            ComPtr<IEventSource> es;
            CHECK_ERROR(console, COMGETTER(EventSource)(es.asOutParam()));
            consoleListener.createObject();
            consoleListener->init(new ConsoleEventListener());
            com::SafeArray<VPoxEventType_T> eventTypes;
            eventTypes.push_back(VPoxEventType_OnMouseCapabilityChanged);
            eventTypes.push_back(VPoxEventType_OnStateChanged);
            eventTypes.push_back(VPoxEventType_OnVRDEServerInfoChanged);
            eventTypes.push_back(VPoxEventType_OnCanShowWindow);
            eventTypes.push_back(VPoxEventType_OnShowWindow);
            eventTypes.push_back(VPoxEventType_OnGuestPropertyChanged);
            CHECK_ERROR(es, RegisterListener(consoleListener, ComSafeArrayAsInParam(eventTypes), true));
        }

        /* Default is to use the VM setting for the VRDE server. */
        enum VRDEOption
        {
            VRDEOption_Config,
            VRDEOption_Off,
            VRDEOption_On
        };
        VRDEOption enmVRDEOption = VRDEOption_Config;
        BOOL fVRDEEnabled;
        ComPtr <IVRDEServer> vrdeServer;
        CHECK_ERROR_BREAK(machine, COMGETTER(VRDEServer)(vrdeServer.asOutParam()));
        CHECK_ERROR_BREAK(vrdeServer, COMGETTER(Enabled)(&fVRDEEnabled));

        if (vrdeEnabled != NULL)
        {
            /* -vrde on|off|config */
            if (!strcmp(vrdeEnabled, "off") || !strcmp(vrdeEnabled, "disable"))
                enmVRDEOption = VRDEOption_Off;
            else if (!strcmp(vrdeEnabled, "on") || !strcmp(vrdeEnabled, "enable"))
                enmVRDEOption = VRDEOption_On;
            else if (strcmp(vrdeEnabled, "config"))
            {
                RTPrintf("-vrde requires an argument (on|off|config)\n");
                break;
            }
        }

        Log(("VPoxHeadless: enmVRDE %d, fVRDEEnabled %d\n", enmVRDEOption, fVRDEEnabled));

        if (enmVRDEOption != VRDEOption_Off)
        {
            /* Set other specified options. */

            /* set VRDE port if requested by the user */
            if (vrdePort != NULL)
            {
                Bstr bstr = vrdePort;
                CHECK_ERROR_BREAK(vrdeServer, SetVRDEProperty(Bstr("TCP/Ports").raw(), bstr.raw()));
            }
            /* set VRDE address if requested by the user */
            if (vrdeAddress != NULL)
            {
                CHECK_ERROR_BREAK(vrdeServer, SetVRDEProperty(Bstr("TCP/Address").raw(), Bstr(vrdeAddress).raw()));
            }

            /* Set VRDE properties. */
            if (cVRDEProperties > 0)
            {
                for (unsigned i = 0; i < cVRDEProperties; i++)
                {
                    /* Parse 'name=value' */
                    char *pszProperty = RTStrDup(aVRDEProperties[i]);
                    if (pszProperty)
                    {
                        char *pDelimiter = strchr(pszProperty, '=');
                        if (pDelimiter)
                        {
                            *pDelimiter = '\0';

                            Bstr bstrName = pszProperty;
                            Bstr bstrValue = &pDelimiter[1];
                            CHECK_ERROR_BREAK(vrdeServer, SetVRDEProperty(bstrName.raw(), bstrValue.raw()));
                        }
                        else
                        {
                            RTPrintf("Error: Invalid VRDE property '%s'\n", aVRDEProperties[i]);
                            RTStrFree(pszProperty);
                            rc = E_INVALIDARG;
                            break;
                        }
                        RTStrFree(pszProperty);
                    }
                    else
                    {
                        RTPrintf("Error: Failed to allocate memory for VRDE property '%s'\n", aVRDEProperties[i]);
                        rc = E_OUTOFMEMORY;
                        break;
                    }
                }
                if (FAILED(rc))
                    break;
            }

        }

        if (enmVRDEOption == VRDEOption_On)
        {
            /* enable VRDE server (only if currently disabled) */
            if (!fVRDEEnabled)
            {
                CHECK_ERROR_BREAK(vrdeServer, COMSETTER(Enabled)(TRUE));
            }
        }
        else if (enmVRDEOption == VRDEOption_Off)
        {
            /* disable VRDE server (only if currently enabled */
            if (fVRDEEnabled)
            {
                CHECK_ERROR_BREAK(vrdeServer, COMSETTER(Enabled)(FALSE));
            }
        }

        /* Disable the host clipboard before powering up */
        console->COMSETTER(UseHostClipboard)(false);

        Log(("VPoxHeadless: Powering up the machine...\n"));


        /**
         * @todo We should probably install handlers earlier so that
         * we can undo any temporary settings we do above in case of
         * an early signal and use RAII to ensure proper cleanup.
         */
#if !defined(RT_OS_WINDOWS)
        signal(SIGPIPE, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        struct sigaction sa;
        RT_ZERO(sa);
        sa.sa_handler = HandleSignal;
        sigaction(SIGHUP,  &sa, NULL);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
        /* Don't touch SIGUSR2 as IPRT could be using it for RTThreadPoke(). */

#else /* RT_OS_WINDOWS */
        /*
         * Register windows console signal handler to react to Ctrl-C,
         * Ctrl-Break, Close, non-interactive session termination.
         */
        ::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif


        ComPtr <IProgress> progress;
        if (!fPaused)
            CHECK_ERROR_BREAK(console, PowerUp(progress.asOutParam()));
        else
            CHECK_ERROR_BREAK(console, PowerUpPaused(progress.asOutParam()));

        rc = showProgress(progress);
        if (FAILED(rc))
        {
            com::ProgressErrorInfo info(progress);
            if (info.isBasicAvailable())
            {
                RTPrintf("Error: failed to start machine. Error message: %ls\n", info.getText().raw());
            }
            else
            {
                RTPrintf("Error: failed to start machine. No error message available!\n");
            }
            break;
        }

#ifdef RT_OS_WINDOWS
        /*
         * Spawn windows message pump to monitor session events.
         */
        RTTHREAD hThrMsg;
        irc = RTThreadCreate(&hThrMsg,
                            windowsMessageMonitor, NULL,
                            0, /* :cbStack */
                            RTTHREADTYPE_MSG_PUMP, 0,
                            "MSG");
        if (RT_FAILURE(irc))    /* not fatal */
            LogRel(("VPoxHeadless: failed to start windows message monitor: %Rrc\n", irc));
#endif /* RT_OS_WINDOWS */


        /*
         * Pump vpox events forever
         */
        LogRel(("VPoxHeadless: starting event loop\n"));
        for (;;)
        {
            irc = gEventQ->processEventQueue(RT_INDEFINITE_WAIT);

            /*
             * interruptEventQueueProcessing from another thread is
             * reported as VERR_INTERRUPTED, so check the flag first.
             */
            if (g_fTerminateFE)
            {
                LogRel(("VPoxHeadless: processEventQueue: %Rrc, termination requested\n", irc));
                break;
            }

            if (RT_FAILURE(irc))
            {
                LogRel(("VPoxHeadless: processEventQueue: %Rrc\n", irc));
                RTMsgError("event loop: %Rrc", irc);
                break;
            }
        }

        Log(("VPoxHeadless: event loop has terminated...\n"));

#ifdef VPOX_WITH_RECORDING
        if (fRecordEnabled)
        {
            if (!machine.isNull())
            {
                ComPtr<IRecordingSettings> recordingSettings;
                CHECK_ERROR_BREAK(machine, COMGETTER(RecordingSettings)(recordingSettings.asOutParam()));
                CHECK_ERROR_BREAK(recordingSettings, COMSETTER(Enabled)(FALSE));
            }
        }
#endif /* VPOX_WITH_RECORDING */

        /* we don't have to disable VRDE here because we don't save the settings of the VM */
    }
    while (0);

    /*
     * Get the machine state.
     */
    MachineState_T machineState = MachineState_Aborted;
    if (!machine.isNull())
    {
        rc = machine->COMGETTER(State)(&machineState);
        if (SUCCEEDED(rc))
            Log(("machine state = %RU32\n", machineState));
        else
            Log(("IMachine::getState: %Rhrc\n", rc));
    }
    else
    {
        Log(("machine == NULL\n"));
    }

    /*
     * Turn off the VM if it's running
     */
    if (   gConsole
        && (   machineState == MachineState_Running
            || machineState == MachineState_Teleporting
            || machineState == MachineState_LiveSnapshotting
            /** @todo power off paused VMs too? */
           )
       )
    do
    {
        consoleListener->getWrapped()->ignorePowerOffEvents(true);

        ComPtr<IProgress> pProgress;
        if (!machine.isNull())
            CHECK_ERROR_BREAK(machine, SaveState(pProgress.asOutParam()));
        else
            CHECK_ERROR_BREAK(gConsole, PowerDown(pProgress.asOutParam()));

        rc = showProgress(pProgress);
        if (FAILED(rc))
        {
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
                com::GluePrintRCMessage(rc);
            else
                com::GluePrintErrorInfo(info);
            break;
        }
    } while (0);

    /* VirtualPox callback unregistration. */
    if (vpoxListener)
    {
        ComPtr<IEventSource> es;
        CHECK_ERROR(virtualPox, COMGETTER(EventSource)(es.asOutParam()));
        if (!es.isNull())
            CHECK_ERROR(es, UnregisterListener(vpoxListener));
        vpoxListener.setNull();
    }

    /* Console callback unregistration. */
    if (consoleListener)
    {
        ComPtr<IEventSource> es;
        CHECK_ERROR(gConsole, COMGETTER(EventSource)(es.asOutParam()));
        if (!es.isNull())
            CHECK_ERROR(es, UnregisterListener(consoleListener));
        consoleListener.setNull();
    }

    /* VirtualPoxClient callback unregistration. */
    if (vpoxClientListener)
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(pVirtualPoxClient, COMGETTER(EventSource)(pES.asOutParam()));
        if (!pES.isNull())
            CHECK_ERROR(pES, UnregisterListener(vpoxClientListener));
        vpoxClientListener.setNull();
    }

    /* No more access to the 'console' object, which will be uninitialized by the next session->Close call. */
    gConsole = NULL;

    if (fSessionOpened)
    {
        /*
         * Close the session. This will also uninitialize the console and
         * unregister the callback we've registered before.
         */
        Log(("VPoxHeadless: Closing the session...\n"));
        session->UnlockMachine();
    }

    /* Must be before com::Shutdown */
    session.setNull();
    virtualPox.setNull();
    pVirtualPoxClient.setNull();
    machine.setNull();

    com::Shutdown();

#ifdef RT_OS_WINDOWS
    /* tell the session monitor it can ack WM_ENDSESSION */
    if (g_hCanQuit != NIL_RTSEMEVENT)
    {
        RTSemEventSignal(g_hCanQuit);
    }

    /* tell the session monitor to quit */
    if (g_hWindow != NULL)
    {
        ::PostMessage(g_hWindow, WM_QUIT, 0, 0);
    }
#endif

    LogRel(("VPoxHeadless: exiting\n"));
    return FAILED(rc) ? 1 : 0;
}


#ifndef VPOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main(int argc, char **argv, char **envp)
{
    // initialize VPox Runtime
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
    {
        RTPrintf("VPoxHeadless: Runtime Error:\n"
                 " %Rrc -- %Rrf\n", rc, rc);
        switch (rc)
        {
            case VERR_VM_DRIVER_NOT_INSTALLED:
                RTPrintf("Cannot access the kernel driver. Make sure the kernel module has been \n"
                        "loaded successfully. Aborting ...\n");
                break;
            default:
                break;
        }
        return 1;
    }

    return TrustedMain(argc, argv, envp);
}
#endif /* !VPOX_WITH_HARDENING */
