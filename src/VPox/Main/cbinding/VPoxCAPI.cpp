/* $Id: VPoxCAPI.cpp $ */
/** @file VPoxCAPI.cpp
 * Utility functions to use with the C API binding.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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

#include "VPoxCAPI.h"

#ifdef VPOX_WITH_XPCOM
# include <nsMemory.h>
# include <nsIServiceManager.h>
# include <nsEventQueueUtils.h>
# include <nsIExceptionService.h>
# include <stdlib.h>
#endif /* VPOX_WITH_XPCOM */

#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/utf16.h>
#include <iprt/uuid.h>
#include <VPox/log.h>
#include <VPox/version.h>

#include "VPox/com/com.h"
#include "VPox/com/NativeEventQueue.h"


#ifndef RT_OS_DARWIN /* Probably not used for xpcom, so clang gets upset: error: using directive refers to implicitly-defined namespace 'std' [-Werror]*/
using namespace std;
#endif

/* The following 2 object references should be eliminated once the legacy
 * way to initialize the COM/XPCOM C bindings is removed. */
static ISession            *g_Session           = NULL;
static IVirtualPox         *g_VirtualPox        = NULL;

#ifdef VPOX_WITH_XPCOM
/* This object reference should be eliminated once the legacy way of handling
 * the event queue (XPCOM specific) is removed. */
static nsIEventQueue       *g_EventQueue        = NULL;
#endif /* VPOX_WITH_XPCOM */

static void VPoxComUninitialize(void);
static void VPoxClientUninitialize(void);

static int
VPoxUtf16ToUtf8(CBSTR pwszString, char **ppszString)
{
    if (!pwszString)
    {
        *ppszString = NULL;
        return VINF_SUCCESS;
    }
    return RTUtf16ToUtf8(pwszString, ppszString);
}

static int
VPoxUtf8ToUtf16(const char *pszString, BSTR *ppwszString)
{
    *ppwszString = NULL;
    if (!pszString)
        return VINF_SUCCESS;
#ifdef VPOX_WITH_XPCOM
    return RTStrToUtf16(pszString, ppwszString);
#else /* !VPOX_WITH_XPCOM */
    PRTUTF16 pwsz;
    int vrc = RTStrToUtf16(pszString, &pwsz);
    if (RT_SUCCESS(vrc))
    {
        *ppwszString = ::SysAllocString(pwsz);
        if (!*ppwszString)
            vrc = VERR_NO_STR_MEMORY;
        RTUtf16Free(pwsz);
    }
    return vrc;
#endif /* !VPOX_WITH_XPCOM */
}

static void
VPoxUtf8Clear(char *pszString)
{
    RT_BZERO(pszString, strlen(pszString));
}

static void
VPoxUtf16Clear(BSTR pwszString)
{
    RT_BZERO(pwszString, RTUtf16Len(pwszString) * sizeof(RTUTF16));
}

static void
VPoxUtf16Free(BSTR pwszString)
{
#ifdef VPOX_WITH_XPCOM
    RTUtf16Free(pwszString);
#else
    ::SysFreeString(pwszString);
#endif
}

static void
VPoxUtf8Free(char *pszString)
{
    RTStrFree(pszString);
}

static void
VPoxComUnallocString(BSTR pwsz)
{
    if (pwsz)
    {
#ifdef VPOX_WITH_XPCOM
        nsMemory::Free(pwsz);
#else
        ::SysFreeString(pwsz);
#endif
    }
}

static void
VPoxComUnallocMem(void *pv)
{
    VPoxComUnallocString((BSTR)pv);
}

static ULONG
VPoxVTElemSize(VARTYPE vt)
{
    switch (vt)
    {
        case VT_BOOL:
        case VT_I1:
        case VT_UI1:
            return 1;
        case VT_I2:
        case VT_UI2:
            return 2;
        case VT_I4:
        case VT_UI4:
        case VT_HRESULT:
            return 4;
        case VT_I8:
        case VT_UI8:
            return 8;
        case VT_BSTR:
        case VT_DISPATCH:
        case VT_UNKNOWN:
            return sizeof(void *);
        default:
            return 0;
    }
}

