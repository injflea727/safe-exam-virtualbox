/* $Id: VPoxDispDbg.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispDbg_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispDbg_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOX_VIDEO_LOG_NAME "VPoxD3D"
#define VPOX_VIDEO_LOG_LOGGER vpoxVDbgInternalLogLogger
#define VPOX_VIDEO_LOGREL_LOGGER vpoxVDbgInternalLogRelLogger
#define VPOX_VIDEO_LOGFLOW_LOGGER vpoxVDbgInternalLogFlowLogger
#define VPOX_VIDEO_LOG_FN_FMT "%s"

#include "../../common/VPoxVideoLog.h"

#ifdef DEBUG
/* debugging configuration flags */

/* Adds vectored exception handler to be able to catch non-debug UM exceptions in kernel debugger. */
#define VPOXWDDMDISP_DEBUG_VEHANDLER

/* generic debugging facilities & extra data checks */
# define VPOXWDDMDISP_DEBUG
/* for some reason when debugging with VirtualKD, user-mode DbgPrint's are discarded
 * the workaround so far is to pass the log info to the kernel driver and DbgPrint'ed from there,
 * which is enabled by this define */
//#  define VPOXWDDMDISP_DEBUG_PRINTDRV

/* Uncomment to use OutputDebugString */
//#define VPOXWDDMDISP_DEBUG_PRINT

/* disable shared resource creation with wine */
//#  define VPOXWDDMDISP_DEBUG_NOSHARED

//#  define VPOXWDDMDISP_DEBUG_PRINT_SHARED_CREATE
//#  define VPOXWDDMDISP_DEBUG_TIMER

/* debug config vars */
extern DWORD g_VPoxVDbgFDumpSetTexture;
extern DWORD g_VPoxVDbgFDumpDrawPrim;
extern DWORD g_VPoxVDbgFDumpTexBlt;
extern DWORD g_VPoxVDbgFDumpBlt;
extern DWORD g_VPoxVDbgFDumpRtSynch;
extern DWORD g_VPoxVDbgFDumpFlush;
extern DWORD g_VPoxVDbgFDumpShared;
extern DWORD g_VPoxVDbgFDumpLock;
extern DWORD g_VPoxVDbgFDumpUnlock;
extern DWORD g_VPoxVDbgFDumpPresentEnter;
extern DWORD g_VPoxVDbgFDumpPresentLeave;
extern DWORD g_VPoxVDbgFDumpScSync;

extern DWORD g_VPoxVDbgFBreakShared;
extern DWORD g_VPoxVDbgFBreakDdi;

extern DWORD g_VPoxVDbgFCheckSysMemSync;
extern DWORD g_VPoxVDbgFCheckBlt;
extern DWORD g_VPoxVDbgFCheckTexBlt;
extern DWORD g_VPoxVDbgFCheckScSync;

extern DWORD g_VPoxVDbgFSkipCheckTexBltDwmWndUpdate;

extern DWORD g_VPoxVDbgCfgMaxDirectRts;
extern DWORD g_VPoxVDbgCfgForceDummyDevCreate;

extern struct VPOXWDDMDISP_DEVICE *g_VPoxVDbgInternalDevice;
extern struct VPOXWDDMDISP_RESOURCE *g_VPoxVDbgInternalRc;

#endif

#if defined(VPOXWDDMDISP_DEBUG)
/* log enable flags */
extern DWORD g_VPoxVDbgFLogRel;
extern DWORD g_VPoxVDbgFLog;
extern DWORD g_VPoxVDbgFLogFlow;
#endif

#ifdef VPOXWDDMDISP_DEBUG_VEHANDLER
void vpoxVDbgVEHandlerRegister();
void vpoxVDbgVEHandlerUnregister();
#endif

#if defined(LOG_TO_BACKDOOR_DRV) || defined(VPOXWDDMDISP_DEBUG_PRINTDRV)
# define DbgPrintDrv(_m) do { vpoxDispLogDrvF _m; } while (0)
# define DbgPrintDrvRel(_m) do { vpoxDispLogDrvF _m; } while (0)
# define DbgPrintDrvFlow(_m) do { vpoxDispLogDrvF _m; } while (0)
#else
# define DbgPrintDrv(_m) do { } while (0)
# define DbgPrintDrvRel(_m) do { } while (0)
# define DbgPrintDrvFlow(_m) do { } while (0)
#endif

