/* $Id: VPoxDispD3D.cpp $ */
/** @file
 * VPoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define INITGUID

#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VPox/Log.h>

#include <VPox/VPoxGuestLib.h>

#include "VPoxDispD3D.h"
#include "VPoxDispDbg.h"

#include <Psapi.h>

#define VPOXDISP_IS_MODULE_FUNC(_pvModule, _cbModule, _pfn) ( \
           (((uintptr_t)(_pfn)) >= ((uintptr_t)(_pvModule))) \
        && (((uintptr_t)(_pfn)) < (((uintptr_t)(_pvModule)) + ((DWORD)(_cbModule)))) \
        )

static BOOL vpoxDispIsDDraw(D3DDDIARG_OPENADAPTER const *pOpenData)
{
    /*if we are loaded by ddraw module, the Interface version should be 7
     * and pAdapterCallbacks should be ddraw-supplied, i.e. reside in ddraw module */
    if (pOpenData->Interface != 7)
        return FALSE;

    HMODULE hDDraw = GetModuleHandleA("ddraw.dll");
    if (!hDDraw)
        return FALSE;

    HANDLE hProcess = GetCurrentProcess();
    MODULEINFO ModuleInfo = {0};

    if (!GetModuleInformation(hProcess, hDDraw, &ModuleInfo, sizeof (ModuleInfo)))
    {
        DWORD winEr = GetLastError(); NOREF(winEr);
        WARN(("GetModuleInformation failed, %d", winEr));
        return FALSE;
    }

    if (VPOXDISP_IS_MODULE_FUNC(ModuleInfo.lpBaseOfDll, ModuleInfo.SizeOfImage,
                                pOpenData->pAdapterCallbacks->pfnQueryAdapterInfoCb))
        return TRUE;
    if (VPOXDISP_IS_MODULE_FUNC(ModuleInfo.lpBaseOfDll, ModuleInfo.SizeOfImage,
                                pOpenData->pAdapterCallbacks->pfnGetMultisampleMethodListCb))
        return TRUE;

    return FALSE;
}

static HRESULT vpoxDispQueryAdapterInfo(D3DDDIARG_OPENADAPTER const *pOpenData, VPOXWDDM_QAI **ppAdapterInfo)
{
    VPOXWDDM_QAI *pAdapterInfo = (VPOXWDDM_QAI *)RTMemAllocZ(sizeof(VPOXWDDM_QAI));
    AssertReturn(pAdapterInfo, E_OUTOFMEMORY);

    D3DDDICB_QUERYADAPTERINFO DdiQuery;
    DdiQuery.PrivateDriverDataSize = sizeof(VPOXWDDM_QAI);
    DdiQuery.pPrivateDriverData = pAdapterInfo;
    HRESULT hr = pOpenData->pAdapterCallbacks->pfnQueryAdapterInfoCb(pOpenData->hAdapter, &DdiQuery);
    AssertReturnStmt(SUCCEEDED(hr), RTMemFree(pAdapterInfo), hr);

    /* Check that the miniport version match display version. */
    if (pAdapterInfo->u32Version == VPOXVIDEOIF_VERSION)
    {
        *ppAdapterInfo = pAdapterInfo;
    }
    else
    {
        LOGREL_EXACT((__FUNCTION__": miniport version mismatch, expected (%d), but was (%d)\n",
                      VPOXVIDEOIF_VERSION, pAdapterInfo->u32Version));
        hr = E_FAIL;
    }

    return hr;
}

static HRESULT vpoxDispAdapterInit(D3DDDIARG_OPENADAPTER const *pOpenData, VPOXWDDM_QAI *pAdapterInfo,
                                   PVPOXWDDMDISP_ADAPTER *ppAdapter)
{
#ifdef VPOX_WITH_VIDEOHWACCEL
    Assert(pAdapterInfo->cInfos >= 1);
    PVPOXWDDMDISP_ADAPTER pAdapter = (PVPOXWDDMDISP_ADAPTER)RTMemAllocZ(RT_UOFFSETOF_DYN(VPOXWDDMDISP_ADAPTER,
                                                                                         aHeads[pAdapterInfo->cInfos]));
#else
    Assert(pAdapterInfo->cInfos == 0);
    PVPOXWDDMDISP_ADAPTER pAdapter = (PVPOXWDDMDISP_ADAPTER)RTMemAllocZ(sizeof(VPOXWDDMDISP_ADAPTER));
#endif
    AssertReturn(pAdapter, E_OUTOFMEMORY);

    pAdapter->hAdapter    = pOpenData->hAdapter;
    pAdapter->uIfVersion  = pOpenData->Interface;
    pAdapter->uRtVersion  = pOpenData->Version;
    pAdapter->RtCallbacks = *pOpenData->pAdapterCallbacks;
    pAdapter->enmHwType   = pAdapterInfo->enmHwType;
    if (pAdapter->enmHwType == VPOXVIDEO_HWTYPE_VPOX)
        pAdapter->u32VPox3DCaps = pAdapterInfo->u.vpox.u32VPox3DCaps;
    pAdapter->AdapterInfo = *pAdapterInfo;
    pAdapter->f3D         =    RT_BOOL(pAdapterInfo->u32AdapterCaps & VPOXWDDM_QAI_CAP_3D)
                            && !vpoxDispIsDDraw(pOpenData);
#ifdef VPOX_WITH_VIDEOHWACCEL
    pAdapter->cHeads      = pAdapterInfo->cInfos;
    for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
        pAdapter->aHeads[i].Vhwa.Settings = pAdapterInfo->aInfos[i];
#endif

    *ppAdapter = pAdapter;
    return S_OK;
}

