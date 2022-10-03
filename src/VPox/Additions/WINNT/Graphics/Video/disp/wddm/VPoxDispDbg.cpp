/* $Id: VPoxDispDbg.cpp $ */
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

#include "VPoxDispD3DCmn.h"

#ifdef VPOXWDDMDISP_DEBUG_VEHANDLER
#include <Psapi.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include <iprt/asm.h>
#include <iprt/assert.h>

static DWORD g_VPoxVDbgFIsModuleNameInited = 0;
static char g_VPoxVDbgModuleName[MAX_PATH];

char *vpoxVDbgDoGetModuleName()
{
    if (!g_VPoxVDbgFIsModuleNameInited)
    {
        DWORD cName = GetModuleFileNameA(NULL, g_VPoxVDbgModuleName, RT_ELEMENTS(g_VPoxVDbgModuleName));
        if (!cName)
        {
#ifdef LOG_ENABLED
            DWORD winEr = GetLastError();
#endif
            WARN(("GetModuleFileNameA failed, winEr %d", winEr));
            return NULL;
        }
        g_VPoxVDbgFIsModuleNameInited = TRUE;
    }
    return g_VPoxVDbgModuleName;
}

static void vpoxDispLogDbgFormatStringV(char * szBuffer, uint32_t cbBuffer, const char * szString, va_list pArgList)
{
    uint32_t cbWritten = sprintf(szBuffer, "['%s' 0x%x.0x%x] Disp: ", vpoxVDbgDoGetModuleName(), GetCurrentProcessId(), GetCurrentThreadId());
    if (cbWritten > cbBuffer)
    {
        AssertReleaseFailed();
        return;
    }

    _vsnprintf(szBuffer + cbWritten, cbBuffer - cbWritten, szString, pArgList);
}

#if defined(VPOXWDDMDISP_DEBUG)
LONG g_VPoxVDbgFIsDwm = -1;

DWORD g_VPoxVDbgPid = 0;

DWORD g_VPoxVDbgFLogRel = 1;
# if !defined(VPOXWDDMDISP_DEBUG)
DWORD g_VPoxVDbgFLog = 0;
# else
DWORD g_VPoxVDbgFLog = 1;
# endif
DWORD g_VPoxVDbgFLogFlow = 0;

#endif

#ifdef VPOXWDDMDISP_DEBUG

#define VPOXWDDMDISP_DEBUG_DUMP_DEFAULT 0
DWORD g_VPoxVDbgFDumpSetTexture = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpDrawPrim = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpTexBlt = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpBlt = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpRtSynch = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpFlush = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpShared = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpLock = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpUnlock = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpPresentEnter = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpPresentLeave = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFDumpScSync = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;

DWORD g_VPoxVDbgFBreakShared = VPOXWDDMDISP_DEBUG_DUMP_DEFAULT;
DWORD g_VPoxVDbgFBreakDdi = 0;

DWORD g_VPoxVDbgFCheckSysMemSync = 0;
DWORD g_VPoxVDbgFCheckBlt = 0;
DWORD g_VPoxVDbgFCheckTexBlt = 0;
DWORD g_VPoxVDbgFCheckScSync = 0;

DWORD g_VPoxVDbgFSkipCheckTexBltDwmWndUpdate = 1;

DWORD g_VPoxVDbgCfgMaxDirectRts = 3;
DWORD g_VPoxVDbgCfgForceDummyDevCreate = 0;

PVPOXWDDMDISP_DEVICE g_VPoxVDbgInternalDevice = NULL;
PVPOXWDDMDISP_RESOURCE g_VPoxVDbgInternalRc = NULL;

VOID vpoxVDbgDoPrintDmlCmd(const char* pszDesc, const char* pszCmd)
{
    vpoxVDbgPrint(("<?dml?><exec cmd=\"%s\">%s</exec>, ( %s )\n", pszCmd, pszDesc, pszCmd));
}

VOID vpoxVDbgDoPrintDumpCmd(const char* pszDesc, const void *pvData, uint32_t width, uint32_t height, uint32_t bpp, uint32_t pitch)
{
    char Cmd[1024];
    sprintf(Cmd, "!vbvdbg.ms 0x%p 0n%d 0n%d 0n%d 0n%d", pvData, width, height, bpp, pitch);
    vpoxVDbgDoPrintDmlCmd(pszDesc, Cmd);
}

