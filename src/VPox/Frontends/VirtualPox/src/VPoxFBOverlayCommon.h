/* $Id: VPoxFBOverlayCommon.h $ */
/** @file
 * VPox Qt GUI - VPoxFrameBuffer Overlay classes declarations.
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

#ifndef FEQT_INCLUDED_SRC_VPoxFBOverlayCommon_h
#define FEQT_INCLUDED_SRC_VPoxFBOverlayCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if 0 //defined(DEBUG_misha)
DECLINLINE(VOID) vpoxDbgPrintF(LPCSTR szString, ...)
{
    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szString, pArgList);
    va_end(pArgList);

    OutputDebugStringA(szBuffer);
}

# include "iprt/stream.h"
# define VPOXQGLLOG(_m) RTPrintf _m
# define VPOXQGLLOGREL(_m) do { RTPrintf _m ; LogRel( _m ); } while(0)
# define VPOXQGLDBGPRINT(_m) vpoxDbgPrintF _m
#else
# define VPOXQGLLOG(_m)    do {}while(0)
# define VPOXQGLLOGREL(_m) LogRel( _m )
# define VPOXQGLDBGPRINT(_m) do {}while(0)
#endif
#define VPOXQGLLOG_ENTER(_m) do {}while(0)
//do{VPOXQGLLOG(("==>[%s]:", __FUNCTION__)); VPOXQGLLOG(_m);}while(0)
#define VPOXQGLLOG_EXIT(_m) do {}while(0)
//do{VPOXQGLLOG(("<==[%s]:", __FUNCTION__)); VPOXQGLLOG(_m);}while(0)
#ifdef DEBUG
 #define VPOXQGL_ASSERTNOERR() \
    do { GLenum err = glGetError(); \
        if(err != GL_NO_ERROR) VPOXQGLLOG(("gl error occurred (0x%x)\n", err)); \
        Assert(err == GL_NO_ERROR); \
    }while(0)

 #define VPOXQGL_CHECKERR(_op) \
    do { \
        glGetError(); \
        _op \
        VPOXQGL_ASSERTNOERR(); \
    }while(0)
#else
 #define VPOXQGL_ASSERTNOERR() \
    do {}while(0)

 #define VPOXQGL_CHECKERR(_op) \
    do { \
        _op \
    }while(0)
#endif

#ifdef DEBUG
#include <iprt/time.h>

#define VPOXGETTIME() RTTimeNanoTS()

#define VPOXPRINTDIF(_nano, _m) do{\
        uint64_t cur = VPOXGETTIME(); NOREF(cur); \
        VPOXQGLLOG(_m); \
        VPOXQGLLOG(("(%Lu)\n", cur - (_nano))); \
    }while(0)

class VPoxVHWADbgTimeCounter
{
public:
    VPoxVHWADbgTimeCounter(const char* msg) {mTime = VPOXGETTIME(); mMsg=msg;}
    ~VPoxVHWADbgTimeCounter() {VPOXPRINTDIF(mTime, (mMsg));}
private:
    uint64_t mTime;
    const char* mMsg;
};

#define VPOXQGLLOG_METHODTIME(_m) VPoxVHWADbgTimeCounter _dbgTimeCounter(_m)

#define VPOXQG_CHECKCONTEXT() \
        { \
            const GLubyte * str; \
            VPOXQGL_CHECKERR(   \
                    str = glGetString(GL_VERSION); \
            ); \
            Assert(str); \
            if(str) \
            { \
                Assert(str[0]); \
            } \
        }
#else
#define VPOXQGLLOG_METHODTIME(_m)
#define VPOXQG_CHECKCONTEXT() do{}while(0)
#endif

#define VPOXQGLLOG_QRECT(_p, _pr, _s) do{\
    VPOXQGLLOG((_p " x(%d), y(%d), w(%d), h(%d)" _s, (_pr)->x(), (_pr)->y(), (_pr)->width(), (_pr)->height()));\
    }while(0)

#define VPOXQGLLOG_CKEY(_p, _pck, _s) do{\
    VPOXQGLLOG((_p " l(0x%x), u(0x%x)" _s, (_pck)->lower(), (_pck)->upper()));\
    }while(0)

#endif /* !FEQT_INCLUDED_SRC_VPoxFBOverlayCommon_h */

