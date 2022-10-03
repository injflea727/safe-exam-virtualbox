/* $Id: VPoxDispMpLogger.cpp $ */
/** @file
 * VPox WDDM Display backdoor logger implementation
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

/* We're unable to use standard r3 vbgl-based backdoor logging API because win8 Metro apps
 * can not do CreateFile/Read/Write by default
 * this is why we use miniport escape functionality to issue backdoor log string to the miniport
 * and submit it to host via standard r0 backdoor logging api accordingly */

#include <VPoxDispMpLogger.h>
#include <iprt/win/windows.h>
#include <iprt/win/d3d9.h>
#include <d3dumddi.h>
#include <../../../common/wddm/VPoxMPIf.h>
#include <VPoxDispKmt.h>

#define VPOX_VIDEO_LOG_NAME "VPoxDispMpLogger"
#include <../../../common/VPoxVideoLog.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>

#include <stdio.h>

typedef enum
{
    VPOXDISPMPLOGGER_STATE_UNINITIALIZED = 0,
    VPOXDISPMPLOGGER_STATE_INITIALIZING,
    VPOXDISPMPLOGGER_STATE_INITIALIZED,
    VPOXDISPMPLOGGER_STATE_UNINITIALIZING
} VPOXDISPMPLOGGER_STATE;

typedef struct VPOXDISPMPLOGGER
{
    VPOXDISPKMT_CALLBACKS KmtCallbacks;
    VPOXDISPMPLOGGER_STATE enmState;
} VPOXDISPMPLOGGER, *PVPOXDISPMPLOGGER;

static VPOXDISPMPLOGGER g_VPoxDispMpLogger = {0};

static PVPOXDISPMPLOGGER vpoxDispMpLoggerGet()
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VPoxDispMpLogger.enmState, VPOXDISPMPLOGGER_STATE_INITIALIZING, VPOXDISPMPLOGGER_STATE_UNINITIALIZED))
    {
        HRESULT hr = vpoxDispKmtCallbacksInit(&g_VPoxDispMpLogger.KmtCallbacks);
        if (hr == S_OK)
        {
            /* we are on Vista+
             * check if we can Open Adapter, i.e. WDDM driver is installed */
            VPOXDISPKMT_ADAPTER Adapter;
            hr = vpoxDispKmtOpenAdapter(&g_VPoxDispMpLogger.KmtCallbacks, &Adapter);
            if (hr == S_OK)
            {
                ASMAtomicWriteU32((volatile uint32_t *)&g_VPoxDispMpLogger.enmState, VPOXDISPMPLOGGER_STATE_INITIALIZED);
                vpoxDispKmtCloseAdapter(&Adapter);
                return &g_VPoxDispMpLogger;
            }
            vpoxDispKmtCallbacksTerm(&g_VPoxDispMpLogger.KmtCallbacks);
        }
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VPoxDispMpLogger.enmState) == VPOXDISPMPLOGGER_STATE_INITIALIZED)
    {
        return &g_VPoxDispMpLogger;
    }
    return NULL;
}

VPOXDISPMPLOGGER_DECL(int) VPoxDispMpLoggerInit(void)
{
    PVPOXDISPMPLOGGER pLogger = vpoxDispMpLoggerGet();
    if (!pLogger)
        return VERR_NOT_SUPPORTED;
    return VINF_SUCCESS;
}

VPOXDISPMPLOGGER_DECL(int) VPoxDispMpLoggerTerm(void)
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VPoxDispMpLogger.enmState, VPOXDISPMPLOGGER_STATE_UNINITIALIZING, VPOXDISPMPLOGGER_STATE_INITIALIZED))
    {
        vpoxDispKmtCallbacksTerm(&g_VPoxDispMpLogger.KmtCallbacks);
        ASMAtomicWriteU32((volatile uint32_t *)&g_VPoxDispMpLogger.enmState, VPOXDISPMPLOGGER_STATE_UNINITIALIZED);
        return S_OK;
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VPoxDispMpLogger.enmState) == VPOXDISPMPLOGGER_STATE_UNINITIALIZED)
    {
        return S_OK;
    }
    return VERR_NOT_SUPPORTED;
}