VOID vpoxVDbgDoPrintLopLastCmd(const char* pszDesc)
{
    vpoxVDbgDoPrintDmlCmd(pszDesc, "ed @@(&vpoxVDbgLoop) 0");
}

typedef struct VPOXVDBG_DUMP_INFO
{
    DWORD fFlags;
    const VPOXWDDMDISP_ALLOCATION *pAlloc;
    IDirect3DResource9 *pD3DRc;
    const RECT *pRect;
} VPOXVDBG_DUMP_INFO, *PVPOXVDBG_DUMP_INFO;

typedef DECLCALLBACK(void) FNVPOXVDBG_CONTENTS_DUMPER(PVPOXVDBG_DUMP_INFO pInfo, BOOLEAN fBreak, void *pvDumper);
typedef FNVPOXVDBG_CONTENTS_DUMPER *PFNVPOXVDBG_CONTENTS_DUMPER;

static VOID vpoxVDbgDoDumpSummary(const char * pPrefix, PVPOXVDBG_DUMP_INFO pInfo, const char * pSuffix)
{
    const VPOXWDDMDISP_ALLOCATION *pAlloc = pInfo->pAlloc;
    IDirect3DResource9 *pD3DRc = pInfo->pD3DRc;
    char rectBuf[24];
    if (pInfo->pRect)
        _snprintf(rectBuf, sizeof(rectBuf) / sizeof(rectBuf[0]), "(%d:%d);(%d:%d)",
                pInfo->pRect->left, pInfo->pRect->top,
                pInfo->pRect->right, pInfo->pRect->bottom);
    else
        strcpy(rectBuf, "n/a");

    vpoxVDbgPrint(("%s Sh(0x%p), Rc(0x%p), pAlloc(0x%x), pD3DIf(0x%p), Type(%s), Rect(%s), Locks(%d) %s",
                    pPrefix ? pPrefix : "",
                    pAlloc ? pAlloc->pRc->aAllocations[0].hSharedHandle : NULL,
                    pAlloc ? pAlloc->pRc : NULL,
                    pAlloc,
                    pD3DRc,
                    pD3DRc ? vpoxDispLogD3DRcType(pD3DRc->GetType()) : "n/a",
                    rectBuf,
                    pAlloc ? pAlloc->LockInfo.cLocks : 0,
                    pSuffix ? pSuffix : ""));
}

VOID vpoxVDbgDoDumpPerform(const char * pPrefix, PVPOXVDBG_DUMP_INFO pInfo, const char * pSuffix,
        PFNVPOXVDBG_CONTENTS_DUMPER pfnCd, void *pvCd)
{
    DWORD fFlags = pInfo->fFlags;

    if (!VPOXVDBG_DUMP_TYPE_ENABLED_FOR_INFO(pInfo, fFlags))
        return;

    if (!pInfo->pD3DRc && pInfo->pAlloc)
        pInfo->pD3DRc = (IDirect3DResource9*)pInfo->pAlloc->pD3DIf;

    BOOLEAN bLogOnly = VPOXVDBG_DUMP_TYPE_FLOW_ONLY(fFlags);
    if (bLogOnly || !pfnCd)
    {
        vpoxVDbgDoDumpSummary(pPrefix, pInfo, pSuffix);
        if (VPOXVDBG_DUMP_FLAGS_IS_SET(fFlags, VPOXVDBG_DUMP_TYPEF_BREAK_ON_FLOW)
                || (!bLogOnly && VPOXVDBG_DUMP_FLAGS_IS_CLEARED(fFlags, VPOXVDBG_DUMP_TYPEF_DONT_BREAK_ON_CONTENTS)))
            Assert(0);
        return;
    }

    vpoxVDbgDoDumpSummary(pPrefix, pInfo, NULL);

    pfnCd(pInfo, VPOXVDBG_DUMP_FLAGS_IS_CLEARED(fFlags, VPOXVDBG_DUMP_TYPEF_DONT_BREAK_ON_CONTENTS), pvCd);

    if (pSuffix && pSuffix[0] != '\0')
        vpoxVDbgPrint(("%s", pSuffix));
}

