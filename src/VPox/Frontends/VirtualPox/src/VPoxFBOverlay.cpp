/* $Id: VPoxFBOverlay.cpp $ */
/** @file
 * VPox Qt GUI - VPoxFBOverlay implementation.
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

#if defined(VPOX_GUI_USE_QGL) /* entire file */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUI

/* Qt includes: */
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h> /* QGLWidget drags in Windows.h; -Wall forces us to use wrapper. */
# include <iprt/stdint.h>      /* QGLWidget drags in stdint.h; -Wall forces us to use wrapper. */
#endif
#include <QApplication>
#include <QGLWidget>
#include <QFile>
#include <QTextStream>

/* GUI includes: */
#include "VPoxFBOverlay.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIPopupCenter.h"
#include "UICommon.h"

/* COM includes: */
#include "CSession.h"
#include "CConsole.h"
#include "CMachine.h"
#include "CDisplay.h"

/* Other VPox includes: */
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <VPox/AssertGuest.h>

#include <VPox/VPoxGL2D.h>

#ifdef VPOX_WS_MAC
#include "VPoxUtils-darwin.h"
#endif

/* Other VPox includes: */
#include <iprt/memcache.h>
#include <VPox/err.h>

#ifdef VPOX_WITH_VIDEOHWACCEL
# include <VPoxVideo.h>
# include <VPox/vmm/ssm.h>
#endif

/* Other includes: */
#include <math.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* 128 should be enough */
#define VPOXVHWA_MAX_SURFACES 128
#define VPOXVHWA_MAX_WIDTH    4096
#define VPOXVHWA_MAX_HEIGHT   4096

#ifdef VPOXQGL_PROF_BASE
# ifdef VPOXQGL_DBG_SURF
#  define VPOXQGL_PROF_WIDTH 1400
#  define VPOXQGL_PROF_HEIGHT 1050
# else
# define VPOXQGL_PROF_WIDTH 1400
# define VPOXQGL_PROF_HEIGHT 1050
//#define VPOXQGL_PROF_WIDTH 720
//#define VPOXQGL_PROF_HEIGHT 480
# endif
#endif

#define VPOXQGL_STATE_NAMEBASE "QGLVHWAData"
#define VPOXQGL_STATE_VERSION_PIPESAVED    3
#define VPOXQGL_STATE_VERSION              3

//#define VPOXQGLOVERLAY_STATE_NAMEBASE "QGLOverlayVHWAData"
//#define VPOXQGLOVERLAY_STATE_VERSION 1

#ifdef DEBUG_misha
//# define VPOXQGL_STATE_DEBUG
#endif

#ifdef VPOXQGL_STATE_DEBUG
#define VPOXQGL_STATE_START_MAGIC        0x12345678
#define VPOXQGL_STATE_STOP_MAGIC         0x87654321

#define VPOXQGL_STATE_SURFSTART_MAGIC    0x9abcdef1
#define VPOXQGL_STATE_SURFSTOP_MAGIC     0x1fedcba9

#define VPOXQGL_STATE_OVERLAYSTART_MAGIC 0x13579bdf
#define VPOXQGL_STATE_OVERLAYSTOP_MAGIC  0xfdb97531

#define VPOXQGL_SAVE_START(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VPOXQGL_STATE_START_MAGIC); AssertRC(rc);}while(0)
#define VPOXQGL_SAVE_STOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VPOXQGL_STATE_STOP_MAGIC); AssertRC(rc);}while(0)

#define VPOXQGL_SAVE_SURFSTART(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VPOXQGL_STATE_SURFSTART_MAGIC); AssertRC(rc);}while(0)
#define VPOXQGL_SAVE_SURFSTOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VPOXQGL_STATE_SURFSTOP_MAGIC); AssertRC(rc);}while(0)

#define VPOXQGL_SAVE_OVERLAYSTART(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VPOXQGL_STATE_OVERLAYSTART_MAGIC); AssertRC(rc);}while(0)
#define VPOXQGL_SAVE_OVERLAYSTOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VPOXQGL_STATE_OVERLAYSTOP_MAGIC); AssertRC(rc);}while(0)

#define VPOXQGL_LOAD_CHECK(_pSSM, _v) \
    do{ \
        uint32_t _u32; \
        int rcCheck = SSMR3GetU32(_pSSM, &_u32); AssertRC(rcCheck); \
        if (_u32 != (_v)) \
        { \
            VPOXQGLLOG(("load error: expected magic (0x%x), but was (0x%x)\n", (_v), _u32));\
        }\
        Assert(_u32 == (_v)); \
    }while(0)

#define VPOXQGL_LOAD_START(_pSSM) VPOXQGL_LOAD_CHECK(_pSSM, VPOXQGL_STATE_START_MAGIC)
#define VPOXQGL_LOAD_STOP(_pSSM) VPOXQGL_LOAD_CHECK(_pSSM, VPOXQGL_STATE_STOP_MAGIC)

#define VPOXQGL_LOAD_SURFSTART(_pSSM) VPOXQGL_LOAD_CHECK(_pSSM, VPOXQGL_STATE_SURFSTART_MAGIC)
#define VPOXQGL_LOAD_SURFSTOP(_pSSM) VPOXQGL_LOAD_CHECK(_pSSM, VPOXQGL_STATE_SURFSTOP_MAGIC)

#define VPOXQGL_LOAD_OVERLAYSTART(_pSSM) VPOXQGL_LOAD_CHECK(_pSSM, VPOXQGL_STATE_OVERLAYSTART_MAGIC)
#define VPOXQGL_LOAD_OVERLAYSTOP(_pSSM) VPOXQGL_LOAD_CHECK(_pSSM, VPOXQGL_STATE_OVERLAYSTOP_MAGIC)

#else

#define VPOXQGL_SAVE_START(_pSSM) do{}while(0)
#define VPOXQGL_SAVE_STOP(_pSSM) do{}while(0)

#define VPOXQGL_SAVE_SURFSTART(_pSSM) do{}while(0)
#define VPOXQGL_SAVE_SURFSTOP(_pSSM) do{}while(0)

#define VPOXQGL_SAVE_OVERLAYSTART(_pSSM) do{}while(0)
#define VPOXQGL_SAVE_OVERLAYSTOP(_pSSM) do{}while(0)

#define VPOXQGL_LOAD_START(_pSSM) do{}while(0)
#define VPOXQGL_LOAD_STOP(_pSSM) do{}while(0)

#define VPOXQGL_LOAD_SURFSTART(_pSSM) do{}while(0)
#define VPOXQGL_LOAD_SURFSTOP(_pSSM) do{}while(0)

#define VPOXQGL_LOAD_OVERLAYSTART(_pSSM) do{}while(0)
#define VPOXQGL_LOAD_OVERLAYSTOP(_pSSM) do{}while(0)

#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static VPoxVHWAInfo g_VPoxVHWASupportInfo;


#ifdef DEBUG

VPoxVHWADbgTimer::VPoxVHWADbgTimer(uint32_t cPeriods)
    : mPeriodSum(0)
    , mPrevTime(0)
    , mcFrames(0)
    , mcPeriods(cPeriods)
    , miPeriod(0)
{
    mpaPeriods = new uint64_t[cPeriods];
    memset(mpaPeriods, 0, cPeriods * sizeof(mpaPeriods[0]));
}

VPoxVHWADbgTimer::~VPoxVHWADbgTimer()
{
    delete[] mpaPeriods;
}

void VPoxVHWADbgTimer::frame()
{
    uint64_t cur = VPOXGETTIME();
    if (mPrevTime)
    {
        uint64_t curPeriod = cur - mPrevTime;
        mPeriodSum += curPeriod - mpaPeriods[miPeriod];
        mpaPeriods[miPeriod] = curPeriod;
        ++miPeriod;
        miPeriod %= mcPeriods;
    }
    mPrevTime = cur;
    ++mcFrames;
}

#endif /* DEBUG */


class VPoxVHWAEntriesCache
{
public:
    VPoxVHWAEntriesCache()
    {
        int rc = RTMemCacheCreate(&mVPoxCmdEntryCache, sizeof (VPoxVHWACommandElement),
                                    0, /* size_t cbAlignment */
                                    UINT32_MAX, /* uint32_t cMaxObjects */
                                    NULL, /* PFNMEMCACHECTOR pfnCtor*/
                                    NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                                    NULL, /* void *pvUser*/
                                    0 /* uint32_t fFlags*/
                                    );
        AssertRC(rc);
    }

    ~VPoxVHWAEntriesCache()
    {
        RTMemCacheDestroy(mVPoxCmdEntryCache);
    }

    VPoxVHWACommandElement * alloc()
    {
        return (VPoxVHWACommandElement*)RTMemCacheAlloc(mVPoxCmdEntryCache);
    }

    void free(VPoxVHWACommandElement * pEl)
    {
        RTMemCacheFree(mVPoxCmdEntryCache, pEl);
    }

private:
    RTMEMCACHE mVPoxCmdEntryCache;
};

static struct VPOXVHWACMD * vhwaHHCmdCreate(VPOXVHWACMD_TYPE type, size_t size)
{
    char *buf = (char *)malloc(VPOXVHWACMD_SIZE_FROMBODYSIZE(size));
    memset(buf, 0, size);
    VPOXVHWACMD *pCmd = (VPOXVHWACMD *)buf;
    pCmd->enmCmd = type;
    pCmd->Flags = VPOXVHWACMD_FLAG_HH_CMD;
    return pCmd;
}

static const VPoxVHWAInfo & vpoxVHWAGetSupportInfo(const QGLContext *pContext)
{
    if (!g_VPoxVHWASupportInfo.isInitialized())
    {
        if (pContext)
        {
            g_VPoxVHWASupportInfo.init(pContext);
        }
        else
        {
            VPoxGLTmpContext ctx;
            const QGLContext *pContext = ctx.makeCurrent();
            Assert(pContext);
            if (pContext)
            {
                g_VPoxVHWASupportInfo.init(pContext);
            }
        }
    }
    return g_VPoxVHWASupportInfo;
}

class VPoxVHWACommandProcessEvent : public QEvent
{
public:
    VPoxVHWACommandProcessEvent ()
        : QEvent ((QEvent::Type) VHWACommandProcessType),
          fProcessed(false)
    {
#ifdef DEBUG_misha
        g_EventCounter.inc();
#endif
    }

    void setProcessed()
    {
        fProcessed = true;
    }

    ~VPoxVHWACommandProcessEvent()
    {
        if (!fProcessed)
        {
            AssertMsgFailed(("VHWA command beinf destroyed unproceessed!"));
            LogRel(("GUI: VHWA command being destroyed unproceessed!"));
        }
#ifdef DEBUG_misha
        g_EventCounter.dec();
#endif
    }
#ifdef DEBUG_misha
    static uint32_t cPending() { return g_EventCounter.refs(); }
#endif

private:
    bool fProcessed;
#ifdef DEBUG_misha
    static VPoxVHWARefCounter g_EventCounter;
#endif
};

#ifdef DEBUG_misha
VPoxVHWARefCounter VPoxVHWACommandProcessEvent::g_EventCounter;
#endif

VPoxVHWAHandleTable::VPoxVHWAHandleTable(uint32_t maxSize)
    :
    mcSize(maxSize),
    mcUsage(0),
    mCursor(1) /* 0 is treated as invalid */
{
    mTable = (void **)RTMemAllocZ(sizeof(void *) * maxSize);
}

VPoxVHWAHandleTable::~VPoxVHWAHandleTable()
{
    RTMemFree(mTable);
}

uint32_t VPoxVHWAHandleTable::put(void *data)
{
    AssertPtrReturn(data, VPOXVHWA_SURFHANDLE_INVALID);
    AssertReturn(mcUsage < mcSize, VPOXVHWA_SURFHANDLE_INVALID);

    for (int k = 0; k < 2; ++k)
    {
        Assert(mCursor != 0);
        for (uint32_t i = mCursor; i < mcSize; ++i)
        {
            if (!mTable[i])
            {
                doPut(i, data);
                mCursor = i+1;
                return i;
            }
        }
        mCursor = 1; /* 0 is treated as invalid */
    }

    AssertFailed();
    return VPOXVHWA_SURFHANDLE_INVALID;
}

bool VPoxVHWAHandleTable::mapPut(uint32_t h, void *data)
{
    AssertReturn(h > 0 && h < mcSize, false);
    RT_UNTRUSTED_VALIDATED_FENCE();
    if (mTable[h])
        return false;
    doPut(h, data);
    return true;
}

void * VPoxVHWAHandleTable::get(uint32_t h)
{
    AssertReturn(h > 0 && h < mcSize, NULL);
    RT_UNTRUSTED_VALIDATED_FENCE();
    return mTable[h];
}

void * VPoxVHWAHandleTable::remove(uint32_t h)
{
    Assert(mcUsage);
    AssertReturn(h > 0 && h < mcSize, NULL);
    RT_UNTRUSTED_VALIDATED_FENCE();
    void *val = mTable[h];
    Assert(val);
    if (val)
    {
        doRemove(h);
    }
    return val;
}

void VPoxVHWAHandleTable::doPut(uint32_t h, void * data)
{
    ++mcUsage;
    mTable[h] = data;
}

void VPoxVHWAHandleTable::doRemove(uint32_t h)
{
    mTable[h] = 0;
    --mcUsage;
}

static VPoxVHWATextureImage *vpoxVHWAImageCreate(const QRect & aRect, const VPoxVHWAColorFormat & aFormat,
                                                 class VPoxVHWAGlProgramMngr * pMgr, VPOXVHWAIMG_TYPE flags)
{
    bool bCanLinearNonFBO = false;
    if (!aFormat.fourcc())
    {
        flags &= ~VPOXVHWAIMG_FBO;
        bCanLinearNonFBO = true;
    }

    const VPoxVHWAInfo & info = vpoxVHWAGetSupportInfo(NULL);
    if ((flags & VPOXVHWAIMG_PBO) && !info.getGlInfo().isPBOSupported())
        flags &= ~VPOXVHWAIMG_PBO;

    if ((flags & VPOXVHWAIMG_PBOIMG) &&
            (!info.getGlInfo().isPBOSupported() || !info.getGlInfo().isPBOOffsetSupported()))
        flags &= ~VPOXVHWAIMG_PBOIMG;

    if ((flags & VPOXVHWAIMG_FBO) && !info.getGlInfo().isFBOSupported())
        flags &= ~VPOXVHWAIMG_FBO;

    /* ensure we don't create a PBO-based texture in case we use a PBO-based image */
    if (flags & VPOXVHWAIMG_PBOIMG)
        flags &= ~VPOXVHWAIMG_PBO;

    if (flags & VPOXVHWAIMG_FBO)
    {
        if (flags & VPOXVHWAIMG_PBOIMG)
        {
            VPOXQGLLOG(("FBO PBO Image\n"));
            return new VPoxVHWATextureImageFBO<VPoxVHWATextureImagePBO>(aRect, aFormat, pMgr, flags);
        }
        VPOXQGLLOG(("FBO Generic Image\n"));
        return new VPoxVHWATextureImageFBO<VPoxVHWATextureImage>(aRect, aFormat, pMgr, flags);
    }

    if (!bCanLinearNonFBO)
    {
        VPOXQGLLOG(("Disabling Linear stretching\n"));
        flags &= ~VPOXVHWAIMG_LINEAR;
    }

    if (flags & VPOXVHWAIMG_PBOIMG)
    {
        VPOXQGLLOG(("PBO Image\n"));
        return new VPoxVHWATextureImagePBO(aRect, aFormat, pMgr, flags);
    }

    VPOXQGLLOG(("Generic Image\n"));
    return new VPoxVHWATextureImage(aRect, aFormat, pMgr, flags);
}

static VPoxVHWATexture* vpoxVHWATextureCreate(const QGLContext * pContext, const QRect & aRect,
                                              const VPoxVHWAColorFormat & aFormat, uint32_t bytesPerLine, VPOXVHWAIMG_TYPE flags)
{
    const VPoxVHWAInfo & info = vpoxVHWAGetSupportInfo(pContext);
    GLint scaleFunc = (flags & VPOXVHWAIMG_LINEAR) ? GL_LINEAR : GL_NEAREST;
    if ((flags & VPOXVHWAIMG_PBO) && info.getGlInfo().isPBOSupported())
    {
        VPOXQGLLOG(("VPoxVHWATextureNP2RectPBO\n"));
        return new VPoxVHWATextureNP2RectPBO(aRect, aFormat, bytesPerLine, scaleFunc);
    }
    else if (info.getGlInfo().isTextureRectangleSupported())
    {
        VPOXQGLLOG(("VPoxVHWATextureNP2Rect\n"));
        return new VPoxVHWATextureNP2Rect(aRect, aFormat, bytesPerLine, scaleFunc);
    }
    else if (info.getGlInfo().isTextureNP2Supported())
    {
        VPOXQGLLOG(("VPoxVHWATextureNP2\n"));
        return new VPoxVHWATextureNP2(aRect, aFormat, bytesPerLine, scaleFunc);
    }
    VPOXQGLLOG(("VPoxVHWATexture\n"));
    return new VPoxVHWATexture(aRect, aFormat, bytesPerLine, scaleFunc);
}

class VPoxVHWAGlShaderComponent
{
public:
    VPoxVHWAGlShaderComponent(const char *aRcName, GLenum aType)
        : mRcName(aRcName)
        , mType(aType)
        , mInitialized(false)
    { NOREF(mType); }


    int init();

    const char * contents() { return mSource.constData(); }
    bool isInitialized() { return mInitialized; }
private:
    const char *mRcName;
    GLenum mType;
    QByteArray mSource;
    bool mInitialized;
};

int VPoxVHWAGlShaderComponent::init()
{
    if (isInitialized())
        return VINF_ALREADY_INITIALIZED;

    QFile fi(mRcName);
    if (!fi.open(QIODevice::ReadOnly))
    {
        AssertFailed();
        return VERR_GENERAL_FAILURE;
    }

    QTextStream is(&fi);
    QString program = is.readAll();

    mSource = program.toUtf8();

    mInitialized = true;
    return VINF_SUCCESS;
}

class VPoxVHWAGlShader
{
public:
    VPoxVHWAGlShader() :
        mType(GL_FRAGMENT_SHADER),
        mcComponents(0)
    {}

    VPoxVHWAGlShader & operator= (const VPoxVHWAGlShader & other)
    {
        mcComponents = other.mcComponents;
        mType = other.mType;
        if (mcComponents)
        {
            maComponents = new VPoxVHWAGlShaderComponent*[mcComponents];
            memcpy(maComponents, other.maComponents, mcComponents * sizeof(maComponents[0]));
        }
        return *this;
    }

    VPoxVHWAGlShader(const VPoxVHWAGlShader & other)
    {
        mcComponents = other.mcComponents;
        mType = other.mType;
        if (mcComponents)
        {
            maComponents = new VPoxVHWAGlShaderComponent*[mcComponents];
            memcpy(maComponents, other.maComponents, mcComponents * sizeof(maComponents[0]));
        }
    }

    VPoxVHWAGlShader(GLenum aType, VPoxVHWAGlShaderComponent ** aComponents, int cComponents)
        : mType(aType)
    {
        maComponents = new VPoxVHWAGlShaderComponent*[cComponents];
        mcComponents = cComponents;
        memcpy(maComponents, aComponents, cComponents * sizeof(maComponents[0]));
    }

    ~VPoxVHWAGlShader() {delete[] maComponents;}
    int init();
    GLenum type() { return mType; }
    GLuint shader() { return mShader; }
private:
    GLenum mType;
    GLuint mShader;
    VPoxVHWAGlShaderComponent ** maComponents;
    int mcComponents;
};

int VPoxVHWAGlShader::init()
{
    int rc = VERR_GENERAL_FAILURE;
    GLint *length;
    const char **sources;
    length = new GLint[mcComponents];
    sources = new const char*[mcComponents];
    for (int i = 0; i < mcComponents; i++)
    {
        length[i] = -1;
        rc = maComponents[i]->init();
        AssertRC(rc);
        if (RT_FAILURE(rc))
            break;
        sources[i] = maComponents[i]->contents();
    }

    if (RT_SUCCESS(rc))
    {
#ifdef DEBUG
        VPOXQGLLOG(("\ncompiling shaders:\n------------\n"));
        for (int i = 0; i < mcComponents; i++)
            VPOXQGLLOG(("**********\n%s\n***********\n", sources[i]));
        VPOXQGLLOG(("------------\n"));
#endif
        mShader = vpoxglCreateShader(mType);

        VPOXQGL_CHECKERR(
                vpoxglShaderSource(mShader, mcComponents, sources, length);
                );

        VPOXQGL_CHECKERR(
                vpoxglCompileShader(mShader);
                );

        GLint compiled;
        VPOXQGL_CHECKERR(
                vpoxglGetShaderiv(mShader, GL_COMPILE_STATUS, &compiled);
                );

#ifdef DEBUG
        GLchar * pBuf = new GLchar[16300];
        vpoxglGetShaderInfoLog(mShader, 16300, NULL, pBuf);
        VPOXQGLLOG(("\ncompile log:\n-----------\n%s\n---------\n", pBuf));
        delete[] pBuf;
#endif

        Assert(compiled);
        if (compiled)
        {
            rc = VINF_SUCCESS;
        }
        else
        {
            VPOXQGL_CHECKERR(
                    vpoxglDeleteShader(mShader);
                    );
            mShader = 0;
        }
    }

    delete[] length;
    delete[] sources;
    return rc;
}

class VPoxVHWAGlProgram
{
public:
    VPoxVHWAGlProgram(VPoxVHWAGlShader ** apShaders, int acShaders);

    virtual ~VPoxVHWAGlProgram();

    virtual int init();
    virtual void uninit();
    virtual int start();
    virtual int stop();
    bool isInitialized() { return mProgram; }
    GLuint program() {return mProgram;}
private:
    GLuint mProgram;
    VPoxVHWAGlShader *mShaders;
    int mcShaders;
};

VPoxVHWAGlProgram::VPoxVHWAGlProgram(VPoxVHWAGlShader ** apShaders, int acShaders) :
       mProgram(0),
       mcShaders(0)
{
    Assert(acShaders);
    if (acShaders)
    {
        mShaders = new VPoxVHWAGlShader[acShaders];
        for (int i = 0; i < acShaders; i++)
        {
            mShaders[i] = *apShaders[i];
        }
        mcShaders = acShaders;
    }
}

VPoxVHWAGlProgram::~VPoxVHWAGlProgram()
{
    uninit();

    if (mShaders)
    {
        delete[] mShaders;
    }
}

int VPoxVHWAGlProgram::init()
{
    Assert(!isInitialized());
    if (isInitialized())
        return VINF_ALREADY_INITIALIZED;

    Assert(mcShaders);
    if (!mcShaders)
        return VERR_GENERAL_FAILURE;

    int rc = VINF_SUCCESS;
    for (int i = 0; i < mcShaders; i++)
    {
        int rc = mShaders[i].init();
        AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            break;
        }
    }
    if (RT_FAILURE(rc))
    {
        return rc;
    }

    mProgram = vpoxglCreateProgram();
    Assert(mProgram);
    if (mProgram)
    {
        for (int i = 0; i < mcShaders; i++)
        {
            VPOXQGL_CHECKERR(
                    vpoxglAttachShader(mProgram, mShaders[i].shader());
                    );
        }

        VPOXQGL_CHECKERR(
                vpoxglLinkProgram(mProgram);
                );


        GLint linked;
        vpoxglGetProgramiv(mProgram, GL_LINK_STATUS, &linked);

#ifdef DEBUG
        GLchar * pBuf = new GLchar[16300];
        vpoxglGetProgramInfoLog(mProgram, 16300, NULL, pBuf);
        VPOXQGLLOG(("link log: %s\n", pBuf));
        Assert(linked);
        delete[] pBuf;
#endif

        if (linked)
        {
            return VINF_SUCCESS;
        }

        VPOXQGL_CHECKERR(
                vpoxglDeleteProgram(mProgram);
                );
        mProgram = 0;
    }
    return VERR_GENERAL_FAILURE;
}

void VPoxVHWAGlProgram::uninit()
{
    if (!isInitialized())
        return;

    VPOXQGL_CHECKERR(
            vpoxglDeleteProgram(mProgram);
            );
    mProgram = 0;
}

int VPoxVHWAGlProgram::start()
{
    VPOXQGL_CHECKERR(
            vpoxglUseProgram(mProgram);
            );
    return VINF_SUCCESS;
}

int VPoxVHWAGlProgram::stop()
{
    VPOXQGL_CHECKERR(
            vpoxglUseProgram(0);
            );
    return VINF_SUCCESS;
}

#define VPOXVHWA_PROGRAM_DSTCOLORKEY        0x00000001
#define VPOXVHWA_PROGRAM_SRCCOLORKEY        0x00000002
#define VPOXVHWA_PROGRAM_COLORCONV          0x00000004
#define VPOXVHWA_PROGRAM_COLORKEYNODISCARD  0x00000008

#define VPOXVHWA_SUPPORTED_PROGRAM ( \
        VPOXVHWA_PROGRAM_DSTCOLORKEY \
        | VPOXVHWA_PROGRAM_SRCCOLORKEY \
        | VPOXVHWA_PROGRAM_COLORCONV \
        | VPOXVHWA_PROGRAM_COLORKEYNODISCARD \
        )