#ifdef VPOXWDDMDISP_DEBUG_PRINT
# define DbgPrintUsr(_m) do { vpoxDispLogDbgPrintF _m; } while (0)
# define DbgPrintUsrRel(_m) do { vpoxDispLogDbgPrintF _m; } while (0)
# define DbgPrintUsrFlow(_m) do { vpoxDispLogDbgPrintF _m; } while (0)
#else
# define DbgPrintUsr(_m) do { } while (0)
# define DbgPrintUsrRel(_m) do { } while (0)
# define DbgPrintUsrFlow(_m) do { } while (0)
#endif

#if defined(VPOXWDDMDISP_DEBUG)
#define vpoxVDbgInternalLog(_p) if (g_VPoxVDbgFLog) { _p }
#define vpoxVDbgInternalLogFlow(_p) if (g_VPoxVDbgFLogFlow) { _p }
#define vpoxVDbgInternalLogRel(_p) if (g_VPoxVDbgFLogRel) { _p }
#else
#define vpoxVDbgInternalLog(_p) do {} while (0)
#define vpoxVDbgInternalLogFlow(_p) do {} while (0)
#define vpoxVDbgInternalLogRel(_p) do { _p } while (0)
#endif

/* @todo: remove these from the code and from here */
#define vpoxVDbgPrint(_m) LOG_EXACT(_m)
#define vpoxVDbgPrintF(_m) LOGF_EXACT(_m)
#define vpoxVDbgPrintR(_m)  LOGREL_EXACT(_m)

#define vpoxVDbgInternalLogLogger(_m) do { \
        vpoxVDbgInternalLog( \
            Log(_m); \
            DbgPrintUsr(_m); \
            DbgPrintDrv(_m); \
        ); \
    } while (0)

#define vpoxVDbgInternalLogFlowLogger(_m)  do { \
        vpoxVDbgInternalLogFlow( \
            LogFlow(_m); \
            DbgPrintUsrFlow(_m); \
            DbgPrintDrvFlow(_m); \
        ); \
    } while (0)

#define vpoxVDbgInternalLogRelLogger(_m)  do { \
        vpoxVDbgInternalLogRel( \
            LogRel(_m); \
            DbgPrintUsrRel(_m); \
            DbgPrintDrvRel(_m); \
        ); \
    } while (0)

#if defined(VPOXWDDMDISP_DEBUG)
extern DWORD g_VPoxVDbgPid;
extern LONG g_VPoxVDbgFIsDwm;
#define VPOXVDBG_CHECK_EXE(_pszName) (vpoxVDbgDoCheckExe(_pszName))
#define VPOXVDBG_IS_DWM() (!!(g_VPoxVDbgFIsDwm >=0 ? g_VPoxVDbgFIsDwm : (g_VPoxVDbgFIsDwm = VPOXVDBG_CHECK_EXE("dwm.exe"))))
BOOL vpoxVDbgDoCheckExe(const char * pszName);
#endif
#if defined(VPOXWDDMDISP_DEBUG) || defined(LOG_TO_BACKDOOR_DRV)

#define VPOXVDBG_STRCASE(_t) \
        case _t: return #_t;
#define VPOXVDBG_STRCASE_UNKNOWN() \
        default: Assert(0); return "Unknown";

DECLINLINE(const char*) vpoxDispLogD3DRcType(D3DRESOURCETYPE enmType)
{
    switch (enmType)
    {
        VPOXVDBG_STRCASE(D3DRTYPE_SURFACE);
        VPOXVDBG_STRCASE(D3DRTYPE_VOLUME);
        VPOXVDBG_STRCASE(D3DRTYPE_TEXTURE);
        VPOXVDBG_STRCASE(D3DRTYPE_VOLUMETEXTURE);
        VPOXVDBG_STRCASE(D3DRTYPE_CUBETEXTURE);
        VPOXVDBG_STRCASE(D3DRTYPE_VERTEXBUFFER);
        VPOXVDBG_STRCASE(D3DRTYPE_INDEXBUFFER);
        VPOXVDBG_STRCASE_UNKNOWN();
    }
}

#include <VPoxDispMpLogger.h>

VPOXDISPMPLOGGER_DECL(void) VPoxDispMpLoggerDumpD3DCAPS9(struct _D3DCAPS9 *pCaps);

void vpoxDispLogDrvF(char * szString, ...);