static DECLCALLBACK(void) vpoxVDbgAllocRectContentsDumperCb(PVPOXVDBG_DUMP_INFO pInfo, BOOLEAN fBreak, void *pvDumper)
{
    RT_NOREF(fBreak, pvDumper);
    const VPOXWDDMDISP_ALLOCATION *pAlloc = pInfo->pAlloc;
    const RECT *pRect = pInfo->pRect;

    Assert(pAlloc->hAllocation);

    D3DDDICB_LOCK LockData;
    LockData.hAllocation = pAlloc->hAllocation;
    LockData.PrivateDriverData = 0;
    LockData.NumPages = 0;
    LockData.pPages = NULL;
    LockData.pData = NULL; /* out */
    LockData.Flags.Value = 0;
    LockData.Flags.LockEntire =1;
    LockData.Flags.ReadOnly = 1;

    PVPOXWDDMDISP_DEVICE pDevice = pAlloc->pRc->pDevice;

    HRESULT hr = pDevice->RtCallbacks.pfnLockCb(pDevice->hDevice, &LockData);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        UINT bpp = vpoxWddmCalcBitsPerPixel(pAlloc->SurfDesc.format);
        vpoxVDbgDoPrintDumpCmd("Surf Info", LockData.pData, pAlloc->SurfDesc.d3dWidth, pAlloc->SurfDesc.height, bpp, pAlloc->SurfDesc.pitch);
        if (pRect)
        {
            Assert(pRect->right > pRect->left);
            Assert(pRect->bottom > pRect->top);
            vpoxVDbgDoPrintRect("rect: ", pRect, "\n");
            vpoxVDbgDoPrintDumpCmd("Rect Info", ((uint8_t*)LockData.pData) + (pRect->top * pAlloc->SurfDesc.pitch) + ((pRect->left * bpp) >> 3),
                    pRect->right - pRect->left, pRect->bottom - pRect->top, bpp, pAlloc->SurfDesc.pitch);
        }
        Assert(0);

        D3DDDICB_UNLOCK DdiUnlock;

        DdiUnlock.NumAllocations = 1;
        DdiUnlock.phAllocations = &pAlloc->hAllocation;

        hr = pDevice->RtCallbacks.pfnUnlockCb(pDevice->hDevice, &DdiUnlock);
        Assert(hr == S_OK);
    }
}

VOID vpoxVDbgDoDumpAllocRect(const char * pPrefix, PVPOXWDDMDISP_ALLOCATION pAlloc, RECT *pRect, const char* pSuffix, DWORD fFlags)
{
    VPOXVDBG_DUMP_INFO Info;
    Info.fFlags = fFlags;
    Info.pAlloc = pAlloc;
    Info.pD3DRc = NULL;
    Info.pRect = pRect;
    vpoxVDbgDoDumpPerform(pPrefix, &Info, pSuffix, vpoxVDbgAllocRectContentsDumperCb, NULL);
}

static DECLCALLBACK(void) vpoxVDbgRcRectContentsDumperCb(PVPOXVDBG_DUMP_INFO pInfo, BOOLEAN fBreak, void *pvDumper)
{
    RT_NOREF(pvDumper);
    const VPOXWDDMDISP_ALLOCATION *pAlloc = pInfo->pAlloc;
    const RECT *pRect = pInfo->pRect;
    IDirect3DSurface9 *pSurf;
    HRESULT hr = VPoxD3DIfSurfGet(pAlloc->pRc, pAlloc->iAlloc, &pSurf);
    if (hr != S_OK)
    {
        WARN(("VPoxD3DIfSurfGet failed, hr 0x%x", hr));
        return;
    }

    D3DSURFACE_DESC Desc;
    hr = pSurf->GetDesc(&Desc);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        D3DLOCKED_RECT Lr;
        hr = pSurf->LockRect(&Lr, NULL, D3DLOCK_READONLY);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            UINT bpp = vpoxWddmCalcBitsPerPixel((D3DDDIFORMAT)Desc.Format);
            vpoxVDbgDoPrintDumpCmd("Surf Info", Lr.pBits, Desc.Width, Desc.Height, bpp, Lr.Pitch);
            if (pRect)
            {
                Assert(pRect->right > pRect->left);
                Assert(pRect->bottom > pRect->top);
                vpoxVDbgDoPrintRect("rect: ", pRect, "\n");
                vpoxVDbgDoPrintDumpCmd("Rect Info", ((uint8_t*)Lr.pBits) + (pRect->top * Lr.Pitch) + ((pRect->left * bpp) >> 3),
                        pRect->right - pRect->left, pRect->bottom - pRect->top, bpp, Lr.Pitch);
            }

            if (fBreak)
            {
                Assert(0);
            }
            hr = pSurf->UnlockRect();
            Assert(hr == S_OK);
        }
    }

    pSurf->Release();
}