static SAFEARRAY *
VPoxSafeArrayCreateVector(VARTYPE vt, LONG lLbound, ULONG cElements)
{
#ifdef VPOX_WITH_XPCOM
    NOREF(lLbound);
    ULONG cbElement = VPoxVTElemSize(vt);
    if (!cbElement)
        return NULL;
    SAFEARRAY *psa = (SAFEARRAY *)RTMemAllocZ(sizeof(SAFEARRAY));
    if (!psa)
        return psa;
    if (cElements)
    {
        void *pv = nsMemory::Alloc(cElements * cbElement);
        if (!pv)
        {
            RTMemFree(psa);
            return NULL;
        }
        psa->pv = pv;
        psa->c = cElements;
    }
    return psa;
#else /* !VPOX_WITH_XPCOM */
    return SafeArrayCreateVector(vt, lLbound, cElements);
#endif /* !VPOX_WITH_XPCOM */
}

static SAFEARRAY *
VPoxSafeArrayOutParamAlloc(void)
{
#ifdef VPOX_WITH_XPCOM
    return (SAFEARRAY *)RTMemAllocZ(sizeof(SAFEARRAY));
#else /* !VPOX_WITH_XPCOM */
    return NULL;
#endif /* !VPOX_WITH_XPCOM */
}

static HRESULT
VPoxSafeArrayDestroy(SAFEARRAY *psa)
{
#ifdef VPOX_WITH_XPCOM
    if (psa)
    {
        if (psa->pv)
            nsMemory::Free(psa->pv);
        RTMemFree(psa);
    }
    return S_OK;
#else /* !VPOX_WITH_XPCOM */
    VARTYPE vt = VT_UNKNOWN;
    HRESULT rc = SafeArrayGetVartype(psa, &vt);
    if (FAILED(rc))
        return rc;
    if (vt == VT_BSTR)
    {
        /* Special treatment: strings are to be freed explicitly, see sample
         * C binding code, so zap it here. No way to reach compatible code
         * behavior between COM and XPCOM without this kind of trickery. */
        void *pData;
        rc = SafeArrayAccessData(psa, &pData);
        if (FAILED(rc))
            return rc;
        ULONG cbElement = VPoxVTElemSize(vt);
        if (!cbElement)
            return E_INVALIDARG;
        Assert(cbElement = psa->cbElements);
        ULONG cElements = psa->rgsabound[0].cElements;
        memset(pData, '\0', cbElement * cElements);
        SafeArrayUnaccessData(psa);
    }
    return SafeArrayDestroy(psa);
#endif /* !VPOX_WITH_XPCOM */
}

static HRESULT
VPoxSafeArrayCopyInParamHelper(SAFEARRAY *psa, const void *pv, ULONG cb)
{
    if (!pv || !psa)
        return E_POINTER;
    if (!cb)
        return S_OK;

    void *pData;
#ifdef VPOX_WITH_XPCOM
    pData = psa->pv;
#else /* !VPOX_WITH_XPCOM */
    HRESULT rc = SafeArrayAccessData(psa, &pData);
    if (FAILED(rc))
        return rc;
#endif /* !VPOX_WITH_XPCOM */
    memcpy(pData, pv, cb);
#ifndef VPOX_WITH_XPCOM
    SafeArrayUnaccessData(psa);
#endif
    return S_OK;
}