class VPoxVHWAGlProgramVHWA : public VPoxVHWAGlProgram
{
public:
    VPoxVHWAGlProgramVHWA(uint32_t type, uint32_t fourcc, VPoxVHWAGlShader ** apShaders, int acShaders);

    uint32_t type() const {return mType;}
    uint32_t fourcc() const {return mFourcc;}

    int setDstCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b);

    int setDstCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b);

    int setSrcCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b);

    int setSrcCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b);


    virtual int init();

    bool matches(uint32_t type, uint32_t fourcc)
    {
        return mType == type && mFourcc == fourcc;
    }

    bool equals(const VPoxVHWAGlProgramVHWA & other)
    {
        return matches(other.mType, other.mFourcc);
    }

private:
    uint32_t mType;
    uint32_t mFourcc;

    GLfloat mDstUpperR, mDstUpperG, mDstUpperB;
    GLint mUniDstUpperColor;

    GLfloat mDstLowerR, mDstLowerG, mDstLowerB;
    GLint mUniDstLowerColor;

    GLfloat mSrcUpperR, mSrcUpperG, mSrcUpperB;
    GLint mUniSrcUpperColor;

    GLfloat mSrcLowerR, mSrcLowerG, mSrcLowerB;
    GLint mUniSrcLowerColor;

    GLint mDstTex;
    GLint mUniDstTex;

    GLint mSrcTex;
    GLint mUniSrcTex;

    GLint mVTex;
    GLint mUniVTex;

    GLint mUTex;
    GLint mUniUTex;
};

VPoxVHWAGlProgramVHWA::VPoxVHWAGlProgramVHWA(uint32_t type, uint32_t fourcc, VPoxVHWAGlShader **apShaders, int acShaders)
    : VPoxVHWAGlProgram(apShaders, acShaders)
    , mType(type)
    , mFourcc(fourcc)
    , mDstUpperR(0.0)
    , mDstUpperG(0.0)
    , mDstUpperB(0.0)
    , mUniDstUpperColor(-1)
    , mDstLowerR(0.0)
    , mDstLowerG(0.0)
    , mDstLowerB(0.0)
    , mUniDstLowerColor(-1)
    , mSrcUpperR(0.0)
    , mSrcUpperG(0.0)
    , mSrcUpperB(0.0)
    , mUniSrcUpperColor(-1)
    , mSrcLowerR(0.0)
    , mSrcLowerG(0.0)
    , mSrcLowerB(0.0)
    , mUniSrcLowerColor(-1)
    , mDstTex(-1)
    , mUniDstTex(-1)
    , mSrcTex(-1)
    , mUniSrcTex(-1)
    , mVTex(-1)
    , mUniVTex(-1)
    , mUTex(-1)
    , mUniUTex(-1)
{}

int VPoxVHWAGlProgramVHWA::init()
{
    int rc = VPoxVHWAGlProgram::init();
    if (RT_FAILURE(rc))
        return rc;
    if (rc == VINF_ALREADY_INITIALIZED)
        return rc;

    start();

    rc = VERR_GENERAL_FAILURE;

    do
    {
        GLint tex = 0;
        mUniSrcTex = vpoxglGetUniformLocation(program(), "uSrcTex");
        Assert(mUniSrcTex != -1);
        if (mUniSrcTex == -1)
            break;

        VPOXQGL_CHECKERR(
                vpoxglUniform1i(mUniSrcTex, tex);
                );
        mSrcTex = tex;
        ++tex;

        if (type() & VPOXVHWA_PROGRAM_SRCCOLORKEY)
        {
            mUniSrcLowerColor = vpoxglGetUniformLocation(program(), "uSrcClr");
            Assert(mUniSrcLowerColor != -1);
            if (mUniSrcLowerColor == -1)
                break;

            mSrcLowerR = 0.0; mSrcLowerG = 0.0; mSrcLowerB = 0.0;

            VPOXQGL_CHECKERR(
                    vpoxglUniform4f(mUniSrcLowerColor, 0.0, 0.0, 0.0, 0.0);
                    );
        }

        if (type() & VPOXVHWA_PROGRAM_COLORCONV)
        {
            switch(fourcc())
            {
                case FOURCC_YV12:
                {
                    mUniVTex = vpoxglGetUniformLocation(program(), "uVTex");
                    Assert(mUniVTex != -1);
                    if (mUniVTex == -1)
                        break;

                    VPOXQGL_CHECKERR(
                            vpoxglUniform1i(mUniVTex, tex);
                            );
                    mVTex = tex;
                    ++tex;

                    mUniUTex = vpoxglGetUniformLocation(program(), "uUTex");
                    Assert(mUniUTex != -1);
                    if (mUniUTex == -1)
                        break;
                    VPOXQGL_CHECKERR(
                            vpoxglUniform1i(mUniUTex, tex);
                            );
                    mUTex = tex;
                    ++tex;

                    break;
                }
                case FOURCC_UYVY:
                case FOURCC_YUY2:
                case FOURCC_AYUV:
                    break;
                default:
                    AssertFailed();
                    break;
            }
        }

        if (type() & VPOXVHWA_PROGRAM_DSTCOLORKEY)
        {

            mUniDstTex = vpoxglGetUniformLocation(program(), "uDstTex");
            Assert(mUniDstTex != -1);
            if (mUniDstTex == -1)
                break;
            VPOXQGL_CHECKERR(
                    vpoxglUniform1i(mUniDstTex, tex);
                    );
            mDstTex = tex;
            ++tex;

            mUniDstLowerColor = vpoxglGetUniformLocation(program(), "uDstClr");
            Assert(mUniDstLowerColor != -1);
            if (mUniDstLowerColor == -1)
                break;

            mDstLowerR = 0.0; mDstLowerG = 0.0; mDstLowerB = 0.0;

            VPOXQGL_CHECKERR(
                    vpoxglUniform4f(mUniDstLowerColor, 0.0, 0.0, 0.0, 0.0);
                    );
        }

        rc = VINF_SUCCESS;
    } while(0);


    stop();
    if (rc == VINF_SUCCESS)
        return VINF_SUCCESS;

    AssertFailed();
    VPoxVHWAGlProgram::uninit();
    return VERR_GENERAL_FAILURE;
}

int VPoxVHWAGlProgramVHWA::setDstCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if (!isInitialized())
        return VERR_GENERAL_FAILURE;
    if (mDstUpperR == r && mDstUpperG == g && mDstUpperB == b)
        return VINF_ALREADY_INITIALIZED;
    vpoxglUniform4f(mUniDstUpperColor, r, g, b, 0.0);
    mDstUpperR = r;
    mDstUpperG = g;
    mDstUpperB = b;
    return VINF_SUCCESS;
}

int VPoxVHWAGlProgramVHWA::setDstCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if (!isInitialized())
        return VERR_GENERAL_FAILURE;
    if (mDstLowerR == r && mDstLowerG == g && mDstLowerB == b)
        return VINF_ALREADY_INITIALIZED;

    VPOXQGL_CHECKERR(
            vpoxglUniform4f(mUniDstLowerColor, r, g, b, 0.0);
            );

    mDstLowerR = r;
    mDstLowerG = g;
    mDstLowerB = b;
    return VINF_SUCCESS;
}

int VPoxVHWAGlProgramVHWA::setSrcCKeyUpperRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if (!isInitialized())
        return VERR_GENERAL_FAILURE;
    if (mSrcUpperR == r && mSrcUpperG == g && mSrcUpperB == b)
        return VINF_ALREADY_INITIALIZED;
    vpoxglUniform4f(mUniSrcUpperColor, r, g, b, 0.0);
    mSrcUpperR = r;
    mSrcUpperG = g;
    mSrcUpperB = b;
    return VINF_SUCCESS;
}

int VPoxVHWAGlProgramVHWA::setSrcCKeyLowerRange(GLfloat r, GLfloat g, GLfloat b)
{
    Assert(isInitialized());
    if (!isInitialized())
        return VERR_GENERAL_FAILURE;
    if (mSrcLowerR == r && mSrcLowerG == g && mSrcLowerB == b)
        return VINF_ALREADY_INITIALIZED;
    VPOXQGL_CHECKERR(
            vpoxglUniform4f(mUniSrcLowerColor, r, g, b, 0.0);
            );
    mSrcLowerR = r;
    mSrcLowerG = g;
    mSrcLowerB = b;
    return VINF_SUCCESS;
}

class VPoxVHWAGlProgramMngr
{
public:
    VPoxVHWAGlProgramMngr()
        : mShaderCConvApplyAYUV(":/cconvApplyAYUV.c", GL_FRAGMENT_SHADER)
        , mShaderCConvAYUV(":/cconvAYUV.c", GL_FRAGMENT_SHADER)
        , mShaderCConvBGR(":/cconvBGR.c", GL_FRAGMENT_SHADER)
        , mShaderCConvUYVY(":/cconvUYVY.c", GL_FRAGMENT_SHADER)
        , mShaderCConvYUY2(":/cconvYUY2.c", GL_FRAGMENT_SHADER)
        , mShaderCConvYV12(":/cconvYV12.c", GL_FRAGMENT_SHADER)
        , mShaderSplitBGRA(":/splitBGRA.c", GL_FRAGMENT_SHADER)
        , mShaderCKeyDst(":/ckeyDst.c", GL_FRAGMENT_SHADER)
        , mShaderCKeyDst2(":/ckeyDst2.c", GL_FRAGMENT_SHADER)
        , mShaderMainOverlay(":/mainOverlay.c", GL_FRAGMENT_SHADER)
        , mShaderMainOverlayNoCKey(":/mainOverlayNoCKey.c", GL_FRAGMENT_SHADER)
        , mShaderMainOverlayNoDiscard(":/mainOverlayNoDiscard.c", GL_FRAGMENT_SHADER)
        , mShaderMainOverlayNoDiscard2(":/mainOverlayNoDiscard2.c", GL_FRAGMENT_SHADER)
    {}

    VPoxVHWAGlProgramVHWA *getProgram(uint32_t type, const VPoxVHWAColorFormat * pFrom, const VPoxVHWAColorFormat * pTo);

    void stopCurrentProgram()
    {
        VPOXQGL_CHECKERR(
            vpoxglUseProgram(0);
            );
    }
private:
    VPoxVHWAGlProgramVHWA *searchProgram(uint32_t type, uint32_t fourcc, bool bCreate);
    VPoxVHWAGlProgramVHWA *createProgram(uint32_t type, uint32_t fourcc);

    typedef std::list <VPoxVHWAGlProgramVHWA*> ProgramList;

    ProgramList mPrograms;

    VPoxVHWAGlShaderComponent mShaderCConvApplyAYUV;

    VPoxVHWAGlShaderComponent mShaderCConvAYUV;
    VPoxVHWAGlShaderComponent mShaderCConvBGR;
    VPoxVHWAGlShaderComponent mShaderCConvUYVY;
    VPoxVHWAGlShaderComponent mShaderCConvYUY2;
    VPoxVHWAGlShaderComponent mShaderCConvYV12;
    VPoxVHWAGlShaderComponent mShaderSplitBGRA;

    /* expected the dst surface texture to be bound to the 1-st tex unit */
    VPoxVHWAGlShaderComponent mShaderCKeyDst;
    /* expected the dst surface texture to be bound to the 2-nd tex unit */
    VPoxVHWAGlShaderComponent mShaderCKeyDst2;
    VPoxVHWAGlShaderComponent mShaderMainOverlay;
    VPoxVHWAGlShaderComponent mShaderMainOverlayNoCKey;
    VPoxVHWAGlShaderComponent mShaderMainOverlayNoDiscard;
    VPoxVHWAGlShaderComponent mShaderMainOverlayNoDiscard2;

    friend class VPoxVHWAGlProgramVHWA;
};

VPoxVHWAGlProgramVHWA *VPoxVHWAGlProgramMngr::createProgram(uint32_t type, uint32_t fourcc)
{
    VPoxVHWAGlShaderComponent *apShaders[16];
    uint32_t cShaders = 0;

    /* workaround for NVIDIA driver bug: ensure we attach the shader before those it is used in */
    /* reserve a slot for the mShaderCConvApplyAYUV,
     * in case it is not used the slot will be occupied by mShaderCConvBGR , which is ok */
    cShaders++;

    if (!!(type & VPOXVHWA_PROGRAM_DSTCOLORKEY)
            && !(type & VPOXVHWA_PROGRAM_COLORKEYNODISCARD))
    {
        if (fourcc == FOURCC_YV12)
        {
            apShaders[cShaders++] = &mShaderCKeyDst2;
        }
        else
        {
            apShaders[cShaders++] = &mShaderCKeyDst;
        }
    }

    if (type & VPOXVHWA_PROGRAM_SRCCOLORKEY)
    {
        AssertFailed();
        /* disabled for now, not really necessary for video overlaying */
    }

    bool bFound = false;

//    if (type & VPOXVHWA_PROGRAM_COLORCONV)
    {
        if (fourcc == FOURCC_UYVY)
        {
            apShaders[cShaders++] = &mShaderCConvUYVY;
            bFound = true;
        }
        else if (fourcc == FOURCC_YUY2)
        {
            apShaders[cShaders++] = &mShaderCConvYUY2;
            bFound = true;
        }
        else if (fourcc == FOURCC_YV12)
        {
            apShaders[cShaders++] = &mShaderCConvYV12;
            bFound = true;
        }
        else if (fourcc == FOURCC_AYUV)
        {
            apShaders[cShaders++] = &mShaderCConvAYUV;
            bFound = true;
        }
    }

    if (bFound)
    {
        type |= VPOXVHWA_PROGRAM_COLORCONV;
        apShaders[0] = &mShaderCConvApplyAYUV;
    }
    else
    {
        type &= (~VPOXVHWA_PROGRAM_COLORCONV);
        apShaders[0] = &mShaderCConvBGR;
    }

    if (type & VPOXVHWA_PROGRAM_DSTCOLORKEY)
    {
        if (type & VPOXVHWA_PROGRAM_COLORKEYNODISCARD)
        {
            if (fourcc == FOURCC_YV12)
                apShaders[cShaders++] = &mShaderMainOverlayNoDiscard2;
            else
                apShaders[cShaders++] = &mShaderMainOverlayNoDiscard;
        }
        else
            apShaders[cShaders++] = &mShaderMainOverlay;
    }
    else
    {
        // ensure we don't have empty functions /* paranoia for for ATI on linux */
        apShaders[cShaders++] = &mShaderMainOverlayNoCKey;
    }

    Assert(cShaders <= RT_ELEMENTS(apShaders));

    VPoxVHWAGlShader shader(GL_FRAGMENT_SHADER, apShaders, cShaders);
    VPoxVHWAGlShader *pShader = &shader;

    VPoxVHWAGlProgramVHWA *pProgram =  new VPoxVHWAGlProgramVHWA(/*this, */type, fourcc, &pShader, 1);
    pProgram->init();

    return pProgram;
}

VPoxVHWAGlProgramVHWA * VPoxVHWAGlProgramMngr::getProgram(uint32_t type, const VPoxVHWAColorFormat * pFrom, const VPoxVHWAColorFormat * pTo)
{
    Q_UNUSED(pTo);
    uint32_t fourcc = 0;
    type &= VPOXVHWA_SUPPORTED_PROGRAM;

    if (pFrom && pFrom->fourcc())
    {
        fourcc = pFrom->fourcc();
        type |= VPOXVHWA_PROGRAM_COLORCONV;
    }
    else
        type &= ~VPOXVHWA_PROGRAM_COLORCONV;

    if (   !(type & VPOXVHWA_PROGRAM_DSTCOLORKEY)
        && !(type & VPOXVHWA_PROGRAM_SRCCOLORKEY))
        type &= ~VPOXVHWA_PROGRAM_COLORKEYNODISCARD;

    if (type)
        return searchProgram(type, fourcc, true);
    return NULL;
}

VPoxVHWAGlProgramVHWA * VPoxVHWAGlProgramMngr::searchProgram(uint32_t type, uint32_t fourcc, bool bCreate)
{
    for (ProgramList::const_iterator it = mPrograms.begin(); it != mPrograms.end(); ++ it)
    {
        if (!(*it)->matches(type, fourcc))
            continue;
        return *it;
    }
    if (bCreate)
    {
        VPoxVHWAGlProgramVHWA *pProgram = createProgram(type, fourcc);
        if (pProgram)
        {
            mPrograms.push_back(pProgram);
            return pProgram;
        }
    }
    return NULL;
}

void VPoxVHWASurfaceBase::setAddress(uchar * addr)
{
    Assert(addr);
    if (!addr)
        return;
    if (addr == mAddress)
        return;

    if (mFreeAddress)
        free(mAddress);

    mAddress = addr;
    mFreeAddress = false;

    mImage->setAddress(mAddress);

    mUpdateMem2TexRect.set(mRect);
    Assert(!mUpdateMem2TexRect.isClear());
    Assert(mRect.contains(mUpdateMem2TexRect.rect()));
}

void VPoxVHWASurfaceBase::globalInit()
{
    VPOXQGLLOG(("globalInit\n"));

    glEnable(GL_TEXTURE_RECTANGLE);
    glDisable(GL_DEPTH_TEST);

    VPOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            );
    VPOXQGL_CHECKERR(
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            );
}

VPoxVHWASurfaceBase::VPoxVHWASurfaceBase(class VPoxVHWAImage *pImage,
                                         const QSize &aSize,
                                         const QRect &aTargRect,
                                         const QRect &aSrcRect,
                                         const QRect &aVisTargRect,
                                         VPoxVHWAColorFormat &aColorFormat,
                                         VPoxVHWAColorKey *pSrcBltCKey,
                                         VPoxVHWAColorKey *pDstBltCKey,
                                         VPoxVHWAColorKey *pSrcOverlayCKey,
                                         VPoxVHWAColorKey *pDstOverlayCKey,
                                         VPOXVHWAIMG_TYPE aImgFlags)
    : mRect(0,0,aSize.width(),aSize.height())
    , mAddress(NULL)
    , mpSrcBltCKey(NULL)
    , mpDstBltCKey(NULL)
    , mpSrcOverlayCKey(NULL)
    , mpDstOverlayCKey(NULL)
    , mpDefaultDstOverlayCKey(NULL)
    , mpDefaultSrcOverlayCKey(NULL)
    , mLockCount(0)
    , mFreeAddress(false)
    , mbNotIntersected(false)
    , mComplexList(NULL)
    , mpPrimary(NULL)
    , mHGHandle(VPOXVHWA_SURFHANDLE_INVALID)
    , mpImage(pImage)
#ifdef DEBUG
    , cFlipsCurr(0)
    , cFlipsTarg(0)
#endif
{
    setDstBltCKey(pDstBltCKey);
    setSrcBltCKey(pSrcBltCKey);

    setDefaultDstOverlayCKey(pDstOverlayCKey);
    resetDefaultDstOverlayCKey();

    setDefaultSrcOverlayCKey(pSrcOverlayCKey);
    resetDefaultSrcOverlayCKey();

    mImage = vpoxVHWAImageCreate(QRect(0,0,aSize.width(),aSize.height()), aColorFormat, getGlProgramMngr(), aImgFlags);

    setRectValues(aTargRect, aSrcRect);
    setVisibleRectValues(aVisTargRect);
}

VPoxVHWASurfaceBase::~VPoxVHWASurfaceBase()
{
    uninit();
}

GLsizei VPoxVHWASurfaceBase::makePowerOf2(GLsizei val)
{
    int last = ASMBitLastSetS32(val);
    if (last>1)
    {
        last--;
        if ((1 << last) != val)
        {
            Assert((1 << last) < val);
            val = (1 << (last+1));
        }
    }
    return val;
}

ulong VPoxVHWASurfaceBase::calcBytesPerPixel(GLenum format, GLenum type)
{
    /* we now support only common byte-aligned data */
    int numComponents = 0;
    switch(format)
    {
        case GL_COLOR_INDEX:
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_ALPHA:
        case GL_LUMINANCE:
            numComponents = 1;
            break;
        case GL_RGB:
        case GL_BGR_EXT:
            numComponents = 3;
            break;
        case GL_RGBA:
        case GL_BGRA_EXT:
            numComponents = 4;
            break;
        case GL_LUMINANCE_ALPHA:
            numComponents = 2;
            break;
        default:
            AssertFailed();
            break;
    }

    int componentSize = 0;
    switch(type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            componentSize = 1;
            break;
        //case GL_BITMAP:
        case  GL_UNSIGNED_SHORT:
        case GL_SHORT:
            componentSize = 2;
            break;
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
            componentSize = 4;
            break;
        default:
            AssertFailed();
            break;
    }
    return numComponents * componentSize;
}

void VPoxVHWASurfaceBase::uninit()
{
    delete mImage;

    if (mAddress && mFreeAddress)
    {
        free(mAddress);
        mAddress = NULL;
    }
}

ulong VPoxVHWASurfaceBase::memSize()
{
    return (ulong)mImage->memSize();
}

void VPoxVHWASurfaceBase::init(VPoxVHWASurfaceBase * pPrimary, uchar *pvMem)
{
    if (pPrimary)
    {
        VPOXQGL_CHECKERR(
                vpoxglActiveTexture(GL_TEXTURE1);
            );
    }

    int size = memSize();
    uchar * address = (uchar *)malloc(size);
#ifdef DEBUG_misha
    int tex0Size = mImage->component(0)->memSize();
    if (pPrimary)
    {
        memset(address, 0xff, tex0Size);
        Assert(size >= tex0Size);
        if (size > tex0Size)
            memset(address + tex0Size, 0x0, size - tex0Size);
    }
    else
    {
        memset(address, 0x0f, tex0Size);
        Assert(size >= tex0Size);
        if (size > tex0Size)
            memset(address + tex0Size, 0x3f, size - tex0Size);
    }
#else
    memset(address, 0, size);
#endif

    mImage->init(address);
    mpPrimary = pPrimary;

    if (pvMem)
    {
        mAddress = pvMem;
        free(address);
        mFreeAddress = false;

    }
    else
    {
        mAddress = address;
        mFreeAddress = true;
    }

    mImage->setAddress(mAddress);

    initDisplay();

    mUpdateMem2TexRect.set(mRect);
    Assert(!mUpdateMem2TexRect.isClear());
    Assert(mRect.contains(mUpdateMem2TexRect.rect()));

    if (pPrimary)
    {
        VPOXQGLLOG(("restoring to tex 0"));
        VPOXQGL_CHECKERR(
                vpoxglActiveTexture(GL_TEXTURE0);
            );
    }

}

void VPoxVHWATexture::doUpdate(uchar *pAddress, const QRect *pRect)
{
    GLenum tt = texTarget();
    QRect rect = mRect;
    if (pRect)
        rect = rect.intersected(*pRect);
    AssertReturnVoid(!rect.isEmpty());

    Assert(glIsTexture(mTexture));
    VPOXQGL_CHECKERR(
            glBindTexture(tt, mTexture);
            );

    int x = rect.x()/mColorFormat.widthCompression();
    int y = rect.y()/mColorFormat.heightCompression();
    int width = rect.width()/mColorFormat.widthCompression();
    int height = rect.height()/mColorFormat.heightCompression();

    uchar *address = pAddress + pointOffsetTex(x, y);

    VPOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ROW_LENGTH, mBytesPerLine * 8 /mColorFormat.bitsPerPixelTex());
            );

    VPOXQGL_CHECKERR(
            glTexSubImage2D(tt,
                0,
                x, y, width, height,
                mColorFormat.format(),
                mColorFormat.type(),
                address);
            );

    VPOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            );
}

void VPoxVHWATexture::texCoord(int x, int y)
{
    glTexCoord2f(((float)x)/mTexRect.width()/mColorFormat.widthCompression(),
                 ((float)y)/mTexRect.height()/mColorFormat.heightCompression());
}

void VPoxVHWATexture::multiTexCoord(GLenum texUnit, int x, int y)
{
    vpoxglMultiTexCoord2f(texUnit, ((float)x)/mTexRect.width()/mColorFormat.widthCompression(),
                          ((float)y)/mTexRect.height()/mColorFormat.heightCompression());
}

void VPoxVHWATexture::uninit()
{
    if (mTexture)
        glDeleteTextures(1,&mTexture);
}

VPoxVHWATexture::VPoxVHWATexture(const QRect & aRect, const VPoxVHWAColorFormat &aFormat,
                                 uint32_t bytesPerLine, GLint scaleFuncttion)
    : mAddress(NULL)
    , mTexture(0)
    , mBytesPerPixel(0)
    , mBytesPerPixelTex(0)
    , mBytesPerLine(0)
    , mScaleFuncttion(scaleFuncttion)
{
    mColorFormat      = aFormat;
    mRect             = aRect;
    mBytesPerPixel    = mColorFormat.bitsPerPixel() / 8;
    mBytesPerPixelTex = mColorFormat.bitsPerPixelTex() / 8;
    mBytesPerLine     = bytesPerLine ? bytesPerLine : mBytesPerPixel * mRect.width();
    GLsizei wdt       = VPoxVHWASurfaceBase::makePowerOf2(mRect.width() / mColorFormat.widthCompression());
    GLsizei hgt       = VPoxVHWASurfaceBase::makePowerOf2(mRect.height() / mColorFormat.heightCompression());
    mTexRect = QRect(0,0,wdt,hgt);
}