# define vpoxDispDumpD3DCAPS9(_pCaps) do { VPoxDispMpLoggerDumpD3DCAPS9(_pCaps); } while (0)
#else
# define vpoxDispDumpD3DCAPS9(_pCaps) do { } while (0)
#endif

#ifdef VPOXWDDMDISP_DEBUG

void vpoxDispLogDbgPrintF(char * szString, ...);

typedef struct VPOXWDDMDISP_ALLOCATION *PVPOXWDDMDISP_ALLOCATION;
typedef struct VPOXWDDMDISP_RESOURCE *PVPOXWDDMDISP_RESOURCE;

#define VPOXVDBG_DUMP_TYPEF_FLOW                   0x00000001
#define VPOXVDBG_DUMP_TYPEF_CONTENTS               0x00000002
#define VPOXVDBG_DUMP_TYPEF_DONT_BREAK_ON_CONTENTS 0x00000004
#define VPOXVDBG_DUMP_TYPEF_BREAK_ON_FLOW          0x00000008
#define VPOXVDBG_DUMP_TYPEF_SHARED_ONLY            0x00000010

#define VPOXVDBG_DUMP_FLAGS_IS_SETANY(_fFlags, _Value) (((_fFlags) & (_Value)) != 0)
#define VPOXVDBG_DUMP_FLAGS_IS_SET(_fFlags, _Value) (((_fFlags) & (_Value)) == (_Value))
#define VPOXVDBG_DUMP_FLAGS_IS_CLEARED(_fFlags, _Value) (((_fFlags) & (_Value)) == 0)
#define VPOXVDBG_DUMP_FLAGS_CLEAR(_fFlags, _Value) ((_fFlags) & (~(_Value)))
#define VPOXVDBG_DUMP_FLAGS_SET(_fFlags, _Value) ((_fFlags) | (_Value))

#define VPOXVDBG_DUMP_TYPE_ENABLED(_fFlags) (VPOXVDBG_DUMP_FLAGS_IS_SETANY(_fFlags, VPOXVDBG_DUMP_TYPEF_FLOW | VPOXVDBG_DUMP_TYPEF_CONTENTS))
#define VPOXVDBG_DUMP_TYPE_ENABLED_FOR_INFO(_pInfo, _fFlags) ( \
        VPOXVDBG_DUMP_TYPE_ENABLED(_fFlags) \
        && ( \
                VPOXVDBG_DUMP_FLAGS_IS_CLEARED(_fFlags, VPOXVDBG_DUMP_TYPEF_SHARED_ONLY) \
                || ((_pInfo)->pAlloc && (_pInfo)->pAlloc->pRc->aAllocations[0].hSharedHandle) \
            ))

#define VPOXVDBG_DUMP_TYPE_FLOW_ONLY(_fFlags) (VPOXVDBG_DUMP_FLAGS_IS_SET(_fFlags, VPOXVDBG_DUMP_TYPEF_FLOW) \
        && VPOXVDBG_DUMP_FLAGS_IS_CLEARED(_fFlags, VPOXVDBG_DUMP_TYPEF_CONTENTS))
#define VPOXVDBG_DUMP_TYPE_CONTENTS(_fFlags) (VPOXVDBG_DUMP_FLAGS_IS_SET(_fFlags, VPOXVDBG_DUMP_TYPEF_CONTENTS))
#define VPOXVDBG_DUMP_TYPE_GET_FLOW_ONLY(_fFlags) ( \
        VPOXVDBG_DUMP_FLAGS_SET( \
                VPOXVDBG_DUMP_FLAGS_CLEAR(_fFlags, VPOXVDBG_DUMP_TYPEF_CONTENTS), \
                VPOXVDBG_DUMP_TYPEF_FLOW) \
        )

VOID vpoxVDbgDoDumpAllocRect(const char * pPrefix, PVPOXWDDMDISP_ALLOCATION pAlloc, RECT *pRect, const char* pSuffix, DWORD fFlags);
VOID vpoxVDbgDoDumpRcRect(const char * pPrefix, PVPOXWDDMDISP_ALLOCATION pAlloc, IDirect3DResource9 *pD3DRc, RECT *pRect, const char * pSuffix, DWORD fFlags);
VOID vpoxVDbgDoDumpLockUnlockSurfTex(const char * pPrefix, const VPOXWDDMDISP_ALLOCATION *pAlloc, const char * pSuffix, DWORD fFlags);
VOID vpoxVDbgDoDumpRt(const char * pPrefix, struct VPOXWDDMDISP_DEVICE *pDevice, const char * pSuffix, DWORD fFlags);
VOID vpoxVDbgDoDumpSamplers(const char * pPrefix, struct VPOXWDDMDISP_DEVICE *pDevice, const char * pSuffix, DWORD fFlags);

