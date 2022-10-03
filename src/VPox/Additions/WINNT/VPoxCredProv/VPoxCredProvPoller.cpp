/* $Id: VPoxCredProvPoller.cpp $ */
/** @file
 * VPoxCredPoller - Thread for querying / retrieving user credentials.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/win/windows.h>

#include <VPox/VPoxGuestLib.h>
#include <iprt/string.h>

#include "VPoxCredProvProvider.h"

#include "VPoxCredProvCredential.h"
#include "VPoxCredProvPoller.h"
#include "VPoxCredProvUtils.h"


VPoxCredProvPoller::VPoxCredProvPoller(void) :
    m_hThreadPoller(NIL_RTTHREAD),
    m_pProv(NULL)
{
}


VPoxCredProvPoller::~VPoxCredProvPoller(void)
{
    VPoxCredProvVerbose(0, "VPoxCredProvPoller: Destroying ...\n");

    Shutdown();
}


int
VPoxCredProvPoller::Initialize(VPoxCredProvProvider *pProvider)
{
    AssertPtrReturn(pProvider, VERR_INVALID_POINTER);

    VPoxCredProvVerbose(0, "VPoxCredProvPoller: Initializing\n");

    /* Don't create more than one of them. */
    if (m_hThreadPoller != NIL_RTTHREAD)
    {
        VPoxCredProvVerbose(0, "VPoxCredProvPoller: Thread already running, returning\n");
        return VINF_SUCCESS;
    }

    if (m_pProv != NULL)
        m_pProv->Release();

    m_pProv = pProvider;
    /*
     * We must not add a reference via AddRef() here, otherwise
     * the credential provider does not get destructed properly.
     * In order to get this thread terminated normally the credential
     * provider has to call Shutdown().
     */

    /* Create the poller thread. */
    int rc = RTThreadCreate(&m_hThreadPoller, VPoxCredProvPoller::threadPoller, this, 0, RTTHREADTYPE_INFREQUENT_POLLER,
                            RTTHREADFLAGS_WAITABLE, "credpoll");
    if (RT_FAILURE(rc))
        VPoxCredProvVerbose(0, "VPoxCredProvPoller::Initialize: Failed to create thread, rc=%Rrc\n", rc);

    return rc;
}


int
VPoxCredProvPoller::Shutdown(void)
{
    VPoxCredProvVerbose(0, "VPoxCredProvPoller: Shutdown\n");

    if (m_hThreadPoller == NIL_RTTHREAD)
        return VINF_SUCCESS;

    /* Post termination event semaphore. */
    int rc = RTThreadUserSignal(m_hThreadPoller);
    if (RT_SUCCESS(rc))
    {
        VPoxCredProvVerbose(0, "VPoxCredProvPoller: Waiting for thread to terminate\n");
        /* Wait until the thread has terminated. */
        rc = RTThreadWait(m_hThreadPoller, RT_INDEFINITE_WAIT, NULL);
        if (RT_FAILURE(rc))
            VPoxCredProvVerbose(0, "VPoxCredProvPoller: Wait returned error rc=%Rrc\n", rc);
    }
    else
        VPoxCredProvVerbose(0, "VPoxCredProvPoller: Error waiting for thread shutdown, rc=%Rrc\n", rc);

    m_pProv = NULL;
    m_hThreadPoller = NIL_RTTHREAD;

    VPoxCredProvVerbose(0, "VPoxCredProvPoller: Shutdown returned with rc=%Rrc\n", rc);
    return rc;
}


/*static*/ DECLCALLBACK(int)
VPoxCredProvPoller::threadPoller(RTTHREAD hThreadSelf, void *pvUser)
{
    VPoxCredProvVerbose(0, "VPoxCredProvPoller: Starting, pvUser=0x%p\n", pvUser);

    VPoxCredProvPoller *pThis = (VPoxCredProvPoller*)pvUser;
    AssertPtr(pThis);

    for (;;)
    {
        int rc;
        rc = VbglR3CredentialsQueryAvailability();
        if (RT_FAILURE(rc))
        {
            if (rc != VERR_NOT_FOUND)
                VPoxCredProvVerbose(0, "VPoxCredProvPoller: Could not retrieve credentials! rc=%Rc\n", rc);
        }
        else
        {
            VPoxCredProvVerbose(0, "VPoxCredProvPoller: Credentials available, notifying provider\n");

            if (pThis->m_pProv)
                pThis->m_pProv->OnCredentialsProvided();
        }

        /* Wait a bit. */
        if (RTThreadUserWait(hThreadSelf, 500) == VINF_SUCCESS)
        {
            VPoxCredProvVerbose(0, "VPoxCredProvPoller: Terminating\n");
            break;
        }
    }

    return VINF_SUCCESS;
}