VOID vpoxVDbgDoDumpRcRect(const char * pPrefix, PVPOXWDDMDISP_ALLOCATION pAlloc,
        IDirect3DResource9 *pD3DRc, RECT *pRect, const char * pSuffix, DWORD fFlags)
{
    VPOXVDBG_DUMP_INFO Info;
    Info.fFlags = fFlags;
    Info.pAlloc = pAlloc;
    Info.pD3DRc = pD3DRc;
    Info.pRect = pRect;
    vpoxVDbgDoDumpPerform(pPrefix, &Info, pSuffix, vpoxVDbgRcRectContentsDumperCb, NULL);
}


#define VPOXVDBG_STRCASE(_t) \
        case _t: return #_t;
#define VPOXVDBG_STRCASE_UNKNOWN() \
        default: Assert(0); return "Unknown";

const char* vpoxVDbgStrCubeFaceType(D3DCUBEMAP_FACES enmFace)
{
    switch (enmFace)
    {
    VPOXVDBG_STRCASE(D3DCUBEMAP_FACE_POSITIVE_X);
    VPOXVDBG_STRCASE(D3DCUBEMAP_FACE_NEGATIVE_X);
    VPOXVDBG_STRCASE(D3DCUBEMAP_FACE_POSITIVE_Y);
    VPOXVDBG_STRCASE(D3DCUBEMAP_FACE_NEGATIVE_Y);
    VPOXVDBG_STRCASE(D3DCUBEMAP_FACE_POSITIVE_Z);
    VPOXVDBG_STRCASE(D3DCUBEMAP_FACE_NEGATIVE_Z);
    VPOXVDBG_STRCASE_UNKNOWN();
    }
}

VOID vpoxVDbgDoDumpRt(const char * pPrefix, PVPOXWDDMDISP_DEVICE pDevice, const char * pSuffix, DWORD fFlags)
{
    for (UINT i = 0; i < pDevice->cRTs; ++i)
    {
        IDirect3DSurface9 *pRt;
        PVPOXWDDMDISP_ALLOCATION pAlloc = pDevice->apRTs[i];
        if (!pAlloc) continue;
        IDirect3DDevice9 *pDeviceIf = pDevice->pDevice9If;
        HRESULT hr = pDeviceIf->GetRenderTarget(i, &pRt);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
//            Assert(pAlloc->pD3DIf == pRt);
            vpoxVDbgDoDumpRcRect(pPrefix, pAlloc, NULL, NULL, pSuffix, fFlags);
            pRt->Release();
        }
        else
        {
            vpoxVDbgPrint((__FUNCTION__": ERROR getting rt: 0x%x", hr));
        }
    }
}

VOID vpoxVDbgDoDumpSamplers(const char * pPrefix, PVPOXWDDMDISP_DEVICE pDevice, const char * pSuffix, DWORD fFlags)
{
    for (UINT i = 0, iSampler = 0; iSampler < pDevice->cSamplerTextures; ++i)
    {
        Assert(i < RT_ELEMENTS(pDevice->aSamplerTextures));
        if (!pDevice->aSamplerTextures[i]) continue;
        PVPOXWDDMDISP_RESOURCE pRc = pDevice->aSamplerTextures[i];
        for (UINT j = 0; j < pRc->cAllocations; ++j)
        {
            PVPOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[j];
            vpoxVDbgDoDumpRcRect(pPrefix, pAlloc, NULL, NULL, pSuffix, fFlags);
        }
        ++iSampler;
    }
}