void vpoxVDbgDoPrintRect(const char * pPrefix, const RECT *pRect, const char * pSuffix);
void vpoxVDbgDoPrintAlloc(const char * pPrefix, const VPOXWDDMDISP_RESOURCE *pRc, uint32_t iAlloc, const char * pSuffix);

VOID vpoxVDbgDoDumpLockSurfTex(const char * pPrefix, const D3DDDIARG_LOCK* pData, const char * pSuffix, DWORD fFlags);
VOID vpoxVDbgDoDumpUnlockSurfTex(const char * pPrefix, const D3DDDIARG_UNLOCK* pData, const char * pSuffix, DWORD fFlags);

BOOL vpoxVDbgDoCheckRectsMatch(const VPOXWDDMDISP_RESOURCE *pDstRc, uint32_t iDstAlloc,
                            const VPOXWDDMDISP_RESOURCE *pSrcRc, uint32_t iSrcAlloc,
                            const RECT *pDstRect,
                            const RECT *pSrcRect,
                            BOOL fBreakOnMismatch);

VOID vpoxVDbgDoPrintLopLastCmd(const char* pszDesc);

HRESULT vpoxVDbgTimerStart(HANDLE hTimerQueue, HANDLE *phTimer, DWORD msTimeout);
HRESULT vpoxVDbgTimerStop(HANDLE hTimerQueue, HANDLE hTimer);

#define VPOXVDBG_IS_PID(_pid) ((_pid) == (g_VPoxVDbgPid ? g_VPoxVDbgPid : (g_VPoxVDbgPid = GetCurrentProcessId())))
#define VPOXVDBG_IS_DUMP_ALLOWED_PID(_pid) (((int)(_pid)) > 0 ? VPOXVDBG_IS_PID(_pid) : !VPOXVDBG_IS_PID(-((int)(_pid))))

#define VPOXVDBG_ASSERT_IS_DWM(_bDwm) do { \
        Assert((!VPOXVDBG_IS_DWM()) == (!(_bDwm))); \
    } while (0)

#define VPOXVDBG_DUMP_FLAGS_FOR_TYPE(_type) g_VPoxVDbgFDump##_type
#define VPOXVDBG_BREAK_FLAGS_FOR_TYPE(_type) g_VPoxVDbgFBreak##_type
#define VPOXVDBG_CHECK_FLAGS_FOR_TYPE(_type) g_VPoxVDbgFCheck##_type
#define VPOXVDBG_IS_DUMP_ALLOWED(_type) ( VPOXVDBG_DUMP_TYPE_ENABLED(VPOXVDBG_DUMP_FLAGS_FOR_TYPE(_type)) )

#define VPOXVDBG_IS_BREAK_ALLOWED(_type) ( !!VPOXVDBG_BREAK_FLAGS_FOR_TYPE(_type) )

#define VPOXVDBG_IS_CHECK_ALLOWED(_type) ( !!VPOXVDBG_CHECK_FLAGS_FOR_TYPE(_type) )

#define VPOXVDBG_IS_DUMP_SHARED_ALLOWED(_pRc) (\
        (_pRc)->RcDesc.fFlags.SharedResource \
        && VPOXVDBG_IS_DUMP_ALLOWED(Shared) \
        )

#define VPOXVDBG_IS_BREAK_SHARED_ALLOWED(_pRc) (\
        (_pRc)->RcDesc.fFlags.SharedResource \
        && VPOXVDBG_IS_BREAK_ALLOWED(Shared) \
        )

#define VPOXVDBG_BREAK_SHARED(_pRc) do { \
        if (VPOXVDBG_IS_BREAK_SHARED_ALLOWED(_pRc)) { \
            vpoxVDbgPrint(("Break on shared access: Rc(0x%p), SharedHandle(0x%p)\n", (_pRc), (_pRc)->aAllocations[0].hSharedHandle)); \
            AssertFailed(); \
        } \
    } while (0)

#define VPOXVDBG_BREAK_DDI() do { \
        if (VPOXVDBG_IS_BREAK_ALLOWED(Ddi)) { \
            AssertFailed(); \
        } \
    } while (0)