#ifdef DEBUG_misha
void VPoxVHWATexture::dbgDump()
{
#if 0
    bind();
    GLvoid *pvBuf = malloc(4 * mRect.width() * mRect.height());
    VPOXQGL_CHECKERR(
        glGetTexImage(texTarget(),
            0, /*GLint level*/
            mColorFormat.format(),
            mColorFormat.type(),
            pvBuf);
    );
    VPOXQGLDBGPRINT(("<?dml?><exec cmd=\"!vbvdbg.ms 0x%p 0n%d 0n%d\">texture info</exec>\n",
            pvBuf, mRect.width(), mRect.height()));
    AssertFailed();

    free(pvBuf);
#endif
}
#endif


void VPoxVHWATexture::initParams()
{
    GLenum tt = texTarget();

    glTexParameteri(tt, GL_TEXTURE_MIN_FILTER, mScaleFuncttion);
    VPOXQGL_ASSERTNOERR();
    glTexParameteri(tt, GL_TEXTURE_MAG_FILTER, mScaleFuncttion);
    VPOXQGL_ASSERTNOERR();
    glTexParameteri(tt, GL_TEXTURE_WRAP_S, GL_CLAMP);
    VPOXQGL_ASSERTNOERR();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    VPOXQGL_ASSERTNOERR();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    VPOXQGL_ASSERTNOERR();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    VPOXQGL_ASSERTNOERR();
}

void VPoxVHWATexture::load()
{
    VPOXQGL_CHECKERR(
            glPixelStorei(GL_UNPACK_ROW_LENGTH, mTexRect.width());
            );

    VPOXQGL_CHECKERR(
        glTexImage2D(texTarget(),
                0,
                  mColorFormat.internalFormat(),
                  mTexRect.width(),
                  mTexRect.height(),
                  0,
                  mColorFormat.format(),
                  mColorFormat.type(),
                  (GLvoid *)mAddress);
        );
}

void VPoxVHWATexture::init(uchar *pvMem)
{
//    GLsizei wdt = mTexRect.width();
//    GLsizei hgt = mTexRect.height();

    VPOXQGL_CHECKERR(
            glGenTextures(1, &mTexture);
        );

    VPOXQGLLOG(("tex: %d", mTexture));

    bind();

    initParams();

    setAddress(pvMem);

    load();
}

VPoxVHWATexture::~VPoxVHWATexture()
{
    uninit();
}

void VPoxVHWATextureNP2Rect::texCoord(int x, int y)
{
    glTexCoord2i(x/mColorFormat.widthCompression(), y/mColorFormat.heightCompression());
}

void VPoxVHWATextureNP2Rect::multiTexCoord(GLenum texUnit, int x, int y)
{
    vpoxglMultiTexCoord2i(texUnit, x/mColorFormat.widthCompression(), y/mColorFormat.heightCompression());
}

GLenum VPoxVHWATextureNP2Rect::texTarget()
{
    return GL_TEXTURE_RECTANGLE;
}

bool VPoxVHWASurfaceBase::synchTexMem(const QRect * pRect)
{
    if (pRect)
        AssertReturn(mRect.contains(*pRect), false);

    if (mUpdateMem2TexRect.isClear())
        return false;

    if (pRect && !mUpdateMem2TexRect.rect().intersects(*pRect))
        return false;

#ifdef VPOXVHWA_PROFILE_FPS
    mpImage->reportNewFrame();
#endif

    mImage->update(&mUpdateMem2TexRect.rect());

    mUpdateMem2TexRect.clear();
    Assert(mUpdateMem2TexRect.isClear());

    return true;
}

void VPoxVHWATextureNP2RectPBO::init(uchar *pvMem)
{
    VPOXQGL_CHECKERR(
            vpoxglGenBuffers(1, &mPBO);
            );
    VPoxVHWATextureNP2Rect::init(pvMem);
}

void VPoxVHWATextureNP2RectPBO::doUpdate(uchar *pAddress, const QRect *pRect)
{
    Q_UNUSED(pAddress);
    Q_UNUSED(pRect);

    vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);

    GLvoid *buf;

    VPOXQGL_CHECKERR(
            buf = vpoxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            );
    Assert(buf);
    if (buf)
    {
        memcpy(buf, mAddress, memSize());

        bool unmapped;
        VPOXQGL_CHECKERR(
                unmapped = vpoxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                );

        Assert(unmapped); NOREF(unmapped);

        VPoxVHWATextureNP2Rect::doUpdate(0, &mRect);

        vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    else
    {
        VPOXQGLLOGREL(("failed to map PBO, trying fallback to non-PBO approach\n"));
        /* try fallback to non-PBO approach */
        vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        VPoxVHWATextureNP2Rect::doUpdate(pAddress, pRect);
    }
}

VPoxVHWATextureNP2RectPBO::~VPoxVHWATextureNP2RectPBO()
{
    VPOXQGL_CHECKERR(
            vpoxglDeleteBuffers(1, &mPBO);
            );
}


void VPoxVHWATextureNP2RectPBO::load()
{
    VPoxVHWATextureNP2Rect::load();

    VPOXQGL_CHECKERR(
            vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
        );

    VPOXQGL_CHECKERR(
            vpoxglBufferData(GL_PIXEL_UNPACK_BUFFER, memSize(), NULL, GL_STREAM_DRAW);
        );

    GLvoid *buf = vpoxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    Assert(buf);
    if (buf)
    {
        memcpy(buf, mAddress, memSize());

        bool unmapped = vpoxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        Assert(unmapped); NOREF(unmapped);
    }

    vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

uchar* VPoxVHWATextureNP2RectPBOMapped::mapAlignedBuffer()
{
    Assert(!mpMappedAllignedBuffer);
    if (!mpMappedAllignedBuffer)
    {
        VPOXQGL_CHECKERR(
                vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
            );

        uchar* buf;
        VPOXQGL_CHECKERR(
                buf = (uchar*)vpoxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
        );

        Assert(buf);

        VPOXQGL_CHECKERR(
                vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            );

        mpMappedAllignedBuffer = (uchar*)alignBuffer(buf);

        mcbOffset = calcOffset(buf, mpMappedAllignedBuffer);
    }
    return mpMappedAllignedBuffer;
}

void VPoxVHWATextureNP2RectPBOMapped::unmapBuffer()
{
    Assert(mpMappedAllignedBuffer);
    if (mpMappedAllignedBuffer)
    {
        VPOXQGL_CHECKERR(
                vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
        );

        bool unmapped;
        VPOXQGL_CHECKERR(
                unmapped = vpoxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                );

        Assert(unmapped); NOREF(unmapped);

        VPOXQGL_CHECKERR(
                vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        );

        mpMappedAllignedBuffer = NULL;
    }
}

void VPoxVHWATextureNP2RectPBOMapped::load()
{
    VPoxVHWATextureNP2Rect::load();

    VPOXQGL_CHECKERR(
            vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
        );

    VPOXQGL_CHECKERR(
            vpoxglBufferData(GL_PIXEL_UNPACK_BUFFER, mcbActualBufferSize, NULL, GL_STREAM_DRAW);
        );

    vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void VPoxVHWATextureNP2RectPBOMapped::doUpdate(uchar *pAddress, const QRect *pRect)
{
    Q_UNUSED(pAddress);
    Q_UNUSED(pRect);

    VPOXQGL_CHECKERR(
            vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
    );

    if (mpMappedAllignedBuffer)
    {
        bool unmapped;
        VPOXQGL_CHECKERR(
                unmapped = vpoxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                );

        Assert(unmapped); NOREF(unmapped);

        mpMappedAllignedBuffer = NULL;
    }

    VPoxVHWATextureNP2Rect::doUpdate((uchar *)mcbOffset, &mRect);

    VPOXQGL_CHECKERR(
            vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    );
}

int VPoxVHWASurfaceBase::lock(const QRect *pRect, uint32_t flags)
{
    Q_UNUSED(flags);

    if (pRect)
        AssertReturn(mRect.contains(*pRect), VERR_GENERAL_FAILURE);

    Assert(mLockCount >= 0);
    if (pRect && pRect->isEmpty())
        return VERR_GENERAL_FAILURE;
    if (mLockCount < 0)
        return VERR_GENERAL_FAILURE;

    VPOXQGLLOG(("lock (0x%x)", this));
    VPOXQGLLOG_QRECT("rect: ", pRect ? pRect : &mRect, "\n");
    VPOXQGLLOG_METHODTIME("time ");

    mUpdateMem2TexRect.add(pRect ? mRect.intersected(*pRect) : mRect);

    Assert(!mUpdateMem2TexRect.isClear());
    Assert(mRect.contains(mUpdateMem2TexRect.rect()));
    return VINF_SUCCESS;
}

int VPoxVHWASurfaceBase::unlock()
{
    VPOXQGLLOG(("unlock (0x%x)\n", this));
    mLockCount = 0;
    return VINF_SUCCESS;
}

void VPoxVHWASurfaceBase::setRectValues (const QRect &aTargRect, const QRect &aSrcRect)
{
    mTargRect = aTargRect;
    mSrcRect = mRect.intersected(aSrcRect);
}

void VPoxVHWASurfaceBase::setVisibleRectValues (const QRect &aVisTargRect)
{
    mVisibleTargRect = aVisTargRect.intersected(mTargRect);
    if (mVisibleTargRect.isEmpty() || mTargRect.isEmpty())
        mVisibleSrcRect.setSize(QSize(0, 0));
    else
    {
        float stretchX = float(mSrcRect.width()) / mTargRect.width();
        float stretchY = float(mSrcRect.height()) / mTargRect.height();
        int tx1, tx2, ty1, ty2, vtx1, vtx2, vty1, vty2;
        int sx1, sx2, sy1, sy2;
        mVisibleTargRect.getCoords(&vtx1, &vty1, &vtx2, &vty2);
        mTargRect.getCoords(&tx1, &ty1, &tx2, &ty2);
        mSrcRect.getCoords(&sx1, &sy1, &sx2, &sy2);
        int dx1 = vtx1 - tx1;
        int dy1 = vty1 - ty1;
        int dx2 = vtx2 - tx2;
        int dy2 = vty2 - ty2;
        int vsx1, vsy1, vsx2, vsy2;
        Assert(dx1 >= 0);
        Assert(dy1 >= 0);
        Assert(dx2 <= 0);
        Assert(dy2 <= 0);
        vsx1 = sx1 + int(dx1*stretchX);
        vsy1 = sy1 + int(dy1*stretchY);
        vsx2 = sx2 + int(dx2*stretchX);
        vsy2 = sy2 + int(dy2*stretchY);
        mVisibleSrcRect.setCoords(vsx1, vsy1, vsx2, vsy2);
        Assert(!mVisibleSrcRect.isEmpty());
        Assert(mSrcRect.contains(mVisibleSrcRect));
    }
}


void VPoxVHWASurfaceBase::setRects(const QRect & aTargRect, const QRect & aSrcRect)
{
    if (mTargRect != aTargRect || mSrcRect != aSrcRect)
        setRectValues(aTargRect, aSrcRect);
}

void VPoxVHWASurfaceBase::setTargRectPosition(const QPoint & aPoint)
{
    QRect tRect = targRect();
    tRect.moveTopLeft(aPoint);
    setRects(tRect, srcRect());
}

void VPoxVHWASurfaceBase::updateVisibility(VPoxVHWASurfaceBase *pPrimary, const QRect & aVisibleTargRect,
                                           bool bNotIntersected, bool bForce)
{
    if (bForce || aVisibleTargRect.intersected(mTargRect) != mVisibleTargRect)
        setVisibleRectValues(aVisibleTargRect);

    mpPrimary = pPrimary;
    mbNotIntersected = bNotIntersected;

    initDisplay();
}

void VPoxVHWASurfaceBase::initDisplay()
{
    if (mVisibleTargRect.isEmpty() || mVisibleSrcRect.isEmpty())
    {
        Assert(mVisibleTargRect.isEmpty() && mVisibleSrcRect.isEmpty());
        mImage->deleteDisplay();
        return;
    }

    int rc = mImage->initDisplay(mpPrimary ? mpPrimary->mImage : NULL, &mVisibleTargRect, &mVisibleSrcRect,
                                 getActiveDstOverlayCKey(mpPrimary), getActiveSrcOverlayCKey(), mbNotIntersected);
    AssertRC(rc);
}

void VPoxVHWASurfaceBase::updatedMem(const QRect *rec)
{
    AssertReturnVoid(mRect.contains(*rec));
    mUpdateMem2TexRect.add(*rec);
}

bool VPoxVHWASurfaceBase::performDisplay(VPoxVHWASurfaceBase *pPrimary, bool bForce)
{
    Assert(mImage->displayInitialized());

    if (mVisibleTargRect.isEmpty())
    {
        /* nothing to display, i.e. the surface is not visible,
         * in the sense that it's located behind the viewport ranges */
        Assert(mVisibleSrcRect.isEmpty());
        return false;
    }
    Assert(!mVisibleSrcRect.isEmpty());

    bForce |= synchTexMem(&mVisibleSrcRect);

    const VPoxVHWAColorKey * pDstCKey = getActiveDstOverlayCKey(pPrimary);
    if (pPrimary && pDstCKey)
        bForce |= pPrimary->synchTexMem(&mVisibleTargRect);

    if (!bForce)
        return false;

    mImage->display();

    Assert(bForce);
    return true;
}

class VPoxVHWAGlProgramMngr * VPoxVHWASurfaceBase::getGlProgramMngr()
{
    return mpImage->vpoxVHWAGetGlProgramMngr();
}

class VPoxGLContext : public QGLContext
{
public:
    VPoxGLContext (const QGLFormat & format )
        : QGLContext(format)
        , mAllowDoneCurrent(true)
    {
    }

    void doneCurrent()
    {
        if (!mAllowDoneCurrent)
            return;
        QGLContext::doneCurrent();
    }

    bool isDoneCurrentAllowed() { return mAllowDoneCurrent; }
    void allowDoneCurrent(bool bAllow) { mAllowDoneCurrent = bAllow; }
private:
    bool mAllowDoneCurrent;
};


VPoxGLWgt::VPoxGLWgt(VPoxVHWAImage *pImage, QWidget *parent, const QGLWidget *shareWidget)
    : QGLWidget(new VPoxGLContext(shareWidget->format()), parent, shareWidget)
    , mpImage(pImage)
{
    /* work-around to disable done current needed to old ATI drivers on Linux */
    VPoxGLContext *pc = (VPoxGLContext *)context();
    pc->allowDoneCurrent (false);
    Assert(isSharing());
}

VPoxVHWAImage::VPoxVHWAImage ()
    : mSurfHandleTable(VPOXVHWA_MAX_SURFACES)
    , mRepaintNeeded(false)
//  ,  mbVGASurfCreated(false)
    , mConstructingList(NULL)
    , mcRemaining2Contruct(0)
    , mSettings(NULL)
#ifdef VPOXVHWA_PROFILE_FPS
    , mFPSCounter(64)
    , mbNewFrame(false)
#endif
{
    mpMngr = new VPoxVHWAGlProgramMngr();
//        /* No need for background drawing */
//        setAttribute (Qt::WA_OpaquePaintEvent);
}

int VPoxVHWAImage::init(VPoxVHWASettings *aSettings)
{
    mSettings = aSettings;
    return VINF_SUCCESS;
}

const QGLFormat &VPoxVHWAImage::vpoxGLFormat()
{
    static QGLFormat vpoxFormat = QGLFormat();
    vpoxFormat.setAlpha(true);
    Assert(vpoxFormat.alpha());
    vpoxFormat.setSwapInterval(0);
    Assert(vpoxFormat.swapInterval() == 0);
    vpoxFormat.setAccum(false);
    Assert(!vpoxFormat.accum());
    vpoxFormat.setDepth(false);
    Assert(!vpoxFormat.depth());
//  vpoxFormat.setRedBufferSize(8);
//  vpoxFormat.setGreenBufferSize(8);
//  vpoxFormat.setBlueBufferSize(8);
    return vpoxFormat;
}


VPoxVHWAImage::~VPoxVHWAImage()
{
    delete mpMngr;
}

#ifdef VPOXVHWA_OLD_COORD
void VPoxVHWAImage::doSetupMatrix(const QSize &aSize, bool bInverted)
{
    VPOXQGL_CHECKERR(
            glLoadIdentity();
            );
    if (bInverted)
    {
        VPOXQGL_CHECKERR(
                glScalef(1.0f / aSize.width(), 1.0f / aSize.height(), 1.0f);
                );
    }
    else
    {
        /* make display coordinates be scaled to pixel coordinates */
        VPOXQGL_CHECKERR(
                glTranslatef(0.0f, 1.0f, 0.0f);
                );
        VPOXQGL_CHECKERR(
                glScalef(1.0f / aSize.width(), 1.0f / aSize.height(), 1.0f);
                );
        VPOXQGL_CHECKERR(
                glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
                );
    }
}
#endif

void VPoxVHWAImage::adjustViewport(const QSize &display, const QRect &viewport)
{
    glViewport(-viewport.x(),
               viewport.height() + viewport.y() - display.height(),
               display.width(),
               display.height());
}

void VPoxVHWAImage::setupMatricies(const QSize &display, bool bInverted)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (bInverted)
        glOrtho(0., (GLdouble)display.width(), (GLdouble)display.height(), 0., -1., 1.);
    else
        glOrtho(0., (GLdouble)display.width(), 0., (GLdouble)display.height(), -1., 1.);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int VPoxVHWAImage::reset(VHWACommandList *pCmdList)
{
    VPOXVHWACMD *pCmd;
    const OverlayList & overlays = mDisplay.overlays();
    for (OverlayList::const_iterator oIt = overlays.begin(); oIt != overlays.end(); ++ oIt)
    {
        VPoxVHWASurfList * pSurfList = *oIt;
        if (pSurfList->current())
        {
            /* 1. hide overlay */
            pCmd = vhwaHHCmdCreate(VPOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE, sizeof(VPOXVHWACMD_SURF_OVERLAY_UPDATE));
            VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_GUEST *pOUCmd = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_OVERLAY_UPDATE);
            pOUCmd->u.in.hSrcSurf = pSurfList->current()->handle();
            pOUCmd->u.in.flags = VPOXVHWA_OVER_HIDE;

            pCmdList->push_back(pCmd);
        }

        /* 2. destroy overlay */
        const SurfList & surfaces = pSurfList->surfaces();
        for (SurfList::const_iterator sIt = surfaces.begin(); sIt != surfaces.end(); ++ sIt)
        {
            VPoxVHWASurfaceBase *pCurSurf = (*sIt);
            pCmd = vhwaHHCmdCreate(VPOXVHWACMD_TYPE_SURF_DESTROY, sizeof(VPOXVHWACMD_SURF_DESTROY));
            VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_GUEST *pSDCmd = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_DESTROY);
            pSDCmd->u.in.hSurf = pCurSurf->handle();

            pCmdList->push_back(pCmd);
        }
    }

    /* 3. destroy primaries */
    const SurfList & surfaces = mDisplay.primaries().surfaces();
    for (SurfList::const_iterator sIt = surfaces.begin(); sIt != surfaces.end(); ++ sIt)
    {
        VPoxVHWASurfaceBase *pCurSurf = (*sIt);
        if (pCurSurf->handle() != VPOXVHWA_SURFHANDLE_INVALID)
        {
            pCmd = vhwaHHCmdCreate(VPOXVHWACMD_TYPE_SURF_DESTROY, sizeof(VPOXVHWACMD_SURF_DESTROY));
            VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_GUEST *pSDCmd = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_DESTROY);
            pSDCmd->u.in.hSurf = pCurSurf->handle();

            pCmdList->push_back(pCmd);
        }
    }

    return VINF_SUCCESS;
}

#ifdef VPOX_WITH_VIDEOHWACCEL
int VPoxVHWAImage::vhwaSurfaceCanCreate(struct VPOXVHWACMD_SURF_CANCREATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPOXQGLLOG_ENTER(("\n"));

    if (pCmd->SurfInfo.width > VPOXVHWA_MAX_WIDTH || pCmd->SurfInfo.height > VPOXVHWA_MAX_HEIGHT)
    {
        AssertFailed();
        pCmd->u.out.ErrInfo = -1;
        return VINF_SUCCESS;
    }

    const VPoxVHWAInfo & info = vpoxVHWAGetSupportInfo(NULL);

    if (!(pCmd->SurfInfo.flags & VPOXVHWA_SD_CAPS))
    {
        AssertFailed();
        pCmd->u.out.ErrInfo = -1;
        return VINF_SUCCESS;
    }
#ifdef VPOXVHWA_ALLOW_PRIMARY_AND_OVERLAY_ONLY
    if (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_OFFSCREENPLAIN)
    {
#ifdef DEBUGVHWASTRICT
        AssertFailed();
#endif
        pCmd->u.out.ErrInfo = -1;
        return VINF_SUCCESS;
    }
#endif

    if (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_PRIMARYSURFACE)
    {
        if (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_COMPLEX)
        {
#ifdef DEBUG_misha
            AssertFailed();
#endif
            pCmd->u.out.ErrInfo = -1;
        }
        else
        {
            pCmd->u.out.ErrInfo = 0;
        }
        return VINF_SUCCESS;
    }

#ifdef VPOXVHWA_ALLOW_PRIMARY_AND_OVERLAY_ONLY
    if ((pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_OVERLAY) == 0)
    {
#ifdef DEBUGVHWASTRICT
        AssertFailed();
#endif
        pCmd->u.out.ErrInfo = -1;
        return VINF_SUCCESS;
    }
#endif

    if (pCmd->u.in.bIsDifferentPixelFormat)
    {
        if (!(pCmd->SurfInfo.flags & VPOXVHWA_SD_PIXELFORMAT))
        {
            AssertFailed();
            pCmd->u.out.ErrInfo = -1;
            return VINF_SUCCESS;
        }

        if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_RGB)
        {
            if (pCmd->SurfInfo.PixelFormat.c.rgbBitCount != 32
                    && pCmd->SurfInfo.PixelFormat.c.rgbBitCount != 24)
            {
                AssertFailed();
                pCmd->u.out.ErrInfo = -1;
                return VINF_SUCCESS;
            }
        }
        else if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_FOURCC)
        {
            /* detect whether we support this format */
            bool bFound = mSettings->isSupported (info, pCmd->SurfInfo.PixelFormat.fourCC);

            if (!bFound)
            {
                VPOXQGLLOG (("!!unsupported fourcc!!!: %c%c%c%c\n",
                             (pCmd->SurfInfo.PixelFormat.fourCC & 0x000000ff),
                             (pCmd->SurfInfo.PixelFormat.fourCC & 0x0000ff00) >> 8,
                             (pCmd->SurfInfo.PixelFormat.fourCC & 0x00ff0000) >> 16,
                             (pCmd->SurfInfo.PixelFormat.fourCC & 0xff000000) >> 24
                             ));
                pCmd->u.out.ErrInfo = -1;
                return VINF_SUCCESS;
            }
        }
        else
        {
            AssertFailed();
            pCmd->u.out.ErrInfo = -1;
            return VINF_SUCCESS;
        }
    }

    pCmd->u.out.ErrInfo = 0;
    return VINF_SUCCESS;
}