static DECLCALLBACK(void) vpoxVDbgLockUnlockSurfTexContentsDumperCb(PVPOXVDBG_DUMP_INFO pInfo, BOOLEAN fBreak, void *pvDumper)
{
    RT_NOREF(pvDumper);
    const VPOXWDDMDISP_ALLOCATION *pAlloc = pInfo->pAlloc;
    const RECT *pRect = pInfo->pRect;
    UINT bpp = vpoxWddmCalcBitsPerPixel(pAlloc->SurfDesc.format);
    uint32_t width, height, pitch = 0;
    void *pvData;
    if (pAlloc->LockInfo.fFlags.AreaValid)
    {
        width = pAlloc->LockInfo.Area.left - pAlloc->LockInfo.Area.right;
        height = pAlloc->LockInfo.Area.bottom - pAlloc->LockInfo.Area.top;
    }
    else
    {
        width = pAlloc->SurfDesc.width;
        height = pAlloc->SurfDesc.height;
    }

    if (pAlloc->LockInfo.fFlags.NotifyOnly)
    {
        pitch = pAlloc->SurfDesc.pitch;
        pvData = ((uint8_t*)pAlloc->pvMem) + pitch*pRect->top + ((bpp*pRect->left) >> 3);
    }
    else
    {
        pvData = pAlloc->LockInfo.pvData;
    }

    vpoxVDbgDoPrintDumpCmd("Surf Info", pvData, width, height, bpp, pitch);

    if (fBreak)
    {
        Assert(0);
    }
}

VOID vpoxVDbgDoDumpLockUnlockSurfTex(const char * pPrefix, const VPOXWDDMDISP_ALLOCATION *pAlloc, const char * pSuffix, DWORD fFlags)
{
    Assert(!pAlloc->hSharedHandle);

    RECT Rect;
    const RECT *pRect;
    Assert(!pAlloc->LockInfo.fFlags.RangeValid);
    Assert(!pAlloc->LockInfo.fFlags.BoxValid);
    if (pAlloc->LockInfo.fFlags.AreaValid)
    {
        pRect = &pAlloc->LockInfo.Area;
    }
    else
    {
        Rect.top = 0;
        Rect.bottom = pAlloc->SurfDesc.height;
        Rect.left = 0;
        Rect.right = pAlloc->SurfDesc.width;
        pRect = &Rect;
    }

    VPOXVDBG_DUMP_INFO Info;
    Info.fFlags = fFlags;
    Info.pAlloc = pAlloc;
    Info.pD3DRc = NULL;
    Info.pRect = pRect;
    vpoxVDbgDoDumpPerform(pPrefix, &Info, pSuffix, vpoxVDbgLockUnlockSurfTexContentsDumperCb, NULL);
}

VOID vpoxVDbgDoDumpLockSurfTex(const char * pPrefix, const D3DDDIARG_LOCK* pData, const char * pSuffix, DWORD fFlags)
{
    const VPOXWDDMDISP_RESOURCE *pRc = (const VPOXWDDMDISP_RESOURCE*)pData->hResource;
    const VPOXWDDMDISP_ALLOCATION *pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
#ifdef VPOXWDDMDISP_DEBUG
    VPOXWDDMDISP_ALLOCATION *pUnconstpAlloc = (VPOXWDDMDISP_ALLOCATION *)pAlloc;
    pUnconstpAlloc->LockInfo.pvData = pData->pSurfData;
#endif
    vpoxVDbgDoDumpLockUnlockSurfTex(pPrefix, pAlloc, pSuffix, fFlags);
}

VOID vpoxVDbgDoDumpUnlockSurfTex(const char * pPrefix, const D3DDDIARG_UNLOCK* pData, const char * pSuffix, DWORD fFlags)
{
    const VPOXWDDMDISP_RESOURCE *pRc = (const VPOXWDDMDISP_RESOURCE*)pData->hResource;
    const VPOXWDDMDISP_ALLOCATION *pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    vpoxVDbgDoDumpLockUnlockSurfTex(pPrefix, pAlloc, pSuffix, fFlags);
}

