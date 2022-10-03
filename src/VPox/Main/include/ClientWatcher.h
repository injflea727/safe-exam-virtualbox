/* $Id: ClientWatcher.h $ */
/** @file
 * VirtualPox API client session watcher
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

#ifndef MAIN_INCLUDED_ClientWatcher_h
#define MAIN_INCLUDED_ClientWatcher_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#include <list>
#include <VPox/com/ptr.h>
#include <VPox/com/AutoLock.h>

#include "VirtualPoxImpl.h"

#if defined(RT_OS_WINDOWS)
# define CWUPDATEREQARG NULL
# define CWUPDATEREQTYPE HANDLE
# define CW_MAX_CLIENTS  _16K            /**< Max number of clients we can watch (windows). */
# ifndef DEBUG /* The debug version triggers worker thread code much much earlier. */
#  define CW_MAX_CLIENTS_PER_THREAD 63   /**< Max clients per watcher thread (windows). */
# else
#  define CW_MAX_CLIENTS_PER_THREAD 3    /**< Max clients per watcher thread (windows). */
# endif
# define CW_MAX_HANDLES_PER_THREAD (CW_MAX_CLIENTS_PER_THREAD + 1) /**< Max handles per thread. */

#elif defined(RT_OS_OS2)
# define CWUPDATEREQARG NIL_RTSEMEVENT
# define CWUPDATEREQTYPE RTSEMEVENT

#elif defined(VPOX_WITH_SYS_V_IPC_SESSION_WATCHER) || defined(VPOX_WITH_GENERIC_SESSION_WATCHER)
# define CWUPDATEREQARG NIL_RTSEMEVENT
# define CWUPDATEREQTYPE RTSEMEVENT

#else
# error "Port me!"
#endif

/**
 * Class which checks for API clients which have crashed/exited, and takes
 * the necessary cleanup actions. Singleton.
 */
class VirtualPox::ClientWatcher
{
public:
    /**
     * Constructor which creates a usable instance
     *
     * @param pVirtualPox   Reference to VirtualPox object
     */
    ClientWatcher(const ComObjPtr<VirtualPox> &pVirtualPox);

    /**
     * Default destructor. Cleans everything up.
     */
    ~ClientWatcher();

    bool isReady();

    void update();
    void addProcess(RTPROCESS pid);

private:
    /**
     * Default constructor. Don't use, will not create a sensible instance.
     */
    ClientWatcher();

    static DECLCALLBACK(int) worker(RTTHREAD hThreadSelf, void *pvUser);
    uint32_t reapProcesses(void);

    VirtualPox *mVirtualPox;
    RTTHREAD mThread;
    CWUPDATEREQTYPE mUpdateReq;
    util::RWLockHandle mLock;

    typedef std::list<RTPROCESS> ProcessList;
    ProcessList mProcesses;

#if defined(VPOX_WITH_SYS_V_IPC_SESSION_WATCHER) || defined(VPOX_WITH_GENERIC_SESSION_WATCHER)
    uint8_t mUpdateAdaptCtr;
#endif
#ifdef RT_OS_WINDOWS
    /** Indicate a real update request is pending.
     * To avoid race conditions this must be set before mUpdateReq is signalled and
     * read after resetting mUpdateReq. */
    volatile bool mfUpdateReq;
    /** Set when the worker threads are supposed to shut down. */
    volatile bool mfTerminate;
    /** Number of active subworkers.
     * When decremented to 0, subworker zero is signalled. */
    uint32_t volatile mcActiveSubworkers;
    /** Number of valid handles in mahWaitHandles. */
    uint32_t    mcWaitHandles;
    /** The wait interval (usually INFINITE). */
    uint32_t    mcMsWait;
    /** Per subworker data. Subworker 0 is the main worker and does not have a
     *  pReq pointer since. */
    struct PerSubworker
    {
        /** The wait result. */
        DWORD                       dwWait;
        /** The subworker index. */
        uint32_t                    iSubworker;
        /** The subworker thread handle. */
        RTTHREAD                    hThread;
        /** Self pointer (for worker thread). */
        VirtualPox::ClientWatcher  *pSelf;
    } maSubworkers[(CW_MAX_CLIENTS + CW_MAX_CLIENTS_PER_THREAD - 1) / CW_MAX_CLIENTS_PER_THREAD];
    /** Wait handle array. The mUpdateReq manual reset event handle is inserted
     * every 64 entries, first entry being 0. */
    HANDLE      mahWaitHandles[CW_MAX_CLIENTS + (CW_MAX_CLIENTS + CW_MAX_CLIENTS_PER_THREAD - 1) / CW_MAX_CLIENTS_PER_THREAD];

    void subworkerWait(VirtualPox::ClientWatcher::PerSubworker *pSubworker, uint32_t cMsWait);
    static DECLCALLBACK(int) subworkerThread(RTTHREAD hThreadSelf, void *pvUser);
    void winResetHandleArray(uint32_t cProcHandles);
#endif
};

#endif /* !MAIN_INCLUDED_ClientWatcher_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
