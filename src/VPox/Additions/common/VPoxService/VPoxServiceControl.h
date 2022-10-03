/* $Id: VPoxServiceControl.h $ */
/** @file
 * VPoxServiceControl.h - Internal guest control definitions.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_common_VPoxService_VPoxServiceControl_h
#define GA_INCLUDED_SRC_common_VPoxService_VPoxServiceControl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>
#include <iprt/list.h>
#include <iprt/req.h>

#include <VPox/VPoxGuestLib.h>
#include <VPox/GuestHost/GuestControl.h>
#include <VPox/HostServices/GuestControlSvc.h>


/**
 * Pipe IDs for handling the guest process poll set.
 */
typedef enum VPOXSERVICECTRLPIPEID
{
    VPOXSERVICECTRLPIPEID_UNKNOWN           = 0,
    VPOXSERVICECTRLPIPEID_STDIN             = 10,
    VPOXSERVICECTRLPIPEID_STDIN_WRITABLE    = 11,
    /** Pipe for reading from guest process' stdout. */
    VPOXSERVICECTRLPIPEID_STDOUT            = 40,
    /** Pipe for reading from guest process' stderr. */
    VPOXSERVICECTRLPIPEID_STDERR            = 50,
    /** Notification pipe for waking up the guest process
     *  control thread. */
    VPOXSERVICECTRLPIPEID_IPC_NOTIFY        = 100
} VPOXSERVICECTRLPIPEID;

/**
 * Structure for one (opened) guest file.
 */
typedef struct VPOXSERVICECTRLFILE
{
    /** Pointer to list archor of following
     *  list node.
     *  @todo Would be nice to have a RTListGetAnchor(). */
    PRTLISTANCHOR                   pAnchor;
    /** Node to global guest control file list. */
    /** @todo Use a map later? */
    RTLISTNODE                      Node;
    /** The file name. */
    char                           *pszName;
    /** The file handle on the guest. */
    RTFILE                          hFile;
    /** File handle to identify this file. */
    uint32_t                        uHandle;
    /** Context ID. */
    uint32_t                        uContextID;
    /** RTFILE_O_XXX flags. */
    uint64_t                        fOpen;
} VPOXSERVICECTRLFILE;
/** Pointer to thread data. */
typedef VPOXSERVICECTRLFILE *PVPOXSERVICECTRLFILE;

/**
 * Structure for a guest session thread to
 * observe/control the forked session instance from
 * the VPoxService main executable.
 */
typedef struct VPOXSERVICECTRLSESSIONTHREAD
{
    /** Node to global guest control session list. */
    /** @todo Use a map later? */
    RTLISTNODE                      Node;
    /** The sessions's startup info. */
    PVBGLR3GUESTCTRLSESSIONSTARTUPINFO
                                    pStartupInfo;
    /** Critical section for thread-safe use. */
    RTCRITSECT                      CritSect;
    /** The worker thread. */
    RTTHREAD                        Thread;
    /** Process handle for forked child. */
    RTPROCESS                       hProcess;
    /** Shutdown indicator; will be set when the thread
      * needs (or is asked) to shutdown. */
    bool volatile                   fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile                   fStopped;
    /** Whether the thread was started or not. */
    bool                            fStarted;
#if 0 /* Pipe IPC not used yet. */
    /** Pollset containing all the pipes. */
    RTPOLLSET                       hPollSet;
    RTPIPE                          hStdInW;
    RTPIPE                          hStdOutR;
    RTPIPE                          hStdErrR;
    struct StdPipe
    {
        RTHANDLE  hChild;
        PRTHANDLE phChild;
    }                               StdIn,
                                    StdOut,
                                    StdErr;
    /** The notification pipe associated with this guest session.
     *  This is NIL_RTPIPE for output pipes. */
    RTPIPE                          hNotificationPipeW;
    /** The other end of hNotificationPipeW. */
    RTPIPE                          hNotificationPipeR;
#endif
    /** Pipe for handing the secret key to the session process. */
    RTPIPE                          hKeyPipe;
    /** Secret key. */
    uint8_t                         abKey[_4K];
} VPOXSERVICECTRLSESSIONTHREAD;
/** Pointer to thread data. */
typedef VPOXSERVICECTRLSESSIONTHREAD *PVPOXSERVICECTRLSESSIONTHREAD;

/** Defines the prefix being used for telling our service executable that we're going
 *  to spawn a new (Guest Control) user session. */
#define VPOXSERVICECTRLSESSION_GETOPT_PREFIX             "guestsession"