BOOL vpoxVDbgDoCheckLRects(D3DLOCKED_RECT *pDstLRect, const RECT *pDstRect, D3DLOCKED_RECT *pSrcLRect, const RECT *pSrcRect, DWORD bpp, BOOL fBreakOnMismatch)
{
    LONG DstH, DstW, SrcH, SrcW, DstWBytes;
    BOOL fMatch = FALSE;
    DstH = pDstRect->bottom - pDstRect->top;
    DstW = pDstRect->right - pDstRect->left;
    SrcH = pSrcRect->bottom - pSrcRect->top;
    SrcW = pSrcRect->right - pSrcRect->left;

    DstWBytes = ((DstW * bpp + 7) >> 3);

    if(DstW != SrcW && DstH != SrcH)
    {
        WARN(("stretched comparison not supported!!"));
        return FALSE;
    }

    uint8_t *pDst = (uint8_t*)pDstLRect->pBits;
    uint8_t *pSrc = (uint8_t*)pSrcLRect->pBits;
    for (LONG i = 0; i < DstH; ++i)
    {
        if (!(fMatch = !memcmp(pDst, pSrc, DstWBytes)))
        {
            vpoxVDbgPrint(("not match!\n"));
            if (fBreakOnMismatch)
                Assert(0);
            break;
        }
        pDst += pDstLRect->Pitch;
        pSrc += pSrcLRect->Pitch;
    }
    return fMatch;
}

BOOL vpoxVDbgDoCheckRectsMatch(const VPOXWDDMDISP_RESOURCE *pDstRc, uint32_t iDstAlloc,
                            const VPOXWDDMDISP_RESOURCE *pSrcRc, uint32_t iSrcAlloc,
                            const RECT *pDstRect,
                            const RECT *pSrcRect,
                            BOOL fBreakOnMismatch)
{
    BOOL fMatch = FALSE;
    RECT DstRect = {0}, SrcRect = {0};
    if (!pDstRect)
    {
        DstRect.left = 0;
        DstRect.right = pDstRc->aAllocations[iDstAlloc].SurfDesc.width;
        DstRect.top = 0;
        DstRect.bottom = pDstRc->aAllocations[iDstAlloc].SurfDesc.height;
        pDstRect = &DstRect;
    }

    if (!pSrcRect)
    {
        SrcRect.left = 0;
        SrcRect.right = pSrcRc->aAllocations[iSrcAlloc].SurfDesc.width;
        SrcRect.top = 0;
        SrcRect.bottom = pSrcRc->aAllocations[iSrcAlloc].SurfDesc.height;
        pSrcRect = &SrcRect;
    }

    if (pDstRc == pSrcRc
            && iDstAlloc == iSrcAlloc)
    {
        if (!memcmp(pDstRect, pSrcRect, sizeof (*pDstRect)))
        {
            vpoxVDbgPrint(("matching same rect of one allocation, skipping..\n"));
            return TRUE;
        }
        WARN(("matching different rects of the same allocation, unsupported!"));
        return FALSE;
    }

    if (pDstRc->RcDesc.enmFormat != pSrcRc->RcDesc.enmFormat)
    {
        WARN(("matching different formats, unsupported!"));
        return FALSE;
    }

    DWORD bpp = pDstRc->aAllocations[iDstAlloc].SurfDesc.bpp;
    if (!bpp)
    {
        WARN(("uninited bpp! unsupported!"));
        return FALSE;
    }

    LONG DstH, DstW, SrcH, SrcW;
    DstH = pDstRect->bottom - pDstRect->top;
    DstW = pDstRect->right - pDstRect->left;
    SrcH = pSrcRect->bottom - pSrcRect->top;
    SrcW = pSrcRect->right - pSrcRect->left;

    if(DstW != SrcW && DstH != SrcH)
    {
        WARN(("stretched comparison not supported!!"));
        return FALSE;
    }

    D3DLOCKED_RECT SrcLRect, DstLRect;
    HRESULT hr = VPoxD3DIfLockRect((VPOXWDDMDISP_RESOURCE *)pDstRc, iDstAlloc, &DstLRect, pDstRect, D3DLOCK_READONLY);
    if (FAILED(hr))
    {
        WARN(("VPoxD3DIfLockRect failed, hr(0x%x)", hr));
        return FALSE;
    }

    hr = VPoxD3DIfLockRect((VPOXWDDMDISP_RESOURCE *)pSrcRc, iSrcAlloc, &SrcLRect, pSrcRect, D3DLOCK_READONLY);
    if (FAILED(hr))
    {
        WARN(("VPoxD3DIfLockRect failed, hr(0x%x)", hr));
        hr = VPoxD3DIfUnlockRect((VPOXWDDMDISP_RESOURCE *)pDstRc, iDstAlloc);
        return FALSE;
    }

    fMatch = vpoxVDbgDoCheckLRects(&DstLRect, pDstRect, &SrcLRect, pSrcRect, bpp, fBreakOnMismatch);

    hr = VPoxD3DIfUnlockRect((VPOXWDDMDISP_RESOURCE *)pDstRc, iDstAlloc);
    Assert(hr == S_OK);

    hr = VPoxD3DIfUnlockRect((VPOXWDDMDISP_RESOURCE *)pSrcRc, iSrcAlloc);
    Assert(hr == S_OK);

    return fMatch;
}