VPOXDISPMPLOGGER_DECL(void) VPoxDispMpLoggerLog(const char *pszString)
{
    PVPOXDISPMPLOGGER pLogger = vpoxDispMpLoggerGet();
    if (!pLogger)
        return;

    VPOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vpoxDispKmtOpenAdapter(&pLogger->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbString = (uint32_t)strlen(pszString) + 1;
        uint32_t cbCmd = RT_UOFFSETOF_DYN(VPOXDISPIFESCAPE_DBGPRINT, aStringBuf[cbString]);
        PVPOXDISPIFESCAPE_DBGPRINT pCmd = (PVPOXDISPIFESCAPE_DBGPRINT)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VPOXESC_DBGPRINT;
            memcpy(pCmd->aStringBuf, pszString, cbString);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pLogger->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                BP_WARN();
            }

            RTMemFree(pCmd);
        }
        else
        {
            BP_WARN();
        }
        hr = vpoxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            BP_WARN();
        }
    }
}

VPOXDISPMPLOGGER_DECL(void) VPoxDispMpLoggerLogF(const char *pszString, ...)
{
    PVPOXDISPMPLOGGER pLogger = vpoxDispMpLoggerGet();
    if (!pLogger)
        return;

    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, pszString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), pszString, pArgList);
    va_end(pArgList);

    VPoxDispMpLoggerLog(szBuffer);
}

static void vpoxDispMpLoggerDumpBuf(void *pvBuf, uint32_t cbBuf, VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE enmBuf)
{
    PVPOXDISPMPLOGGER pLogger = vpoxDispMpLoggerGet();
    if (!pLogger)
        return;

    VPOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vpoxDispKmtOpenAdapter(&pLogger->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbCmd = RT_UOFFSETOF_DYN(VPOXDISPIFESCAPE_DBGDUMPBUF, aBuf[cbBuf]);
        PVPOXDISPIFESCAPE_DBGDUMPBUF pCmd = (PVPOXDISPIFESCAPE_DBGDUMPBUF)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VPOXESC_DBGDUMPBUF;
            pCmd->enmType = enmBuf;
#ifdef VPOX_WDDM_WOW64
            pCmd->Flags.WoW64 = 1;
#endif
            memcpy(pCmd->aBuf, pvBuf, cbBuf);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pLogger->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                BP_WARN();
            }

            RTMemFree(pCmd);
        }
        else
        {
            BP_WARN();
        }
        hr = vpoxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            BP_WARN();
        }
    }
}

VPOXDISPMPLOGGER_DECL(void) VPoxDispMpLoggerDumpD3DCAPS9(struct _D3DCAPS9 *pCaps)
{
    vpoxDispMpLoggerDumpBuf(pCaps, sizeof (*pCaps), VPOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9);
}

/*
 * Prefix the output string with module name and pid/tid.
 */
static const char *vpoxUmLogGetModuleName(void)
{
    static int s_fModuleNameInited = 0;
    static char s_szModuleName[MAX_PATH];

    if (!s_fModuleNameInited)
    {
        const DWORD cchName = GetModuleFileNameA(NULL, s_szModuleName, RT_ELEMENTS(s_szModuleName));
        if (cchName == 0)
        {
            return "<no module>";
        }
        s_fModuleNameInited = 1;
    }
    return &s_szModuleName[0];
}

DECLCALLBACK(void) VPoxWddmUmLog(const char *pszString)
{
    char szBuffer[4096];
    const int cbBuffer = sizeof(szBuffer);
    char *pszBuffer = &szBuffer[0];

    int cbWritten = _snprintf(pszBuffer, cbBuffer, "['%s' 0x%x.0x%x]: ",
                              vpoxUmLogGetModuleName(), GetCurrentProcessId(), GetCurrentThreadId());
    if (cbWritten < 0 || cbWritten >= cbBuffer)
    {
        AssertFailed();
        pszBuffer[0] = 0;
        cbWritten = 0;
    }

    const size_t cbLeft = cbBuffer - cbWritten;
    const size_t cbString = strlen(pszString) + 1;
    if (cbString <= cbLeft)
    {
        memcpy(pszBuffer + cbWritten, pszString, cbString);
    }
    else
    {
        memcpy(pszBuffer + cbWritten, pszString, cbLeft - 1);
        pszBuffer[cbWritten + cbLeft - 1] = 0;
    }

    VPoxDispMpLoggerLog(szBuffer);
}