int VPoxVHWAImage::vhwaSurfaceCreate(struct VPOXVHWACMD_SURF_CREATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPOXQGLLOG_ENTER (("\n"));

    uint32_t handle = pCmd->SurfInfo.hSurf;
    AssertReturn(handle == VPOXVHWA_SURFHANDLE_INVALID || handle < VPOXVHWA_MAX_SURFACES, VERR_GENERAL_FAILURE);
    RT_UNTRUSTED_VALIDATED_FENCE();
    if (handle != VPOXVHWA_SURFHANDLE_INVALID)
    {
        if (mSurfHandleTable.get(handle))
        {
            AssertFailed();
            return VERR_GENERAL_FAILURE;
        }
    }

    VPoxVHWASurfaceBase *surf = NULL;
    /* in case the Framebuffer is working in "not using VRAM" mode,
     * we need to report the pitch, etc. info of the form guest expects from us*/
    VPoxVHWAColorFormat reportedFormat;
    /* paranoia to ensure the VPoxVHWAColorFormat API works properly */
    Assert(!reportedFormat.isValid());
    bool bNoPBO = false;
    bool bPrimary = false;

    VPoxVHWAColorKey *pDstBltCKey = NULL, DstBltCKey;
    VPoxVHWAColorKey *pSrcBltCKey = NULL, SrcBltCKey;
    VPoxVHWAColorKey *pDstOverlayCKey = NULL, DstOverlayCKey;
    VPoxVHWAColorKey *pSrcOverlayCKey = NULL, SrcOverlayCKey;
    if (pCmd->SurfInfo.flags & VPOXVHWA_SD_CKDESTBLT)
    {
        DstBltCKey = VPoxVHWAColorKey(pCmd->SurfInfo.DstBltCK.high, pCmd->SurfInfo.DstBltCK.low);
        pDstBltCKey = &DstBltCKey;
    }
    if (pCmd->SurfInfo.flags & VPOXVHWA_SD_CKSRCBLT)
    {
        SrcBltCKey = VPoxVHWAColorKey(pCmd->SurfInfo.SrcBltCK.high, pCmd->SurfInfo.SrcBltCK.low);
        pSrcBltCKey = &SrcBltCKey;
    }
    if (pCmd->SurfInfo.flags & VPOXVHWA_SD_CKDESTOVERLAY)
    {
        DstOverlayCKey = VPoxVHWAColorKey(pCmd->SurfInfo.DstOverlayCK.high, pCmd->SurfInfo.DstOverlayCK.low);
        pDstOverlayCKey = &DstOverlayCKey;
    }
    if (pCmd->SurfInfo.flags & VPOXVHWA_SD_CKSRCOVERLAY)
    {
        SrcOverlayCKey = VPoxVHWAColorKey(pCmd->SurfInfo.SrcOverlayCK.high, pCmd->SurfInfo.SrcOverlayCK.low);
        pSrcOverlayCKey = &SrcOverlayCKey;
    }

    if (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_PRIMARYSURFACE)
    {
        bNoPBO = true;
        bPrimary = true;
        VPoxVHWASurfaceBase *pVga = vgaSurface();
#ifdef VPOX_WITH_WDDM
        uchar *addr = vpoxVRAMAddressFromOffset(pCmd->SurfInfo.offSurface);
        AssertPtrReturn(addr, VERR_GENERAL_FAILURE);
        RT_UNTRUSTED_VALIDATED_FENCE();
        pVga->setAddress(addr);
#endif

        Assert((pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_OFFSCREENPLAIN) == 0);

        reportedFormat = VPoxVHWAColorFormat(pCmd->SurfInfo.PixelFormat.c.rgbBitCount,
                                             pCmd->SurfInfo.PixelFormat.m1.rgbRBitMask,
                                             pCmd->SurfInfo.PixelFormat.m2.rgbGBitMask,
                                             pCmd->SurfInfo.PixelFormat.m3.rgbBBitMask);

        if (pVga->handle() == VPOXVHWA_SURFHANDLE_INVALID
                && (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_OFFSCREENPLAIN) == 0)
        {
            Assert(pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_RGB);
//            if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_RGB)
            {
                Assert(pCmd->SurfInfo.width == pVga->width());
                Assert(pCmd->SurfInfo.height == pVga->height());
//                if (pCmd->SurfInfo.width == pVga->width()
//                        && pCmd->SurfInfo.height == pVga->height())
                {
                    // the assert below is incorrect in case the Framebuffer is working in "not using VRAM" mode
//                    Assert(pVga->pixelFormat().equals(format));
//                    if (pVga->pixelFormat().equals(format))
                    {
                        surf = pVga;

                        surf->setDstBltCKey(pDstBltCKey);
                        surf->setSrcBltCKey(pSrcBltCKey);

                        surf->setDefaultDstOverlayCKey(pDstOverlayCKey);
                        surf->resetDefaultDstOverlayCKey();

                        surf->setDefaultSrcOverlayCKey(pSrcOverlayCKey);
                        surf->resetDefaultSrcOverlayCKey();
//                        mbVGASurfCreated = true;
                    }
                }
            }
        }
    }
    else if (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_OFFSCREENPLAIN)
    {
        bNoPBO = true;
    }

    if (!surf)
    {
        ASSERT_GUEST_RETURN(   pCmd->SurfInfo.width <= VPOXVHWA_MAX_WIDTH
                            && pCmd->SurfInfo.height <= VPOXVHWA_MAX_HEIGHT, VERR_GENERAL_FAILURE);
        ASSERT_GUEST_RETURN(pCmd->SurfInfo.cBackBuffers < VPOXVHWA_MAX_SURFACES, VERR_GENERAL_FAILURE);
        RT_UNTRUSTED_VALIDATED_FENCE();

        VPOXVHWAIMG_TYPE fFlags = 0;
        if (!bNoPBO)
        {
            fFlags |= VPOXVHWAIMG_PBO | VPOXVHWAIMG_PBOIMG | VPOXVHWAIMG_LINEAR;
            if (mSettings->isStretchLinearEnabled())
                fFlags |= VPOXVHWAIMG_FBO;
        }

        QSize surfSize(pCmd->SurfInfo.width, pCmd->SurfInfo.height);
        QRect primaryRect = mDisplay.getPrimary()->rect();
        VPoxVHWAColorFormat format;
        if (bPrimary)
            format = mDisplay.getVGA()->pixelFormat();
        else if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_RGB)
            format = VPoxVHWAColorFormat(pCmd->SurfInfo.PixelFormat.c.rgbBitCount,
                                         pCmd->SurfInfo.PixelFormat.m1.rgbRBitMask,
                                         pCmd->SurfInfo.PixelFormat.m2.rgbGBitMask,
                                         pCmd->SurfInfo.PixelFormat.m3.rgbBBitMask);
        else if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_FOURCC)
            format = VPoxVHWAColorFormat(pCmd->SurfInfo.PixelFormat.fourCC);
        else
            AssertFailed();

        if (format.isValid())
        {
            surf = new VPoxVHWASurfaceBase(this,
                                           surfSize,
                                           primaryRect,
                                           QRect(0, 0, surfSize.width(), surfSize.height()),
                                           mViewport,
                                           format,
                                           pSrcBltCKey, pDstBltCKey, pSrcOverlayCKey, pDstOverlayCKey,
#ifdef VPOXVHWA_USE_TEXGROUP
                                           0,
#endif
                                           fFlags);
        }
        else
        {
            AssertFailed();
            VPOXQGLLOG_EXIT(("pSurf (0x%p)\n",surf));
            return VERR_GENERAL_FAILURE;
        }

        uchar *addr = vpoxVRAMAddressFromOffset(pCmd->SurfInfo.offSurface);
        AssertReturn(addr || pCmd->SurfInfo.offSurface == VPOXVHWA_OFFSET64_VOID, VERR_GENERAL_FAILURE);
        surf->init(mDisplay.getPrimary(), addr);

        if (pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_OVERLAY)
        {
#ifdef DEBUG_misha
            Assert(!bNoPBO);
#endif

            if (!mConstructingList)
            {
                mConstructingList = new VPoxVHWASurfList();
                mcRemaining2Contruct = pCmd->SurfInfo.cBackBuffers+1;
                mDisplay.addOverlay(mConstructingList);
            }

            mConstructingList->add(surf);
            mcRemaining2Contruct--;
            if (!mcRemaining2Contruct)
                mConstructingList = NULL;
        }
        else
        {
            VPoxVHWASurfaceBase * pVga = vgaSurface();
            Assert(pVga->handle() != VPOXVHWA_SURFHANDLE_INVALID);
            Assert(pVga != surf); NOREF(pVga);
            mDisplay.getVGA()->getComplexList()->add(surf);
#ifdef DEBUGVHWASTRICT
            Assert(pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_VISIBLE);
#endif
            if (bPrimary)
            {
                Assert(surf->getComplexList() == mDisplay.getVGA()->getComplexList());
                surf->getComplexList()->setCurrentVisible(surf);
                mDisplay.updateVGA(surf);
            }
        }
    }
    else
        Assert(pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_PRIMARYSURFACE);

    Assert(mDisplay.getVGA() == mDisplay.getPrimary());

    /* tell the guest how we think the memory is organized */
    VPOXQGLLOG(("bps: %d\n", surf->bitsPerPixel()));

    if (!reportedFormat.isValid())
    {
        pCmd->SurfInfo.pitch = surf->bytesPerLine();
        pCmd->SurfInfo.sizeX = surf->memSize();
        pCmd->SurfInfo.sizeY = 1;
    }
    else
    {
        /* this is the case of Framebuffer not using Guest VRAM */
        /* can happen for primary surface creation only */
        Assert(pCmd->SurfInfo.surfCaps & VPOXVHWA_SCAPS_PRIMARYSURFACE);
        pCmd->SurfInfo.pitch = (reportedFormat.bitsPerPixel() * surf->width() + 7) / 8;
        /* we support only RGB case now, otherwise we would need more complicated mechanism of memsize calculation */
        Assert(!reportedFormat.fourcc());
        pCmd->SurfInfo.sizeX = (reportedFormat.bitsPerPixel() * surf->width() + 7) / 8 * surf->height();
        pCmd->SurfInfo.sizeY = 1;
    }

    if (handle != VPOXVHWA_SURFHANDLE_INVALID)
    {
        bool bSuccess = mSurfHandleTable.mapPut(handle, surf);
        Assert(bSuccess);
        if (!bSuccess)
        {
            /** @todo this is very bad, should not be here */
            return VERR_GENERAL_FAILURE;
        }
    }
    else
    {
        /* tell the guest our handle */
        handle = mSurfHandleTable.put(surf);
        pCmd->SurfInfo.hSurf = (VPOXVHWA_SURFHANDLE)handle;
    }

    Assert(handle != VPOXVHWA_SURFHANDLE_INVALID);
    Assert(surf->handle() == VPOXVHWA_SURFHANDLE_INVALID);
    surf->setHandle(handle);
    Assert(surf->handle() == handle);

    VPOXQGLLOG_EXIT(("pSurf (0x%p)\n",surf));

    return VINF_SUCCESS;
}

#ifdef VPOX_WITH_WDDM
int VPoxVHWAImage::vhwaSurfaceGetInfo(struct VPOXVHWACMD_SURF_GETINFO RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    ASSERT_GUEST_RETURN(   pCmd->SurfInfo.width <= VPOXVHWA_MAX_WIDTH
                        && pCmd->SurfInfo.height <= VPOXVHWA_MAX_HEIGHT, VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();
    VPoxVHWAColorFormat format;
    Assert(!format.isValid());
    if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_RGB)
        format = VPoxVHWAColorFormat(pCmd->SurfInfo.PixelFormat.c.rgbBitCount,
                                        pCmd->SurfInfo.PixelFormat.m1.rgbRBitMask,
                                        pCmd->SurfInfo.PixelFormat.m2.rgbGBitMask,
                                        pCmd->SurfInfo.PixelFormat.m3.rgbBBitMask);
    else if (pCmd->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_FOURCC)
        format = VPoxVHWAColorFormat(pCmd->SurfInfo.PixelFormat.fourCC);
    else
        AssertFailed();

    Assert(format.isValid());
    if (format.isValid())
    {
        pCmd->SurfInfo.pitch = VPoxVHWATextureImage::calcBytesPerLine(format, pCmd->SurfInfo.width);
        pCmd->SurfInfo.sizeX = VPoxVHWATextureImage::calcMemSize(format,
                pCmd->SurfInfo.width, pCmd->SurfInfo.height);
        pCmd->SurfInfo.sizeY = 1;
        return VINF_SUCCESS;
    }
    return VERR_INVALID_PARAMETER;
}
#endif

int VPoxVHWAImage::vhwaSurfaceDestroy(struct VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    AssertReturn(pSurf, VERR_INVALID_PARAMETER);
    VPoxVHWASurfList *pList = pSurf->getComplexList();
    Assert(pSurf->handle() != VPOXVHWA_SURFHANDLE_INVALID);

    VPOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));
    if (pList != mDisplay.getVGA()->getComplexList())
    {
        Assert(pList);
        if (pList)
        {
            pList->remove(pSurf);
            if (pList->surfaces().empty())
            {
                mDisplay.removeOverlay(pList);
                if (pList == mConstructingList)
                {
                    mConstructingList = NULL;
                    mcRemaining2Contruct = 0;
                }
                delete pList;
            }
        }

        delete(pSurf);
    }
    else
    {
        Assert(pList && pList->size() >= 1);
        if (pList && pList->size() > 1)
        {
            if (pSurf == mDisplay.getVGA())
            {
                const SurfList & surfaces = pList->surfaces();

                for (SurfList::const_iterator it = surfaces.begin();
                         it != surfaces.end(); ++ it)
                {
                    VPoxVHWASurfaceBase *pCurSurf = (*it);
                    Assert(pCurSurf);
                    if (pCurSurf != pSurf)
                    {
                        mDisplay.updateVGA(pCurSurf);
                        pList->setCurrentVisible(pCurSurf);
                        break;
                    }
                }
            }

            pList->remove(pSurf);
            delete(pSurf);
        }
        else
        {
            pSurf->setHandle(VPOXVHWA_SURFHANDLE_INVALID);
        }
    }

    /* just in case we destroy a visible overlay surface */
    mRepaintNeeded = true;

    void * test = mSurfHandleTable.remove(pCmd->u.in.hSurf);
    Assert(test); NOREF(test);

    return VINF_SUCCESS;
}

#define VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_RB(a_pr) \
    QRect((a_pr)->left, \
          (a_pr)->top, \
          (a_pr)->right  - (a_pr)->left + 1, \
          (a_pr)->bottom - (a_pr)->top + 1)

#define VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(a_pr) \
    QRect((a_pr)->left, \
          (a_pr)->top, \
          (a_pr)->right  - (a_pr)->left, \
          (a_pr)->bottom - (a_pr)->top)

int VPoxVHWAImage::vhwaSurfaceLock(struct VPOXVHWACMD_SURF_LOCK RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    AssertReturn(pSurf, VERR_INVALID_PARAMETER);
    VPOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));

    vpoxCheckUpdateAddress (pSurf, pCmd->u.in.offSurface);
    if (pCmd->u.in.rectValid)
    {
        QRect r = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.rect);
        return pSurf->lock(&r, pCmd->u.in.flags);
    }
    return pSurf->lock(NULL, pCmd->u.in.flags);
}

int VPoxVHWAImage::vhwaSurfaceUnlock(struct VPOXVHWACMD_SURF_UNLOCK RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    AssertReturn(pSurf, VERR_INVALID_PARAMETER);
#ifdef DEBUG_misha
    /* for performance reasons we should receive unlock for visible surfaces only
     * other surfaces receive unlock only once becoming visible, e.g. on DdFlip
     * Ensure this is so*/
    if (pSurf != mDisplay.getPrimary())
    {
        const OverlayList & overlays = mDisplay.overlays();
        bool bFound = false;

        if (!mDisplay.isPrimary(pSurf))
        {
            for (OverlayList::const_iterator it = overlays.begin();
                 it != overlays.end(); ++ it)
            {
                VPoxVHWASurfList * pSurfList = *it;
                if (pSurfList->current() == pSurf)
                {
                    bFound = true;
                    break;
                }
            }

            Assert(bFound);
        }

//        Assert(bFound);
    }
#endif
    VPOXQGLLOG_ENTER(("pSurf (0x%x)\n",pSurf));
    if (pCmd->u.in.xUpdatedMemValid)
    {
        QRect r = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.xUpdatedMemRect);
        pSurf->updatedMem(&r);
    }

    return pSurf->unlock();
}

int VPoxVHWAImage::vhwaSurfaceBlt(struct VPOXVHWACMD_SURF_BLT RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    Q_UNUSED(pCmd);
    return VERR_NOT_IMPLEMENTED;
}

int VPoxVHWAImage::vhwaSurfaceFlip(struct VPOXVHWACMD_SURF_FLIP RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pTargSurf = handle2Surface(pCmd->u.in.hTargSurf);
    AssertReturn(pTargSurf, VERR_INVALID_PARAMETER);
    VPoxVHWASurfaceBase *pCurrSurf = handle2Surface(pCmd->u.in.hCurrSurf);
    AssertReturn(pCurrSurf, VERR_INVALID_PARAMETER);
    VPOXQGLLOG_ENTER(("pTargSurf (0x%x), pCurrSurf (0x%x)\n", pTargSurf, pCurrSurf));
    vpoxCheckUpdateAddress (pCurrSurf, pCmd->u.in.offCurrSurface);
    vpoxCheckUpdateAddress (pTargSurf, pCmd->u.in.offTargSurface);

    if (pCmd->u.in.xUpdatedTargMemValid)
    {
        QRect r = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.xUpdatedTargMemRect);
        pTargSurf->updatedMem(&r);
    }
    pTargSurf->getComplexList()->setCurrentVisible(pTargSurf);

    mRepaintNeeded = true;
#ifdef DEBUG
    pCurrSurf->cFlipsCurr++;
    pTargSurf->cFlipsTarg++;
#endif

    return VINF_SUCCESS;
}

int VPoxVHWAImage::vhwaSurfaceColorFill(struct VPOXVHWACMD_SURF_COLORFILL RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    NOREF(pCmd);
    return VERR_NOT_IMPLEMENTED;
}

void VPoxVHWAImage::vhwaDoSurfaceOverlayUpdate(VPoxVHWASurfaceBase *pDstSurf, VPoxVHWASurfaceBase *pSrcSurf,
                                               struct VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    if (pCmd->u.in.flags & VPOXVHWA_OVER_KEYDEST)
    {
        VPOXQGLLOG((", KEYDEST"));
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well
         *  i.e.
         * 1. indicate the value is NUL overridden, just set NULL*/
        pSrcSurf->setOverriddenDstOverlayCKey(NULL);
    }
    else if (pCmd->u.in.flags & VPOXVHWA_OVER_KEYDESTOVERRIDE)
    {
        VPOXQGLLOG((", KEYDESTOVERRIDE"));
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well
         *  i.e.
         * 1. indicate the value is overridden (no matter what we write here, bu it should be not NULL)*/
        VPoxVHWAColorKey ckey(pCmd->u.in.desc.DstCK.high, pCmd->u.in.desc.DstCK.low);
        VPOXQGLLOG_CKEY(" ckey: ",&ckey, "\n");
        pSrcSurf->setOverriddenDstOverlayCKey(&ckey);
        /* tell the ckey is enabled */
        pSrcSurf->setDefaultDstOverlayCKey(&ckey);
    }
    else
    {
        VPOXQGLLOG((", no KEYDEST"));
        /* we use src (overlay) surface to maintain overridden dst ckey info
         * to allow multiple overlays have different overridden dst keys for one primary surface */
        /* non-null dstOverlayCKey for overlay would mean the overlay surface contains the overridden
         * dst ckey value in defaultDstOverlayCKey
         * this allows the NULL to be a valid overridden value as well
         * i.e.
         * 1. indicate the value is overridden (no matter what we write here, bu it should be not NULL)*/
        VPoxVHWAColorKey dummyCKey(0, 0);
        pSrcSurf->setOverriddenDstOverlayCKey(&dummyCKey);
        /* tell the ckey is disabled */
        pSrcSurf->setDefaultDstOverlayCKey(NULL);
    }

    if (pCmd->u.in.flags & VPOXVHWA_OVER_KEYSRC)
    {
        VPOXQGLLOG((", KEYSRC"));
        pSrcSurf->resetDefaultSrcOverlayCKey();
    }
    else if (pCmd->u.in.flags & VPOXVHWA_OVER_KEYSRCOVERRIDE)
    {
        VPOXQGLLOG((", KEYSRCOVERRIDE"));
        VPoxVHWAColorKey ckey(pCmd->u.in.desc.SrcCK.high, pCmd->u.in.desc.SrcCK.low);
        pSrcSurf->setOverriddenSrcOverlayCKey(&ckey);
    }
    else
    {
        VPOXQGLLOG((", no KEYSRC"));
        pSrcSurf->setOverriddenSrcOverlayCKey(NULL);
    }
    VPOXQGLLOG(("\n"));
    if (pDstSurf)
    {
        QRect dstRect = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.dstRect);
        QRect srcRect = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.srcRect);

        VPOXQGLLOG(("*******overlay update*******\n"));
        VPOXQGLLOG(("dstSurfSize: w(%d), h(%d)\n", pDstSurf->width(), pDstSurf->height()));
        VPOXQGLLOG(("srcSurfSize: w(%d), h(%d)\n", pSrcSurf->width(), pSrcSurf->height()));
        VPOXQGLLOG_QRECT("dstRect:", &dstRect, "\n");
        VPOXQGLLOG_QRECT("srcRect:", &srcRect, "\n");

        pSrcSurf->setPrimary(pDstSurf);

        pSrcSurf->setRects(dstRect, srcRect);
    }
}

int VPoxVHWAImage::vhwaSurfaceOverlayUpdate(struct VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pSrcSurf = handle2Surface(pCmd->u.in.hSrcSurf);
    AssertReturn(pSrcSurf, VERR_INVALID_PARAMETER);
    VPoxVHWASurfList *pList = pSrcSurf->getComplexList();
    vpoxCheckUpdateAddress (pSrcSurf, pCmd->u.in.offSrcSurface);
    VPOXQGLLOG(("OverlayUpdate: pSrcSurf (0x%x)\n",pSrcSurf));
    VPoxVHWASurfaceBase *pDstSurf = NULL;

    if (pCmd->u.in.hDstSurf)
    {
        pDstSurf = handle2Surface(pCmd->u.in.hDstSurf);
        AssertReturn(pDstSurf, VERR_INVALID_PARAMETER);
        vpoxCheckUpdateAddress (pDstSurf, pCmd->u.in.offDstSurface);
        VPOXQGLLOG(("pDstSurf (0x%x)\n",pDstSurf));
#ifdef DEBUGVHWASTRICT
        Assert(pDstSurf == mDisplay.getVGA());
        Assert(mDisplay.getVGA() == mDisplay.getPrimary());
#endif
        Assert(pDstSurf->getComplexList() == mDisplay.getVGA()->getComplexList());

        if (pCmd->u.in.flags & VPOXVHWA_OVER_SHOW)
        {
            if (pDstSurf != mDisplay.getPrimary())
            {
                mDisplay.updateVGA(pDstSurf);
                pDstSurf->getComplexList()->setCurrentVisible(pDstSurf);
            }
        }
    }

#ifdef VPOX_WITH_WDDM
    if (pCmd->u.in.xFlags & VPOXVHWACMD_SURF_OVERLAY_UPDATE_F_SRCMEMRECT)
    {
        QRect r = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.xUpdatedSrcMemRect);
        pSrcSurf->updatedMem(&r);
    }
    if (pCmd->u.in.xFlags & VPOXVHWACMD_SURF_OVERLAY_UPDATE_F_DSTMEMRECT)
    {
        AssertReturn(pDstSurf, VERR_INVALID_PARAMETER);
        QRect r = VPOXVHWA_CONSTRUCT_QRECT_FROM_RECTL_WH(&pCmd->u.in.xUpdatedDstMemRect);
        pDstSurf->updatedMem(&r);
    }
#endif

    const SurfList & surfaces = pList->surfaces();

    for (SurfList::const_iterator it = surfaces.begin();
             it != surfaces.end(); ++ it)
    {
        VPoxVHWASurfaceBase *pCurSrcSurf = (*it);
        vhwaDoSurfaceOverlayUpdate(pDstSurf, pCurSrcSurf, pCmd);
    }

    if (pCmd->u.in.flags & VPOXVHWA_OVER_HIDE)
    {
        VPOXQGLLOG(("hide\n"));
        pList->setCurrentVisible(NULL);
    }
    else if (pCmd->u.in.flags & VPOXVHWA_OVER_SHOW)
    {
        VPOXQGLLOG(("show\n"));
        pList->setCurrentVisible(pSrcSurf);
    }

    mRepaintNeeded = true;

    return VINF_SUCCESS;
}

int VPoxVHWAImage::vhwaSurfaceOverlaySetPosition(struct VPOXVHWACMD_SURF_OVERLAY_SETPOSITION RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pDstSurf = handle2Surface(pCmd->u.in.hDstSurf);
    AssertReturn(pDstSurf, VERR_INVALID_PARAMETER);
    VPoxVHWASurfaceBase *pSrcSurf = handle2Surface(pCmd->u.in.hSrcSurf);
    AssertReturn(pSrcSurf, VERR_INVALID_PARAMETER);

    VPOXQGLLOG_ENTER(("pDstSurf (0x%x), pSrcSurf (0x%x)\n",pDstSurf,pSrcSurf));

    vpoxCheckUpdateAddress (pSrcSurf, pCmd->u.in.offSrcSurface);
    vpoxCheckUpdateAddress (pDstSurf, pCmd->u.in.offDstSurface);

    VPoxVHWASurfList *pList = pSrcSurf->getComplexList();
    const SurfList & surfaces = pList->surfaces();

    QPoint pos(pCmd->u.in.xPos, pCmd->u.in.yPos);

#ifdef DEBUGVHWASTRICT
    Assert(pDstSurf == mDisplay.getVGA());
    Assert(mDisplay.getVGA() == mDisplay.getPrimary());
#endif
    if (pSrcSurf->getComplexList()->current() != NULL)
    {
        if (pDstSurf != mDisplay.getPrimary())
        {
            mDisplay.updateVGA(pDstSurf);
            pDstSurf->getComplexList()->setCurrentVisible(pDstSurf);
        }
    }

    mRepaintNeeded = true;

    for (SurfList::const_iterator it = surfaces.begin();
             it != surfaces.end(); ++ it)
    {
        VPoxVHWASurfaceBase *pCurSrcSurf = (*it);
        pCurSrcSurf->setTargRectPosition(pos);
    }

    return VINF_SUCCESS;
}

int VPoxVHWAImage::vhwaSurfaceColorkeySet(struct VPOXVHWACMD_SURF_COLORKEY_SET RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPoxVHWASurfaceBase *pSurf = handle2Surface(pCmd->u.in.hSurf);
    AssertReturn(pSurf, VERR_INVALID_PARAMETER);
    VPOXQGLLOG_ENTER(("pSurf (0x%x)\n", pSurf));

    vpoxCheckUpdateAddress (pSurf, pCmd->u.in.offSurface);

    mRepaintNeeded = true;

    if (pCmd->u.in.flags & VPOXVHWA_CKEY_DESTBLT)
    {
        VPoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setDstBltCKey(&ckey);
    }
    if (pCmd->u.in.flags & VPOXVHWA_CKEY_DESTOVERLAY)
    {
        VPoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setDefaultDstOverlayCKey(&ckey);
    }
    if (pCmd->u.in.flags & VPOXVHWA_CKEY_SRCBLT)
    {
        VPoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setSrcBltCKey(&ckey);

    }
    if (pCmd->u.in.flags & VPOXVHWA_CKEY_SRCOVERLAY)
    {
        VPoxVHWAColorKey ckey(pCmd->u.in.CKey.high, pCmd->u.in.CKey.low);
        pSurf->setDefaultSrcOverlayCKey(&ckey);
    }

    return VINF_SUCCESS;
}

