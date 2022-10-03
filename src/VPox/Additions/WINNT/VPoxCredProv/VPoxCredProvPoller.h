/* $Id: VPoxCredProvPoller.h $ */
/** @file
 * VPoxCredPoller - Thread for retrieving user credentials.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VPoxCredProv_VPoxCredProvPoller_h
#define GA_INCLUDED_SRC_WINNT_VPoxCredProv_VPoxCredProvPoller_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>

class VPoxCredProvProvider;

class VPoxCredProvPoller
{
public:
    VPoxCredProvPoller(void);
    virtual ~VPoxCredProvPoller(void);

    int Initialize(VPoxCredProvProvider *pProvider);
    int Shutdown(void);

protected:
    /** Static function for our poller routine, used in an own thread to
     * check for credentials on the host. */
    static DECLCALLBACK(int) threadPoller(RTTHREAD ThreadSelf, void *pvUser);

    /** Thread handle. */
    RTTHREAD              m_hThreadPoller;
    /** Pointer to parent. Needed for notifying if credentials
     *  are available. */
    VPoxCredProvProvider *m_pProv;
};

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxCredProv_VPoxCredProvPoller_h */