static HRESULT
VPoxSafeArrayCopyOutParamHelper(void **ppv, ULONG *pcb, VARTYPE vt, SAFEARRAY *psa)
{
    if (!ppv)
        return E_POINTER;
    ULONG cbElement = VPoxVTElemSize(vt);
    if (!cbElement)
    {
        *ppv = NULL;
        if (pcb)
            *pcb = 0;
        return E_INVALIDARG;
    }
#ifndef VPOX_WITH_XPCOM
    if (psa->cDims != 1)
    {
        *ppv = NULL;
        if (pcb)
            *pcb = 0;
        return E_INVALIDARG;
    }
    Assert(cbElement = psa->cbElements);
#endif /* !VPOX_WITH_XPCOM */
    void *pData;
    ULONG cElements;
#ifdef VPOX_WITH_XPCOM
    pData = psa->pv;
    cElements = psa->c;
#else /* !VPOX_WITH_XPCOM */
    HRESULT rc = SafeArrayAccessData(psa, &pData);
    if (FAILED(rc))
    {
        *ppv = NULL;
        if (pcb)
            *pcb = 0;
        return rc;
    }
    cElements = psa->rgsabound[0].cElements;
#endif /* !VPOX_WITH_XPCOM */
    size_t cbTotal = cbElement * cElements;
    void *pv = NULL;
    if (cbTotal)
    {
        pv = malloc(cbTotal);
        if (!pv)
        {
            *ppv = NULL;
            if (pcb)
                *pcb = 0;
            return E_OUTOFMEMORY;
        }
        else
            memcpy(pv, pData, cbTotal);
    }
    *ppv = pv;
    if (pcb)
        *pcb = (ULONG)cbTotal;
#ifndef VPOX_WITH_XPCOM
    SafeArrayUnaccessData(psa);
#endif
    return S_OK;
}

static HRESULT
VPoxSafeArrayCopyOutIfaceParamHelper(IUnknown ***ppaObj, ULONG *pcObj, SAFEARRAY *psa)
{
    ULONG mypcb;
    HRESULT rc = VPoxSafeArrayCopyOutParamHelper((void **)ppaObj, &mypcb, VT_UNKNOWN, psa);
    if (FAILED(rc))
    {
        if (pcObj)
            *pcObj = 0;
        return rc;
    }
    ULONG cElements = mypcb / sizeof(void *);
    if (pcObj)
        *pcObj = cElements;
#ifndef VPOX_WITH_XPCOM
    /* Do this only for COM, as there the SAFEARRAY destruction will release
     * the contained references automatically. XPCOM doesn't do that, which
     * means that copying implicitly transfers ownership. */
    IUnknown **paObj = *ppaObj;
    for (ULONG i = 0; i < cElements; i++)
    {
        IUnknown *pObj = paObj[i];
        if (pObj)
            pObj->AddRef();
    }
#endif /* VPOX_WITH_XPCOM */
    return S_OK;
}

static HRESULT
VPoxArrayOutFree(void *pv)
{
    free(pv);
    return S_OK;
}