int VPoxVHWAImage::vhwaQueryInfo1(struct VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPOXQGLLOG_ENTER(("\n"));
    bool bEnabled = false;
    const VPoxVHWAInfo & info = vpoxVHWAGetSupportInfo(NULL);
    if (info.isVHWASupported())
    {
        Assert(pCmd->u.in.guestVersion.maj == VPOXVHWA_VERSION_MAJ);
        if (pCmd->u.in.guestVersion.maj == VPOXVHWA_VERSION_MAJ)
        {
            Assert(pCmd->u.in.guestVersion.min == VPOXVHWA_VERSION_MIN);
            if (pCmd->u.in.guestVersion.min == VPOXVHWA_VERSION_MIN)
            {
                Assert(pCmd->u.in.guestVersion.bld == VPOXVHWA_VERSION_BLD);
                if (pCmd->u.in.guestVersion.bld == VPOXVHWA_VERSION_BLD)
                {
                    Assert(pCmd->u.in.guestVersion.reserved == VPOXVHWA_VERSION_RSV);
                    if (pCmd->u.in.guestVersion.reserved == VPOXVHWA_VERSION_RSV)
                    {
                        bEnabled = true;
                    }
                }
            }
        }
    }

    memset((void *)pCmd, 0, sizeof(VPOXVHWACMD_QUERYINFO1));
    if (bEnabled)
    {
        pCmd->u.out.cfgFlags = VPOXVHWA_CFG_ENABLED;

        pCmd->u.out.caps =
                    /* we do not support blitting for now */
//                        VPOXVHWA_CAPS_BLT | VPOXVHWA_CAPS_BLTSTRETCH | VPOXVHWA_CAPS_BLTQUEUE
//                                 | VPOXVHWA_CAPS_BLTCOLORFILL not supported, although adding it is trivial
//                                 | VPOXVHWA_CAPS_BLTFOURCC set below if shader support is available
                           VPOXVHWA_CAPS_OVERLAY
                         | VPOXVHWA_CAPS_OVERLAYSTRETCH
                         | VPOXVHWA_CAPS_OVERLAYCANTCLIP
                                 // | VPOXVHWA_CAPS_OVERLAYFOURCC set below if shader support is available
                         ;

        /** @todo check if we could use DDSCAPS_ALPHA instead of colorkeying */

        pCmd->u.out.caps2 = VPOXVHWA_CAPS2_CANRENDERWINDOWED
                                    | VPOXVHWA_CAPS2_WIDESURFACES;

        /// @todo setup stretchCaps
        pCmd->u.out.stretchCaps = 0;

        pCmd->u.out.numOverlays = 1;
        /** @todo set curOverlays properly */
        pCmd->u.out.curOverlays = 0;

        pCmd->u.out.surfaceCaps = VPOXVHWA_SCAPS_PRIMARYSURFACE
#ifndef VPOXVHWA_ALLOW_PRIMARY_AND_OVERLAY_ONLY
                                | VPOXVHWA_SCAPS_OFFSCREENPLAIN
#endif
                                | VPOXVHWA_SCAPS_FLIP
                                | VPOXVHWA_SCAPS_LOCALVIDMEM
                                | VPOXVHWA_SCAPS_OVERLAY
                    //            | VPOXVHWA_SCAPS_BACKBUFFER
                    //            | VPOXVHWA_SCAPS_FRONTBUFFER
                    //            | VPOXVHWA_SCAPS_VIDEOMEMORY
                    //            | VPOXVHWA_SCAPS_COMPLEX
                    //            | VPOXVHWA_SCAPS_VISIBLE
                                ;

        if (info.getGlInfo().isFragmentShaderSupported() && info.getGlInfo().getMultiTexNumSupported() >= 2)
        {
            pCmd->u.out.caps |= VPOXVHWA_CAPS_COLORKEY
                             |  VPOXVHWA_CAPS_COLORKEYHWASSIST;

            pCmd->u.out.colorKeyCaps = 0
//                                   | VPOXVHWA_CKEYCAPS_DESTBLT | VPOXVHWA_CKEYCAPS_DESTBLTCLRSPACE
//                                   | VPOXVHWA_CKEYCAPS_SRCBLT| VPOXVHWA_CKEYCAPS_SRCBLTCLRSPACE
//                                   | VPOXVHWA_CKEYCAPS_SRCOVERLAY | VPOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE
                                     | VPOXVHWA_CKEYCAPS_DESTOVERLAY
                                     | VPOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE;

            if (info.getGlInfo().isTextureRectangleSupported())
            {
                pCmd->u.out.caps |= VPOXVHWA_CAPS_OVERLAYFOURCC
//                               |  VPOXVHWA_CAPS_BLTFOURCC
                                 ;

                pCmd->u.out.colorKeyCaps |= 0
//                                       |  VPOXVHWA_CKEYCAPS_SRCOVERLAYYUV
                                         |  VPOXVHWA_CKEYCAPS_DESTOVERLAYYUV;

//              pCmd->u.out.caps2 |= VPOXVHWA_CAPS2_COPYFOURCC;

                pCmd->u.out.numFourCC = mSettings->getIntersection(info, 0, NULL);
            }
        }
    }

    return VINF_SUCCESS;
}

int VPoxVHWAImage::vhwaQueryInfo2(struct VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    VPOXQGLLOG_ENTER(("\n"));

    const VPoxVHWAInfo &info = vpoxVHWAGetSupportInfo(NULL);
    uint32_t aFourcc[VPOXVHWA_NUMFOURCC];
    int num = mSettings->getIntersection(info, VPOXVHWA_NUMFOURCC, aFourcc);
    Assert(pCmd->numFourCC >= (uint32_t)num);
    if (pCmd->numFourCC < (uint32_t)num)
        return VERR_GENERAL_FAILURE;

    pCmd->numFourCC = (uint32_t)num;
    memcpy((void *)&pCmd->FourCC[0], aFourcc, num * sizeof(aFourcc[0]));
    return VINF_SUCCESS;
}

//static DECLCALLBACK(void) vpoxQGLSaveExec(PSSMHANDLE pSSM, void *pvUser)
//{
//    VPoxVHWAImage * pw = (VPoxVHWAImage*)pvUser;
//    pw->vhwaSaveExec(pSSM);
//}
//
//static DECLCALLBACK(int) vpoxQGLLoadExec(PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version, uint32_t uPass)
//{
//    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
//    VPoxVHWAImage * pw = (VPoxVHWAImage*)pvUser;
//    return VPoxVHWAImage::vhwaLoadExec(&pw->onResizeCmdList(), pSSM, u32Version);
//}

int VPoxVHWAImage::vhwaSaveSurface(struct SSMHANDLE *pSSM, VPoxVHWASurfaceBase *pSurf, uint32_t surfCaps)
{
    VPOXQGL_SAVE_SURFSTART(pSSM);

    uint64_t u64 = vpoxVRAMOffset(pSurf);
    int rc;
    rc = SSMR3PutU32(pSSM, pSurf->handle());
    rc = SSMR3PutU64(pSSM, u64);
    rc = SSMR3PutU32(pSSM, pSurf->width());
    rc = SSMR3PutU32(pSSM, pSurf->height());
    rc = SSMR3PutU32(pSSM, surfCaps);

    uint32_t flags = 0;
    const VPoxVHWAColorKey *pDstBltCKey = pSurf->dstBltCKey();
    const VPoxVHWAColorKey *pSrcBltCKey = pSurf->srcBltCKey();
    const VPoxVHWAColorKey *pDstOverlayCKey = pSurf->dstOverlayCKey();
    const VPoxVHWAColorKey *pSrcOverlayCKey = pSurf->srcOverlayCKey();
    if (pDstBltCKey)
        flags |= VPOXVHWA_SD_CKDESTBLT;
    if (pSrcBltCKey)
        flags |= VPOXVHWA_SD_CKSRCBLT;
    if (pDstOverlayCKey)
        flags |= VPOXVHWA_SD_CKDESTOVERLAY;
    if (pSrcOverlayCKey)
        flags |= VPOXVHWA_SD_CKSRCOVERLAY;
    rc = SSMR3PutU32(pSSM, flags);

    if (pDstBltCKey)
    {
        rc = SSMR3PutU32(pSSM, pDstBltCKey->lower());
        rc = SSMR3PutU32(pSSM, pDstBltCKey->upper());
    }
    if (pSrcBltCKey)
    {
        rc = SSMR3PutU32(pSSM, pSrcBltCKey->lower());
        rc = SSMR3PutU32(pSSM, pSrcBltCKey->upper());
    }
    if (pDstOverlayCKey)
    {
        rc = SSMR3PutU32(pSSM, pDstOverlayCKey->lower());
        rc = SSMR3PutU32(pSSM, pDstOverlayCKey->upper());
    }
    if (pSrcOverlayCKey)
    {
        rc = SSMR3PutU32(pSSM, pSrcOverlayCKey->lower());
        rc = SSMR3PutU32(pSSM, pSrcOverlayCKey->upper());
    }
    AssertRCReturn(rc, rc);

    const VPoxVHWAColorFormat & format = pSurf->pixelFormat();
    flags = 0;
    if (format.fourcc())
    {
        flags |= VPOXVHWA_PF_FOURCC;
        rc = SSMR3PutU32(pSSM, flags);
        rc = SSMR3PutU32(pSSM, format.fourcc());
    }
    else
    {
        flags |= VPOXVHWA_PF_RGB;
        rc = SSMR3PutU32(pSSM, flags);
        rc = SSMR3PutU32(pSSM, format.bitsPerPixel());
        rc = SSMR3PutU32(pSSM, format.r().mask());
        rc = SSMR3PutU32(pSSM, format.g().mask());
        rc = SSMR3PutU32(pSSM, format.b().mask());
        rc = SSMR3PutU32(pSSM, format.a().mask());
    }
    AssertRCReturn(rc, rc);

    VPOXQGL_SAVE_SURFSTOP(pSSM);
    return rc;
}

int VPoxVHWAImage::vhwaLoadSurface(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t cBackBuffers, uint32_t u32Version)
{
    Q_UNUSED(u32Version);

    VPOXQGL_LOAD_SURFSTART(pSSM);

    char *buf = (char*)malloc(VPOXVHWACMD_SIZE(VPOXVHWACMD_SURF_CREATE));
    memset(buf, 0, sizeof(VPOXVHWACMD_SIZE(VPOXVHWACMD_SURF_CREATE)));
    VPOXVHWACMD *pCmd = (VPOXVHWACMD *)buf;
    pCmd->enmCmd = VPOXVHWACMD_TYPE_SURF_CREATE;
    pCmd->Flags = VPOXVHWACMD_FLAG_HH_CMD;

    VPOXVHWACMD_SURF_CREATE *pCreateSurf = VPOXVHWACMD_BODY_HOST_HEAP(pCmd, VPOXVHWACMD_SURF_CREATE);
    int rc;
    uint32_t u32;
    rc = SSMR3GetU32(pSSM, &u32);         AssertRC(rc);
    pCreateSurf->SurfInfo.hSurf = (VPOXVHWA_SURFHANDLE)u32;
    if (RT_SUCCESS(rc))
    {
        rc = SSMR3GetU64(pSSM, &pCreateSurf->SurfInfo.offSurface);      AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.width);           AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.height);          AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.surfCaps);        AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.flags);           AssertRC(rc);
        if (pCreateSurf->SurfInfo.flags & VPOXVHWA_SD_CKDESTBLT)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstBltCK.low);    AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstBltCK.high);   AssertRC(rc);
        }
        if (pCreateSurf->SurfInfo.flags & VPOXVHWA_SD_CKSRCBLT)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcBltCK.low);    AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcBltCK.high);   AssertRC(rc);
        }
        if (pCreateSurf->SurfInfo.flags & VPOXVHWA_SD_CKDESTOVERLAY)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstOverlayCK.low);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.DstOverlayCK.high); AssertRC(rc);
        }
        if (pCreateSurf->SurfInfo.flags & VPOXVHWA_SD_CKSRCOVERLAY)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcOverlayCK.low);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.SrcOverlayCK.high); AssertRC(rc);
        }

        rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.flags);     AssertRC(rc);
        if (pCreateSurf->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_RGB)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.c.rgbBitCount);   AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m1.rgbRBitMask);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m2.rgbGBitMask);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m3.rgbBBitMask);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.m4.rgbABitMask);  AssertRC(rc);
        }
        else if (pCreateSurf->SurfInfo.PixelFormat.flags & VPOXVHWA_PF_FOURCC)
        {
            rc = SSMR3GetU32(pSSM, &pCreateSurf->SurfInfo.PixelFormat.fourCC);
            AssertRC(rc);
        }
        else
        {
            AssertFailed();
        }
        AssertRCReturnStmt(rc, free(buf), rc);

        if (cBackBuffers)
        {
            pCreateSurf->SurfInfo.cBackBuffers = cBackBuffers;
            pCreateSurf->SurfInfo.surfCaps |= VPOXVHWA_SCAPS_COMPLEX;
        }

        pCmdList->push_back(pCmd);
//        vpoxExecOnResize(&VPoxVHWAImage::vpoxDoVHWACmdAndFree, pCmd); AssertRC(rc);
//        if (RT_SUCCESS(rc))
//        {
//            rc = pCmd->rc;
//            AssertRC(rc);
//        }
    }
    else
        free(buf);

    VPOXQGL_LOAD_SURFSTOP(pSSM);

    return rc;
}

int VPoxVHWAImage::vhwaSaveOverlayData(struct SSMHANDLE * pSSM, VPoxVHWASurfaceBase *pSurf, bool bVisible)
{
    VPOXQGL_SAVE_OVERLAYSTART(pSSM);

    uint32_t flags = 0;
    const VPoxVHWAColorKey * dstCKey = pSurf->dstOverlayCKey();
    const VPoxVHWAColorKey * defaultDstCKey = pSurf->defaultDstOverlayCKey();
    const VPoxVHWAColorKey * srcCKey = pSurf->srcOverlayCKey();;
    const VPoxVHWAColorKey * defaultSrcCKey = pSurf->defaultSrcOverlayCKey();
    bool bSaveDstCKey = false;
    bool bSaveSrcCKey = false;

    if (bVisible)
    {
        flags |= VPOXVHWA_OVER_SHOW;
    }
    else
    {
        flags |= VPOXVHWA_OVER_HIDE;
    }

    if (!dstCKey)
    {
        flags |= VPOXVHWA_OVER_KEYDEST;
    }
    else if (defaultDstCKey)
    {
        flags |= VPOXVHWA_OVER_KEYDESTOVERRIDE;
        bSaveDstCKey = true;
    }

    if (srcCKey == defaultSrcCKey)
    {
        flags |= VPOXVHWA_OVER_KEYSRC;
    }
    else if (srcCKey)
    {
        flags |= VPOXVHWA_OVER_KEYSRCOVERRIDE;
        bSaveSrcCKey = true;
    }

    int rc = SSMR3PutU32(pSSM, flags);

    rc = SSMR3PutU32(pSSM, mDisplay.getPrimary()->handle());
    rc = SSMR3PutU32(pSSM, pSurf->handle());

    if (bSaveDstCKey)
    {
        rc = SSMR3PutU32(pSSM, dstCKey->lower());
        rc = SSMR3PutU32(pSSM, dstCKey->upper());
    }
    if (bSaveSrcCKey)
    {
        rc = SSMR3PutU32(pSSM, srcCKey->lower());
        rc = SSMR3PutU32(pSSM, srcCKey->upper());
    }

    int x1, x2, y1, y2;
    pSurf->targRect().getCoords(&x1, &y1, &x2, &y2);
    rc = SSMR3PutS32(pSSM, x1);
    rc = SSMR3PutS32(pSSM, x2+1);
    rc = SSMR3PutS32(pSSM, y1);
    rc = SSMR3PutS32(pSSM, y2+1);

    pSurf->srcRect().getCoords(&x1, &y1, &x2, &y2);
    rc = SSMR3PutS32(pSSM, x1);
    rc = SSMR3PutS32(pSSM, x2+1);
    rc = SSMR3PutS32(pSSM, y1);
    rc = SSMR3PutS32(pSSM, y2+1);
    AssertRCReturn(rc, rc);

    VPOXQGL_SAVE_OVERLAYSTOP(pSSM);

    return rc;
}

int VPoxVHWAImage::vhwaLoadOverlayData(VHWACommandList *pCmdList, struct SSMHANDLE *pSSM, uint32_t u32Version)
{
    Q_UNUSED(u32Version);

    VPOXQGL_LOAD_OVERLAYSTART(pSSM);

    char *buf = (char *)malloc(VPOXVHWACMD_SIZE(VPOXVHWACMD_SURF_CREATE)); /** @todo r=andy Any reason not using the RTMem API? */
    memset(buf, 0, VPOXVHWACMD_SIZE(VPOXVHWACMD_SURF_CREATE));
    VPOXVHWACMD * pCmd = (VPOXVHWACMD*)buf;
    pCmd->enmCmd = VPOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE;
    pCmd->Flags = VPOXVHWACMD_FLAG_HH_CMD;

    VPOXVHWACMD_SURF_OVERLAY_UPDATE *pUpdateOverlay = VPOXVHWACMD_BODY_HOST_HEAP(pCmd, VPOXVHWACMD_SURF_OVERLAY_UPDATE);
    int rc;

    rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.flags); AssertRC(rc);
    uint32_t hSrc, hDst;
    rc = SSMR3GetU32(pSSM, &hDst); AssertRC(rc);
    rc = SSMR3GetU32(pSSM, &hSrc); AssertRC(rc);
    pUpdateOverlay->u.in.hSrcSurf = hSrc;
    pUpdateOverlay->u.in.hDstSurf = hDst;
    {
        pUpdateOverlay->u.in.offDstSurface = VPOXVHWA_OFFSET64_VOID;
        pUpdateOverlay->u.in.offSrcSurface = VPOXVHWA_OFFSET64_VOID;

        if (pUpdateOverlay->u.in.flags & VPOXVHWA_OVER_KEYDESTOVERRIDE)
        {
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.DstCK.low);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.DstCK.high); AssertRC(rc);
        }

        if (pUpdateOverlay->u.in.flags & VPOXVHWA_OVER_KEYSRCOVERRIDE)
        {
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.SrcCK.low);  AssertRC(rc);
            rc = SSMR3GetU32(pSSM, &pUpdateOverlay->u.in.desc.SrcCK.high); AssertRC(rc);
        }

        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.left);   AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.right);  AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.top);    AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.dstRect.bottom); AssertRC(rc);

        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.left);   AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.right);  AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.top);    AssertRC(rc);
        rc = SSMR3GetS32(pSSM, &pUpdateOverlay->u.in.srcRect.bottom);
        AssertRCReturnStmt(rc, free(buf), rc);

        pCmdList->push_back(pCmd);
    }

    VPOXQGL_LOAD_OVERLAYSTOP(pSSM);

    return rc;
}

void VPoxVHWAImage::vhwaSaveExecVoid(struct SSMHANDLE *pSSM)
{
    VPOXQGL_SAVE_START(pSSM);
    SSMR3PutU32(pSSM, 0); /* 0 primaries */
    VPOXQGL_SAVE_STOP(pSSM);
}

void VPoxVHWAImage::vhwaSaveExec(struct SSMHANDLE *pSSM)
{
    VPOXQGL_SAVE_START(pSSM);

    /* the mechanism of restoring data is based on generating VHWA commands that restore the surfaces state
     * the following commands are generated:
     * I.    CreateSurface
     * II.   UpdateOverlay
     *
     * Data format is the following:
     * I.    u32 - Num primary surfaces - (the current frontbuffer is detected in the stored surf flags which are posted to the generated CreateSurface cmd)
     * II.   for each primary surf
     * II.1    generate & execute CreateSurface cmd (see below on the generation logic)
     * III.  u32 - Num overlays
     * IV.   for each overlay
     * IV.1    u32 - Num surfaces in overlay (the current frontbuffer is detected in the stored surf flags which are posted to the generated CreateSurface cmd)
     * IV.2    for each surface in overlay
     * IV.2.a    generate & execute CreateSurface cmd (see below on the generation logic)
     * IV.2.b    generate & execute UpdateOverlay cmd (see below on the generation logic)
     *
     */
    const SurfList & primaryList = mDisplay.primaries().surfaces();
    uint32_t cPrimary = (uint32_t)primaryList.size();
    if (   cPrimary
        && (   mDisplay.getVGA() == NULL
            || mDisplay.getVGA()->handle() == VPOXVHWA_SURFHANDLE_INVALID))
    {
        cPrimary -= 1;
    }

    int rc = SSMR3PutU32(pSSM, cPrimary);
    AssertRCReturnVoid(rc);
    if (cPrimary)
    {
        for (SurfList::const_iterator pr = primaryList.begin(); pr != primaryList.end(); ++ pr)
        {
            VPoxVHWASurfaceBase *pSurf = *pr;
    //        bool bVga = (pSurf == mDisplay.getVGA());
            bool bVisible = (pSurf == mDisplay.getPrimary());
            uint32_t flags = VPOXVHWA_SCAPS_PRIMARYSURFACE;
            if (bVisible)
                flags |= VPOXVHWA_SCAPS_VISIBLE;

            if (pSurf->handle() != VPOXVHWA_SURFHANDLE_INVALID)
            {
                rc = vhwaSaveSurface(pSSM, *pr, flags);
#ifdef DEBUG
                --cPrimary;
                Assert(cPrimary < UINT32_MAX / 2);
#endif
            }
            else
            {
                Assert(pSurf == mDisplay.getVGA());
            }
        }

#ifdef DEBUG
        Assert(!cPrimary);
#endif

        const OverlayList & overlays = mDisplay.overlays();
        rc = SSMR3PutU32(pSSM, (uint32_t)overlays.size());

        for (OverlayList::const_iterator it = overlays.begin(); it != overlays.end(); ++ it)
        {
            VPoxVHWASurfList * pSurfList = *it;
            const SurfList & surfaces = pSurfList->surfaces();
            uint32_t cSurfs = (uint32_t)surfaces.size();
            uint32_t flags = VPOXVHWA_SCAPS_OVERLAY;
            if (cSurfs > 1)
                flags |= VPOXVHWA_SCAPS_COMPLEX;
            rc = SSMR3PutU32(pSSM, cSurfs);
            for (SurfList::const_iterator sit = surfaces.begin(); sit != surfaces.end(); ++ sit)
                rc = vhwaSaveSurface(pSSM, *sit, flags);

            bool bVisible = true;
            VPoxVHWASurfaceBase * pOverlayData = pSurfList->current();
            if (!pOverlayData)
            {
                pOverlayData = surfaces.front();
                bVisible = false;
            }

            rc = vhwaSaveOverlayData(pSSM, pOverlayData, bVisible);
        }
    }

    VPOXQGL_SAVE_STOP(pSSM);
}

int VPoxVHWAImage::vhwaLoadVHWAEnable(VHWACommandList * pCmdList)
{
    char *buf = (char *)malloc(sizeof(VPOXVHWACMD));
    Assert(buf);
    if (buf)
    {
        memset(buf, 0, sizeof(VPOXVHWACMD));
        VPOXVHWACMD *pCmd = (VPOXVHWACMD *)buf;
        pCmd->enmCmd = VPOXVHWACMD_TYPE_ENABLE;
        pCmd->Flags = VPOXVHWACMD_FLAG_HH_CMD;
        pCmdList->push_back(pCmd);
        return VINF_SUCCESS;
    }

    return VERR_OUT_OF_RESOURCES;
}

int VPoxVHWAImage::vhwaLoadExec(VHWACommandList *pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version)
{
    VPOXQGL_LOAD_START(pSSM);

    if (u32Version > VPOXQGL_STATE_VERSION)
        return VERR_VERSION_MISMATCH;

    int rc;
    uint32_t u32;

    rc = vhwaLoadVHWAEnable(pCmdList); AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = SSMR3GetU32(pSSM, &u32); AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            if (u32Version == 1U && u32 == UINT32_MAX) /* work around the v1 bug */
                u32 = 0;
            if (u32)
            {
                for (uint32_t i = 0; i < u32; ++i)
                {
                    rc = vhwaLoadSurface(pCmdList, pSSM, 0, u32Version);
                    AssertRCBreak(rc);
                }

                if (RT_SUCCESS(rc))
                {
                    rc = SSMR3GetU32(pSSM, &u32); AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        for (uint32_t i = 0; i < u32; ++i)
                        {
                            uint32_t cSurfs;
                            rc = SSMR3GetU32(pSSM, &cSurfs); AssertRC(rc);
                            for (uint32_t j = 0; j < cSurfs; ++j)
                            {
                                rc = vhwaLoadSurface(pCmdList, pSSM, cSurfs - 1, u32Version);
                                AssertRCBreak(rc);
                            }

                            if (RT_SUCCESS(rc))
                            {
                                rc = vhwaLoadOverlayData(pCmdList, pSSM, u32Version);
                                AssertRCBreak(rc);
                            }
                            else
                                break;
                        }
                    }
                }
            }
