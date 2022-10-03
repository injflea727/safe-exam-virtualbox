/* $Id: VirtualPoxErrorInfoImpl.cpp $ */
/** @file
 * VirtualPoxErrorInfo COM class implementation
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

#define LOG_GROUP LOG_GROUP_MAIN
#include "VirtualPoxErrorInfoImpl.h"

#include <VPox/com/ErrorInfo.h>

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

HRESULT VirtualPoxErrorInfo::init(HRESULT aResultCode,
                                  const GUID &aIID,
                                  const char *pcszComponent,
                                  const Utf8Str &strText,
                                  IVirtualPoxErrorInfo *aNext)
{
    m_resultCode = aResultCode;
    m_resultDetail = 0; /* Not being used. */
    m_IID = aIID;
    m_strComponent = pcszComponent;
    m_strText = strText;
    mNext = aNext;

    return S_OK;
}

HRESULT VirtualPoxErrorInfo::initEx(HRESULT aResultCode,
                                    LONG aResultDetail,
                                    const GUID &aIID,
                                    const char *pcszComponent,
                                    const Utf8Str &strText,
                                    IVirtualPoxErrorInfo *aNext)
{
    HRESULT hr = init(aResultCode, aIID, pcszComponent, strText, aNext);
    m_resultDetail = aResultDetail;

    return hr;
}

HRESULT VirtualPoxErrorInfo::init(const com::ErrorInfo &info,
                                  IVirtualPoxErrorInfo *aNext)
{
    m_resultCode = info.getResultCode();
    m_resultDetail = info.getResultDetail();
    m_IID = info.getInterfaceID();
    m_strComponent = info.getComponent();
    m_strText = info.getText();

    /* Recursively create VirtualPoxErrorInfo instances for the next objects. */
    const com::ErrorInfo *pInfo = info.getNext();
    if (pInfo)
    {
        ComObjPtr<VirtualPoxErrorInfo> nextEI;
        HRESULT rc = nextEI.createObject();
        if (FAILED(rc)) return rc;
        rc = nextEI->init(*pInfo, aNext);
        if (FAILED(rc)) return rc;
        mNext = nextEI;
    }
    else
        mNext = aNext;

    return S_OK;
}

// IVirtualPoxErrorInfo properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualPoxErrorInfo::COMGETTER(ResultCode)(LONG *aResultCode)
{
    CheckComArgOutPointerValid(aResultCode);

    *aResultCode = m_resultCode;
    return S_OK;
}

STDMETHODIMP VirtualPoxErrorInfo::COMGETTER(ResultDetail)(LONG *aResultDetail)
{
    CheckComArgOutPointerValid(aResultDetail);

    *aResultDetail = m_resultDetail;
    return S_OK;
}

STDMETHODIMP VirtualPoxErrorInfo::COMGETTER(InterfaceID)(BSTR *aIID)
{
    CheckComArgOutPointerValid(aIID);

    m_IID.toUtf16().cloneTo(aIID);
    return S_OK;
}

STDMETHODIMP VirtualPoxErrorInfo::COMGETTER(Component)(BSTR *aComponent)
{
    CheckComArgOutPointerValid(aComponent);

    m_strComponent.cloneTo(aComponent);
    return S_OK;
}

STDMETHODIMP VirtualPoxErrorInfo::COMGETTER(Text)(BSTR *aText)
{
    CheckComArgOutPointerValid(aText);

    m_strText.cloneTo(aText);
    return S_OK;
}

STDMETHODIMP VirtualPoxErrorInfo::COMGETTER(Next)(IVirtualPoxErrorInfo **aNext)
{
    CheckComArgOutPointerValid(aNext);

    /* this will set aNext to NULL if mNext is null */
    return mNext.queryInterfaceTo(aNext);
}

#if !defined(VPOX_WITH_XPCOM)

/**
 *  Initializes itself by fetching error information from the given error info
 *  object.
 */