static void
VPoxComInitialize(const char *pszVirtualPoxIID, IVirtualPox **ppVirtualPox,
                  const char *pszSessionIID, ISession **ppSession)
{
    int vrc;
    IID virtualPoxIID;
    IID sessionIID;

    *ppSession    = NULL;
    *ppVirtualPox = NULL;

    /* convert the string representation of the UUIDs (if provided) to IID */
    if (pszVirtualPoxIID && *pszVirtualPoxIID)
    {
        vrc = ::RTUuidFromStr((RTUUID *)&virtualPoxIID, pszVirtualPoxIID);
        if (RT_FAILURE(vrc))
            return;
    }
    else
        virtualPoxIID = IID_IVirtualPox;
    if (pszSessionIID && *pszSessionIID)
    {
        vrc = ::RTUuidFromStr((RTUUID *)&sessionIID, pszSessionIID);
        if (RT_FAILURE(vrc))
            return;
    }
    else
        sessionIID = IID_ISession;

    HRESULT rc = com::Initialize(VPOX_COM_INIT_F_DEFAULT | VPOX_COM_INIT_F_NO_COM_PATCHING);
    if (FAILED(rc))
    {
        Log(("Cbinding: COM/XPCOM could not be initialized! rc=%Rhrc\n", rc));
        VPoxComUninitialize();
        return;
    }

#ifdef VPOX_WITH_XPCOM
    rc = NS_GetMainEventQ(&g_EventQueue);
    if (FAILED(rc))
    {
        Log(("Cbinding: Could not get XPCOM event queue! rc=%Rhrc\n", rc));
        VPoxComUninitialize();
        return;
    }
#endif /* VPOX_WITH_XPCOM */

#ifdef VPOX_WITH_XPCOM
    nsIComponentManager *pManager;
    rc = NS_GetComponentManager(&pManager);
    if (FAILED(rc))
    {
        Log(("Cbinding: Could not get component manager! rc=%Rhrc\n", rc));
        VPoxComUninitialize();
        return;
    }

    rc = pManager->CreateInstanceByContractID(NS_VIRTUALPOX_CONTRACTID,
                                              nsnull,
                                              virtualPoxIID,
                                              (void **)&g_VirtualPox);
#else /* !VPOX_WITH_XPCOM */
    IVirtualPoxClient *pVirtualPoxClient;
    rc = CoCreateInstance(CLSID_VirtualPoxClient, NULL, CLSCTX_INPROC_SERVER, IID_IVirtualPoxClient, (void **)&pVirtualPoxClient);
    if (SUCCEEDED(rc))
    {
        IVirtualPox *pVirtualPox;
        rc = pVirtualPoxClient->get_VirtualPox(&pVirtualPox);
        if (SUCCEEDED(rc))
        {
            rc = pVirtualPox->QueryInterface(virtualPoxIID, (void **)&g_VirtualPox);
            pVirtualPox->Release();
        }
        pVirtualPoxClient->Release();
    }
#endif /* !VPOX_WITH_XPCOM */
    if (FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate VirtualPox object! rc=%Rhrc\n",rc));
#ifdef VPOX_WITH_XPCOM
        pManager->Release();
        pManager = NULL;
#endif /* VPOX_WITH_XPCOM */
        VPoxComUninitialize();
        return;
    }

    Log(("Cbinding: IVirtualPox object created.\n"));

#ifdef VPOX_WITH_XPCOM
    rc = pManager->CreateInstanceByContractID(NS_SESSION_CONTRACTID,
                                              nsnull,
                                              sessionIID,
                                              (void **)&g_Session);
#else /* !VPOX_WITH_XPCOM */
    rc = CoCreateInstance(CLSID_Session, NULL, CLSCTX_INPROC_SERVER, sessionIID, (void **)&g_Session);
#endif /* !VPOX_WITH_XPCOM */
    if (FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate Session object! rc=%Rhrc\n",rc));
#ifdef VPOX_WITH_XPCOM
        pManager->Release();
        pManager = NULL;
#endif /* VPOX_WITH_XPCOM */
        VPoxComUninitialize();
        return;
    }

    Log(("Cbinding: ISession object created.\n"));

#ifdef VPOX_WITH_XPCOM
    pManager->Release();
    pManager = NULL;
#endif /* VPOX_WITH_XPCOM */

    *ppSession = g_Session;
    *ppVirtualPox = g_VirtualPox;
}

static void
VPoxComInitializeV1(IVirtualPox **ppVirtualPox, ISession **ppSession)
{
    VPoxComInitialize(NULL, ppVirtualPox, NULL, ppSession);
}

static void
VPoxComUninitialize(void)
{
    if (g_Session)
    {
        g_Session->Release();
        g_Session = NULL;
    }
    if (g_VirtualPox)
    {
        g_VirtualPox->Release();
        g_VirtualPox = NULL;
    }
#ifdef VPOX_WITH_XPCOM
    if (g_EventQueue)
    {
        g_EventQueue->Release();
        g_EventQueue = NULL;
    }
#endif /* VPOX_WITH_XPCOM */
    com::Shutdown();
    Log(("Cbinding: Cleaned up the created objects.\n"));
}