void vpoxVDbgDoPrintAlloc(const char * pPrefix, const VPOXWDDMDISP_RESOURCE *pRc, uint32_t iAlloc, const char * pSuffix)
{
    Assert(pRc->cAllocations > iAlloc);
    const VPOXWDDMDISP_ALLOCATION *pAlloc = &pRc->aAllocations[iAlloc];
    BOOL bPrimary = pRc->RcDesc.fFlags.Primary;
    BOOL bFrontBuf = FALSE;
    vpoxVDbgPrint(("%s d3dWidth(%d), width(%d), height(%d), format(%d), usage(%s), %s", pPrefix,
            pAlloc->SurfDesc.d3dWidth, pAlloc->SurfDesc.width, pAlloc->SurfDesc.height, pAlloc->SurfDesc.format,
            bPrimary ?
                    (bFrontBuf ? "Front Buffer" : "Back Buffer")
                    : "?Everage? Alloc",
            pSuffix));
}

void vpoxVDbgDoPrintRect(const char * pPrefix, const RECT *pRect, const char * pSuffix)
{
    vpoxVDbgPrint(("%s left(%d), top(%d), right(%d), bottom(%d) %s", pPrefix, pRect->left, pRect->top, pRect->right, pRect->bottom, pSuffix));
}

static VOID CALLBACK vpoxVDbgTimerCb(__in PVOID lpParameter, __in BOOLEAN TimerOrWaitFired)
{
    RT_NOREF(lpParameter, TimerOrWaitFired);
    Assert(0);
}

HRESULT vpoxVDbgTimerStart(HANDLE hTimerQueue, HANDLE *phTimer, DWORD msTimeout)
{
    if (!CreateTimerQueueTimer(phTimer, hTimerQueue,
                               vpoxVDbgTimerCb,
                               NULL,
                               msTimeout, /* ms*/
                               0,
                               WT_EXECUTEONLYONCE))
    {
        DWORD winEr = GetLastError();
        AssertMsgFailed(("CreateTimerQueueTimer failed, winEr (%d)\n", winEr));
        return E_FAIL;
    }
    return S_OK;
}

HRESULT vpoxVDbgTimerStop(HANDLE hTimerQueue, HANDLE hTimer)
{
    if (!DeleteTimerQueueTimer(hTimerQueue, hTimer, NULL))
    {
        DWORD winEr = GetLastError();
        AssertMsg(winEr == ERROR_IO_PENDING, ("DeleteTimerQueueTimer failed, winEr (%d)\n", winEr));
    }
    return S_OK;
}
#endif

#if defined(VPOXWDDMDISP_DEBUG)
BOOL vpoxVDbgDoCheckExe(const char * pszName)
{
    char *pszModule = vpoxVDbgDoGetModuleName();
    if (!pszModule)
        return FALSE;
    size_t cbModule, cbName;
    cbModule = strlen(pszModule);
    cbName = strlen(pszName);
    if (cbName > cbModule)
        return FALSE;
    if (_stricmp(pszName, pszModule + (cbModule - cbName)))
        return FALSE;
    return TRUE;
}
#endif

#ifdef VPOXWDDMDISP_DEBUG_VEHANDLER