#define VPOXVDBG_LOOP_LAST() do { vpoxVDbgLoop = 0; } while (0)

#define VPOXVDBG_LOOP(_op) do { \
        DWORD vpoxVDbgLoop = 1; \
        do { \
            _op; \
        } while (vpoxVDbgLoop); \
    } while (0)

#define VPOXVDBG_CHECK_SMSYNC(_pRc) do { \
        if (VPOXVDBG_IS_CHECK_ALLOWED(SysMemSync)) { \
            vpoxWddmDbgRcSynchMemCheck((_pRc)); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_RECTS_INIT(_d) DWORD vpoxVDbgDumpRects = _d; NOREF(vpoxVDbgDumpRects)
#define VPOXVDBG_DUMP_RECTS_FORCE() vpoxVDbgDumpRects = 1;
#define VPOXVDBG_DUMP_RECTS_FORCED() (!!vpoxVDbgDumpRects)

#define VPOXVDBG_CHECK_RECTS(_opRests, _opDump, _pszOpName, _pDstRc, _iDstAlloc, _pSrcRc, _iSrcAlloc, _pDstRect, _pSrcRect) do { \
        VPOXVDBG_LOOP(\
                VPOXVDBG_DUMP_RECTS_INIT(0); \
                _opRests; \
                if (vpoxVDbgDoCheckRectsMatch(_pDstRc, _iDstAlloc, _pSrcRc, _iSrcAlloc, _pDstRect, _pSrcRect, FALSE)) { \
                    VPOXVDBG_LOOP_LAST(); \
                } \
                else \
                { \
                    VPOXVDBG_DUMP_RECTS_FORCE(); \
                    vpoxVDbgPrint(("vpoxVDbgDoCheckRectsMatch failed! The " _pszOpName " will be re-done so it can be debugged\n")); \
                    vpoxVDbgDoPrintLopLastCmd("Don't redo the" _pszOpName); \
                    Assert(0); \
                } \
                _opDump; \
         ); \
    } while (0)

#define VPOXVDBG_DEV_CHECK_SHARED(_pDevice, _pIsShared) do { \
        *(_pIsShared) = FALSE; \
        for (UINT i = 0; i < (_pDevice)->cRTs; ++i) { \
            PVPOXWDDMDISP_ALLOCATION pRtVar = (_pDevice)->apRTs[i]; \
            if (pRtVar && pRtVar->pRc->RcDesc.fFlags.SharedResource) { *(_pIsShared) = TRUE; break; } \
        } \
        if (!*(_pIsShared)) { \
            for (UINT i = 0, iSampler = 0; iSampler < (_pDevice)->cSamplerTextures; ++i) { \
                Assert(i < RT_ELEMENTS((_pDevice)->aSamplerTextures)); \
                if (!(_pDevice)->aSamplerTextures[i]) continue; \
                ++iSampler; \
                if (!(_pDevice)->aSamplerTextures[i]->RcDesc.fFlags.SharedResource) continue; \
                *(_pIsShared) = TRUE; break; \
            } \
        } \
    } while (0)

#define VPOXVDBG_IS_DUMP_SHARED_ALLOWED_DEV(_pDevice, _pIsAllowed) do { \
        VPOXVDBG_DEV_CHECK_SHARED(_pDevice, _pIsAllowed); \
        if (*(_pIsAllowed)) \
        { \
            *(_pIsAllowed) = VPOXVDBG_IS_DUMP_ALLOWED(Shared); \
        } \
    } while (0)

#define VPOXVDBG_IS_BREAK_SHARED_ALLOWED_DEV(_pDevice, _pIsAllowed) do { \
        VPOXVDBG_DEV_CHECK_SHARED(_pDevice, _pIsAllowed); \
        if (*(_pIsAllowed)) \
        { \
            *(_pIsAllowed) = VPOXVDBG_IS_BREAK_ALLOWED(Shared); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_DRAWPRIM_ENTER(_pDevice) do { \
        BOOL fDumpShaded = FALSE; \
        VPOXVDBG_IS_DUMP_SHARED_ALLOWED_DEV(_pDevice, &fDumpShaded); \
        if (fDumpShaded \
                || VPOXVDBG_IS_DUMP_ALLOWED(DrawPrim)) \
        { \
            vpoxVDbgDoDumpRt("==>"__FUNCTION__": Rt: ", (_pDevice), "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(DrawPrim)); \
            vpoxVDbgDoDumpSamplers("==>"__FUNCTION__": Sl: ", (_pDevice), "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(DrawPrim)); \
        }\
    } while (0)

#define VPOXVDBG_DUMP_DRAWPRIM_LEAVE(_pDevice) do { \
        BOOL fDumpShaded = FALSE; \
        VPOXVDBG_IS_DUMP_SHARED_ALLOWED_DEV(_pDevice, &fDumpShaded); \
        if (fDumpShaded \
                || VPOXVDBG_IS_DUMP_ALLOWED(DrawPrim)) \
        { \
            vpoxVDbgDoDumpRt("<=="__FUNCTION__": Rt: ", (_pDevice), "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(DrawPrim)); \
            vpoxVDbgDoDumpSamplers("<=="__FUNCTION__": Sl: ", (_pDevice), "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(DrawPrim)); \
        }\
    } while (0)