HRESULT VirtualPoxErrorInfo::init(IErrorInfo *aInfo)
{
    AssertReturn(aInfo, E_FAIL);

    HRESULT rc = S_OK;

    /* We don't return a failure if talking to IErrorInfo fails below to
     * protect ourselves from bad IErrorInfo implementations (the
     * corresponding fields will simply remain null in this case). */

    m_resultCode = S_OK;
    m_resultDetail = 0;
    rc = aInfo->GetGUID(m_IID.asOutParam());
    AssertComRC(rc);
    Bstr bstrComponent;
    rc = aInfo->GetSource(bstrComponent.asOutParam());
    AssertComRC(rc);
    m_strComponent = bstrComponent;
    Bstr bstrText;
    rc = aInfo->GetDescription(bstrText.asOutParam());
    AssertComRC(rc);
    m_strText = bstrText;

    return S_OK;
}

// IErrorInfo methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualPoxErrorInfo::GetDescription(BSTR *description)
{
    return COMGETTER(Text)(description);
}

STDMETHODIMP VirtualPoxErrorInfo::GetGUID(GUID *guid)
{
    Bstr iid;
    HRESULT rc = COMGETTER(InterfaceID)(iid.asOutParam());
    if (SUCCEEDED(rc))
        *guid = Guid(iid).ref();
    return rc;
}

STDMETHODIMP VirtualPoxErrorInfo::GetHelpContext(DWORD *pdwHelpContext)
{
    RT_NOREF(pdwHelpContext);
    return E_NOTIMPL;
}

STDMETHODIMP VirtualPoxErrorInfo::GetHelpFile(BSTR *pBstrHelpFile)
{
    RT_NOREF(pBstrHelpFile);
    return E_NOTIMPL;
}

STDMETHODIMP VirtualPoxErrorInfo::GetSource(BSTR *pBstrSource)
{
    return COMGETTER(Component)(pBstrSource);
}

#else // defined(VPOX_WITH_XPCOM)

/**
 *  Initializes itself by fetching error information from the given error info
 *  object.
 */
HRESULT VirtualPoxErrorInfo::init(nsIException *aInfo)
{
    AssertReturn(aInfo, E_FAIL);

    HRESULT rc = S_OK;

    /* We don't return a failure if talking to nsIException fails below to
     * protect ourselves from bad nsIException implementations (the
     * corresponding fields will simply remain null in this case). */

    rc = aInfo->GetResult(&m_resultCode);
    AssertComRC(rc);
    m_resultDetail = 0; /* Not being used. */

    char *pszMsg;             /* No Utf8Str.asOutParam, different allocator! */
    rc = aInfo->GetMessage(&pszMsg);
    AssertComRC(rc);
    if (NS_SUCCEEDED(rc))
    {
        m_strText = pszMsg;
        nsMemory::Free(pszMsg);
    }
    else
        m_strText.setNull();

    return S_OK;
}

// nsIException methods
////////////////////////////////////////////////////////////////////////////////

/* readonly attribute string message; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetMessage(char **aMessage)
{
    CheckComArgOutPointerValid(aMessage);

    m_strText.cloneTo(aMessage);
    return S_OK;
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetResult(nsresult *aResult)
{
    if (!aResult)
      return NS_ERROR_INVALID_POINTER;

    PRInt32 lrc;
    nsresult rc = COMGETTER(ResultCode)(&lrc);
    if (SUCCEEDED(rc))
      *aResult = lrc;
    return rc;
}

/* readonly attribute string name; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetName(char ** /* aName */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string filename; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetFilename(char ** /* aFilename */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint32 lineNumber; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetLineNumber(PRUint32 * /* aLineNumber */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute PRUint32 columnNumber; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetColumnNumber(PRUint32 * /*aColumnNumber */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetLocation(nsIStackFrame ** /* aLocation */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetInner(nsIException **aInner)
{
    ComPtr<IVirtualPoxErrorInfo> info;
    nsresult rv = COMGETTER(Next)(info.asOutParam());
    if (FAILED(rv)) return rv;
    return info.queryInterfaceTo(aInner);
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP VirtualPoxErrorInfo::GetData(nsISupports ** /* aData */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string toString(); */
NS_IMETHODIMP VirtualPoxErrorInfo::ToString(char ** /* retval */)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(VirtualPoxErrorInfo,
                              nsIException, IVirtualPoxErrorInfo)

#endif // defined(VPOX_WITH_XPCOM)
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