#ifdef VPOX_WITH_XPCOM
static void
VPoxGetEventQueue(nsIEventQueue **ppEventQueue)
{
    *ppEventQueue = g_EventQueue;
}
#endif /* VPOX_WITH_XPCOM */

static int
VPoxProcessEventQueue(LONG64 iTimeoutMS)
{
    RTMSINTERVAL iTimeout;
    if (iTimeoutMS < 0 || iTimeoutMS > UINT32_MAX)
        iTimeout = RT_INDEFINITE_WAIT;
    else
        iTimeout = (RTMSINTERVAL)iTimeoutMS;
    int vrc = com::NativeEventQueue::getMainEventQueue()->processEventQueue(iTimeout);
    switch (vrc)
    {
        case VINF_SUCCESS:
            return 0;
        case VINF_INTERRUPTED:
            return 1;
        case VERR_INTERRUPTED:
            return 2;
        case VERR_TIMEOUT:
            return 3;
        case VERR_INVALID_CONTEXT:
            return 4;
        default:
            return 5;
    }
}

static int
VPoxInterruptEventQueueProcessing(void)
{
    com::NativeEventQueue::getMainEventQueue()->interruptEventQueueProcessing();
    return 0;
}

static HRESULT
VPoxGetException(IErrorInfo **ppException)
{
    HRESULT rc;

    *ppException = NULL;

#ifdef VPOX_WITH_XPCOM
    nsIServiceManager *mgr = NULL;
    rc = NS_GetServiceManager(&mgr);
    if (FAILED(rc) || !mgr)
        return rc;

    IID esid = NS_IEXCEPTIONSERVICE_IID;
    nsIExceptionService *es = NULL;
    rc = mgr->GetServiceByContractID(NS_EXCEPTIONSERVICE_CONTRACTID, esid, (void **)&es);
    if (FAILED(rc) || !es)
    {
        mgr->Release();
        return rc;
    }

    nsIExceptionManager *em;
    rc = es->GetCurrentExceptionManager(&em);
    if (FAILED(rc) || !em)
    {
        es->Release();
        mgr->Release();
        return rc;
    }

    nsIException *ex;
    rc = em->GetCurrentException(&ex);
    if (FAILED(rc))
    {
        em->Release();
        es->Release();
        mgr->Release();
        return rc;
    }

    *ppException = ex;
    em->Release();
    es->Release();
    mgr->Release();
#else /* !VPOX_WITH_XPCOM */
    IErrorInfo *ex;
    rc = ::GetErrorInfo(0, &ex);
    if (FAILED(rc))
        return rc;

    *ppException = ex;
#endif /* !VPOX_WITH_XPCOM */

    return rc;
}

static HRESULT
VPoxClearException(void)
{
    HRESULT rc;

#ifdef VPOX_WITH_XPCOM
    nsIServiceManager *mgr = NULL;
    rc = NS_GetServiceManager(&mgr);
    if (FAILED(rc) || !mgr)
        return rc;

    IID esid = NS_IEXCEPTIONSERVICE_IID;
    nsIExceptionService *es = NULL;
    rc = mgr->GetServiceByContractID(NS_EXCEPTIONSERVICE_CONTRACTID, esid, (void **)&es);
    if (FAILED(rc) || !es)
    {
        mgr->Release();
        return rc;
    }

    nsIExceptionManager *em;
    rc = es->GetCurrentExceptionManager(&em);
    if (FAILED(rc) || !em)
    {
        es->Release();
        mgr->Release();
        return rc;
    }

    rc = em->SetCurrentException(NULL);
    em->Release();
    es->Release();
    mgr->Release();
#else /* !VPOX_WITH_XPCOM */
    rc = ::SetErrorInfo(0, NULL);
#endif /* !VPOX_WITH_XPCOM */

    return rc;
}

