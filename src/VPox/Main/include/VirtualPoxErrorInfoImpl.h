/* $Id: VirtualPoxErrorInfoImpl.h $ */
/** @file
 * VirtualPoxErrorInfo COM class definition.
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

#ifndef MAIN_INCLUDED_VirtualPoxErrorInfoImpl_h
#define MAIN_INCLUDED_VirtualPoxErrorInfoImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VirtualPoxBase.h"

using namespace com;

class ATL_NO_VTABLE VirtualPoxErrorInfo
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>
    , VPOX_SCRIPTABLE_IMPL(IVirtualPoxErrorInfo)
#ifndef VPOX_WITH_XPCOM /* IErrorInfo doesn't inherit from IDispatch, ugly 3am hack: */
    , public IDispatch
#endif
{
public:

    DECLARE_NOT_AGGREGATABLE(VirtualPoxErrorInfo)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualPoxErrorInfo)
        COM_INTERFACE_ENTRY(IErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualPoxErrorInfo)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, m_pUnkMarshaler)
    END_COM_MAP()

    HRESULT FinalConstruct()
    {
#ifndef VPOX_WITH_XPCOM
        return CoCreateFreeThreadedMarshaler((IUnknown *)(void *)this, &m_pUnkMarshaler);
#else
        return S_OK;
#endif
    }

    void FinalRelease()
    {
#ifndef VPOX_WITH_XPCOM
        if (m_pUnkMarshaler)
        {
            m_pUnkMarshaler->Release();
            m_pUnkMarshaler = NULL;
        }
#endif
    }

#ifndef VPOX_WITH_XPCOM

    HRESULT init(IErrorInfo *aInfo);

    STDMETHOD(GetGUID)(GUID *guid);
    STDMETHOD(GetSource)(BSTR *pBstrSource);
    STDMETHOD(GetDescription)(BSTR *description);
    STDMETHOD(GetHelpFile)(BSTR *pBstrHelpFile);
    STDMETHOD(GetHelpContext)(DWORD *pdwHelpContext);

    // IDispatch forwarding - 3am hack.
    typedef IDispatchImpl<IVirtualPoxErrorInfo, &IID_IVirtualPoxErrorInfo, &LIBID_VirtualPox, kTypeLibraryMajorVersion, kTypeLibraryMinorVersion> idi;

    STDMETHOD(GetTypeInfoCount)(UINT *pcInfo)
    {
        return idi::GetTypeInfoCount(pcInfo);
    }

    STDMETHOD(GetTypeInfo)(UINT iInfo, LCID Lcid, ITypeInfo **ppTypeInfo)
    {
        return idi::GetTypeInfo(iInfo, Lcid, ppTypeInfo);
    }

    STDMETHOD(GetIDsOfNames)(REFIID rIID, LPOLESTR *papwszNames, UINT cNames, LCID Lcid, DISPID *paDispIDs)
    {
        return idi::GetIDsOfNames(rIID, papwszNames, cNames, Lcid, paDispIDs);
    }

    STDMETHOD(Invoke)(DISPID idDispMember, REFIID rIID, LCID Lcid, WORD fw, DISPPARAMS *pDispParams,
                      VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *piErrArg)
    {
        return idi::Invoke(idDispMember, rIID, Lcid, fw, pDispParams, pVarResult, pExcepInfo, piErrArg);
    }

#else // defined(VPOX_WITH_XPCOM)

    HRESULT init(nsIException *aInfo);

    NS_DECL_NSIEXCEPTION

#endif

    VirtualPoxErrorInfo()
        : m_resultCode(S_OK),
          m_resultDetail(0)
    {}
    virtual ~VirtualPoxErrorInfo() {}

    // public initializer/uninitializer for internal purposes only
    HRESULT init(HRESULT aResultCode,
                 const GUID &aIID,
                 const char *pcszComponent,
                 const Utf8Str &strText,
                 IVirtualPoxErrorInfo *aNext = NULL);

    HRESULT initEx(HRESULT aResultCode,
                   LONG aResultDetail,
                   const GUID &aIID,
                   const char *pcszComponent,
                   const Utf8Str &strText,
                   IVirtualPoxErrorInfo *aNext = NULL);

    HRESULT init(const com::ErrorInfo &ei,
                 IVirtualPoxErrorInfo *aNext = NULL);

    // IVirtualPoxErrorInfo properties
    STDMETHOD(COMGETTER(ResultCode))(LONG *aResultCode);
    STDMETHOD(COMGETTER(ResultDetail))(LONG *aResultDetail);
    STDMETHOD(COMGETTER(InterfaceID))(BSTR *aIID);
    STDMETHOD(COMGETTER(Component))(BSTR *aComponent);
    STDMETHOD(COMGETTER(Text))(BSTR *aText);
    STDMETHOD(COMGETTER(Next))(IVirtualPoxErrorInfo **aNext);

private:
    // FIXME: declare these here until VPoxSupportsTranslation base
    //        is available in this class.
    static const char *tr(const char *a) { return a; }
    static HRESULT setError(HRESULT rc,
                            const char * /* a */,
                            const char * /* b */,
                            void *       /* c */) { return rc; }

    HRESULT m_resultCode;
    LONG    m_resultDetail;
    Utf8Str m_strText;
    Guid    m_IID;
    Utf8Str m_strComponent;
    ComPtr<IVirtualPoxErrorInfo> mNext;

#ifndef VPOX_WITH_XPCOM
    IUnknown *m_pUnkMarshaler;
#endif
};

#endif /* !MAIN_INCLUDED_VirtualPoxErrorInfoImpl_h */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