#ifdef VPOXQGL_STATE_DEBUG
            else if (u32Version == 1) /* read the 0 overlay count to ensure the following VPOXQGL_LOAD_STOP succeeds */
            {
                rc = SSMR3GetU32(pSSM, &u32);
                AssertRC(rc);
                Assert(u32 == 0 || RT_FAILURE(rc));
            }
#endif
        }
    }

    VPOXQGL_LOAD_STOP(pSSM);

    return rc;
}

int VPoxVHWAImage::vhwaConstruct(struct VPOXVHWACMD_HH_CONSTRUCT *pCmd)
{
//    PVM pVM = (PVM)pCmd->pVM;
//    uint32_t intsId = 0; /** @todo set the proper id */
//
//    char nameFuf[sizeof(VPOXQGL_STATE_NAMEBASE) + 8];
//
//    char * pszName = nameFuf;
//    sprintf(pszName, "%s%d", VPOXQGL_STATE_NAMEBASE, intsId);
//    int rc = SSMR3RegisterExternal(
//            pVM,                    /* The VM handle*/
//            pszName,                /* Data unit name. */
//            intsId,                 /* The instance identifier of the data unit.
//                                     * This must together with the name be unique. */
//            VPOXQGL_STATE_VERSION,   /* Data layout version number. */
//            128,             /* The approximate amount of data in the unit.
//                              * Only for progress indicators. */
//            NULL, NULL, NULL, /* pfnLiveXxx */
//            NULL,            /* Prepare save callback, optional. */
//            vpoxQGLSaveExec, /* Execute save callback, optional. */
//            NULL,            /* Done save callback, optional. */
//            NULL,            /* Prepare load callback, optional. */
//            vpoxQGLLoadExec, /* Execute load callback, optional. */
//            NULL,            /* Done load callback, optional. */
//            this             /* User argument. */
//            );
//    AssertRC(rc);
    mpvVRAM = pCmd->pvVRAM;
    mcbVRAM = pCmd->cbVRAM;
    return VINF_SUCCESS;
}

uchar *VPoxVHWAImage::vpoxVRAMAddressFromOffset(uint64_t offset)
{
    if (offset == VPOXVHWA_OFFSET64_VOID)
        return NULL;
    AssertReturn(offset <= vramSize(), NULL);
    RT_UNTRUSTED_VALIDATED_FENCE();
    return (uint8_t *)vramBase() + offset;
}

uint64_t VPoxVHWAImage::vpoxVRAMOffsetFromAddress(uchar *addr)
{
    AssertReturn((uintptr_t)addr >= (uintptr_t)vramBase(), VPOXVHWA_OFFSET64_VOID);
    uint64_t const offset = uint64_t((uintptr_t)addr - (uintptr_t)vramBase());
    AssertReturn(offset <= vramSize(), VPOXVHWA_OFFSET64_VOID);
    return offset;
}

uint64_t VPoxVHWAImage::vpoxVRAMOffset(VPoxVHWASurfaceBase *pSurf)
{
    return pSurf->addressAlocated() ? VPOXVHWA_OFFSET64_VOID : vpoxVRAMOffsetFromAddress(pSurf->address());
}

#endif

#ifdef VPOXQGL_DBG_SURF

int g_iCur = 0;
VPoxVHWASurfaceBase *g_apSurf[] = {NULL, NULL, NULL};

void VPoxVHWAImage::vpoxDoTestSurfaces(void *context)
{
    if (g_iCur >= RT_ELEMENTS(g_apSurf))
        g_iCur = 0;
    VPoxVHWASurfaceBase * pSurf1 = g_apSurf[g_iCur];
    if (pSurf1)
        pSurf1->getComplexList()->setCurrentVisible(pSurf1);
}
#endif

void VPoxVHWAImage::vpoxDoUpdateViewport(const QRect & aRect)
{
    adjustViewport(mDisplay.getPrimary()->size(), aRect);
    mViewport = aRect;

    const SurfList &primaryList = mDisplay.primaries().surfaces();

    for (SurfList::const_iterator pr = primaryList.begin(); pr != primaryList.end(); ++pr)
    {
        VPoxVHWASurfaceBase *pSurf = *pr;
        pSurf->updateVisibility(NULL, aRect, false, false);
    }

    const OverlayList & overlays = mDisplay.overlays();
    QRect overInter = overlaysRectIntersection();
    overInter = overInter.intersected(aRect);

    bool bDisplayPrimary = true;

    for (OverlayList::const_iterator it = overlays.begin(); it != overlays.end(); ++it)
    {
        VPoxVHWASurfList *pSurfList = *it;
        const SurfList &surfaces = pSurfList->surfaces();
        if (surfaces.size())
        {
            bool bNotIntersected = !overInter.isEmpty() && surfaces.front()->targRect().contains(overInter);
            Assert(bNotIntersected);

            bDisplayPrimary &= !bNotIntersected;
            for (SurfList::const_iterator sit = surfaces.begin();
                 sit != surfaces.end(); ++ sit)
            {
                VPoxVHWASurfaceBase *pSurf = *sit;
                pSurf->updateVisibility(mDisplay.getPrimary(), aRect, bNotIntersected, false);
            }
        }
    }

    Assert(!bDisplayPrimary);
    mDisplay.setDisplayPrimary(bDisplayPrimary);
}

bool VPoxVHWAImage::hasSurfaces() const
{
    if (mDisplay.overlays().size() != 0)
        return true;
    if (mDisplay.primaries().size() > 1)
        return true;
    /* in case gl was never turned on, we have no surfaces at all including VGA */
    if (!mDisplay.getVGA())
        return false;
    return mDisplay.getVGA()->handle() != VPOXVHWA_SURFHANDLE_INVALID;
}

bool VPoxVHWAImage::hasVisibleOverlays()
{
    const OverlayList &overlays = mDisplay.overlays();
    for (OverlayList::const_iterator it = overlays.begin(); it != overlays.end(); ++ it)
    {
        VPoxVHWASurfList * pSurfList = *it;
        if (pSurfList->current() != NULL)
            return true;
    }
    return false;
}

QRect VPoxVHWAImage::overlaysRectUnion()
{
    const OverlayList &overlays = mDisplay.overlays();
    VPoxVHWADirtyRect un;
    for (OverlayList::const_iterator it = overlays.begin(); it != overlays.end(); ++ it)
    {
        VPoxVHWASurfaceBase *pOverlay = (*it)->current();
        if (pOverlay != NULL)
            un.add(pOverlay->targRect());
    }
    return un.toRect();
}

QRect VPoxVHWAImage::overlaysRectIntersection()
{
    const OverlayList &overlays = mDisplay.overlays();
    QRect rect;
    VPoxVHWADirtyRect un;
    for (OverlayList::const_iterator it = overlays.begin(); it != overlays.end(); ++ it)
    {
        VPoxVHWASurfaceBase *pOverlay = (*it)->current();
        if (pOverlay != NULL)
        {
            if (rect.isNull())
                rect = pOverlay->targRect();
            else
            {
                rect = rect.intersected(pOverlay->targRect());
                if (rect.isNull())
                    break;
            }
        }
    }
    return rect;
}

void VPoxVHWAImage::vpoxDoUpdateRect(const QRect *pRect)
{
    mDisplay.getPrimary()->updatedMem(pRect);
}

void VPoxVHWAImage::resize(const VPoxFBSizeInfo &size)
{
    VPOXQGL_CHECKERR(
            vpoxglActiveTexture(GL_TEXTURE0);
        );

    bool remind = false;
    bool fallback = false;

    VPOXQGLLOG(("resizing: fmt=%d, vram=%p, bpp=%d, bpl=%d, width=%d, height=%d\n",
                size.pixelFormat(), size.VRAM(),
                size.bitsPerPixel(), size.bytesPerLine(),
                size.width(), size.height()));

    /* clean the old values first */

    ulong    bytesPerLine = 0; /* Shut up MSC. */
    uint32_t bitsPerPixel = 0; /* Shut up MSC. */
    uint32_t b =     0xff;
    uint32_t g =   0xff00;
    uint32_t r = 0xff0000;
    bool fUsesGuestVram = false; /* Shut up MSC. */

    /* check if we support the pixel format and can use the guest VRAM directly */
    if (size.pixelFormat() == KBitmapFormat_BGR)
    {

        bitsPerPixel = size.bitsPerPixel();
        bytesPerLine = size.bytesPerLine();
        ulong bitsPerLine = bytesPerLine * 8;

        switch (bitsPerPixel)
        {
            case 32:
                break;
            case 24:
#ifdef DEBUG_misha
                AssertFailed();
#endif
                break;
            case 8:
#ifdef DEBUG_misha
                AssertFailed();
#endif
                g = b = 0;
                remind = true;
                break;
            case 1:
#ifdef DEBUG_misha
                AssertFailed();
#endif
                r = 1;
                g = b = 0;
                remind = true;
                break;
            default:
#ifdef DEBUG_misha
                AssertFailed();
#endif
                remind = true;
                fallback = true;
                break;
        }

        if (!fallback)
        {
            /* QImage only supports 32-bit aligned scan lines... */
            Assert ((size.bytesPerLine() & 3) == 0);
            fallback = ((size.bytesPerLine() & 3) != 0);
            Assert(!fallback);
        }
        if (!fallback)
        {
            /* ...and the scan lines ought to be a whole number of pixels. */
            Assert ((bitsPerLine & (size.bitsPerPixel() - 1)) == 0);
            fallback = ((bitsPerLine & (size.bitsPerPixel() - 1)) != 0);
            Assert(!fallback);
        }
        if (!fallback)
        {
            // ulong virtWdt = bitsPerLine / size.bitsPerPixel();
            fUsesGuestVram = true;
        }
    }
    else
    {
        AssertFailed();
        fallback = true;
    }

    if (fallback)
    {
        /* we should never come to fallback more now */
        AssertFailed();
        /* we don't support either the pixel format or the color depth,
         * fallback to a self-provided 32bpp RGB buffer */
        bitsPerPixel = 32;
        b = 0xff;
        g = 0xff00;
        r = 0xff0000;
        bytesPerLine = size.width() * bitsPerPixel / 8;
        fUsesGuestVram = false;
    }

    ulong bytesPerPixel = bitsPerPixel / 8;
    const QSize scaledSize = size.scaledSize();
    const ulong displayWidth = scaledSize.isValid() ? scaledSize.width() : bytesPerLine / bytesPerPixel;
    const ulong displayHeight = scaledSize.isValid() ? scaledSize.height() : size.height();

#ifdef VPOXQGL_DBG_SURF
    for (int i = 0; i < RT_ELEMENTS(g_apSurf); i++)
    {
        VPoxVHWASurfaceBase * pSurf1 = g_apSurf[i];
        if (pSurf1)
        {
            VPoxVHWASurfList *pConstructingList = pSurf1->getComplexList();
            delete pSurf1;
            if (pConstructingList)
                delete pConstructingList;
        }
    }
#endif

    VPoxVHWASurfaceBase *pDisplay = mDisplay.setVGA(NULL);
    if (pDisplay)
        delete pDisplay;

    VPoxVHWAColorFormat format(bitsPerPixel, r,g,b);
    QSize dispSize(displayWidth, displayHeight);
    QRect dispRect(0, 0, displayWidth, displayHeight);
    pDisplay = new VPoxVHWASurfaceBase(this,
                                       dispSize,
                                       dispRect,
                                       dispRect,
                                       dispRect, /* we do not know viewport at the stage of precise, set as a
                                                    disp rect, it will be updated on repaint */
                                       format,
                                       NULL, NULL, NULL, NULL,
#ifdef VPOXVHWA_USE_TEXGROUP
                                       0,
#endif
                                       0 /* VPOXVHWAIMG_TYPE fFlags */);
    pDisplay->init(NULL, fUsesGuestVram ? size.VRAM() : NULL);
    mDisplay.setVGA(pDisplay);
//    VPOXQGLLOG(("\n\n*******\n\n     viewport size is: (%d):(%d)\n\n*******\n\n", size().width(), size().height()));
    mViewport = QRect(0,0,displayWidth, displayHeight);
    adjustViewport(dispSize, mViewport);
    setupMatricies(dispSize, true);

#ifdef VPOXQGL_DBG_SURF
    {
        uint32_t width = 100;
        uint32_t height = 60;

        for (int i = 0; i < RT_ELEMENTS(g_apSurf); i++)
        {
            VPoxVHWAColorFormat tmpFormat(FOURCC_YV12);
            QSize tmpSize(width, height) ;
            VPoxVHWASurfaceBase *pSurf1 = new VPoxVHWASurfaceBase(this, tmpSize,
                                                                  mDisplay.getPrimary()->rect(),
                                                                  QRect(0, 0, width, height),
                                                                  mViewport,
                                                                  tmpFormat,
                                                                  NULL, NULL, NULL, &VPoxVHWAColorKey(0,0),
#ifdef VPOXVHWA_USE_TEXGROUP
                                                                  0,
#endif
                                                                  false);

            Assert(mDisplay.getVGA());
            pSurf1->init(mDisplay.getVGA(), NULL);
            uchar *addr = pSurf1->address();
            uchar cur = 0;
            for (uint32_t k = 0; k < width*height; k++)
            {
                addr[k] = cur;
                cur+=64;
            }
            pSurf1->updatedMem(&QRect(0,0,width, height));

            VPoxVHWASurfList *pConstructingList = new VPoxVHWASurfList();
            mDisplay.addOverlay(pConstructingList);
            pConstructingList->add(pSurf1);
            g_apSurf[i] = pSurf1;

        }

        VPOXVHWACMD_SURF_OVERLAY_UPDATE updateCmd;
        memset(&updateCmd, 0, sizeof(updateCmd));
        updateCmd.u.in.hSrcSurf = (VPOXVHWA_SURFHANDLE)g_apSurf[0];
        updateCmd.u.in.hDstSurf = (VPOXVHWA_SURFHANDLE)pDisplay;
        updateCmd.u.in.flags = VPOXVHWA_OVER_SHOW
                             | VPOXVHWA_OVER_KEYDESTOVERRIDE;

        updateCmd.u.in.desc.DstCK.high = 1;
        updateCmd.u.in.desc.DstCK.low = 1;

        updateCmd.u.in.dstRect.left = 0;
        updateCmd.u.in.dstRect.right = pDisplay->width();
        updateCmd.u.in.dstRect.top = (pDisplay->height() - height) / 2;
        updateCmd.u.in.dstRect.bottom = updateCmd.u.in.dstRect.top + height;

        updateCmd.u.in.srcRect.left = 0;
        updateCmd.u.in.srcRect.right = width;
        updateCmd.u.in.srcRect.top = 0;
        updateCmd.u.in.srcRect.bottom = height;

        updateCmd.u.in.offDstSurface = VPOXVHWA_OFFSET64_VOID; /* just a magic to avoid surf mem buffer change  */
        updateCmd.u.in.offSrcSurface = VPOXVHWA_OFFSET64_VOID; /* just a magic to avoid surf mem buffer change  */

        vhwaSurfaceOverlayUpdate(&updateCmd);
    }
#endif

//    if (!mOnResizeCmdList.empty())
//    {
//        for (VHWACommandList::const_iterator it = mOnResizeCmdList.begin();
//             it != mOnResizeCmdList.end(); ++ it)
//        {
//            VPOXVHWACMD * pCmd = (*it);
//            vpoxDoVHWACmdExec(pCmd);
//            free(pCmd);
//        }
//        mOnResizeCmdList.clear();
//    }

    if (remind)
        popupCenter().remindAboutWrongColorDepth(windowManager().mainWindowShown(), size.bitsPerPixel(), 32);
    else
        popupCenter().forgetAboutWrongColorDepth(windowManager().mainWindowShown());
}

VPoxVHWAColorFormat::VPoxVHWAColorFormat (uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b)
    : mWidthCompression(1)
    , mHeightCompression(1)
{
    init(bitsPerPixel, r, g, b);
}

VPoxVHWAColorFormat::VPoxVHWAColorFormat(uint32_t fourcc)
    : mWidthCompression(1)
    , mHeightCompression(1)
{
    init(fourcc);
}

void VPoxVHWAColorFormat::init(uint32_t fourcc)
{
    mDataFormat = fourcc;
    mInternalFormat = GL_RGBA8;//GL_RGB;
    mFormat = GL_BGRA_EXT;//GL_RGBA;
    mType = GL_UNSIGNED_BYTE;
    mR = VPoxVHWAColorComponent(0xff);
    mG = VPoxVHWAColorComponent(0xff);
    mB = VPoxVHWAColorComponent(0xff);
    mA = VPoxVHWAColorComponent(0xff);
    mBitsPerPixelTex = 32;

    switch(fourcc)
    {
        case FOURCC_AYUV:
            mBitsPerPixel = 32;
            mWidthCompression = 1;
            break;
        case FOURCC_UYVY:
        case FOURCC_YUY2:
            mBitsPerPixel = 16;
            mWidthCompression = 2;
            break;
        case FOURCC_YV12:
            mBitsPerPixel = 8;
            mWidthCompression = 4;
            break;
        default:
            AssertFailed();
            mBitsPerPixel = 0;
            mBitsPerPixelTex = 0;
            mWidthCompression = 0;
            break;
    }
}

void VPoxVHWAColorFormat::init(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b)
{
    mBitsPerPixel = bitsPerPixel;
    mBitsPerPixelTex = bitsPerPixel;
    mDataFormat = 0;
    switch (bitsPerPixel)
    {
        case 32:
            mInternalFormat = GL_RGB;//3;//GL_RGB;
            mFormat = GL_BGRA_EXT;//GL_RGBA;
            mType = GL_UNSIGNED_BYTE;
            mR = VPoxVHWAColorComponent(r);
            mG = VPoxVHWAColorComponent(g);
            mB = VPoxVHWAColorComponent(b);
            break;
        case 24:
#ifdef DEBUG_misha
            AssertFailed();
#endif
            mInternalFormat = 3;//GL_RGB;
            mFormat = GL_BGR_EXT;
            mType = GL_UNSIGNED_BYTE;
            mR = VPoxVHWAColorComponent(r);
            mG = VPoxVHWAColorComponent(g);
            mB = VPoxVHWAColorComponent(b);
            break;
        case 16:
#ifdef DEBUG_misha
            AssertFailed();
#endif
            mInternalFormat = GL_RGB5;
            mFormat = GL_BGR_EXT;
            mType = GL_UNSIGNED_BYTE; /** @todo ??? */
            mR = VPoxVHWAColorComponent(r);
            mG = VPoxVHWAColorComponent(g);
            mB = VPoxVHWAColorComponent(b);
            break;
        case 8:
#ifdef DEBUG_misha
            AssertFailed();
#endif
            mInternalFormat = 1;//GL_RGB;
            mFormat = GL_RED;//GL_RGB;
            mType = GL_UNSIGNED_BYTE;
            mR = VPoxVHWAColorComponent(0xff);
            break;
        case 1:
#ifdef DEBUG_misha
            AssertFailed();
#endif
            mInternalFormat = 1;
            mFormat = GL_COLOR_INDEX;
            mType = GL_BITMAP;
            mR = VPoxVHWAColorComponent(0x1);
            break;
        default:
#ifdef DEBUG_misha
            AssertFailed();
#endif
            mBitsPerPixel = 0;
            mBitsPerPixelTex = 0;
            break;
    }
}

bool VPoxVHWAColorFormat::equals(const VPoxVHWAColorFormat &other) const
{
    if (fourcc())
        return fourcc() == other.fourcc();
    if (other.fourcc())
        return false;

    return bitsPerPixel() == other.bitsPerPixel();
}

VPoxVHWAColorComponent::VPoxVHWAColorComponent (uint32_t aMask)
{
    unsigned f = ASMBitFirstSetU32(aMask);
    if (f)
    {
        mOffset = f - 1;
        f = ASMBitFirstSetU32(~(aMask >> mOffset));
        if (f)
            mcBits = f - 1;
        else
            mcBits = 32 - mOffset;

        Assert(mcBits);
        mMask = (((uint32_t)0xffffffff) >> (32 - mcBits)) << mOffset;
        Assert(mMask == aMask);

        mRange = (mMask >> mOffset) + 1;
    }
    else
    {
        mMask = 0;
        mRange = 0;
        mOffset = 32;
        mcBits = 0;
    }
}

void VPoxVHWAColorFormat::pixel2Normalized(uint32_t pix, float *r, float *g, float *b) const
{
    *r = mR.colorValNorm(pix);
    *g = mG.colorValNorm(pix);
    *b = mB.colorValNorm(pix);
}

VPoxQGLOverlay::VPoxQGLOverlay()
    : mpOverlayWgt(NULL)
    , mpViewport(NULL)
    , mGlOn(false)
    , mOverlayWidgetVisible(false)
    , mOverlayVisible(false)
    , mGlCurrent(false)
    , mProcessingCommands(false)
    , mNeedOverlayRepaint(false)
    , mNeedSetVisible(false)
    , mCmdPipe()
    , mSettings()
    , mpSession()
    , mpShareWgt(NULL)
    , m_id(0)
{
    /* postpone the gl widget initialization to avoid conflict with 3D on Mac */
}

void VPoxQGLOverlay::init(QWidget *pViewport, QObject *pPostEventObject, CSession *aSession, uint32_t id)
{
    mpViewport = pViewport;
    mpSession = aSession;
    m_id = id;
    mSettings.init(*aSession);
    mCmdPipe.init(pPostEventObject);
}

class VPoxGLShareWgt : public QGLWidget
{
public:
    VPoxGLShareWgt()
        : QGLWidget(new VPoxGLContext(VPoxVHWAImage::vpoxGLFormat()))
    {
        /* work-around to disable done current needed to old ATI drivers on Linux */
        VPoxGLContext *pc = (VPoxGLContext *)context();
        pc->allowDoneCurrent(false);
    }

protected:
    void initializeGL()
    {
        vpoxVHWAGetSupportInfo(context());
        VPoxVHWASurfaceBase::globalInit();
    }
};
void VPoxQGLOverlay::initGl()
{
    if (mpOverlayWgt)
    {
        Assert(mpShareWgt);
        return;
    }

    if (!mpShareWgt)
    {
        mpShareWgt = new VPoxGLShareWgt();
        /* force initializeGL */
        mpShareWgt->updateGL();
    }

    mOverlayImage.init(&mSettings);
    mpOverlayWgt = new VPoxGLWgt(&mOverlayImage, mpViewport, mpShareWgt);

    mOverlayWidgetVisible = true; /* to ensure it is set hidden with vpoxShowOverlay */
    vpoxShowOverlay(false);

    mpOverlayWgt->setMouseTracking(true);
}

void VPoxQGLOverlay::updateAttachment(QWidget *pViewport, QObject *pPostEventObject)
{
    if (mpViewport != pViewport)
    {
        mpViewport = pViewport;
        mpOverlayWgt = NULL;
        mOverlayWidgetVisible = false;
        if (mOverlayImage.hasSurfaces())
        {
//            Assert(!mOverlayVisible);
            if (pViewport)
            {
                initGl();
//            vpoxDoCheckUpdateViewport();
            }
//            Assert(!mOverlayVisible);
        }
        mGlCurrent = false;
    }
    mCmdPipe.setNotifyObject(pPostEventObject);
}

int VPoxQGLOverlay::reset()
{
    CDisplay display = mpSession->GetConsole().GetDisplay();
    Assert (!display.isNull());

    mCmdPipe.reset(&display);

    resetGl();

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vbvaVHWAHHCommandFreeCmd(void *pvContext)
{
    free(pvContext);
}

int VPoxQGLOverlay::resetGl()
{
    VHWACommandList list;
    int rc = mOverlayImage.reset(&list);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        for (VHWACommandList::const_iterator sIt = list.begin(); sIt != list.end(); ++ sIt)
        {
            VPOXVHWACMD *pCmd = (*sIt);
            VPOXVHWA_HH_CALLBACK_SET(pCmd, vbvaVHWAHHCommandFreeCmd, pCmd);
            mCmdPipe.postCmd(VPOXVHWA_PIPECMD_VHWA, pCmd, pCmd->enmCmd, false /*fGuestCmd*/);
        }
    }
    return VINF_SUCCESS;
}