typedef BOOL WINAPI FNGetModuleInformation(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb);
typedef FNGetModuleInformation *PFNGetModuleInformation;

static PFNGetModuleInformation g_pfnGetModuleInformation = NULL;
static HMODULE g_hModPsapi = NULL;
static PVOID g_VPoxWDbgVEHandler = NULL;

static bool vpoxVDbgIsAddressInModule(PVOID pv, const char *pszModuleName)
{
    HMODULE hMod = GetModuleHandleA(pszModuleName);
    if (!hMod)
        return false;

    HANDLE hProcess = GetCurrentProcess();

    if (!g_pfnGetModuleInformation)
        return false;

    MODULEINFO ModuleInfo = {0};
    if (!g_pfnGetModuleInformation(hProcess, hMod, &ModuleInfo, sizeof(ModuleInfo)))
        return false;

    return    (uintptr_t)ModuleInfo.lpBaseOfDll <= (uintptr_t)pv
           && (uintptr_t)pv < (uintptr_t)ModuleInfo.lpBaseOfDll + ModuleInfo.SizeOfImage;
}

static bool vpoxVDbgIsExceptionIgnored(PEXCEPTION_RECORD pExceptionRecord)
{
    /* Module (dll) names for GetModuleHandle.
     * Exceptions originated from these modules will be ignored.
     */
    static const char *apszIgnoredModuleNames[] =
    {
        NULL
    };

    int i = 0;
    while (apszIgnoredModuleNames[i])
    {
        if (vpoxVDbgIsAddressInModule(pExceptionRecord->ExceptionAddress, apszIgnoredModuleNames[i]))
            return true;

        ++i;
    }

    return false;
}

LONG WINAPI vpoxVDbgVectoredHandler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    static volatile bool g_fAllowIgnore = true; /* Might be changed in kernel debugger. */

    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;
    /* PCONTEXT pContextRecord = pExceptionInfo->ContextRecord; */

    switch (pExceptionRecord->ExceptionCode)
    {
        default:
            break;
        case EXCEPTION_BREAKPOINT:
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            if (g_fAllowIgnore && vpoxVDbgIsExceptionIgnored(pExceptionRecord))
                break;
            ASMBreakpoint();
            break;
        case 0x40010006: /* OutputDebugStringA? */
        case 0x4001000a: /* OutputDebugStringW? */
            break;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void vpoxVDbgVEHandlerRegister()
{
    Assert(!g_VPoxWDbgVEHandler);
    g_VPoxWDbgVEHandler = AddVectoredExceptionHandler(1,vpoxVDbgVectoredHandler);
    Assert(g_VPoxWDbgVEHandler);

    g_hModPsapi = GetModuleHandleA("Psapi.dll"); /* Usually already loaded. */
    if (g_hModPsapi)
        g_pfnGetModuleInformation = (PFNGetModuleInformation)GetProcAddress(g_hModPsapi, "GetModuleInformation");
}

void vpoxVDbgVEHandlerUnregister()
{
    Assert(g_VPoxWDbgVEHandler);
    ULONG uResult = RemoveVectoredExceptionHandler(g_VPoxWDbgVEHandler);
    Assert(uResult); RT_NOREF(uResult);
    g_VPoxWDbgVEHandler = NULL;

    g_hModPsapi = NULL;
    g_pfnGetModuleInformation = NULL;
}

#endif /* VPOXWDDMDISP_DEBUG_VEHANDLER */

#if defined(VPOXWDDMDISP_DEBUG) || defined(LOG_TO_BACKDOOR_DRV)
void vpoxDispLogDrvF(char * szString, ...)
{
    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    vpoxDispLogDbgFormatStringV(szBuffer, sizeof (szBuffer), szString, pArgList);
    va_end(pArgList);

    VPoxDispMpLoggerLog(szBuffer);
}

void vpoxDispLogDbgPrintF(char * szString, ...)
{
    char szBuffer[4096] = { 0 };
    va_list pArgList;
    va_start(pArgList, szString);
    vpoxDispLogDbgFormatStringV(szBuffer, sizeof(szBuffer), szString, pArgList);
    va_end(pArgList);

    OutputDebugStringA(szBuffer);
}
#endif