#define VPOXVDBG_BREAK_SHARED_DEV(_pDevice)  do { \
        BOOL fBreakShaded = FALSE; \
        VPOXVDBG_IS_BREAK_SHARED_ALLOWED_DEV(_pDevice, &fBreakShaded); \
        if (fBreakShaded) { \
            vpoxVDbgPrint((__FUNCTION__"== Break on shared access\n")); \
            AssertFailed(); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_SETTEXTURE(_pRc) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(SetTexture) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED(_pRc) \
                ) \
        { \
            vpoxVDbgDoDumpRcRect("== "__FUNCTION__": ", &(_pRc)->aAllocations[0], NULL, NULL, "", \
                    VPOXVDBG_DUMP_FLAGS_FOR_TYPE(SetTexture) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared)); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_TEXBLT_ENTER(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(TexBlt) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED(_pSrcRc) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED(_pDstRc) \
                ) \
        { \
            RECT SrcRect = *(_pSrcRect); \
            RECT _DstRect; \
            vpoxWddmRectMoved(&_DstRect, &SrcRect, (_pDstPoint)->x, (_pDstPoint)->y); \
            vpoxVDbgDoDumpRcRect("==> "__FUNCTION__": Src: ", &(_pSrcRc)->aAllocations[0], NULL, &SrcRect, "", \
                    VPOXVDBG_DUMP_FLAGS_FOR_TYPE(TexBlt) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared)); \
            vpoxVDbgDoDumpRcRect("==> "__FUNCTION__": Dst: ", &(_pDstRc)->aAllocations[0], NULL, &_DstRect, "", \
                    VPOXVDBG_DUMP_FLAGS_FOR_TYPE(TexBlt) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared)); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_TEXBLT_LEAVE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { \
        if (VPOXVDBG_DUMP_RECTS_FORCED() \
                || VPOXVDBG_IS_DUMP_ALLOWED(TexBlt) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED(_pSrcRc) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED(_pDstRc) \
                ) \
        { \
            RECT SrcRect = *(_pSrcRect); \
            RECT _DstRect; \
            vpoxWddmRectMoved(&_DstRect, &SrcRect, (_pDstPoint)->x, (_pDstPoint)->y); \
            vpoxVDbgDoDumpRcRect("<== "__FUNCTION__": Src: ", &(_pSrcRc)->aAllocations[0], NULL, &SrcRect, "", \
                    VPOXVDBG_DUMP_FLAGS_FOR_TYPE(TexBlt) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared)); \
            vpoxVDbgDoDumpRcRect("<== "__FUNCTION__": Dst: ", &(_pDstRc)->aAllocations[0], NULL, &_DstRect, "", \
                    VPOXVDBG_DUMP_FLAGS_FOR_TYPE(TexBlt) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared)); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_STRETCH_RECT(_type, _str, _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(_type) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED((_pSrcAlloc)->pRc) \
                || VPOXVDBG_IS_DUMP_SHARED_ALLOWED((_pDstAlloc)->pRc) \
                ) \
        { \
            DWORD fFlags = VPOXVDBG_DUMP_FLAGS_FOR_TYPE(_type) | VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Shared); \
            if (VPOXVDBG_DUMP_TYPE_CONTENTS(fFlags) && \
                    ((_pSrcSurf) == (_pDstSurf) \
                    && ( ((_pSrcRect) && (_pDstRect) && !memcmp((_pSrcRect), (_pDstRect), sizeof (_pDstRect))) \
                            || ((_pSrcRect) == (_pDstRect)) \
                            )) ) \
            { \
                vpoxVDbgPrint((_str #_type ": skipping content dump of the same rect for one surfcace\n")); \
                fFlags = VPOXVDBG_DUMP_TYPE_GET_FLOW_ONLY(fFlags); \
            } \
            RECT Rect, *pRect; \
            if (_pSrcRect) \
            { \
                Rect = *((RECT*)(_pSrcRect)); \
                pRect = &Rect; \
            } \
            else \
                pRect = NULL; \
            vpoxVDbgDoDumpRcRect(_str __FUNCTION__" Src: ", (_pSrcAlloc), (_pSrcSurf), pRect, "", fFlags); \
            if (_pDstRect) \
            { \
                Rect = *((RECT*)(_pDstRect)); \
                pRect = &Rect; \
            } \
            else \
                pRect = NULL; \
            vpoxVDbgDoDumpRcRect(_str __FUNCTION__" Dst: ", (_pDstAlloc), (_pDstSurf), pRect, "", fFlags); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_BLT_ENTER(_pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) \
    VPOXVDBG_DUMP_STRETCH_RECT(Blt, "==>", _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect)

#define VPOXVDBG_DUMP_BLT_LEAVE(_pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) \
        VPOXVDBG_DUMP_STRETCH_RECT(Blt, "<==", _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect)

#define VPOXVDBG_IS_SKIP_DWM_WND_UPDATE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) ( \
            g_VPoxVDbgFSkipCheckTexBltDwmWndUpdate \
            && ( \
                VPOXVDBG_IS_DWM() \
                && (_pSrcRc)->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM \
                && (_pSrcRc)->RcDesc.enmFormat == D3DDDIFMT_A8R8G8B8 \
                && (_pSrcRc)->cAllocations == 1 \
                && (_pDstRc)->RcDesc.enmPool == D3DDDIPOOL_VIDEOMEMORY \
                && (_pDstRc)->RcDesc.enmFormat == D3DDDIFMT_A8R8G8B8 \
                && (_pDstRc)->RcDesc.fFlags.RenderTarget \
                && (_pDstRc)->RcDesc.fFlags.NotLockable \
                && (_pDstRc)->cAllocations == 1 \
                && (_pSrcRc)->aAllocations[0].SurfDesc.width == (_pDstRc)->aAllocations[0].SurfDesc.width \
                && (_pSrcRc)->aAllocations[0].SurfDesc.height == (_pDstRc)->aAllocations[0].SurfDesc.height \
            ) \
        )

#define VPOXVDBG_CHECK_TEXBLT(_opTexBlt, _pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { \
        if (VPOXVDBG_IS_CHECK_ALLOWED(TexBlt)) { \
            if (VPOXVDBG_IS_SKIP_DWM_WND_UPDATE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint)) \
            { \
                vpoxVDbgPrint(("TEXBLT: skipping check for dwm wnd update\n")); \
            } \
            else \
            { \
                RECT DstRect; \
                DstRect.left = (_pDstPoint)->x; \
                DstRect.right = (_pDstPoint)->x + (_pSrcRect)->right - (_pSrcRect)->left; \
                DstRect.top = (_pDstPoint)->y; \
                DstRect.bottom = (_pDstPoint)->y + (_pSrcRect)->bottom - (_pSrcRect)->top; \
                VPOXVDBG_CHECK_RECTS(\
                        VPOXVDBG_DUMP_TEXBLT_ENTER(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint); \
                        _opTexBlt ,\
                        VPOXVDBG_DUMP_TEXBLT_LEAVE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint), \
                        "TexBlt", \
                        _pDstRc, 0, _pSrcRc, 0, &DstRect, _pSrcRect); \
                break; \
            } \
        } \
        VPOXVDBG_DUMP_RECTS_INIT(0); \
        VPOXVDBG_DUMP_TEXBLT_ENTER(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint); \
        _opTexBlt;\
        VPOXVDBG_DUMP_TEXBLT_LEAVE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint); \
    } while (0)