int VPoxQGLOverlay::onVHWACommand(struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCmd,
                                  int /*VPOXVHWACMD_TYPE*/ enmCmdInt, bool fGuestCmd)
{
    VPOXVHWACMD_TYPE const enmCmd = (VPOXVHWACMD_TYPE)enmCmdInt;
    Log(("VHWA Command >>> %#p, %d\n", pCmd, enmCmd));
    switch (enmCmd)
    {
        case VPOXVHWACMD_TYPE_SURF_FLIP:
        case VPOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE:
        case VPOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION:
            break;
        case VPOXVHWACMD_TYPE_HH_CONSTRUCT:
        {
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            VPOXVHWACMD_HH_CONSTRUCT *pBody = VPOXVHWACMD_BODY_HOST_HEAP(pCmd, VPOXVHWACMD_HH_CONSTRUCT);
            pCmd->rc = vhwaConstruct(pBody);
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;
        }

        case VPOXVHWACMD_TYPE_HH_RESET:
        {
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            /* we do not post a reset command to the gui thread since this may lead to a deadlock
             * when reset is initiated by the gui thread*/
            pCmd->rc = reset();
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;
        }

        case VPOXVHWACMD_TYPE_HH_ENABLE:
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            pCmd->rc = VINF_SUCCESS;
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;

        case VPOXVHWACMD_TYPE_HH_DISABLE:
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            pCmd->rc = VINF_SUCCESS;
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;

        case VPOXVHWACMD_TYPE_HH_SAVESTATE_SAVEBEGIN:
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            mCmdPipe.disable();
            pCmd->rc = VINF_SUCCESS;
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;

        case VPOXVHWACMD_TYPE_HH_SAVESTATE_SAVEEND:
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            mCmdPipe.enable();
            pCmd->rc = VINF_SUCCESS;
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;

        case VPOXVHWACMD_TYPE_HH_SAVESTATE_SAVEPERFORM:
        {
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            VPOXVHWACMD_HH_SAVESTATE_SAVEPERFORM *pSave = VPOXVHWACMD_BODY_HOST_HEAP(pCmd, VPOXVHWACMD_HH_SAVESTATE_SAVEPERFORM);
            PSSMHANDLE pSSM = pSave->pSSM;
            int rc = SSMR3PutU32(pSSM, VPOXQGL_STATE_VERSION); AssertRC(rc);
            if (RT_SUCCESS(rc))
                vhwaSaveExec(pSSM);
            pCmd->rc = rc;
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;
        }
        case VPOXVHWACMD_TYPE_HH_SAVESTATE_LOADPERFORM:
        {
            pCmd->Flags &= ~VPOXVHWACMD_FLAG_HG_ASYNCH;
            ASSERT_GUEST_STMT_RETURN(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED, VINF_SUCCESS);
            VPOXVHWACMD_HH_SAVESTATE_LOADPERFORM *pLoad = VPOXVHWACMD_BODY_HOST_HEAP(pCmd, VPOXVHWACMD_HH_SAVESTATE_LOADPERFORM);
            PSSMHANDLE pSSM = pLoad->pSSM;
            uint32_t u32Version = 0;
            int rc = SSMR3GetU32(pSSM, &u32Version); Assert(RT_SUCCESS(rc) || rc == VERR_SSM_LOADED_TOO_MUCH);
            if (RT_SUCCESS(rc))
            {
                rc = vhwaLoadExec(pSSM, u32Version);
                AssertRC(rc);
            }
            else
            {
                /* sanity */
                u32Version = 0;

                if (rc == VERR_SSM_LOADED_TOO_MUCH)
                    rc = VINF_SUCCESS;
            }
            pCmd->rc = rc;
            Log(("VHWA Command <<< Sync %#p, %d\n", pCmd, enmCmd));
            return VINF_SUCCESS;
        }

        case VPOXVHWACMD_TYPE_QUERY_INFO1:
        {
#ifdef RT_STRICT
            VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO1);
#endif
            Assert(pBody->u.in.guestVersion.maj == VPOXVHWA_VERSION_MAJ);
            Assert(pBody->u.in.guestVersion.min == VPOXVHWA_VERSION_MIN);
            Assert(pBody->u.in.guestVersion.bld == VPOXVHWA_VERSION_BLD);
            Assert(pBody->u.in.guestVersion.reserved == VPOXVHWA_VERSION_RSV);
            /* do NOT break!! make it proceed asynchronously */
        }

        default:
            break;
    }

    Log(("VHWA Command --- Going Async %#p, %d\n", pCmd, enmCmd));
    /* indicate that we process and complete the command asynchronously */
    pCmd->Flags |= VPOXVHWACMD_FLAG_HG_ASYNCH;

    mCmdPipe.postCmd(VPOXVHWA_PIPECMD_VHWA, (void *)pCmd, enmCmd, fGuestCmd);
    return VINF_CALLBACK_RETURN;

}

void VPoxQGLOverlay::onVHWACommandEvent(QEvent *pEvent)
{
    VPoxVHWACommandProcessEvent *pVhwaEvent = (VPoxVHWACommandProcessEvent *)pEvent;
    /* sanity actually */
    pVhwaEvent->setProcessed();

    Assert(!mProcessingCommands);
    mProcessingCommands = true;
    Assert(!mGlCurrent);
    mGlCurrent = false; /* just a fall-back */
    VPoxVHWACommandElement *pCmd = mCmdPipe.getCmd();
    if (pCmd)
    {
        processCmd(pCmd);
        mCmdPipe.doneCmd();
    }

    mProcessingCommands = false;
    repaint();
    mGlCurrent = false;
}

bool VPoxQGLOverlay::onNotifyUpdate(ULONG uX, ULONG uY, ULONG uW, ULONG uH)
{
    /* Prepare corresponding viewport part: */
    QRect rect(uX, uY, uW, uH);

    /* Take the scaling into account: */
    const double dScaleFactor = mSizeInfo.scaleFactor();
    const QSize scaledSize = mSizeInfo.scaledSize();
    if (scaledSize.isValid())
    {
        /* Calculate corresponding scale-factors: */
        const double xScaleFactor = mSizeInfo.visualState() == UIVisualStateType_Scale
                                  ? (double)scaledSize.width()  / mSizeInfo.width()  : dScaleFactor;
        const double yScaleFactor = mSizeInfo.visualState() == UIVisualStateType_Scale
                                  ? (double)scaledSize.height() / mSizeInfo.height() : dScaleFactor;
        /* Adjust corresponding viewport part: */
        rect.moveTo((int)floor((double)rect.x() * xScaleFactor) - 1,
                    (int)floor((double)rect.y() * yScaleFactor) - 1);
        rect.setSize(QSize((int)ceil((double)rect.width()  * xScaleFactor) + 2,
                           (int)ceil((double)rect.height() * yScaleFactor) + 2));
    }

    /* Take the device-pixel-ratio into account: */
    if (mSizeInfo.useUnscaledHiDPIOutput())
    {
        const double dDevicePixelRatio = gpDesktop->devicePixelRatio(mpViewport->window());
        if (dDevicePixelRatio > 1.0)
        {
            rect.moveTo((int)floor((double)rect.x() / dDevicePixelRatio) - 1,
                        (int)floor((double)rect.y() / dDevicePixelRatio) - 1);
            rect.setSize(QSize((int)ceil((double)rect.width()  / dDevicePixelRatio) + 2,
                               (int)ceil((double)rect.height() / dDevicePixelRatio) + 2));
        }
    }

    /* we do not to miss notify updates, because we have to update bg textures for it,
     * so no not check for m_fUnused here,
     * mOverlay will store the required info for us */
    mCmdPipe.postCmd(VPOXVHWA_PIPECMD_PAINT, &rect, -1, false);

    return true;
}

void VPoxQGLOverlay::onResizeEventPostprocess(const VPoxFBSizeInfo &re, const QPoint &topLeft)
{
    mSizeInfo = re;
    mContentsTopLeft = topLeft;

    if (mGlOn)
    {
        Assert(mOverlayImage.hasSurfaces());
        Assert(!mGlCurrent);
        Assert(!mNeedOverlayRepaint);
        mGlCurrent = false;
        makeCurrent();
        /* need to ensure we're in sync */
        mNeedOverlayRepaint = vpoxSynchGl();

        if (!mOverlayImage.hasSurfaces())
            vpoxSetGlOn(false);
    }
    else
        Assert(!mOverlayImage.hasSurfaces());

    if (!mOnResizeCmdList.empty())
    {
        for (VHWACommandList::const_iterator it = mOnResizeCmdList.begin(); it != mOnResizeCmdList.end(); ++ it)
        {
            VPOXVHWACMD *pCmd = (*it);
            vpoxDoVHWACmdExec(pCmd, pCmd->enmCmd, false);
            free(pCmd);
        }
        mOnResizeCmdList.clear();
    }

    repaintOverlay();
    mGlCurrent = false;
}

void VPoxQGLOverlay::repaintMain()
{
    if (mMainDirtyRect.isClear())
        return;

    const QRect &rect = mMainDirtyRect.rect();
    if (mOverlayWidgetVisible)
        if (mOverlayViewport.contains(rect))
            return;

    mpViewport->repaint(rect.x() - mContentsTopLeft.x(),
                        rect.y() - mContentsTopLeft.y(),
                        rect.width(),
                        rect.height());

    mMainDirtyRect.clear();
}

void VPoxQGLOverlay::vpoxDoVHWACmd(void RT_UNTRUSTED_VOLATILE_GUEST *pvCmd, int /*VPOXVHWACMD_TYPE*/ enmCmd, bool fGuestCmd)
{
    vpoxDoVHWACmdExec(pvCmd, enmCmd, fGuestCmd);

    CDisplay display = mpSession->GetConsole().GetDisplay();
    Assert(!display.isNull());

    Log(("VHWA Command <<< Async %#p, %d\n", pvCmd, enmCmd));

    display.CompleteVHWACommand((BYTE *)pvCmd);
}

bool VPoxQGLOverlay::vpoxSynchGl()
{
    VPoxVHWASurfaceBase *pVGA = mOverlayImage.vgaSurface();
    if (   pVGA
        && mSizeInfo.pixelFormat()  == pVGA->pixelFormat().toVPoxPixelFormat()
        && mSizeInfo.VRAM()         == pVGA->address()
        && mSizeInfo.bitsPerPixel() == pVGA->bitsPerPixel()
        && mSizeInfo.bytesPerLine() == pVGA->bytesPerLine()
        && mSizeInfo.width()        == pVGA->width()
        && mSizeInfo.height()       == pVGA->height()
       )
    {
        return false;
    }
    /* create and issue a resize event to the gl widget to ensure we have all gl data initialized
     * and synchronized with the framebuffer */
    mOverlayImage.resize(mSizeInfo);
    return true;
}

void VPoxQGLOverlay::vpoxSetGlOn(bool on)
{
    if (on == mGlOn)
        return;

    mGlOn = on;

    if (on)
    {
        /* need to ensure we have gl functions initialized */
        mpOverlayWgt->makeCurrent();
        vpoxVHWAGetSupportInfo(mpOverlayWgt->context());

        VPOXQGLLOGREL(("Switching Gl mode on\n"));
        Assert(!mpOverlayWgt->isVisible());
        /* just to ensure */
        vpoxShowOverlay(false);
        mOverlayVisible = false;
        vpoxSynchGl();
    }
    else
    {
        VPOXQGLLOGREL(("Switching Gl mode off\n"));
        mOverlayVisible = false;
        vpoxShowOverlay(false);
        /* for now just set the flag w/o destroying anything */
    }
}

void VPoxQGLOverlay::vpoxDoCheckUpdateViewport()
{
    if (!mOverlayVisible)
    {
        vpoxShowOverlay(false);
        return;
    }

    int cX = mContentsTopLeft.x();
    int cY = mContentsTopLeft.y();
    QRect fbVp(cX, cY, mpViewport->width(), mpViewport->height());
    QRect overVp = fbVp.intersected(mOverlayViewport);

    if (overVp.isEmpty())
        vpoxShowOverlay(false);
    else
    {
        if (overVp != mOverlayImage.vpoxViewport())
        {
            makeCurrent();
            mOverlayImage.vpoxDoUpdateViewport(overVp);
            mNeedOverlayRepaint = true;
        }

        QRect rect(overVp.x() - cX, overVp.y() - cY, overVp.width(), overVp.height());

        vpoxCheckUpdateOverlay(rect);

        vpoxShowOverlay(true);

        /* workaround for linux ATI issue: need to update gl viewport after widget becomes visible */
        mOverlayImage.vpoxDoUpdateViewport(overVp);
    }
}

void VPoxQGLOverlay::vpoxShowOverlay(bool show)
{
    if (mOverlayWidgetVisible != show)
    {
        mpOverlayWgt->setVisible(show);
        mOverlayWidgetVisible = show;
        mGlCurrent = false;
        if (!show)
            mMainDirtyRect.add(mOverlayImage.vpoxViewport());
    }
}

void VPoxQGLOverlay::vpoxCheckUpdateOverlay(const QRect &rect)
{
    QRect overRect(mpOverlayWgt->pos(), mpOverlayWgt->size());
    if (overRect.x() != rect.x() || overRect.y() != rect.y())
    {
#if defined(RT_OS_WINDOWS)
        mpOverlayWgt->setVisible(false);
        mNeedSetVisible = true;
#endif
        VPOXQGLLOG_QRECT("moving wgt to " , &rect, "\n");
        mpOverlayWgt->move(rect.x(), rect.y());
        mGlCurrent = false;
    }

    if (overRect.width() != rect.width() || overRect.height() != rect.height())
    {
#if defined(RT_OS_WINDOWS)
        mpOverlayWgt->setVisible(false);
        mNeedSetVisible = true;
#endif
        VPOXQGLLOG(("resizing wgt to w(%d) ,h(%d)\n" , rect.width(), rect.height()));
        mpOverlayWgt->resize(rect.width(), rect.height());
        mGlCurrent = false;
    }
}

void VPoxQGLOverlay::addMainDirtyRect(const QRect &aRect)
{
    mMainDirtyRect.add(aRect);
    if (mGlOn)
    {
        mOverlayImage.vpoxDoUpdateRect(&aRect);
        mNeedOverlayRepaint = true;
    }
}

int VPoxQGLOverlay::vhwaSurfaceUnlock(struct VPOXVHWACMD_SURF_UNLOCK RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    int rc = mOverlayImage.vhwaSurfaceUnlock(pCmd);
    VPoxVHWASurfaceBase *pVGA = mOverlayImage.vgaSurface();
    const VPoxVHWADirtyRect &rect = pVGA->getDirtyRect();
    mNeedOverlayRepaint = true;
    if (!rect.isClear())
        mMainDirtyRect.add(rect);
    return rc;
}

