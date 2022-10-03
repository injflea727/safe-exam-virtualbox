/* $Id: VirtualPoxClientImpl.h $ */
/** @file
 * Header file for the VirtualPoxClient (IVirtualPoxClient) class, VPoxC.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_VirtualPoxClientImpl_h
#define MAIN_INCLUDED_VirtualPoxClientImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VirtualPoxClientWrap.h"
#include "EventImpl.h"

#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif

class ATL_NO_VTABLE VirtualPoxClient :
    public VirtualPoxClientWrap
#ifdef RT_OS_WINDOWS
    , public ATL::CComCoClass<VirtualPoxClient, &CLSID_VirtualPoxClient>
#endif
{
public:
    DECLARE_CLASSFACTORY_SINGLETON(VirtualPoxClient)

    // Do not use any ATL registry support.
    //DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALPOX)

    DECLARE_NOT_AGGREGATABLE(VirtualPoxClient)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init();
    void uninit();

#ifdef RT_OS_WINDOWS
    /* HACK ALERT! Implemented in dllmain.cpp. */
    ULONG InternalRelease();
#endif

private:
    // wrapped IVirtualPoxClient properties
    virtual HRESULT getVirtualPox(ComPtr<IVirtualPox> &aVirtualPox);
    virtual HRESULT getSession(ComPtr<ISession> &aSession);
    virtual HRESULT getEventSource(ComPtr<IEventSource> &aEventSource);

    // wrapped IVirtualPoxClient methods
    virtual HRESULT checkMachineError(const ComPtr<IMachine> &aMachine);

    /** Instance counter for simulating something similar to a singleton.
     * Only the first instance will be a usable object, all additional
     * instances will return a failure at creation time and will not work. */
    static uint32_t g_cInstances;

#ifdef RT_OS_WINDOWS
    virtual HRESULT i_investigateVirtualPoxObjectCreationFailure(HRESULT hrc);
#endif

#ifdef VPOX_WITH_SDS
    int     i_getServiceAccountAndStartType(const wchar_t *pwszServiceName,
                                            wchar_t *pwszAccountName, size_t cwcAccountName, uint32_t *puStartType);
#endif

    static DECLCALLBACK(int) SVCWatcherThread(RTTHREAD ThreadSelf, void *pvUser);

    struct Data
    {
        Data() : m_ThreadWatcher(NIL_RTTHREAD), m_SemEvWatcher(NIL_RTSEMEVENT)
        {}

        ~Data()
        {
            /* HACK ALERT! This is for DllCanUnloadNow(). */
            if (m_pEventSource.isNotNull())
            {
                s_cUnnecessaryAtlModuleLocks--;
                AssertMsg(s_cUnnecessaryAtlModuleLocks == 0, ("%d\n", s_cUnnecessaryAtlModuleLocks));
            }
        }

        ComPtr<IVirtualPox> m_pVirtualPox;
        ComPtr<IToken> m_pToken;
        const ComObjPtr<EventSource> m_pEventSource;

        RTTHREAD m_ThreadWatcher;
        RTSEMEVENT m_SemEvWatcher;
    };

    Data mData;

public:
    /** Hack for discounting the AtlModule lock held by Data::m_pEventSource during
     * DllCanUnloadNow().  This is incremented to 1 when init() initialized
     * m_pEventSource and is decremented by the Data destructor (above). */
    static LONG s_cUnnecessaryAtlModuleLocks;
};

#endif /* !MAIN_INCLUDED_VirtualPoxClientImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