HRESULT APIENTRY OpenAdapter(__inout D3DDDIARG_OPENADAPTER *pOpenData)
{
    LOG_EXACT(("==> "__FUNCTION__"\n"));

    LOGREL(("Built %s %s", __DATE__, __TIME__));

    VPOXWDDM_QAI *pAdapterInfo = NULL;
    PVPOXWDDMDISP_ADAPTER pAdapter = NULL;

    /* Query the miniport about virtual hardware capabilities. */
    HRESULT hr = vpoxDispQueryAdapterInfo(pOpenData, &pAdapterInfo);
    if (SUCCEEDED(hr))
    {
        hr = vpoxDispAdapterInit(pOpenData, pAdapterInfo, &pAdapter);
        if (SUCCEEDED(hr))
        {
            if (pAdapter->f3D)
            {
                /* 3D adapter. Try enable the 3D. */
                hr = VPoxDispD3DGlobalOpen(&pAdapter->D3D, &pAdapter->Formats, &pAdapter->AdapterInfo);
                if (hr == S_OK)
                {
                    LOG(("SUCCESS 3D Enabled, pAdapter (0x%p)", pAdapter));
                }
                else
                    WARN(("VPoxDispD3DOpen failed, hr (%d)", hr));
            }
#ifdef VPOX_WITH_VIDEOHWACCEL
            else
            {
                /* 2D adapter. */
                hr = VPoxDispD3DGlobal2DFormatsInit(pAdapter);
                if (FAILED(hr))
                    WARN(("VPoxDispD3DGlobal2DFormatsInit failed hr 0x%x", hr));
            }
#endif
        }
    }

    if (SUCCEEDED(hr))
    {
        /* Return data to the OS. */
        if (pAdapter->enmHwType == VPOXVIDEO_HWTYPE_VPOX)
        {
            /* Not supposed to work with this. */
            hr = E_FAIL;
        }
#ifdef VPOX_WITH_MESA3D
        else if (pAdapter->enmHwType == VPOXVIDEO_HWTYPE_VMSVGA)
        {
            pOpenData->hAdapter                       = pAdapter;
            pOpenData->pAdapterFuncs->pfnGetCaps      = GaDdiAdapterGetCaps;
            pOpenData->pAdapterFuncs->pfnCreateDevice = GaDdiAdapterCreateDevice;
            pOpenData->pAdapterFuncs->pfnCloseAdapter = GaDdiAdapterCloseAdapter;
            pOpenData->DriverVersion                  = RT_BOOL(pAdapterInfo->u32AdapterCaps & VPOXWDDM_QAI_CAP_WIN7)
                                                      ? D3D_UMD_INTERFACE_VERSION_WIN7
                                                      : D3D_UMD_INTERFACE_VERSION_VISTA;
        }
#endif
        else
            hr = E_FAIL;
    }

    if (FAILED(hr))
    {
        WARN(("OpenAdapter failed hr 0x%x", hr));
        RTMemFree(pAdapter);
    }

    RTMemFree(pAdapterInfo);

    LOG_EXACT(("<== "__FUNCTION__", hr (%x)\n", hr));
    return hr;
}


/**
 * DLL entry point.
 */
BOOL WINAPI DllMain(HINSTANCE hInstance,
                    DWORD     dwReason,
                    LPVOID    lpReserved)
{
    RT_NOREF(hInstance, lpReserved);

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            vpoxVDbgPrint(("VPoxDispD3D: DLL loaded.\n"));
#ifdef VPOXWDDMDISP_DEBUG_VEHANDLER
            vpoxVDbgVEHandlerRegister();
#endif
            int rc = RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                VPoxDispD3DGlobalInit();
                vpoxVDbgPrint(("VPoxDispD3D: DLL loaded OK\n"));
                return TRUE;
            }

#ifdef VPOXWDDMDISP_DEBUG_VEHANDLER
            vpoxVDbgVEHandlerUnregister();
#endif
            break;
        }

        case DLL_PROCESS_DETACH:
        {
#ifdef VPOXWDDMDISP_DEBUG_VEHANDLER
            vpoxVDbgVEHandlerUnregister();
#endif
            /// @todo RTR3Term();
            VPoxDispD3DGlobalTerm();
            return TRUE;

            break;
        }

        default:
            return TRUE;
    }
    return FALSE;
}
