/* $Id: VirtualPoxSDSImpl.h $ */
/** @file
 * VPox Global COM Class definition
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_VirtualPoxSDSImpl_h
#define MAIN_INCLUDED_VirtualPoxSDSImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VirtualPoxBase.h"

/* Enable the watcher code in debug builds. */
#ifdef DEBUG
# define WITH_WATCHER
#endif


class VPoxSDSPerUserData; /* See VirtualPoxSDSImpl.cpp. */
struct VPoxSDSWatcher;    /* See VirtualPoxSDSImpl.cpp. */

/**
 * The IVirtualPoxSDS implementation.
 *
 * This class helps different VPoxSVC processes make sure a user only have a
 * single VirtualPox instance.
 *
 * @note This is a simple internal class living in a privileged process.  So, we
 *       do not use the API wrappers as they add complexity.  In particular,
 *       they add the auto caller logic, which is an excellent tool to create
 *       unkillable processes.  If an API method during development or product
 *       for instance triggers an NT exception like STATUS_ACCESS_VIOLATION, the
 *       caller will be unwound without releasing the caller.  When uninit is
 *       called during COM shutdown/whatever, the thread gets stuck waiting for
 *       the long gone caller and cannot be killed (Windows 10, build 16299),
 *       requiring a reboot to continue.
 *
 * @todo Would be very nice to get rid of the ATL cruft too here.
 */
class VirtualPoxSDS
    : public IVirtualPoxSDS
    , public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>
    , public ATL::CComCoClass<VirtualPoxSDS, &CLSID_VirtualPoxSDS>
{
private:
    typedef std::map<com::Utf8Str, VPoxSDSPerUserData *> UserDataMap_T;
    /** Per user data map (key is SID string).
     * This is an insert-only map! */
    UserDataMap_T           m_UserDataMap;
    /** Number of registered+watched VPoxSVC processes. */
    uint32_t                m_cVPoxSvcProcesses;
#ifdef WITH_WATCHER
    /** Number of watcher threads.   */
    uint32_t                m_cWatchers;
    /** Pointer to an array of watcher pointers. */
    VPoxSDSWatcher        **m_papWatchers;
    /** Lock protecting m_papWatchers and associated structures. */
    RTCRITSECT              m_WatcherCritSect;
#endif
    /** Lock protecting m_UserDataMap . */
    RTCRITSECTRW            m_MapCritSect;

public:
    DECLARE_CLASSFACTORY_SINGLETON(VirtualPoxSDS)
    DECLARE_NOT_AGGREGATABLE(VirtualPoxSDS)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualPoxSDS)
        COM_INTERFACE_ENTRY(IVirtualPoxSDS)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(VirtualPoxSDS)

    HRESULT FinalConstruct();
    void    FinalRelease();

private:

    /** @name IVirtualPoxSDS methods
     * @{ */
    STDMETHOD(RegisterVPoxSVC)(IVPoxSVCRegistration *aVPoxSVC, LONG aPid, IUnknown **aExistingVirtualPox);
    STDMETHOD(DeregisterVPoxSVC)(IVPoxSVCRegistration *aVPoxSVC, LONG aPid);
    STDMETHOD(LaunchVMProcess)(IN_BSTR aMachine, IN_BSTR aComment, IN_BSTR aFrontend,
                               ComSafeArrayIn(IN_BSTR, aEnvironmentChanges), IN_BSTR aCmdOptions,
                               ULONG aSessionId, ULONG *aPid);
    /** @} */


    /** @name Private methods
     * @{ */
    /**
     * Gets the client user SID of the
     */
    static bool i_getClientUserSid(com::Utf8Str *a_pStrSid, com::Utf8Str *a_pStrUsername);

    /**
     * Looks up the given user.
     *
     * @returns Pointer to the LOCKED per user data.  NULL if not found.
     * @param   a_rStrUserSid   The user SID.
     */
    VPoxSDSPerUserData *i_lookupPerUserData(com::Utf8Str const &a_rStrUserSid);

    /**
     * Looks up the given user, creating it if not found
     *
     * @returns Pointer to the LOCKED per user data.  NULL on allocation error.
     * @param   a_rStrUserSid   The user SID.
     * @param   a_rStrUsername  The user name if available.
     */
    VPoxSDSPerUserData *i_lookupOrCreatePerUserData(com::Utf8Str const &a_rStrUserSid, com::Utf8Str const &a_rStrUsername);

#ifdef WITH_WATCHER
    static DECLCALLBACK(int) i_watcherThreadProc(RTTHREAD hSelf, void *pvUser);
    bool i_watchIt(VPoxSDSPerUserData *pProcess, HANDLE hProcess, RTPROCESS pid);
    void i_stopWatching(VPoxSDSPerUserData *pProcess, RTPROCESS pid);
    void i_shutdownAllWatchers(void);

    void i_decrementClientCount();
    void i_incrementClientCount();
#endif
    /** @} */
};

#ifdef WITH_WATCHER
void VPoxSDSNotifyClientCount(uint32_t cClients);
#endif

#endif /* !MAIN_INCLUDED_VirtualPoxSDSImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