/** Flag indicating that this session has been spawned from
 *  the main executable. */
#define VPOXSERVICECTRLSESSION_FLAG_SPAWN                RT_BIT(0)
/** Flag indicating that this session is anonymous, that is,
 *  it will run start guest processes with the same credentials
 *  as the main executable. */
#define VPOXSERVICECTRLSESSION_FLAG_ANONYMOUS            RT_BIT(1)
/** Flag indicating that started guest processes will dump their
 *  stdout output to a separate file on disk. For debugging. */
#define VPOXSERVICECTRLSESSION_FLAG_DUMPSTDOUT           RT_BIT(2)
/** Flag indicating that started guest processes will dump their
 *  stderr output to a separate file on disk. For debugging. */
#define VPOXSERVICECTRLSESSION_FLAG_DUMPSTDERR           RT_BIT(3)

/**
 * Structure for maintaining a guest session. This also
 * contains all started threads (e.g. for guest processes).
 *
 * This structure can act in two different ways:
 * - For legacy guest control handling (protocol version < 2)
 *   this acts as a per-guest process structure containing all
 *   the information needed to get a guest process up and running.
 * - For newer guest control protocols (>= 2) this structure is
 *   part of the forked session child, maintaining all guest
 *   control objects under it.
 */
typedef struct VPOXSERVICECTRLSESSION
{
    /* The session's startup information. */
    VBGLR3GUESTCTRLSESSIONSTARTUPINFO
                                    StartupInfo;
    /** List of active guest process threads
     *  (VPOXSERVICECTRLPROCESS). */
    RTLISTANCHOR                    lstProcesses;
    /** Number of guest processes in the process list. */
    uint32_t                        cProcesses;
    /** List of guest control files (VPOXSERVICECTRLFILE). */
    RTLISTANCHOR                    lstFiles;
    /** Number of guest files in the file list. */
    uint32_t                        cFiles;
    /** The session's critical section. */
    RTCRITSECT                      CritSect;
    /** Internal session flags, not related
     *  to StartupInfo stuff.
     *  @sa VPOXSERVICECTRLSESSION_FLAG_* flags. */
    uint32_t                        fFlags;
    /** How many processes do we allow keeping around at a time? */
    uint32_t                        uProcsMaxKept;
} VPOXSERVICECTRLSESSION;
/** Pointer to guest session. */
typedef VPOXSERVICECTRLSESSION *PVPOXSERVICECTRLSESSION;

/**
 * Structure for holding data for one (started) guest process.
 */
typedef struct VPOXSERVICECTRLPROCESS
{
    /** Node. */
    RTLISTNODE                      Node;
    /** Process handle. */
    RTPROCESS                       hProcess;
    /** Number of references using this struct. */
    uint32_t                        cRefs;
    /** The worker thread. */
    RTTHREAD                        Thread;
    /** The session this guest process
     *  is bound to. */
    PVPOXSERVICECTRLSESSION         pSession;
    /** Shutdown indicator; will be set when the thread
      * needs (or is asked) to shutdown. */
    bool volatile                   fShutdown;
    /** Whether the guest process thread was stopped or not. */
    bool volatile                   fStopped;
    /** Whether the guest process thread was started or not. */
    bool                            fStarted;
    /** Context ID. */
    uint32_t                        uContextID;
    /** Critical section for thread-safe use. */
    RTCRITSECT                      CritSect;
    /** Process startup information. */
    PVBGLR3GUESTCTRLPROCSTARTUPINFO
                                    pStartupInfo;
    /** The process' PID assigned by the guest OS. */
    uint32_t                        uPID;
    /** The process' request queue to handle requests
     *  from the outside, e.g. the session. */
    RTREQQUEUE                      hReqQueue;
    /** Our pollset, used for accessing the process'
     *  std* pipes + the notification pipe. */
    RTPOLLSET                       hPollSet;
    /** StdIn pipe for addressing writes to the
     *  guest process' stdin.*/
    RTPIPE                          hPipeStdInW;
    /** StdOut pipe for addressing reads from
     *  guest process' stdout.*/
    RTPIPE                          hPipeStdOutR;
    /** StdOut pipe for addressing reads from
     *  guest process' stderr.*/
    RTPIPE                          hPipeStdErrR;

    /** The write end of the notification pipe that is used to poke the thread
     * monitoring the process.
     *  This is NIL_RTPIPE for output pipes. */
    RTPIPE                          hNotificationPipeW;
    /** The other end of hNotificationPipeW, read by vgsvcGstCtrlProcessProcLoop(). */
    RTPIPE                          hNotificationPipeR;
} VPOXSERVICECTRLPROCESS;
/** Pointer to thread data. */
typedef VPOXSERVICECTRLPROCESS *PVPOXSERVICECTRLPROCESS;