static HRESULT
VPoxClientInitialize(const char *pszVirtualPoxClientIID, IVirtualPoxClient **ppVirtualPoxClient)
{
    IID virtualPoxClientIID;

    *ppVirtualPoxClient = NULL;

    /* convert the string representation of UUID to IID type */
    if (pszVirtualPoxClientIID && *pszVirtualPoxClientIID)
    {
        int vrc = ::RTUuidFromStr((RTUUID *)&virtualPoxClientIID, pszVirtualPoxClientIID);
        if (RT_FAILURE(vrc))
            return E_INVALIDARG;
    }
    else
        virtualPoxClientIID = IID_IVirtualPoxClient;

    HRESULT rc = com::Initialize(VPOX_COM_INIT_F_DEFAULT | VPOX_COM_INIT_F_NO_COM_PATCHING);
    if (FAILED(rc))
    {
        Log(("Cbinding: COM/XPCOM could not be initialized! rc=%Rhrc\n", rc));
        VPoxClientUninitialize();
        return rc;
    }

#ifdef VPOX_WITH_XPCOM
    rc = NS_GetMainEventQ(&g_EventQueue);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get XPCOM event queue! rc=%Rhrc\n", rc));
        VPoxClientUninitialize();
        return rc;
    }
#endif /* VPOX_WITH_XPCOM */

#ifdef VPOX_WITH_XPCOM
    nsIComponentManager *pManager;
    rc = NS_GetComponentManager(&pManager);
    if (FAILED(rc))
    {
        Log(("Cbinding: Could not get component manager! rc=%Rhrc\n", rc));
        VPoxClientUninitialize();
        return rc;
    }

    rc = pManager->CreateInstanceByContractID(NS_VIRTUALPOXCLIENT_CONTRACTID,
                                              nsnull,
                                              virtualPoxClientIID,
                                              (void **)ppVirtualPoxClient);
#else /* !VPOX_WITH_XPCOM */
    rc = CoCreateInstance(CLSID_VirtualPoxClient, NULL, CLSCTX_INPROC_SERVER, virtualPoxClientIID, (void **)ppVirtualPoxClient);
#endif /* !VPOX_WITH_XPCOM */
    if (FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate VirtualPoxClient object! rc=%Rhrc\n",rc));
#ifdef VPOX_WITH_XPCOM
        pManager->Release();
        pManager = NULL;
#endif /* VPOX_WITH_XPCOM */
        VPoxClientUninitialize();
        return rc;
    }

#ifdef VPOX_WITH_XPCOM
    pManager->Release();
    pManager = NULL;
#endif /* VPOX_WITH_XPCOM */

    Log(("Cbinding: IVirtualPoxClient object created.\n"));

    return S_OK;
}

static HRESULT
VPoxClientThreadInitialize(void)
{
    return com::Initialize(VPOX_COM_INIT_F_DEFAULT | VPOX_COM_INIT_F_NO_COM_PATCHING);
}

static HRESULT
VPoxClientThreadUninitialize(void)
{
    return com::Shutdown();
}

static void
VPoxClientUninitialize(void)
{
#ifdef VPOX_WITH_XPCOM
    if (g_EventQueue)
    {
        NS_RELEASE(g_EventQueue);
        g_EventQueue = NULL;
    }
#endif /* VPOX_WITH_XPCOM */
    com::Shutdown();
    Log(("Cbinding: Cleaned up the created objects.\n"));
}

static unsigned int
VPoxVersion(void)
{
    return VPOX_VERSION_MAJOR * 1000 * 1000 + VPOX_VERSION_MINOR * 1000 + VPOX_VERSION_BUILD;
}

static unsigned int
VPoxAPIVersion(void)
{
    return VPOX_VERSION_MAJOR * 1000 + VPOX_VERSION_MINOR + (VPOX_VERSION_BUILD > 50 ? 1 : 0);
}