#define VPOXVDBG_CHECK_STRETCH_RECT(_type, _op, _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) do { \
        if (VPOXVDBG_IS_CHECK_ALLOWED(_type)) { \
            VPOXVDBG_CHECK_RECTS(\
                    VPOXVDBG_DUMP_STRETCH_RECT(_type, "==>", _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect); \
                    _op ,\
                    VPOXVDBG_DUMP_STRETCH_RECT(_type, "<==", _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect), \
                    #_type , \
                    _pDstAlloc->pRc, _pDstAlloc->iAlloc, _pSrcAlloc->pRc, _pSrcAlloc->iAlloc, _pDstRect, _pSrcRect); \
        } \
        else \
        { \
            VPOXVDBG_DUMP_RECTS_INIT(0); \
            VPOXVDBG_DUMP_STRETCH_RECT(_type, "==>", _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect); \
            _op;\
            VPOXVDBG_DUMP_STRETCH_RECT(_type, "<==", _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect); \
        } \
    } while (0)

#define VPOXVDBG_CHECK_BLT(_opBlt, _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) \
        VPOXVDBG_CHECK_STRETCH_RECT(Blt, _opBlt, _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect)

#define VPOXVDBG_DUMP_SYNC_RT(_pBbSurf) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(RtSynch)) \
        { \
            vpoxVDbgDoDumpRcRect("== "__FUNCTION__" Bb:\n", NULL, (_pBbSurf), NULL, "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(RtSynch)); \
        } \
    } while (0)


#define VPOXVDBG_DUMP_FLUSH(_pDevice) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(Flush)) \
        { \
            vpoxVDbgDoDumpRt("== "__FUNCTION__": Rt: ", (_pDevice), "", \
                    VPOXVDBG_DUMP_FLAGS_CLEAR(VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Flush), VPOXVDBG_DUMP_TYPEF_SHARED_ONLY)); \
        }\
    } while (0)

