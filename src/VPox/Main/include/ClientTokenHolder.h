/* $Id: ClientTokenHolder.h $ */

/** @file
 *
 * VirtualPox API client session token holder (in the client process)
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

#ifndef MAIN_INCLUDED_ClientTokenHolder_h
#define MAIN_INCLUDED_ClientTokenHolder_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "SessionImpl.h"

#if defined(RT_OS_WINDOWS)
# define CTHSEMARG NULL
# define CTHSEMTYPE HANDLE
/* this second semaphore is only used on Windows */
# define CTHTHREADSEMARG NULL
# define CTHTHREADSEMTYPE HANDLE
#elif defined(RT_OS_OS2)
# define CTHSEMARG NIL_RTSEMEVENT
# define CTHSEMTYPE RTSEMEVENT
#elif defined(VPOX_WITH_SYS_V_IPC_SESSION_WATCHER)
# define CTHSEMARG -1
# define CTHSEMTYPE int
#elif defined(VPOX_WITH_GENERIC_SESSION_WATCHER)
/* the token object based implementation needs no semaphores */
#else
# error "Port me!"
#endif


/**
 * Class which holds a client token.
 */
class Session::ClientTokenHolder
{
public:
#ifndef VPOX_WITH_GENERIC_SESSION_WATCHER
    /**
     * Constructor which creates a usable instance
     *
     * @param strTokenId    String with identifier of the token
     */
    ClientTokenHolder(const Utf8Str &strTokenId);
#else /* VPOX_WITH_GENERIC_SESSION_WATCHER */
    /**
     * Constructor which creates a usable instance
     *
     * @param aToken        Reference to token object
     */
    ClientTokenHolder(IToken *aToken);
#endif /* VPOX_WITH_GENERIC_SESSION_WATCHER */

    /**
     * Default destructor. Cleans everything up.
     */
    ~ClientTokenHolder();

    /**
     * Check if object contains a usable token.
     */
    bool isReady();

private:
    /**
     * Default constructor. Don't use, will not create a sensible instance.
     */
    ClientTokenHolder();

#ifndef VPOX_WITH_GENERIC_SESSION_WATCHER
    Utf8Str mClientTokenId;
#else /* VPOX_WITH_GENERIC_SESSION_WATCHER */
    ComPtr<IToken> mToken;
#endif /* VPOX_WITH_GENERIC_SESSION_WATCHER */
#ifdef CTHSEMTYPE
    CTHSEMTYPE mSem;
#endif
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    RTTHREAD mThread;
#endif
#ifdef RT_OS_WINDOWS
    CTHTHREADSEMTYPE mThreadSem;
#endif
};

#endif /* !MAIN_INCLUDED_ClientTokenHolder_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
