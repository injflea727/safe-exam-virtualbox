/* $Id: VPoxCredProvFactory.cpp $ */
/** @file
 * VPoxCredentialProvFactory - The VirtualPox Credential Provider Factory.
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
#include "VPoxCredentialProvider.h"
#include "VPoxCredProvFactory.h"
#include "VPoxCredProvProvider.h"


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
extern HRESULT VPoxCredProvProviderCreate(REFIID interfaceID, void **ppvInterface);


VPoxCredProvFactory::VPoxCredProvFactory(void) :
    m_cRefs(1) /* Start with one instance. */
{
}

VPoxCredProvFactory::~VPoxCredProvFactory(void)
{
}

ULONG
VPoxCredProvFactory::AddRef(void)
{
    LONG cRefs = InterlockedIncrement(&m_cRefs);
    VPoxCredProvVerbose(0, "VPoxCredProvFactory: AddRef: Returning refcount=%ld\n",
                        cRefs);
    return cRefs;
}

ULONG
VPoxCredProvFactory::Release(void)
{
    LONG cRefs = InterlockedDecrement(&m_cRefs);
    VPoxCredProvVerbose(0, "VPoxCredProvFactory: Release: Returning refcount=%ld\n",
                        cRefs);
    if (!cRefs)
    {
        VPoxCredProvVerbose(0, "VPoxCredProvFactory: Calling destructor\n");
        delete this;
    }
    return cRefs;
}

HRESULT
VPoxCredProvFactory::QueryInterface(REFIID interfaceID, void **ppvInterface)
{
    VPoxCredProvVerbose(0, "VPoxCredProvFactory: QueryInterface\n");

    HRESULT hr = S_OK;
    if (ppvInterface)
    {
        if (   IID_IClassFactory == interfaceID
            || IID_IUnknown      == interfaceID)
        {
            *ppvInterface = static_cast<IUnknown*>(this);
            reinterpret_cast<IUnknown*>(*ppvInterface)->AddRef();
        }
        else
        {
            *ppvInterface = NULL;
            hr = E_NOINTERFACE;
        }
    }
    else
        hr = E_INVALIDARG;
    return hr;
}

HRESULT
VPoxCredProvFactory::CreateInstance(IUnknown *pUnkOuter, REFIID interfaceID, void **ppvInterface)
{
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;
    return VPoxCredProvProviderCreate(interfaceID, ppvInterface);
}

HRESULT
VPoxCredProvFactory::LockServer(BOOL fLock)
{
    if (fLock)
        VPoxCredentialProviderAcquire();
    else
        VPoxCredentialProviderRelease();
    return S_OK;
}