RT_C_DECLS_BEGIN

extern RTLISTANCHOR             g_lstControlSessionThreads;
extern VPOXSERVICECTRLSESSION   g_Session;
extern uint32_t                 g_idControlSvcClient;
extern uint64_t                 g_fControlHostFeatures0;
extern bool                     g_fControlSupportsOptimizations;


/** @name Guest session thread handling.
 * @{ */
extern int                      VGSvcGstCtrlSessionThreadCreate(PRTLISTANCHOR pList, const PVBGLR3GUESTCTRLSESSIONSTARTUPINFO pSessionStartupInfo, PVPOXSERVICECTRLSESSIONTHREAD *ppSessionThread);
extern int                      VGSvcGstCtrlSessionThreadDestroy(PVPOXSERVICECTRLSESSIONTHREAD pSession, uint32_t uFlags);
extern int                      VGSvcGstCtrlSessionThreadDestroyAll(PRTLISTANCHOR pList, uint32_t uFlags);
extern int                      VGSvcGstCtrlSessionThreadTerminate(PVPOXSERVICECTRLSESSIONTHREAD pSession);
extern RTEXITCODE               VGSvcGstCtrlSessionSpawnInit(int argc, char **argv);
/** @} */
/** @name Per-session functions.
 * @{ */
extern PVPOXSERVICECTRLPROCESS  VGSvcGstCtrlSessionRetainProcess(PVPOXSERVICECTRLSESSION pSession, uint32_t uPID);
extern int                      VGSvcGstCtrlSessionClose(PVPOXSERVICECTRLSESSION pSession);
extern int                      VGSvcGstCtrlSessionDestroy(PVPOXSERVICECTRLSESSION pSession);
extern int                      VGSvcGstCtrlSessionInit(PVPOXSERVICECTRLSESSION pSession, uint32_t uFlags);
extern int                      VGSvcGstCtrlSessionHandler(PVPOXSERVICECTRLSESSION pSession, uint32_t uMsg, PVBGLR3GUESTCTRLCMDCTX pHostCtx, void *pvScratchBuf, size_t cbScratchBuf, volatile bool *pfShutdown);
extern int                      VGSvcGstCtrlSessionProcessAdd(PVPOXSERVICECTRLSESSION pSession, PVPOXSERVICECTRLPROCESS pProcess);
extern int                      VGSvcGstCtrlSessionProcessRemove(PVPOXSERVICECTRLSESSION pSession, PVPOXSERVICECTRLPROCESS pProcess);
extern int                      VGSvcGstCtrlSessionProcessStartAllowed(const PVPOXSERVICECTRLSESSION pSession, bool *pfAllowed);
extern int                      VGSvcGstCtrlSessionReapProcesses(PVPOXSERVICECTRLSESSION pSession);
/** @} */
/** @name Per-guest process functions.
 * @{ */
extern int                      VGSvcGstCtrlProcessFree(PVPOXSERVICECTRLPROCESS pProcess);
extern int                      VGSvcGstCtrlProcessHandleInput(PVPOXSERVICECTRLPROCESS pProcess, PVBGLR3GUESTCTRLCMDCTX pHostCtx, bool fPendingClose, void *pvBuf, uint32_t cbBuf);
extern int                      VGSvcGstCtrlProcessHandleOutput(PVPOXSERVICECTRLPROCESS pProcess, PVBGLR3GUESTCTRLCMDCTX pHostCtx, uint32_t uHandle, uint32_t cbToRead, uint32_t uFlags);
extern int                      VGSvcGstCtrlProcessHandleTerm(PVPOXSERVICECTRLPROCESS pProcess);
extern void                     VGSvcGstCtrlProcessRelease(PVPOXSERVICECTRLPROCESS pProcess);
extern int                      VGSvcGstCtrlProcessStart(const PVPOXSERVICECTRLSESSION pSession, const PVBGLR3GUESTCTRLPROCSTARTUPINFO pStartupInfo, uint32_t uContext);
extern int                      VGSvcGstCtrlProcessStop(PVPOXSERVICECTRLPROCESS pProcess);
extern int                      VGSvcGstCtrlProcessWait(const PVPOXSERVICECTRLPROCESS pProcess, RTMSINTERVAL msTimeout, int *pRc);
/** @} */

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_common_VPoxService_VPoxServiceControl_h */