VPOXCAPI_DECL(PCVPOXCAPI)
VPoxGetCAPIFunctions(unsigned uVersion)
{
    /* This is the first piece of code which knows that IPRT exists, so
     * initialize it properly. The limited initialization in VPoxC is not
     * sufficient, and causes trouble with com::Initialize() misbehaving. */
    RTR3InitDll(0);

    /*
     * The current interface version.
     */
    static const VPOXCAPI s_Functions =
    {
        sizeof(VPOXCAPI),
        VPOX_CAPI_VERSION,

        VPoxVersion,
        VPoxAPIVersion,

        VPoxClientInitialize,
        VPoxClientThreadInitialize,
        VPoxClientThreadUninitialize,
        VPoxClientUninitialize,

        VPoxComInitialize,
        VPoxComUninitialize,

        VPoxComUnallocString,

        VPoxUtf16ToUtf8,
        VPoxUtf8ToUtf16,
        VPoxUtf8Free,
        VPoxUtf16Free,

        VPoxSafeArrayCreateVector,
        VPoxSafeArrayOutParamAlloc,
        VPoxSafeArrayCopyInParamHelper,
        VPoxSafeArrayCopyOutParamHelper,
        VPoxSafeArrayCopyOutIfaceParamHelper,
        VPoxSafeArrayDestroy,
        VPoxArrayOutFree,

#ifdef VPOX_WITH_XPCOM
        VPoxGetEventQueue,
#endif /* VPOX_WITH_XPCOM */
        VPoxGetException,
        VPoxClearException,
        VPoxProcessEventQueue,
        VPoxInterruptEventQueueProcessing,

        VPoxUtf8Clear,
        VPoxUtf16Clear,

        VPOX_CAPI_VERSION
    };

    if ((uVersion & 0xffff0000U) == (VPOX_CAPI_VERSION & 0xffff0000U))
        return &s_Functions;

    /*
     * Legacy interface version 3.0.
     */
    static const struct VPOXCAPIV3
    {
        /** The size of the structure. */
        unsigned cb;
        /** The structure version. */
        unsigned uVersion;

        unsigned int (*pfnGetVersion)(void);

        unsigned int (*pfnGetAPIVersion)(void);

        HRESULT (*pfnClientInitialize)(const char *pszVirtualPoxClientIID,
                                       IVirtualPoxClient **ppVirtualPoxClient);
        void (*pfnClientUninitialize)(void);

        void (*pfnComInitialize)(const char *pszVirtualPoxIID,
                                 IVirtualPox **ppVirtualPox,
                                 const char *pszSessionIID,
                                 ISession **ppSession);

        void (*pfnComUninitialize)(void);

        void (*pfnComUnallocMem)(void *pv);

        int (*pfnUtf16ToUtf8)(CBSTR pwszString, char **ppszString);
        int (*pfnUtf8ToUtf16)(const char *pszString, BSTR *ppwszString);
        void (*pfnUtf8Free)(char *pszString);
        void (*pfnUtf16Free)(BSTR pwszString);

#ifdef VPOX_WITH_XPCOM
        void (*pfnGetEventQueue)(nsIEventQueue **ppEventQueue);
#endif /* VPOX_WITH_XPCOM */
        HRESULT (*pfnGetException)(IErrorInfo **ppException);
        HRESULT (*pfnClearException)(void);

        /** Tail version, same as uVersion. */
        unsigned uEndVersion;
    } s_Functions_v3_0 =
    {
        sizeof(s_Functions_v3_0),
        0x00030000U,

        VPoxVersion,
        VPoxAPIVersion,

        VPoxClientInitialize,
        VPoxClientUninitialize,

        VPoxComInitialize,
        VPoxComUninitialize,

        VPoxComUnallocMem,

        VPoxUtf16ToUtf8,
        VPoxUtf8ToUtf16,
        VPoxUtf8Free,
        VPoxUtf16Free,

#ifdef VPOX_WITH_XPCOM
        VPoxGetEventQueue,
#endif /* VPOX_WITH_XPCOM */
        VPoxGetException,
        VPoxClearException,

        0x00030000U
    };

    if ((uVersion & 0xffff0000U) == 0x00030000U)
        return (PCVPOXCAPI)&s_Functions_v3_0;

    /*
     * Legacy interface version 2.0.
     */
    static const struct VPOXCAPIV2
    {
        /** The size of the structure. */
        unsigned cb;
        /** The structure version. */
        unsigned uVersion;

        unsigned int (*pfnGetVersion)(void);

        void (*pfnComInitialize)(const char *pszVirtualPoxIID,
                                 IVirtualPox **ppVirtualPox,
                                 const char *pszSessionIID,
                                 ISession **ppSession);

        void (*pfnComUninitialize)(void);

        void (*pfnComUnallocMem)(void *pv);
        void (*pfnUtf16Free)(BSTR pwszString);
        void (*pfnUtf8Free)(char *pszString);

        int (*pfnUtf16ToUtf8)(CBSTR pwszString, char **ppszString);
        int (*pfnUtf8ToUtf16)(const char *pszString, BSTR *ppwszString);

#ifdef VPOX_WITH_XPCOM
        void (*pfnGetEventQueue)(nsIEventQueue **ppEventQueue);
#endif /* VPOX_WITH_XPCOM */

        /** Tail version, same as uVersion. */
        unsigned uEndVersion;
    } s_Functions_v2_0 =
    {
        sizeof(s_Functions_v2_0),
        0x00020000U,

        VPoxVersion,

        VPoxComInitialize,
        VPoxComUninitialize,

        VPoxComUnallocMem,
        VPoxUtf16Free,
        VPoxUtf8Free,

        VPoxUtf16ToUtf8,
        VPoxUtf8ToUtf16,

#ifdef VPOX_WITH_XPCOM
        VPoxGetEventQueue,
#endif /* VPOX_WITH_XPCOM */

        0x00020000U
    };

    if ((uVersion & 0xffff0000U) == 0x00020000U)
        return (PCVPOXCAPI)&s_Functions_v2_0;

    /*
     * Legacy interface version 1.0.
     */
    static const struct VPOXCAPIV1
    {
        /** The size of the structure. */
        unsigned cb;
        /** The structure version. */
        unsigned uVersion;

        unsigned int (*pfnGetVersion)(void);

        void (*pfnComInitialize)(IVirtualPox **virtualPox, ISession **session);
        void (*pfnComUninitialize)(void);

        void (*pfnComUnallocMem)(void *pv);
        void (*pfnUtf16Free)(BSTR pwszString);
        void (*pfnUtf8Free)(char *pszString);

        int (*pfnUtf16ToUtf8)(CBSTR pwszString, char **ppszString);
        int (*pfnUtf8ToUtf16)(const char *pszString, BSTR *ppwszString);

        /** Tail version, same as uVersion. */
        unsigned uEndVersion;
    } s_Functions_v1_0 =
    {
        sizeof(s_Functions_v1_0),
        0x00010000U,

        VPoxVersion,

        VPoxComInitializeV1,
        VPoxComUninitialize,

        VPoxComUnallocMem,
        VPoxUtf16Free,
        VPoxUtf8Free,

        VPoxUtf16ToUtf8,
        VPoxUtf8ToUtf16,

        0x00010000U
    };

    if ((uVersion & 0xffff0000U) == 0x00010000U)
        return (PCVPOXCAPI)&s_Functions_v1_0;

    /*
     * Unsupported interface version.
     */
    return NULL;
}

#ifdef VPOX_WITH_XPCOM
VPOXCAPI_DECL(PCVPOXCAPI)
VPoxGetXPCOMCFunctions(unsigned uVersion)
{
    return VPoxGetCAPIFunctions(uVersion);
}
#endif /* VPOX_WITH_XPCOM */
/* vim: set ts=4 sw=4 et: */