void VPoxQGLOverlay::vpoxDoVHWACmdExec(void RT_UNTRUSTED_VOLATILE_GUEST *pvCmd, int /*VPOXVHWACMD_TYPE*/ enmCmdInt, bool fGuestCmd)
{
    struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCmd = (struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *)pvCmd;
    VPOXVHWACMD_TYPE enmCmd = (VPOXVHWACMD_TYPE)enmCmdInt;

    switch (enmCmd)
    {
        case VPOXVHWACMD_TYPE_SURF_CANCREATE:
        {
            VPOXVHWACMD_SURF_CANCREATE RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_CANCREATE);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceCanCreate(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_CREATE:
        {
            VPOXVHWACMD_SURF_CREATE RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_CREATE);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            vpoxSetGlOn(true);
            pCmd->rc = mOverlayImage.vhwaSurfaceCreate(pBody);
            if (!mOverlayImage.hasSurfaces())
            {
                vpoxSetGlOn(false);
            }
            else
            {
                mOverlayVisible = mOverlayImage.hasVisibleOverlays();
                if (mOverlayVisible)
                {
                    mOverlayViewport = mOverlayImage.overlaysRectUnion();
                }
                vpoxDoCheckUpdateViewport();
                mNeedOverlayRepaint = true;
            }

            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_DESTROY:
        {
            VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_DESTROY);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceDestroy(pBody);
            if (!mOverlayImage.hasSurfaces())
            {
                vpoxSetGlOn(false);
            }
            else
            {
                mOverlayVisible = mOverlayImage.hasVisibleOverlays();
                if (mOverlayVisible)
                {
                    mOverlayViewport = mOverlayImage.overlaysRectUnion();
                }
                vpoxDoCheckUpdateViewport();
                mNeedOverlayRepaint = true;
            }
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_LOCK:
        {
            VPOXVHWACMD_SURF_LOCK RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_LOCK);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceLock(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_UNLOCK:
        {
            VPOXVHWACMD_SURF_UNLOCK RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_UNLOCK);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = vhwaSurfaceUnlock(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            /* mNeedOverlayRepaint is set inside the vhwaSurfaceUnlock */
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_BLT:
        {
            VPOXVHWACMD_SURF_BLT RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_BLT);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceBlt(pBody);
            mNeedOverlayRepaint = true;
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_FLIP:
        {
            VPOXVHWACMD_SURF_FLIP RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_FLIP);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceFlip(pBody);
            mNeedOverlayRepaint = true;
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE:
        {
            VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_OVERLAY_UPDATE);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceOverlayUpdate(pBody);
            mOverlayVisible = mOverlayImage.hasVisibleOverlays();
            if (mOverlayVisible)
            {
                mOverlayViewport = mOverlayImage.overlaysRectUnion();
            }
            vpoxDoCheckUpdateViewport();
            mNeedOverlayRepaint = true;
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION:
        {
            VPOXVHWACMD_SURF_OVERLAY_SETPOSITION RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_OVERLAY_SETPOSITION);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceOverlaySetPosition(pBody);
            mOverlayVisible = mOverlayImage.hasVisibleOverlays();
            if (mOverlayVisible)
            {
                mOverlayViewport = mOverlayImage.overlaysRectUnion();
            }
            vpoxDoCheckUpdateViewport();
            mNeedOverlayRepaint = true;
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

#ifdef VPOX_WITH_WDDM
        case VPOXVHWACMD_TYPE_SURF_COLORFILL:
        {
            VPOXVHWACMD_SURF_COLORFILL RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_COLORFILL);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceColorFill(pBody);
            mNeedOverlayRepaint = true;
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }
#endif
        case VPOXVHWACMD_TYPE_SURF_COLORKEY_SET:
        {
            VPOXVHWACMD_SURF_COLORKEY_SET RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_COLORKEY_SET);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaSurfaceColorkeySet(pBody);
            /* this is here to ensure we have color key changes picked up */
            vpoxDoCheckUpdateViewport();
            mNeedOverlayRepaint = true;
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_QUERY_INFO1:
        {
            VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO1);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaQueryInfo1(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_QUERY_INFO2:
        {
            VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_QUERYINFO2);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            makeCurrent();
            pCmd->rc = mOverlayImage.vhwaQueryInfo2(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

        case VPOXVHWACMD_TYPE_ENABLE:
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            initGl();
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            pCmd->rc = VINF_SUCCESS;
            break;

        case VPOXVHWACMD_TYPE_DISABLE:
            pCmd->rc = VINF_SUCCESS;
            break;

        case VPOXVHWACMD_TYPE_HH_CONSTRUCT:
        {
            ASSERT_GUEST_STMT_RETURN_VOID(!fGuestCmd, pCmd->rc = VERR_ACCESS_DENIED);
            VPOXVHWACMD_HH_CONSTRUCT *pBody = VPOXVHWACMD_BODY_HOST_HEAP(pCmd, VPOXVHWACMD_HH_CONSTRUCT);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            pCmd->rc = vhwaConstruct(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }

#ifdef VPOX_WITH_WDDM
        case VPOXVHWACMD_TYPE_SURF_GETINFO:
        {
            VPOXVHWACMD_SURF_GETINFO RT_UNTRUSTED_VOLATILE_GUEST *pBody = VPOXVHWACMD_BODY(pCmd, VPOXVHWACMD_SURF_GETINFO);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            pCmd->rc = mOverlayImage.vhwaSurfaceGetInfo(pBody);
            Assert(!mGlOn == !mOverlayImage.hasSurfaces());
            break;
        }
#endif

        default:
            AssertFailed();
            pCmd->rc = VERR_NOT_IMPLEMENTED;
            break;
    }
}

#if 0
static DECLCALLBACK(void) vpoxQGLOverlaySaveExec(PSSMHANDLE pSSM, void *pvUser)
{
    VPoxQGLOverlay * fb = (VPoxQGLOverlay*)pvUser;
    fb->vhwaSaveExec(pSSM);
}
#endif

static DECLCALLBACK(int) vpoxQGLOverlayLoadExec(PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version, uint32_t uPass)
{
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);
    VPoxQGLOverlay *fb = (VPoxQGLOverlay *)pvUser;
    return fb->vhwaLoadExec(pSSM, u32Version);
}

int VPoxQGLOverlay::vhwaLoadExec(struct SSMHANDLE *pSSM, uint32_t u32Version)
{
    int rc = VPoxVHWAImage::vhwaLoadExec(&mOnResizeCmdList, pSSM, u32Version);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        if (u32Version >= VPOXQGL_STATE_VERSION_PIPESAVED)
        {
            rc = mCmdPipe.loadExec(pSSM, u32Version, mOverlayImage.vramBase());
            AssertRC(rc);
        }
    }
    return rc;
}

void VPoxQGLOverlay::vhwaSaveExec(struct SSMHANDLE *pSSM)
{
    mOverlayImage.vhwaSaveExec(pSSM);
    mCmdPipe.saveExec(pSSM, mOverlayImage.vramBase());
}

int VPoxQGLOverlay::vhwaConstruct(struct VPOXVHWACMD_HH_CONSTRUCT *pCmd)
{
    PUVM pUVM = VMR3GetUVM((PVM)pCmd->pVM);
    uint32_t intsId = m_id;

    char nameFuf[sizeof(VPOXQGL_STATE_NAMEBASE) + 8];

    char * pszName = nameFuf;
    sprintf(pszName, "%s%d", VPOXQGL_STATE_NAMEBASE, intsId);
    int rc = SSMR3RegisterExternal(pUVM,                    /* The VM handle*/
                                   pszName,                 /* Data unit name. */
                                   intsId,                  /* The instance identifier of the data unit.
                                                             * This must together with the name be unique. */
                                   VPOXQGL_STATE_VERSION,   /* Data layout version number. */
                                   128,                     /* The approximate amount of data in the unit.
                                                             * Only for progress indicators. */
                                   NULL, NULL, NULL,        /* pfnLiveXxx */
                                   NULL,                    /* Prepare save callback, optional. */
                                   NULL, //vpoxQGLOverlaySaveExec, /* Execute save callback, optional. */
                                   NULL,                    /* Done save callback, optional. */
                                   NULL,                    /* Prepare load callback, optional. */
                                   vpoxQGLOverlayLoadExec,  /* Execute load callback, optional. */
                                   NULL,                    /* Done load callback, optional. */
                                   this                     /* User argument. */
                                   );
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = mOverlayImage.vhwaConstruct(pCmd);
        AssertRC(rc);
    }
    return rc;
}

void VPoxQGLOverlay::processCmd(VPoxVHWACommandElement * pCmd)
{
    switch(pCmd->type())
    {
        case VPOXVHWA_PIPECMD_PAINT:
            addMainDirtyRect(pCmd->rect());
            break;

#ifdef VPOX_WITH_VIDEOHWACCEL
        case VPOXVHWA_PIPECMD_VHWA:
            vpoxDoVHWACmd(pCmd->vhwaCmdPtr(), pCmd->vhwaCmdType(), pCmd->vhwaIsGuestCmd());
            break;

        case VPOXVHWA_PIPECMD_FUNC:
        {
            const VPOXVHWAFUNCCALLBACKINFO & info = pCmd->func();
            info.pfnCallback(info.pContext1, info.pContext2);
            break;
        }
#endif
        default:
            AssertFailed();
    }
}

VPoxVHWACommandElementProcessor::VPoxVHWACommandElementProcessor()
    : m_pNotifyObject(NULL)
    , mpCurCmd(NULL)
    , mbResetting(false)
    , mcDisabled(0)
{
    int rc = RTCritSectInit(&mCritSect);
    AssertRC(rc);

    RTListInit(&mCommandList);

    m_pCmdEntryCache = new VPoxVHWAEntriesCache;
}

void VPoxVHWACommandElementProcessor::init(QObject *pNotifyObject)
{
    m_pNotifyObject = pNotifyObject;
}

VPoxVHWACommandElementProcessor::~VPoxVHWACommandElementProcessor()
{
    Assert(!m_NotifyObjectRefs.refs());
    RTListIsEmpty(&mCommandList);

    RTCritSectDelete(&mCritSect);

    delete m_pCmdEntryCache;
}

void VPoxVHWACommandElementProcessor::postCmd(VPOXVHWA_PIPECMD_TYPE aType, void *pvData,
                                              int /*VPOXVHWACMD_TYPE*/ enmCmd, bool fGuestCmd)
{
    QObject *pNotifyObject = NULL;

    Log(("VHWA post %d %#p\n", aType, pvData));

    /* 1. lock*/
    RTCritSectEnter(&mCritSect);

    VPoxVHWACommandElement *pCmd = m_pCmdEntryCache->alloc();
    if (!pCmd)
    {
        VPOXQGLLOG(("!!!no more free elements!!!\n"));
#ifdef VPOXQGL_PROF_BASE
        RTCritSectLeave(&mCritSect);
        return;
#else
    /// @todo
#endif
    }
    pCmd->setData(aType, pvData, enmCmd, fGuestCmd);

    /* 2. if can add to current*/
    if (m_pNotifyObject)
    {
        m_NotifyObjectRefs.inc(); /* ensure the parent does not get destroyed while we are using it */
        pNotifyObject = m_pNotifyObject;
    }

    RTListAppend(&mCommandList, &pCmd->ListNode);

    RTCritSectLeave(&mCritSect);

    if (pNotifyObject)
    {
        VPoxVHWACommandProcessEvent *pCurrentEvent = new VPoxVHWACommandProcessEvent();
        QApplication::postEvent(pNotifyObject, pCurrentEvent);
        m_NotifyObjectRefs.dec();
    }
}

void VPoxVHWACommandElementProcessor::setNotifyObject(QObject *pNotifyObject)
{
    int cEventsNeeded = 0;
    RTCritSectEnter(&mCritSect);
    if (m_pNotifyObject == pNotifyObject)
    {
        RTCritSectLeave(&mCritSect);
        return;
    }

    if (m_pNotifyObject)
    {
        m_pNotifyObject = NULL;
        RTCritSectLeave(&mCritSect);

        m_NotifyObjectRefs.wait0();

        RTCritSectEnter(&mCritSect);
    }
    else
    {
        /* NULL can not be references */
        Assert(!m_NotifyObjectRefs.refs());
    }

    if (pNotifyObject)
    {
        m_pNotifyObject = pNotifyObject;

        VPoxVHWACommandElement *pCur;
        RTListForEachCpp(&mCommandList, pCur, VPoxVHWACommandElement, ListNode)
        {
            ++cEventsNeeded;
        }

        if (cEventsNeeded)
            m_NotifyObjectRefs.inc();
    }
    else
    {
        /* should be zeroed already */
        Assert(!m_pNotifyObject);
    }

    RTCritSectLeave(&mCritSect);

    if (cEventsNeeded)
    {
        /* cEventsNeeded can only be != 0 if pNotifyObject is valid */
        Assert(pNotifyObject);
        for (int i = 0; i < cEventsNeeded; ++i)
        {
            VPoxVHWACommandProcessEvent *pCurrentEvent = new VPoxVHWACommandProcessEvent();
            QApplication::postEvent(pNotifyObject, pCurrentEvent);
        }
        m_NotifyObjectRefs.dec();
    }
}

void VPoxVHWACommandElementProcessor::doneCmd()
{
    VPoxVHWACommandElement *pEl;
    RTCritSectEnter(&mCritSect);
    pEl = mpCurCmd;
    Assert(mpCurCmd);
    mpCurCmd = NULL;
    RTCritSectLeave(&mCritSect);

    if (pEl)
        m_pCmdEntryCache->free(pEl);
}

VPoxVHWACommandElement *VPoxVHWACommandElementProcessor::getCmd()
{
    VPoxVHWACommandElement *pEl = NULL;
    RTCritSectEnter(&mCritSect);

    Assert(!mpCurCmd);

    if (mbResetting)
    {
        RTCritSectLeave(&mCritSect);
        return NULL;
    }

    if (mcDisabled)
    {
        QObject *pNotifyObject = NULL;

        if (!RTListIsEmpty(&mCommandList))
        {
            Assert(m_pNotifyObject);
            if (m_pNotifyObject)
            {
                m_NotifyObjectRefs.inc(); /* ensure the parent does not get destroyed while we are using it */
                pNotifyObject = m_pNotifyObject;
            }
        }

        RTCritSectLeave(&mCritSect);

        if (pNotifyObject)
        {
            VPoxVHWACommandProcessEvent *pCurrentEvent = new VPoxVHWACommandProcessEvent();
            QApplication::postEvent(pNotifyObject, pCurrentEvent);
            m_NotifyObjectRefs.dec();
        }
        return NULL;
    }

    pEl = RTListGetFirstCpp(&mCommandList, VPoxVHWACommandElement, ListNode);
    if (pEl)
    {
        RTListNodeRemove(&pEl->ListNode);
        mpCurCmd = pEl;
    }

    RTCritSectLeave(&mCritSect);

    return pEl;
}

/* it is currently assumed no one sends any new commands while reset is in progress */
void VPoxVHWACommandElementProcessor::reset(CDisplay *pDisplay)
{
    RTCritSectEnter(&mCritSect);

    mbResetting = true;

    if (mpCurCmd)
    {
        for (;;)
        {
            RTCritSectLeave(&mCritSect);
            RTThreadSleep(2); /* 2 ms */
            RTCritSectEnter(&mCritSect);
            /* it is assumed no one sends any new commands while reset is in progress */
            if (!mpCurCmd)
            {
                break;
            }
        }
    }

    RTCritSectLeave(&mCritSect);

    VPoxVHWACommandElement *pCur, *pNext;
    RTListForEachSafeCpp(&mCommandList, pCur, pNext, VPoxVHWACommandElement, ListNode)
    {
        switch (pCur->type())
        {
#ifdef VPOX_WITH_VIDEOHWACCEL
            case VPOXVHWA_PIPECMD_VHWA:
                {
                    struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCmd = pCur->vhwaCmdPtr();
                    pCmd->rc = VERR_INVALID_STATE;
                    Log(("VHWA Command <<< Async RESET %#p, %d\n", pCmd, pCmd->enmCmd));
                    pDisplay->CompleteVHWACommand((BYTE *)pCmd);
                }
                break;

            case VPOXVHWA_PIPECMD_FUNC:
                /* should not happen, don't handle this for now */
                AssertFailed();
                break;
#endif
            case VPOXVHWA_PIPECMD_PAINT:
                break;

            default:
                /* should not happen, don't handle this for now */
                AssertFailed();
                break;
        }

        RTListNodeRemove(&pCur->ListNode);
        m_pCmdEntryCache->free(pCur);
    }

    RTCritSectEnter(&mCritSect);

    mbResetting = false;

    RTCritSectLeave(&mCritSect);
}

#define VPOXVHWACOMMANDELEMENTLISTBEGIN_MAGIC UINT32_C(0x89abcdef)
#define VPOXVHWACOMMANDELEMENTLISTEND_MAGIC   UINT32_C(0xfedcba98)

int VPoxVHWACommandElementProcessor::loadExec(struct SSMHANDLE *pSSM, uint32_t u32Version, void *pvVRAM)
{
    uint32_t u32;

    Q_UNUSED(u32Version);

    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);
    AssertReturn(u32 == VPOXVHWACOMMANDELEMENTLISTBEGIN_MAGIC, VERR_INVALID_MAGIC);

    SSMR3GetU32(pSSM, &u32);
    bool b;
    rc = SSMR3GetBool(pSSM, &b);
    AssertRCReturn(rc, rc);

//    m_NotifyObjectRefs = VPoxVHWARefCounter(u32);
    bool fContinue = true;
    do
    {
        rc = SSMR3GetU32(pSSM, &u32);
        AssertRCReturn(rc, rc);

        bool fNewEvent;
        switch (u32)
        {
            case VPOXVHWA_PIPECMD_PAINT:
            {
                int x,y,w,h;
                rc = SSMR3GetS32(pSSM, &x);
                rc = SSMR3GetS32(pSSM, &y);
                rc = SSMR3GetS32(pSSM, &w);
                rc = SSMR3GetS32(pSSM, &h);
                rc = SSMR3GetBool(pSSM, &fNewEvent);
                AssertRCReturn(rc, rc);

                QRect r = QRect(x, y, w, h);
                postCmd(VPOXVHWA_PIPECMD_PAINT, &r, -1, false);
                break;
            }

            case VPOXVHWA_PIPECMD_VHWA:
            {
                uint32_t offCmd;
                rc = SSMR3GetU32(pSSM, &offCmd);
                rc = SSMR3GetBool(pSSM, &fNewEvent);
                AssertRCReturn(rc, rc);

                VPOXVHWACMD *pCmd = (VPOXVHWACMD *)(((uint8_t *)pvVRAM) + offCmd);
                postCmd(VPOXVHWA_PIPECMD_VHWA, pCmd, pCmd->enmCmd, true);
                break;
            }

            case VPOXVHWACOMMANDELEMENTLISTEND_MAGIC:
                fContinue = false;
                break;

            default:
                AssertLogRelMsgFailed(("u32=%#x\n", u32));
                break;
        }

    } while (fContinue && RT_SUCCESS(rc));

    return rc;
}

void VPoxVHWACommandElementProcessor::saveExec(struct SSMHANDLE *pSSM, void *pvVRAM)
{
    int rc;

    rc = SSMR3PutU32(pSSM, VPOXVHWACOMMANDELEMENTLISTBEGIN_MAGIC);
    rc = SSMR3PutU32(pSSM, m_NotifyObjectRefs.refs());
    rc = SSMR3PutBool(pSSM, true);
    AssertRC(rc);

    VPoxVHWACommandElement *pCur;
    RTListForEachCpp(&mCommandList, pCur, VPoxVHWACommandElement, ListNode)
    {

        switch (pCur->type())
        {
            case VPOXVHWA_PIPECMD_PAINT:
                rc = SSMR3PutU32(pSSM, pCur->type());
                rc = SSMR3PutS32(pSSM, pCur->rect().x());
                rc = SSMR3PutS32(pSSM, pCur->rect().y());
                rc = SSMR3PutS32(pSSM, pCur->rect().width());
                rc = SSMR3PutS32(pSSM, pCur->rect().height());
                rc = SSMR3PutBool(pSSM, true);
                AssertRC(rc);
                break;

            case VPOXVHWA_PIPECMD_VHWA:
                if (pCur->vhwaIsGuestCmd())
                {
                    rc = SSMR3PutU32(pSSM, pCur->type());
                    rc = SSMR3PutU32(pSSM, (uint32_t)(uintptr_t)pCur->vhwaCmdPtr() - (uintptr_t)pvVRAM);
                    rc = SSMR3PutBool(pSSM, true);
                    AssertRC(rc);
                }
                break;

            default:
                AssertFailed();
                break;
        }
    }

    rc = SSMR3PutU32(pSSM, VPOXVHWACOMMANDELEMENTLISTEND_MAGIC);
    AssertRC(rc);
}

void VPoxVHWACommandElementProcessor::lock()
{
    RTCritSectEnter(&mCritSect);

    if (mpCurCmd)
    {
        for (;;)
        {
            RTCritSectLeave(&mCritSect);
            RTThreadSleep(2); /* 2 ms */
            RTCritSectEnter(&mCritSect);
            /* it is assumed no one sends any new commands while reset is in progress */
            if (!mpCurCmd)
            {
                break;
            }
        }
    }

    Assert(!mpCurCmd);
}

void VPoxVHWACommandElementProcessor::unlock()
{
    RTCritSectLeave(&mCritSect);
}

void VPoxVHWACommandElementProcessor::disable()
{
    lock();
    ++mcDisabled;
    unlock();
}

void VPoxVHWACommandElementProcessor::enable()
{
    lock();
    --mcDisabled;
    unlock();
}

/* static */
uint32_t VPoxVHWATextureImage::calcBytesPerLine(const VPoxVHWAColorFormat &format, int width)
{
    uint32_t pitch = (format.bitsPerPixel() * width + 7) / 8;
    switch (format.fourcc())
    {
        case FOURCC_YV12:
            /* make sure the color components pitch is multiple of 8
             * where 8 is 2 (for color component width is Y width / 2) * 4 for 4byte texture format */
            pitch = (pitch + 7) & ~7;
            break;

        default:
            pitch = (pitch + 3) & ~3;
            break;
    }
    return pitch;
}

/* static */
uint32_t VPoxVHWATextureImage::calcMemSize(const VPoxVHWAColorFormat &format, int width, int height)
{
    uint32_t pitch = calcBytesPerLine(format, width);
    switch (format.fourcc())
    {
        case FOURCC_YV12:
            /* we have 3 separate planes here
             * Y - pitch x height
             * U - pitch / 2 x height / 2
             * V - pitch / 2 x height / 2
             * */
            return 3 * pitch * height / 2;

        default:
            return pitch * height;
    }
}

VPoxVHWATextureImage::VPoxVHWATextureImage(const QRect &size, const VPoxVHWAColorFormat &format,
                                           class VPoxVHWAGlProgramMngr * aMgr, VPOXVHWAIMG_TYPE flags)
    : mVisibleDisplay(0)
    , mpProgram(0)
    , mProgramMngr(aMgr)
    , mpDst(NULL)
    , mpDstCKey(NULL)
    , mpSrcCKey(NULL)
    , mbNotIntersected(false)
{
    uint32_t pitch = calcBytesPerLine(format, size.width());

    mpTex[0] = vpoxVHWATextureCreate(NULL, size, format, pitch, flags);
    mColorFormat = format;
    if (mColorFormat.fourcc() == FOURCC_YV12)
    {
        QRect rect(size.x() / 2,size.y() / 2,size.width() / 2,size.height() / 2);
        mpTex[1] = vpoxVHWATextureCreate(NULL, rect, format, pitch / 2, flags);
        mpTex[2] = vpoxVHWATextureCreate(NULL, rect, format, pitch / 2, flags);
        mcTex = 3;
    }
    else
        mcTex = 1;
}

void VPoxVHWATextureImage::deleteDisplayList()
{
    if (mVisibleDisplay)
    {
        glDeleteLists(mVisibleDisplay, 1);
        mVisibleDisplay = 0;
    }
}

void VPoxVHWATextureImage::deleteDisplay()
{
    deleteDisplayList();
    mpProgram = NULL;
}

void VPoxVHWATextureImage::draw(VPoxVHWATextureImage *pDst, const QRect *pDstRect, const QRect *pSrcRect)
{
    int tx1, ty1, tx2, ty2;
    pSrcRect->getCoords(&tx1, &ty1, &tx2, &ty2);

    int bx1, by1, bx2, by2;
    pDstRect->getCoords(&bx1, &by1, &bx2, &by2);

    tx2++; ty2++;bx2++; by2++;

    glBegin(GL_QUADS);
    uint32_t c = texCoord(GL_TEXTURE0, tx1, ty1);
    if (pDst)
        pDst->texCoord(GL_TEXTURE0 + c, bx1, by1);
    glVertex2i(bx1, by1);

    texCoord(GL_TEXTURE0, tx1, ty2);
    if (pDst)
        pDst->texCoord(GL_TEXTURE0 + c, bx1, by2);
    glVertex2i(bx1, by2);

    texCoord(GL_TEXTURE0, tx2, ty2);
    if (pDst)
        pDst->texCoord(GL_TEXTURE0 + c, bx2, by2);
    glVertex2i(bx2, by2);

    texCoord(GL_TEXTURE0, tx2, ty1);
    if (pDst)
        pDst->texCoord(GL_TEXTURE0 + c, bx2, by1);
    glVertex2i(bx2, by1);

    glEnd();
}

void VPoxVHWATextureImage::internalSetDstCKey(const VPoxVHWAColorKey * pDstCKey)
{
    if (pDstCKey)
    {
        mDstCKey = *pDstCKey;
        mpDstCKey = &mDstCKey;
    }
    else
    {
        mpDstCKey = NULL;
    }
}

void VPoxVHWATextureImage::internalSetSrcCKey(const VPoxVHWAColorKey * pSrcCKey)
{
    if (pSrcCKey)
    {
        mSrcCKey = *pSrcCKey;
        mpSrcCKey = &mSrcCKey;
    }
    else
    {
        mpSrcCKey = NULL;
    }
}

int VPoxVHWATextureImage::initDisplay(VPoxVHWATextureImage *pDst,
                                      const QRect *pDstRect, const QRect *pSrcRect,
                                      const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey *pSrcCKey, bool bNotIntersected)
{
    if (  !mVisibleDisplay
        || mpDst != pDst
        || *pDstRect != mDstRect
        || *pSrcRect != mSrcRect
        || !!(pDstCKey) != !!(mpDstCKey)
        || !!(pSrcCKey) != !!(mpSrcCKey)
        || mbNotIntersected != bNotIntersected
        || mpProgram != calcProgram(pDst, pDstCKey, pSrcCKey, bNotIntersected))
        return createSetDisplay(pDst, pDstRect, pSrcRect, pDstCKey, pSrcCKey, bNotIntersected);
    if (   (pDstCKey && mpDstCKey && *pDstCKey != *mpDstCKey)
        || (pSrcCKey && mpSrcCKey && *pSrcCKey != *mpSrcCKey))
    {
        Assert(mpProgram);
        updateSetCKeys(pDstCKey, pSrcCKey);
        return VINF_SUCCESS;
    }
    return VINF_SUCCESS;
}

void VPoxVHWATextureImage::bind(VPoxVHWATextureImage * pPrimary)
{
    for (uint32_t i = 1; i < mcTex; i++)
    {
        vpoxglActiveTexture(GL_TEXTURE0 + i);
        mpTex[i]->bind();
    }
    if (pPrimary)
        for (uint32_t i = 0; i < pPrimary->mcTex; i++)
        {
            vpoxglActiveTexture(GL_TEXTURE0 + i + mcTex);
            pPrimary->mpTex[i]->bind();
        }

    vpoxglActiveTexture(GL_TEXTURE0);
    mpTex[0]->bind();
}

uint32_t VPoxVHWATextureImage::calcProgramType(VPoxVHWATextureImage *pDst, const VPoxVHWAColorKey *pDstCKey,
                                               const VPoxVHWAColorKey *pSrcCKey, bool bNotIntersected)
{
    uint32_t type = 0;

    if (pDstCKey != NULL)
        type |= VPOXVHWA_PROGRAM_DSTCOLORKEY;
    if (pSrcCKey)
        type |= VPOXVHWA_PROGRAM_SRCCOLORKEY;
    if ((pDstCKey || pSrcCKey) && bNotIntersected)
        type |= VPOXVHWA_PROGRAM_COLORKEYNODISCARD;

    NOREF(pDst);
    return type;
}

class VPoxVHWAGlProgramVHWA *VPoxVHWATextureImage::calcProgram(VPoxVHWATextureImage *pDst, const VPoxVHWAColorKey *pDstCKey,
                                                               const VPoxVHWAColorKey *pSrcCKey, bool bNotIntersected)
{
    uint32_t type = calcProgramType(pDst, pDstCKey, pSrcCKey, bNotIntersected);

    return mProgramMngr->getProgram(type, &pixelFormat(), pDst ? &pDst->pixelFormat() : NULL);
}

int VPoxVHWATextureImage::createSetDisplay(VPoxVHWATextureImage *pDst, const QRect *pDstRect, const QRect *pSrcRect,
                                           const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey * pSrcCKey,
                                           bool bNotIntersected)
{
    deleteDisplay();
    int rc = createDisplay(pDst, pDstRect, pSrcRect, pDstCKey, pSrcCKey, bNotIntersected, &mVisibleDisplay, &mpProgram);
    if (RT_FAILURE(rc))
    {
        mVisibleDisplay = 0;
        mpProgram = NULL;
    }

    mpDst = pDst;

    mDstRect = *pDstRect;
    mSrcRect = *pSrcRect;

    internalSetDstCKey(pDstCKey);
    internalSetSrcCKey(pSrcCKey);

    mbNotIntersected = bNotIntersected;

    return rc;
}


int VPoxVHWATextureImage::createDisplayList(VPoxVHWATextureImage *pDst, const QRect *pDstRect, const QRect *pSrcRect,
                                            const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey *pSrcCKey,
                                            bool bNotIntersected, GLuint *pDisplay)
{
    Q_UNUSED(pDstCKey);
    Q_UNUSED(pSrcCKey);
    Q_UNUSED(bNotIntersected);

    glGetError(); /* clear the err flag */
    GLuint display = glGenLists(1);
    GLenum err = glGetError();
    if (err == GL_NO_ERROR)
    {
        Assert(display);
        if (!display)
        {
            /* well, it seems it should not return 0 on success according to the spec,
             * but just in case, pick another one */
            display = glGenLists(1);
            err = glGetError();
            if (err == GL_NO_ERROR)
            {
                Assert(display);
            }
            else
            {
                /* we are failed */
                Assert(!display);
                display = 0;
            }
        }

        if (display)
        {
            glNewList(display, GL_COMPILE);

            runDisplay(pDst, pDstRect, pSrcRect);

            glEndList();
            VPOXQGL_ASSERTNOERR();
            *pDisplay = display;
            return VINF_SUCCESS;
        }
    }
    else
    {
        VPOXQGLLOG(("gl error ocured (0x%x)\n", err));
        Assert(err == GL_NO_ERROR);
    }
    return VERR_GENERAL_FAILURE;
}

void VPoxVHWATextureImage::updateCKeys(VPoxVHWATextureImage *pDst, class VPoxVHWAGlProgramVHWA *pProgram,
                                       const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey *pSrcCKey)
{
    if (pProgram)
    {
        pProgram->start();
        if (pSrcCKey)
            VPoxVHWATextureImage::setCKey(pProgram, &pixelFormat(), pSrcCKey, false);
        if (pDstCKey)
            VPoxVHWATextureImage::setCKey(pProgram, &pDst->pixelFormat(), pDstCKey, true);
        pProgram->stop();
    }
}

void VPoxVHWATextureImage::updateSetCKeys(const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey *pSrcCKey)
{
    updateCKeys(mpDst, mpProgram, pDstCKey, pSrcCKey);
    internalSetDstCKey(pDstCKey);
    internalSetSrcCKey(pSrcCKey);
}

int VPoxVHWATextureImage::createDisplay(VPoxVHWATextureImage *pDst, const QRect *pDstRect, const QRect *pSrcRect,
                                        const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey *pSrcCKey, bool bNotIntersected,
                                        GLuint *pDisplay, class VPoxVHWAGlProgramVHWA **ppProgram)
{
    VPoxVHWAGlProgramVHWA *pProgram = NULL;
    if (!pDst)
    {
        /* sanity */
        Assert(pDstCKey == NULL);
        pDstCKey = NULL;
    }

    Assert(!pSrcCKey);
    if (pSrcCKey)
        pSrcCKey = NULL; /* fallback */

    pProgram = calcProgram(pDst, pDstCKey, pSrcCKey, bNotIntersected);

    updateCKeys(pDst, pProgram, pDstCKey, pSrcCKey);

    GLuint displ;
    int rc = createDisplayList(pDst, pDstRect, pSrcRect, pDstCKey, pSrcCKey, bNotIntersected, &displ);
    if (RT_SUCCESS(rc))
    {
        *pDisplay = displ;
        *ppProgram = pProgram;
    }

    return rc;
}

void VPoxVHWATextureImage::display(VPoxVHWATextureImage *pDst, const QRect *pDstRect, const QRect *pSrcRect,
                                   const VPoxVHWAColorKey *pDstCKey, const VPoxVHWAColorKey *pSrcCKey, bool bNotIntersected)
{
    VPoxVHWAGlProgramVHWA *pProgram = calcProgram(pDst, pDstCKey, pSrcCKey, bNotIntersected);
    if (pProgram)
        pProgram->start();

    runDisplay(pDst, pDstRect, pSrcRect);

    if (pProgram)
        pProgram->stop();
}

void VPoxVHWATextureImage::display()
{
#ifdef DEBUG_misha
    if (mpDst)
        dbgDump();

    static bool bDisplayOn = true;
#endif
    Assert(mVisibleDisplay);
    if (   mVisibleDisplay
#ifdef DEBUG_misha
        && bDisplayOn
#endif
        )
    {
        if (mpProgram)
            mpProgram->start();

        VPOXQGL_CHECKERR(
                glCallList(mVisibleDisplay);
                );

        if (mpProgram)
            mpProgram->stop();
    }
    else
    {
        display(mpDst, &mDstRect, &mSrcRect,
                mpDstCKey, mpSrcCKey, mbNotIntersected);
    }
}

#ifdef DEBUG_misha
void VPoxVHWATextureImage::dbgDump()
{
    for (uint32_t i = 0; i < mcTex; ++i)
        mpTex[i]->dbgDump();
}
#endif

int VPoxVHWATextureImage::setCKey(VPoxVHWAGlProgramVHWA *pProgram, const VPoxVHWAColorFormat *pFormat,
                                  const VPoxVHWAColorKey *pCKey, bool bDst)
{
    float r,g,b;
    pFormat->pixel2Normalized(pCKey->lower(), &r, &g, &b);
    int rcL = bDst ? pProgram->setDstCKeyLowerRange(r, g, b) : pProgram->setSrcCKeyLowerRange(r, g, b);
    Assert(RT_SUCCESS(rcL));

    return RT_SUCCESS(rcL) /*&& RT_SUCCESS(rcU)*/ ? VINF_SUCCESS: VERR_GENERAL_FAILURE;
}

VPoxVHWASettings::VPoxVHWASettings ()
{
}

void VPoxVHWASettings::init(CSession &session)
{
    const QUuid uMachineID = session.GetMachine().GetId();

    mStretchLinearEnabled = gEDataManager->useLinearStretch(uMachineID);

    uint32_t aFourccs[VPOXVHWA_NUMFOURCC];
    int num = 0;
    if (gEDataManager->usePixelFormatAYUV(uMachineID))
        aFourccs[num++] = FOURCC_AYUV;
    if (gEDataManager->usePixelFormatUYVY(uMachineID))
        aFourccs[num++] = FOURCC_UYVY;
    if (gEDataManager->usePixelFormatYUY2(uMachineID))
        aFourccs[num++] = FOURCC_YUY2;
    if (gEDataManager->usePixelFormatYV12(uMachineID))
        aFourccs[num++] = FOURCC_YV12;

    mFourccEnabledCount = num;
    memcpy(mFourccEnabledList, aFourccs, num* sizeof (aFourccs[0]));
}

int VPoxVHWASettings::calcIntersection(int c1, const uint32_t *a1, int c2, const uint32_t *a2, int cOut, uint32_t *aOut)
{
    /* fourcc arrays are not big, so linear search is enough,
     * also no need to check for duplicates */
    int cMatch = 0;
    for (int i = 0; i < c1; ++i)
    {
        uint32_t cur1 = a1[i];
        for (int j = 0; j < c2; ++j)
        {
            uint32_t cur2 = a2[j];
            if (cur1 == cur2)
            {
                if (cOut > cMatch && aOut)
                    aOut[cMatch] = cur1;
                ++cMatch;
                break;
            }
        }
    }

    return cMatch;
}

#endif /* VPOX_GUI_USE_QGL */