#define VPOXVDBG_DUMP_LOCK_ST(_pData) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(Lock) \
                || VPOXVDBG_IS_DUMP_ALLOWED(Unlock) \
                ) \
        { \
            vpoxVDbgDoDumpLockSurfTex("== "__FUNCTION__": ", (_pData), "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Lock)); \
        } \
    } while (0)

#define VPOXVDBG_DUMP_UNLOCK_ST(_pData) do { \
        if (VPOXVDBG_IS_DUMP_ALLOWED(Unlock) \
                ) \
        { \
            vpoxVDbgDoDumpUnlockSurfTex("== "__FUNCTION__": ", (_pData), "", VPOXVDBG_DUMP_FLAGS_FOR_TYPE(Unlock)); \
        } \
    } while (0)

#else
#define VPOXVDBG_DUMP_DRAWPRIM_ENTER(_pDevice) do { } while (0)
#define VPOXVDBG_DUMP_DRAWPRIM_LEAVE(_pDevice) do { } while (0)
#define VPOXVDBG_DUMP_SETTEXTURE(_pRc) do { } while (0)
#define VPOXVDBG_DUMP_TEXBLT_ENTER(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { } while (0)
#define VPOXVDBG_DUMP_TEXBLT_LEAVE(_pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { } while (0)
#define VPOXVDBG_DUMP_BLT_ENTER(_pSrcRc, _pSrcSurf, _pSrcRect, _pDstRc, _pDstSurf, _pDstRect) do { } while (0)
#define VPOXVDBG_DUMP_BLT_LEAVE(_pSrcRc, _pSrcSurf, _pSrcRect, _pDstRc, _pDstSurf, _pDstRect) do { } while (0)
#define VPOXVDBG_DUMP_SYNC_RT(_pBbSurf) do { } while (0)
#define VPOXVDBG_DUMP_FLUSH(_pDevice) do { } while (0)
#define VPOXVDBG_DUMP_LOCK_ST(_pData) do { } while (0)
#define VPOXVDBG_DUMP_UNLOCK_ST(_pData) do { } while (0)
#define VPOXVDBG_BREAK_SHARED(_pRc) do { } while (0)
#define VPOXVDBG_BREAK_SHARED_DEV(_pDevice) do { } while (0)
#define VPOXVDBG_BREAK_DDI() do { } while (0)
#define VPOXVDBG_CHECK_SMSYNC(_pRc) do { } while (0)
#define VPOXVDBG_CHECK_BLT(_opBlt, _pSrcAlloc, _pSrcSurf, _pSrcRect, _pDstAlloc, _pDstSurf, _pDstRect) do { _opBlt; } while (0)
#define VPOXVDBG_CHECK_TEXBLT(_opTexBlt, _pSrcRc, _pSrcRect, _pDstRc, _pDstPoint) do { _opTexBlt; } while (0)
#define VPOXVDBG_ASSERT_IS_DWM(_bDwm) do { } while (0)
#endif


#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispDbg_h */
