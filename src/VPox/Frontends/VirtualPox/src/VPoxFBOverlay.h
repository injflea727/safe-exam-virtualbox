/* $Id: VPoxFBOverlay.h $ */
/** @file
 * VPox Qt GUI - VPoxFrameBuffer Overly classes declarations.
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

#ifndef FEQT_INCLUDED_SRC_VPoxFBOverlay_h
#define FEQT_INCLUDED_SRC_VPoxFBOverlay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if defined(VPOX_GUI_USE_QGL) || defined(VPOX_WITH_VIDEOHWACCEL)

/* Defines: */
//#define VPOXQGL_PROF_BASE 1
//#define VPOXQGL_DBG_SURF 1
//#define VPOXVHWADBG_RENDERCHECK
#define VPOXVHWA_ALLOW_PRIMARY_AND_OVERLAY_ONLY 1

/* Qt includes: */
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h> /* QGLWidget drags in Windows.h; -Wall forces us to use wrapper. */
# include <iprt/stdint.h>      /* QGLWidget drags in stdint.h; -Wall forces us to use wrapper. */
#endif
#include <QGLWidget>

/* GUI includes: */
#include "UIDefs.h"
#include "VPoxFBOverlayCommon.h"
#include "runtime/UIFrameBuffer.h"
#include "runtime/UIMachineView.h"

/* COM includes: */
#include "COMEnums.h"

#include "CDisplay.h"

/* Other VPox includes: */
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>
#include <iprt/list.h>
#include <VPox/VPoxGL2D.h>
#ifdef VPOXVHWA_PROFILE_FPS
# include <iprt/stream.h>
#endif /* VPOXVHWA_PROFILE_FPS */

#ifdef DEBUG_misha
# define VPOXVHWA_PROFILE_FPS
#endif /* DEBUG_misha */

/* Forward declarations: */
class CSession;

#ifdef DEBUG
class VPoxVHWADbgTimer
{
public:
    VPoxVHWADbgTimer(uint32_t cPeriods);
    ~VPoxVHWADbgTimer();
    void frame();
    uint64_t everagePeriod() {return mPeriodSum / mcPeriods; }
    double fps() {return ((double)1000000000.0) / everagePeriod(); }
    uint64_t frames() {return mcFrames; }
private:
    uint64_t mPeriodSum;
    uint64_t *mpaPeriods;
    uint64_t mPrevTime;
    uint64_t mcFrames;
    uint32_t mcPeriods;
    uint32_t miPeriod;
};

#endif /* DEBUG */

class VPoxVHWASettings
{
public:
    VPoxVHWASettings ();
    void init(CSession &session);

    int fourccEnabledCount() const { return mFourccEnabledCount; }
    const uint32_t * fourccEnabledList() const { return mFourccEnabledList; }

    bool isStretchLinearEnabled() const { return mStretchLinearEnabled; }

    static int calcIntersection (int c1, const uint32_t *a1, int c2, const uint32_t *a2, int cOut, uint32_t *aOut);

    int getIntersection (const VPoxVHWAInfo &aInfo, int cOut, uint32_t *aOut)
    {
        return calcIntersection (mFourccEnabledCount, mFourccEnabledList, aInfo.getFourccSupportedCount(), aInfo.getFourccSupportedList(), cOut, aOut);
    }

    bool isSupported(const VPoxVHWAInfo &aInfo, uint32_t format)
    {
        return calcIntersection (mFourccEnabledCount, mFourccEnabledList, 1, &format, 0, NULL)
                && calcIntersection (aInfo.getFourccSupportedCount(), aInfo.getFourccSupportedList(), 1, &format, 0, NULL);
    }
private:
    uint32_t mFourccEnabledList[VPOXVHWA_NUMFOURCC];
    int mFourccEnabledCount;
    bool mStretchLinearEnabled;
};

class VPoxVHWADirtyRect
{
public:
    VPoxVHWADirtyRect() :
        mIsClear(true)
    {}

    VPoxVHWADirtyRect(const QRect & aRect)
    {
        if(aRect.isEmpty())
        {
            mIsClear = false;
            mRect = aRect;
        }
        else
        {
            mIsClear = true;
        }
    }

    bool isClear() const { return mIsClear; }

    void add(const QRect & aRect)
    {
        if(aRect.isEmpty())
            return;

        mRect = mIsClear ? aRect : mRect.united(aRect);
        mIsClear = false;
    }

    void add(const VPoxVHWADirtyRect & aRect)
    {
        if(aRect.isClear())
            return;
        add(aRect.rect());
    }

    void set(const QRect & aRect)
    {
        if(aRect.isEmpty())
        {
            mIsClear = true;
        }
        else
        {
            mRect = aRect;
            mIsClear = false;
        }
    }

    void clear() { mIsClear = true; }

    const QRect & rect() const {return mRect;}

    const QRect & toRect()
    {
        if(isClear())
        {
            mRect.setCoords(0, 0, -1, -1);
        }
        return mRect;
    }

    bool intersects(const QRect & aRect) const {return mIsClear ? false : mRect.intersects(aRect);}

    bool intersects(const VPoxVHWADirtyRect & aRect) const {return mIsClear ? false : aRect.intersects(mRect);}

    QRect united(const QRect & aRect) const {return mIsClear ? aRect : aRect.united(mRect);}

    bool contains(const QRect & aRect) const {return mIsClear ? false : aRect.contains(mRect);}

    void subst(const VPoxVHWADirtyRect & aRect) { if(!mIsClear && aRect.contains(mRect)) clear(); }

private:
    QRect mRect;
    bool mIsClear;
};

class VPoxVHWAColorKey
{
public:
    VPoxVHWAColorKey() :
        mUpper(0),
        mLower(0)
    {}

    VPoxVHWAColorKey(uint32_t aUpper, uint32_t aLower) :
        mUpper(aUpper),
        mLower(aLower)
    {}

    uint32_t upper() const {return mUpper; }
    uint32_t lower() const {return mLower; }

    bool operator==(const VPoxVHWAColorKey & other) const { return mUpper == other.mUpper && mLower == other.mLower; }
    bool operator!=(const VPoxVHWAColorKey & other) const { return !(*this == other); }
private:
    uint32_t mUpper;
    uint32_t mLower;
};

class VPoxVHWAColorComponent
{
public:
    VPoxVHWAColorComponent() :
        mMask(0),
        mRange(0),
        mOffset(32),
        mcBits(0)
    {}

    VPoxVHWAColorComponent(uint32_t aMask);

    uint32_t mask() const { return mMask; }
    uint32_t range() const { return mRange; }
    uint32_t offset() const { return mOffset; }
    uint32_t cBits() const { return mcBits; }
    uint32_t colorVal(uint32_t col) const { return (col & mMask) >> mOffset; }
    float colorValNorm(uint32_t col) const { return ((float)colorVal(col))/mRange; }
private:
    uint32_t mMask;
    uint32_t mRange;
    uint32_t mOffset;
    uint32_t mcBits;
};

class VPoxVHWAColorFormat
{
public:

    VPoxVHWAColorFormat(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b);
    VPoxVHWAColorFormat(uint32_t fourcc);
    VPoxVHWAColorFormat() :
        mBitsPerPixel(0) /* needed for isValid() to work */
    {}
    GLint internalFormat() const {return mInternalFormat; }
    GLenum format() const {return mFormat; }
    GLenum type() const {return mType; }
    bool isValid() const {return mBitsPerPixel != 0; }
    uint32_t fourcc() const {return mDataFormat;}
    uint32_t bitsPerPixel() const { return mBitsPerPixel; }
    uint32_t bitsPerPixelTex() const { return mBitsPerPixelTex; }
    void pixel2Normalized(uint32_t pix, float *r, float *g, float *b) const;
    uint32_t widthCompression() const {return mWidthCompression;}
    uint32_t heightCompression() const {return mHeightCompression;}
    const VPoxVHWAColorComponent& r() const {return mR;}
    const VPoxVHWAColorComponent& g() const {return mG;}
    const VPoxVHWAColorComponent& b() const {return mB;}
    const VPoxVHWAColorComponent& a() const {return mA;}

    bool equals (const VPoxVHWAColorFormat & other) const;

    ulong toVPoxPixelFormat() const
    {
        if (!mDataFormat)
        {
            /* RGB data */
            switch (mFormat)
            {
                case GL_BGRA_EXT:
                    return KBitmapFormat_BGR;
            }
        }
        return KBitmapFormat_Opaque;
    }

private:
    void init(uint32_t bitsPerPixel, uint32_t r, uint32_t g, uint32_t b);
    void init(uint32_t fourcc);

    GLint mInternalFormat;
    GLenum mFormat;
    GLenum mType;
    uint32_t mDataFormat;

    uint32_t mBitsPerPixel;
    uint32_t mBitsPerPixelTex;
    uint32_t mWidthCompression;
    uint32_t mHeightCompression;
    VPoxVHWAColorComponent mR;
    VPoxVHWAColorComponent mG;
    VPoxVHWAColorComponent mB;
    VPoxVHWAColorComponent mA;
};

class VPoxVHWATexture
{
public:
    VPoxVHWATexture() :
            mAddress(NULL),
            mTexture(0),
            mBytesPerPixel(0),
            mBytesPerPixelTex(0),
            mBytesPerLine(0),
            mScaleFuncttion(GL_NEAREST)
{}
    VPoxVHWATexture(const QRect & aRect, const VPoxVHWAColorFormat &aFormat, uint32_t bytesPerLine, GLint scaleFuncttion);
    virtual ~VPoxVHWATexture();
    virtual void init(uchar *pvMem);
    void setAddress(uchar *pvMem) {mAddress = pvMem;}
    void update(const QRect * pRect) { doUpdate(mAddress, pRect);}
    void bind() {glBindTexture(texTarget(), mTexture);}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);

    const QRect & texRect() {return mTexRect;}
    const QRect & rect() {return mRect;}
    uchar * address(){ return mAddress; }
    uint32_t rectSizeTex(const QRect * pRect) {return pRect->width() * pRect->height() * mBytesPerPixelTex;}
    uchar * pointAddress(int x, int y)
    {
        x = toXTex(x);
        y = toYTex(y);
        return pointAddressTex(x, y);
    }
    uint32_t pointOffsetTex(int x, int y) { return y*mBytesPerLine + x*mBytesPerPixelTex; }
    uchar * pointAddressTex(int x, int y) { return mAddress + pointOffsetTex(x, y); }
    int toXTex(int x) {return x/mColorFormat.widthCompression();}
    int toYTex(int y) {return y/mColorFormat.heightCompression();}
    ulong memSize(){ return mBytesPerLine * mRect.height(); }
    uint32_t bytesPerLine() {return mBytesPerLine; }
#ifdef DEBUG_misha
    void dbgDump();
#endif

protected:
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
    virtual void initParams();
    virtual void load();
    virtual GLenum texTarget() {return GL_TEXTURE_2D; }
    GLuint texture() {return mTexture;}

    QRect mTexRect; /* texture size */
    QRect mRect; /* img size */
    uchar * mAddress;
    GLuint mTexture;
    uint32_t mBytesPerPixel;
    uint32_t mBytesPerPixelTex;
    uint32_t mBytesPerLine;
    VPoxVHWAColorFormat mColorFormat;
    GLint mScaleFuncttion;
private:
    void uninit();

    friend class VPoxVHWAFBO;
};

class VPoxVHWATextureNP2 : public VPoxVHWATexture
{
public:
    VPoxVHWATextureNP2() : VPoxVHWATexture() {}
    VPoxVHWATextureNP2(const QRect & aRect, const VPoxVHWAColorFormat &aFormat, uint32_t bytesPerLine, GLint scaleFuncttion) :
        VPoxVHWATexture(aRect, aFormat, bytesPerLine, scaleFuncttion){
        mTexRect = QRect(0, 0, aRect.width()/aFormat.widthCompression(), aRect.height()/aFormat.heightCompression());
    }
};

class VPoxVHWATextureNP2Rect : public VPoxVHWATextureNP2
{
public:
    VPoxVHWATextureNP2Rect() : VPoxVHWATextureNP2() {}
    VPoxVHWATextureNP2Rect(const QRect & aRect, const VPoxVHWAColorFormat &aFormat, uint32_t bytesPerLine, GLint scaleFuncttion) :
        VPoxVHWATextureNP2(aRect, aFormat, bytesPerLine, scaleFuncttion){}

    virtual void texCoord(int x, int y);
    virtual void multiTexCoord(GLenum texUnit, int x, int y);
protected:
    virtual GLenum texTarget();
};

class VPoxVHWATextureNP2RectPBO : public VPoxVHWATextureNP2Rect
{
public:
    VPoxVHWATextureNP2RectPBO() :
        VPoxVHWATextureNP2Rect(),
        mPBO(0)
    {}
    VPoxVHWATextureNP2RectPBO(const QRect & aRect, const VPoxVHWAColorFormat &aFormat, uint32_t bytesPerLine, GLint scaleFuncttion) :
        VPoxVHWATextureNP2Rect(aRect, aFormat, bytesPerLine, scaleFuncttion),
        mPBO(0)
    {}

    virtual ~VPoxVHWATextureNP2RectPBO();

    virtual void init(uchar *pvMem);
protected:
    virtual void load();
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
    GLuint mPBO;
};

class VPoxVHWATextureNP2RectPBOMapped : public VPoxVHWATextureNP2RectPBO
{
public:
    VPoxVHWATextureNP2RectPBOMapped() :
        VPoxVHWATextureNP2RectPBO(),
        mpMappedAllignedBuffer(NULL),
        mcbAllignedBufferSize(0),
        mcbOffset(0)
    {}
    VPoxVHWATextureNP2RectPBOMapped(const QRect & aRect, const VPoxVHWAColorFormat &aFormat, uint32_t bytesPerLine, GLint scaleFuncttion) :
            VPoxVHWATextureNP2RectPBO(aRect, aFormat, bytesPerLine, scaleFuncttion),
            mpMappedAllignedBuffer(NULL),
            mcbOffset(0)
    {
        mcbAllignedBufferSize = alignSize((size_t)memSize());
        mcbActualBufferSize = mcbAllignedBufferSize + 0x1fff;
    }

    uchar* mapAlignedBuffer();
    void   unmapBuffer();
    size_t alignedBufferSize() { return mcbAllignedBufferSize; }

    static size_t alignSize(size_t size)
    {
        size_t alSize = size & ~((size_t)0xfff);
        return alSize == size ? alSize : alSize + 0x1000;
    }

    static void* alignBuffer(void* pvMem) { return (void*)(((uintptr_t)pvMem) & ~((uintptr_t)0xfff)); }
    static size_t calcOffset(void* pvBase, void* pvOffset) { return (size_t)(((uintptr_t)pvBase) - ((uintptr_t)pvOffset)); }
protected:
    virtual void load();
    virtual void doUpdate(uchar * pAddress, const QRect * pRect);
private:
    uchar* mpMappedAllignedBuffer;
    size_t mcbAllignedBufferSize;
    size_t mcbOffset;
    size_t mcbActualBufferSize;
};

#define VPOXVHWAIMG_PBO    0x00000001U
#define VPOXVHWAIMG_PBOIMG 0x00000002U
#define VPOXVHWAIMG_FBO    0x00000004U
#define VPOXVHWAIMG_LINEAR 0x00000008U
typedef uint32_t VPOXVHWAIMG_TYPE;

class VPoxVHWATextureImage
{
public:
    VPoxVHWATextureImage(const QRect &size, const VPoxVHWAColorFormat &format, class VPoxVHWAGlProgramMngr * aMgr, VPOXVHWAIMG_TYPE flags);

    virtual ~VPoxVHWATextureImage()
    {
        for(uint i = 0; i < mcTex; i++)
        {
            delete mpTex[i];
        }
    }

    virtual void init(uchar *pvMem)
    {
        for(uint32_t i = 0; i < mcTex; i++)
        {
            mpTex[i]->init(pvMem);
            pvMem += mpTex[i]->memSize();
        }
    }

    virtual void update(const QRect * pRect)
    {
        mpTex[0]->update(pRect);
        if(mColorFormat.fourcc() == FOURCC_YV12)
        {
            if(pRect)
            {
                QRect rect(pRect->x()/2, pRect->y()/2,
                        pRect->width()/2, pRect->height()/2);
                mpTex[1]->update(&rect);
                mpTex[2]->update(&rect);
            }
            else
            {
                mpTex[1]->update(NULL);
                mpTex[2]->update(NULL);
            }
        }
    }

    virtual void display(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected);


    virtual void display();

    void deleteDisplay();

    int initDisplay(VPoxVHWATextureImage *pDst,
            const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    bool displayInitialized() { return !!mVisibleDisplay;}

    virtual void setAddress(uchar *pvMem)
    {
        for(uint32_t i = 0; i < mcTex; i++)
        {
            mpTex[i]->setAddress(pvMem);
            pvMem += mpTex[i]->memSize();
        }
    }

    const QRect &rect()
    {
        return mpTex[0]->rect();
    }

    size_t memSize()
    {
        size_t size = 0;
        for(uint32_t i = 0; i < mcTex; i++)
        {
            size+=mpTex[i]->memSize();
        }
        return size;
    }

    uint32_t bytesPerLine() { return mpTex[0]->bytesPerLine(); }

    const VPoxVHWAColorFormat &pixelFormat() { return mColorFormat; }

    uint32_t numComponents() {return mcTex;}

    VPoxVHWATexture* component(uint32_t i) {return mpTex[i]; }

    const VPoxVHWATextureImage *dst() { return mpDst;}
    const QRect& dstRect() { return mDstRect; }
    const QRect& srcRect() { return mSrcRect; }
    const VPoxVHWAColorKey* dstCKey() { return mpDstCKey; }
    const VPoxVHWAColorKey* srcCKey() { return mpSrcCKey; }
    bool notIntersectedMode() { return mbNotIntersected; }

    static uint32_t calcBytesPerLine(const VPoxVHWAColorFormat & format, int width);
    static uint32_t calcMemSize(const VPoxVHWAColorFormat & format, int width, int height);

#ifdef DEBUG_misha
    void dbgDump();
#endif

protected:
    static int setCKey(class VPoxVHWAGlProgramVHWA * pProgram, const VPoxVHWAColorFormat * pFormat, const VPoxVHWAColorKey * pCKey, bool bDst);

    static bool matchCKeys(const VPoxVHWAColorKey * pCKey1, const VPoxVHWAColorKey * pCKey2)
    {
        return (pCKey1 == NULL && pCKey2 == NULL)
                || (*pCKey1 == *pCKey2);
    }

    void runDisplay(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect)
    {
        bind(pDst);

        draw(pDst, pDstRect, pSrcRect);
    }

    virtual void draw(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect);

    virtual uint32_t texCoord(GLenum tex, int x, int y)
    {
        uint32_t c = 1;
        mpTex[0]->multiTexCoord(tex, x, y);
        if(mColorFormat.fourcc() == FOURCC_YV12)
        {
            int x2 = x/2;
            int y2 = y/2;
            mpTex[1]->multiTexCoord(tex + 1, x2, y2);
            ++c;
        }
        return c;
    }

    virtual void bind(VPoxVHWATextureImage * pPrimary);

    virtual uint32_t calcProgramType(VPoxVHWATextureImage *pDst, const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    virtual class VPoxVHWAGlProgramVHWA * calcProgram(VPoxVHWATextureImage *pDst, const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    virtual int createDisplay(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected,
            GLuint *pDisplay, class VPoxVHWAGlProgramVHWA ** ppProgram);

    int createSetDisplay(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected);

    virtual int createDisplayList(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected,
            GLuint *pDisplay);

    virtual void deleteDisplayList();

    virtual void updateCKeys(VPoxVHWATextureImage * pDst, class VPoxVHWAGlProgramVHWA * pProgram, const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey);
    virtual void updateSetCKeys(const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey);

    void internalSetDstCKey(const VPoxVHWAColorKey * pDstCKey);
    void internalSetSrcCKey(const VPoxVHWAColorKey * pSrcCKey);

    VPoxVHWATexture *mpTex[3];
    uint32_t mcTex;
    GLuint mVisibleDisplay;
    class VPoxVHWAGlProgramVHWA * mpProgram;
    class VPoxVHWAGlProgramMngr * mProgramMngr;
    VPoxVHWAColorFormat mColorFormat;

    /* display info */
    VPoxVHWATextureImage *mpDst;
    QRect mDstRect;
    QRect mSrcRect;
    VPoxVHWAColorKey * mpDstCKey;
    VPoxVHWAColorKey * mpSrcCKey;
    VPoxVHWAColorKey mDstCKey;
    VPoxVHWAColorKey mSrcCKey;
    bool mbNotIntersected;
};

class VPoxVHWATextureImagePBO : public VPoxVHWATextureImage
{
public:
    VPoxVHWATextureImagePBO(const QRect &size, const VPoxVHWAColorFormat &format, class VPoxVHWAGlProgramMngr * aMgr, VPOXVHWAIMG_TYPE flags) :
            VPoxVHWATextureImage(size, format, aMgr, flags & (~VPOXVHWAIMG_PBO)),
            mPBO(0)
    {
    }

    virtual ~VPoxVHWATextureImagePBO()
    {
        if(mPBO)
        {
            VPOXQGL_CHECKERR(
                    vpoxglDeleteBuffers(1, &mPBO);
                    );
        }
    }

    virtual void init(uchar *pvMem)
    {
        VPoxVHWATextureImage::init(pvMem);

        VPOXQGL_CHECKERR(
                vpoxglGenBuffers(1, &mPBO);
                );
        mAddress = pvMem;

        VPOXQGL_CHECKERR(
                vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
            );

        VPOXQGL_CHECKERR(
                vpoxglBufferData(GL_PIXEL_UNPACK_BUFFER, memSize(), NULL, GL_STREAM_DRAW);
            );

        GLvoid *buf = vpoxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        Assert(buf);
        if(buf)
        {
            memcpy(buf, mAddress, memSize());

            bool unmapped = vpoxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            Assert(unmapped); NOREF(unmapped);
        }

        vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    }

    virtual void update(const QRect * pRect)
    {
        VPOXQGL_CHECKERR(
                vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPBO);
        );

        GLvoid *buf;

        VPOXQGL_CHECKERR(
                buf = vpoxglMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                );
        Assert(buf);
        if(buf)
        {
#ifdef VPOXVHWADBG_RENDERCHECK
            uint32_t * pBuf32 = (uint32_t*)buf;
            uchar * pBuf8 = (uchar*)buf;
            for(uint32_t i = 0; i < mcTex; i++)
            {
                uint32_t dbgSetVal = 0x40404040 * (i+1);
                for(uint32_t k = 0; k < mpTex[i]->memSize()/sizeof(pBuf32[0]); k++)
                {
                    pBuf32[k] = dbgSetVal;
                }

                pBuf8 += mpTex[i]->memSize();
                pBuf32 = (uint32_t *)pBuf8;
            }
#else
            memcpy(buf, mAddress, memSize());
#endif

            bool unmapped;
            VPOXQGL_CHECKERR(
                    unmapped = vpoxglUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                    );

            Assert(unmapped); NOREF(unmapped);

            VPoxVHWATextureImage::setAddress(0);

            VPoxVHWATextureImage::update(NULL);

            VPoxVHWATextureImage::setAddress(mAddress);

            vpoxglBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        else
        {
            VPOXQGLLOGREL(("failed to map PBO, trying fallback to non-PBO approach\n"));

            VPoxVHWATextureImage::setAddress(mAddress);

            VPoxVHWATextureImage::update(pRect);
        }
    }

    virtual void setAddress(uchar *pvMem)
    {
        mAddress = pvMem;
    }
private:
    GLuint mPBO;
    uchar* mAddress;
};

class VPoxVHWAHandleTable
{
public:
    VPoxVHWAHandleTable(uint32_t maxSize);
    ~VPoxVHWAHandleTable();
    uint32_t put(void * data);
    bool mapPut(uint32_t h, void * data);
    void* get(uint32_t h);
    void* remove(uint32_t h);
private:
    void doPut(uint32_t h, void * data);
    void doRemove(uint32_t h);
    void** mTable;
    uint32_t mcSize;
    uint32_t mcUsage;
    uint32_t mCursor;
};

/* data flow:
 * I. NON-Yinverted surface:
 * 1.direct memory update (paint, lock/unlock):
 *  mem->tex->fb
 * 2.blt
 *  srcTex->invFB->tex->fb
 *              |->mem
 *
 * II. Yinverted surface:
 * 1.direct memory update (paint, lock/unlock):
 *  mem->tex->fb
 * 2.blt
 *  srcTex->fb->tex
 *           |->mem
 *
 * III. flip support:
 * 1. Yinverted<->NON-YInverted conversion :
 *  mem->tex-(rotate model view, force LAZY complete fb update)->invFB->tex
 *  fb-->|                                                           |->mem
 * */
class VPoxVHWASurfaceBase
{
public:
    VPoxVHWASurfaceBase (class VPoxVHWAImage *pImage,
            const QSize & aSize,
            const QRect & aTargRect,
            const QRect & aSrcRect,
            const QRect & aVisTargRect,
            VPoxVHWAColorFormat & aColorFormat,
            VPoxVHWAColorKey * pSrcBltCKey, VPoxVHWAColorKey * pDstBltCKey,
            VPoxVHWAColorKey * pSrcOverlayCKey, VPoxVHWAColorKey * pDstOverlayCKey,
            VPOXVHWAIMG_TYPE aImgFlags);

    virtual ~VPoxVHWASurfaceBase();

    void init (VPoxVHWASurfaceBase * pPrimary, uchar *pvMem);

    void uninit();

    static void globalInit();

    int lock (const QRect * pRect, uint32_t flags);

    int unlock();

    void updatedMem (const QRect * aRect);

    bool performDisplay (VPoxVHWASurfaceBase *pPrimary, bool bForce);

    void setRects (const QRect & aTargRect, const QRect & aSrcRect);
    void setTargRectPosition (const QPoint & aPoint);

    void updateVisibility (VPoxVHWASurfaceBase *pPrimary, const QRect & aVisibleTargRect, bool bNotIntersected, bool bForce);

    static ulong calcBytesPerPixel (GLenum format, GLenum type);

    static GLsizei makePowerOf2 (GLsizei val);

    bool    addressAlocated() const { return mFreeAddress; }
    uchar * address() { return mAddress; }

    ulong   memSize();

    ulong width() const { return mRect.width();  }
    ulong height() const { return mRect.height(); }
    const QSize size() const {return mRect.size();}

    uint32_t fourcc() const {return mImage->pixelFormat().fourcc(); }

    ulong  bitsPerPixel() const { return mImage->pixelFormat().bitsPerPixel(); }
    ulong  bytesPerLine() const { return mImage->bytesPerLine(); }

    const VPoxVHWAColorKey * dstBltCKey() const { return mpDstBltCKey; }
    const VPoxVHWAColorKey * srcBltCKey() const { return mpSrcBltCKey; }
    const VPoxVHWAColorKey * dstOverlayCKey() const { return mpDstOverlayCKey; }
    const VPoxVHWAColorKey * defaultSrcOverlayCKey() const { return mpDefaultSrcOverlayCKey; }
    const VPoxVHWAColorKey * defaultDstOverlayCKey() const { return mpDefaultDstOverlayCKey; }
    const VPoxVHWAColorKey * srcOverlayCKey() const { return mpSrcOverlayCKey; }
    void resetDefaultSrcOverlayCKey() { mpSrcOverlayCKey = mpDefaultSrcOverlayCKey; }
    void resetDefaultDstOverlayCKey() { mpDstOverlayCKey = mpDefaultDstOverlayCKey; }

    void setDstBltCKey (const VPoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mDstBltCKey = *ckey;
            mpDstBltCKey = &mDstBltCKey;
        }
        else
        {
            mpDstBltCKey = NULL;
        }
    }

    void setSrcBltCKey (const VPoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mSrcBltCKey = *ckey;
            mpSrcBltCKey = &mSrcBltCKey;
        }
        else
        {
            mpSrcBltCKey = NULL;
        }
    }

    void setDefaultDstOverlayCKey (const VPoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mDefaultDstOverlayCKey = *ckey;
            mpDefaultDstOverlayCKey = &mDefaultDstOverlayCKey;
        }
        else
        {
            mpDefaultDstOverlayCKey = NULL;
        }
    }

    void setDefaultSrcOverlayCKey (const VPoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mDefaultSrcOverlayCKey = *ckey;
            mpDefaultSrcOverlayCKey = &mDefaultSrcOverlayCKey;
        }
        else
        {
            mpDefaultSrcOverlayCKey = NULL;
        }
    }

    void setOverriddenDstOverlayCKey (const VPoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mOverriddenDstOverlayCKey = *ckey;
            mpDstOverlayCKey = &mOverriddenDstOverlayCKey;
        }
        else
        {
            mpDstOverlayCKey = NULL;
        }
    }

    void setOverriddenSrcOverlayCKey (const VPoxVHWAColorKey * ckey)
    {
        if(ckey)
        {
            mOverriddenSrcOverlayCKey = *ckey;
            mpSrcOverlayCKey = &mOverriddenSrcOverlayCKey;
        }
        else
        {
            mpSrcOverlayCKey = NULL;
        }
    }

    const VPoxVHWAColorKey * getActiveSrcOverlayCKey()
    {
        return mpSrcOverlayCKey;
    }

    const VPoxVHWAColorKey * getActiveDstOverlayCKey (VPoxVHWASurfaceBase * pPrimary)
    {
        return mpDstOverlayCKey ? mpDefaultDstOverlayCKey : (pPrimary ? pPrimary->mpDstOverlayCKey : NULL);
    }

    const VPoxVHWAColorFormat & pixelFormat() const { return mImage->pixelFormat(); }

    void setAddress(uchar * addr);

    const QRect& rect() const {return mRect;}
    const QRect& srcRect() const {return mSrcRect; }
    const QRect& targRect() const {return mTargRect; }
    class VPoxVHWASurfList * getComplexList() {return mComplexList; }

    class VPoxVHWAGlProgramMngr * getGlProgramMngr();

    uint32_t handle() const {return mHGHandle;}
    void setHandle(uint32_t h) {mHGHandle = h;}

    const VPoxVHWADirtyRect & getDirtyRect() { return mUpdateMem2TexRect; }

    VPoxVHWASurfaceBase * primary() { return mpPrimary; }
    void setPrimary(VPoxVHWASurfaceBase *apPrimary) { mpPrimary = apPrimary; }
private:
    void setRectValues (const QRect & aTargRect, const QRect & aSrcRect);
    void setVisibleRectValues (const QRect & aVisTargRect);

    void setComplexList (VPoxVHWASurfList *aComplexList) { mComplexList = aComplexList; }
    void initDisplay();

    bool synchTexMem (const QRect * aRect);

    int performBlt (const QRect * pDstRect, VPoxVHWASurfaceBase * pSrcSurface, const QRect * pSrcRect, const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool blt);

    QRect mRect; /* == Inv FB size */

    QRect mSrcRect;
    QRect mTargRect; /* == Vis FB size */

    QRect mVisibleTargRect;
    QRect mVisibleSrcRect;

    class VPoxVHWATextureImage * mImage;

    uchar * mAddress;

    VPoxVHWAColorKey *mpSrcBltCKey;
    VPoxVHWAColorKey *mpDstBltCKey;
    VPoxVHWAColorKey *mpSrcOverlayCKey;
    VPoxVHWAColorKey *mpDstOverlayCKey;

    VPoxVHWAColorKey *mpDefaultDstOverlayCKey;
    VPoxVHWAColorKey *mpDefaultSrcOverlayCKey;

    VPoxVHWAColorKey mSrcBltCKey;
    VPoxVHWAColorKey mDstBltCKey;
    VPoxVHWAColorKey mOverriddenSrcOverlayCKey;
    VPoxVHWAColorKey mOverriddenDstOverlayCKey;
    VPoxVHWAColorKey mDefaultDstOverlayCKey;
    VPoxVHWAColorKey mDefaultSrcOverlayCKey;

    int mLockCount;
    /* memory buffer not reflected in fm and texture, e.g if memory buffer is replaced or in case of lock/unlock  */
    VPoxVHWADirtyRect mUpdateMem2TexRect;

    bool mFreeAddress;
    bool mbNotIntersected;

    class VPoxVHWASurfList *mComplexList;

    VPoxVHWASurfaceBase *mpPrimary;

    uint32_t mHGHandle;

    class VPoxVHWAImage *mpImage;

#ifdef DEBUG
public:
    uint64_t cFlipsCurr;
    uint64_t cFlipsTarg;
#endif
    friend class VPoxVHWASurfList;
};

typedef std::list <VPoxVHWASurfaceBase*> SurfList;
typedef std::list <VPoxVHWASurfList*> OverlayList;
typedef std::list <struct VPOXVHWACMD *> VHWACommandList;

class VPoxVHWASurfList
{
public:

    VPoxVHWASurfList() : mCurrent(NULL) {}

    void moveTo(VPoxVHWASurfList *pDst)
    {
        for (SurfList::iterator it = mSurfaces.begin();
             it != mSurfaces.end(); it = mSurfaces.begin())
        {
            pDst->add((*it));
        }

        Assert(empty());
    }

    void add(VPoxVHWASurfaceBase *pSurf)
    {
        VPoxVHWASurfList * pOld = pSurf->getComplexList();
        if(pOld)
        {
            pOld->remove(pSurf);
        }
        mSurfaces.push_back(pSurf);
        pSurf->setComplexList(this);
    }
/*
    void clear()
    {
        for (SurfList::iterator it = mSurfaces.begin();
             it != mSurfaces.end(); ++ it)
        {
            (*it)->setComplexList(NULL);
        }
        mSurfaces.clear();
        mCurrent = NULL;
    }
*/
    size_t size() const {return mSurfaces.size(); }

    void remove(VPoxVHWASurfaceBase *pSurf)
    {
        mSurfaces.remove(pSurf);
        pSurf->setComplexList(NULL);
        if(mCurrent == pSurf)
            mCurrent = NULL;
    }

    bool empty() { return mSurfaces.empty(); }

    void setCurrentVisible(VPoxVHWASurfaceBase *pSurf)
    {
        mCurrent = pSurf;
    }

    VPoxVHWASurfaceBase * current() { return mCurrent; }
    const SurfList & surfaces() const {return mSurfaces;}

private:

    SurfList mSurfaces;
    VPoxVHWASurfaceBase* mCurrent;
};

class VPoxVHWADisplay
{
public:
    VPoxVHWADisplay() :
        mSurfVGA(NULL),
        mbDisplayPrimary(true)
//        ,
//        mSurfPrimary(NULL)
    {}

    VPoxVHWASurfaceBase * setVGA(VPoxVHWASurfaceBase * pVga)
    {
        VPoxVHWASurfaceBase * old = mSurfVGA;
        mSurfVGA = pVga;
        if (!mPrimary.empty())
        {
            VPoxVHWASurfList *pNewList = new VPoxVHWASurfList();
            mPrimary.moveTo(pNewList);
            Assert(mPrimary.empty());
        }
        if(pVga)
        {
            Assert(!pVga->getComplexList());
            mPrimary.add(pVga);
            mPrimary.setCurrentVisible(pVga);
        }
        mOverlays.clear();
        return old;
    }

    VPoxVHWASurfaceBase * updateVGA(VPoxVHWASurfaceBase * pVga)
    {
        VPoxVHWASurfaceBase * old = mSurfVGA;
        Assert(old);
        mSurfVGA = pVga;
        return old;
    }

    VPoxVHWASurfaceBase * getVGA() const
    {
        return mSurfVGA;
    }

    VPoxVHWASurfaceBase * getPrimary()
    {
        return mPrimary.current();
    }

    void addOverlay(VPoxVHWASurfList * pSurf)
    {
        mOverlays.push_back(pSurf);
    }

    void checkAddOverlay(VPoxVHWASurfList * pSurf)
    {
        if(!hasOverlay(pSurf))
            addOverlay(pSurf);
    }

    bool hasOverlay(VPoxVHWASurfList * pSurf)
    {
        for (OverlayList::iterator it = mOverlays.begin();
             it != mOverlays.end(); ++ it)
        {
            if((*it) == pSurf)
            {
                return true;
            }
        }
        return false;
    }

    void removeOverlay(VPoxVHWASurfList * pSurf)
    {
        mOverlays.remove(pSurf);
    }

    bool performDisplay(bool bForce)
    {
        VPoxVHWASurfaceBase * pPrimary = mPrimary.current();

        if(mbDisplayPrimary)
        {
#ifdef DEBUG_misha
            /* should only display overlay now */
            AssertBreakpoint();
#endif
            bForce |= pPrimary->performDisplay(NULL, bForce);
        }

        for (OverlayList::const_iterator it = mOverlays.begin();
             it != mOverlays.end(); ++ it)
        {
            VPoxVHWASurfaceBase * pOverlay = (*it)->current();
            if(pOverlay)
            {
                bForce |= pOverlay->performDisplay(pPrimary, bForce);
            }
        }
        return bForce;
    }

    bool isPrimary(VPoxVHWASurfaceBase * pSurf) { return pSurf->getComplexList() == &mPrimary; }

    void setDisplayPrimary(bool bDisplay) { mbDisplayPrimary = bDisplay; }

    const OverlayList & overlays() const {return mOverlays;}
    const VPoxVHWASurfList & primaries() const { return mPrimary; }

private:
    VPoxVHWASurfaceBase *mSurfVGA;
    VPoxVHWASurfList mPrimary;

    OverlayList mOverlays;

    bool mbDisplayPrimary;
};

typedef void (*PFNVPOXQGLFUNC)(void*, void*);

typedef enum
{
    VPOXVHWA_PIPECMD_PAINT = 1,
    VPOXVHWA_PIPECMD_VHWA,
    VPOXVHWA_PIPECMD_FUNC
}VPOXVHWA_PIPECMD_TYPE;

typedef struct VPOXVHWAFUNCCALLBACKINFO
{
    PFNVPOXQGLFUNC pfnCallback;
    void * pContext1;
    void * pContext2;
}VPOXVHWAFUNCCALLBACKINFO;

class VPoxVHWACommandElement
{
public:
    void setVHWACmd(struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCmd, int enmCmd, bool fGuestCmd)
    {
        mType = VPOXVHWA_PIPECMD_VHWA;
        u.s.mpCmd = pCmd;
        u.s.menmCmd = enmCmd;
        u.s.mfGuestCmd = fGuestCmd;
    }

    void setPaintCmd(const QRect & aRect)
    {
        mType = VPOXVHWA_PIPECMD_PAINT;
        mRect = aRect;
    }

    void setFunc(const VPOXVHWAFUNCCALLBACKINFO & aOp)
    {
        mType = VPOXVHWA_PIPECMD_FUNC;
        u.mFuncCallback = aOp;
    }

    void setData(VPOXVHWA_PIPECMD_TYPE aType, void *pvData, int /*VPOXVHWACMD_TYPE*/ enmCmd, bool fGuestCmd = false)
    {
        switch (aType)
        {
        case VPOXVHWA_PIPECMD_PAINT:
            setPaintCmd(*((QRect *)pvData));
            break;
        case VPOXVHWA_PIPECMD_VHWA:
            setVHWACmd((struct VPOXVHWACMD *)pvData, enmCmd, fGuestCmd);
            break;
        case VPOXVHWA_PIPECMD_FUNC:
            setFunc(*((VPOXVHWAFUNCCALLBACKINFO *)pvData));
            break;
        default:
            AssertFailed();
            mType = (VPOXVHWA_PIPECMD_TYPE)0;
            break;
        }
    }

    VPOXVHWA_PIPECMD_TYPE type() const {return mType;}
    const QRect & rect() const {return mRect;}
    struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *vhwaCmdPtr() const      { return u.s.mpCmd; }
    int /*VPOXVHWACMD_TYPE*/                        vhwaCmdType() const     { return u.s.menmCmd; }
    bool                                            vhwaIsGuestCmd() const  { return u.s.mfGuestCmd; }
    const VPOXVHWAFUNCCALLBACKINFO & func() const {return u.mFuncCallback; }

    RTLISTNODE ListNode;
private:
    VPOXVHWA_PIPECMD_TYPE mType;
    union
    {
        struct
        {
            struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *mpCmd;
            int /*VPOXVHWACMD_TYPE*/                        menmCmd;
            bool                                            mfGuestCmd;
        } s;
        VPOXVHWAFUNCCALLBACKINFO mFuncCallback;
    } u;
    QRect                 mRect;
};

class VPoxVHWARefCounter
{
#define VPOXVHWA_INIFITE_WAITCOUNT (~0U)
public:
    VPoxVHWARefCounter() : m_cRefs(0) {}
    VPoxVHWARefCounter(uint32_t cRefs) : m_cRefs(cRefs) {}
    void inc() { ASMAtomicIncU32(&m_cRefs); }
    uint32_t dec()
    {
        uint32_t cRefs = ASMAtomicDecU32(&m_cRefs);
        Assert(cRefs < UINT32_MAX / 2);
        return cRefs;
    }

    uint32_t refs() { return ASMAtomicReadU32(&m_cRefs); }

    int wait0(RTMSINTERVAL ms = 1000, uint32_t cWaits = VPOXVHWA_INIFITE_WAITCOUNT)
    {
        int rc = VINF_SUCCESS;
        do
        {
            if (!refs())
                break;
            if (!cWaits)
            {
                rc = VERR_TIMEOUT;
                break;
            }
            if (cWaits != VPOXVHWA_INIFITE_WAITCOUNT)
                --cWaits;
            rc = RTThreadSleep(ms);
            AssertRC(rc);
            if (!RT_SUCCESS(rc))
                break;
        } while(1);
        return rc;
    }
private:
    volatile uint32_t m_cRefs;
};

class VPoxVHWAEntriesCache;
class VPoxVHWACommandElementProcessor
{
public:
    VPoxVHWACommandElementProcessor();
    void init(QObject *pNotifyObject);
    ~VPoxVHWACommandElementProcessor();
    void postCmd(VPOXVHWA_PIPECMD_TYPE aType, void *pvData, int /*VPOXVHWACMD_TYPE*/ enmCmdInt, bool fGuestCmd);
    VPoxVHWACommandElement *getCmd();
    void doneCmd();
    void reset(CDisplay *pDisplay);
    void setNotifyObject(QObject *pNotifyObject);
    int loadExec (struct SSMHANDLE * pSSM, uint32_t u32Version, void *pvVRAM);
    void saveExec (struct SSMHANDLE * pSSM, void *pvVRAM);
    void disable();
    void enable();
    void lock();
    void unlock();
private:
    RTCRITSECT mCritSect;
    RTLISTNODE mCommandList;
    QObject *m_pNotifyObject;
    VPoxVHWARefCounter m_NotifyObjectRefs;
    VPoxVHWACommandElement *mpCurCmd;
    bool mbResetting;
    uint32_t mcDisabled;
    VPoxVHWAEntriesCache *m_pCmdEntryCache;
};

/* added to workaround this ** [VPox|UI] duplication */
class VPoxFBSizeInfo
{
public:

    VPoxFBSizeInfo() {}
    template<class T> VPoxFBSizeInfo(T *pFb) :
        m_visualState(pFb->visualState()),
        mPixelFormat(pFb->pixelFormat()), mVRAM(pFb->address()), mBitsPerPixel(pFb->bitsPerPixel()),
        mBytesPerLine(pFb->bytesPerLine()), mWidth(pFb->width()), mHeight(pFb->height()),
        m_dScaleFactor(pFb->scaleFactor()), m_scaledSize(pFb->scaledSize()), m_fUseUnscaledHiDPIOutput(pFb->useUnscaledHiDPIOutput()),
        mUsesGuestVram(true) {}

    VPoxFBSizeInfo(UIVisualStateType visualState,
                   ulong aPixelFormat, uchar *aVRAM,
                   ulong aBitsPerPixel, ulong aBytesPerLine,
                   ulong aWidth, ulong aHeight,
                   double dScaleFactor, const QSize &scaledSize, bool fUseUnscaledHiDPIOutput,
                   bool bUsesGuestVram) :
        m_visualState(visualState),
        mPixelFormat(aPixelFormat), mVRAM(aVRAM), mBitsPerPixel(aBitsPerPixel),
        mBytesPerLine(aBytesPerLine), mWidth(aWidth), mHeight(aHeight),
        m_dScaleFactor(dScaleFactor), m_scaledSize(scaledSize), m_fUseUnscaledHiDPIOutput(fUseUnscaledHiDPIOutput),
        mUsesGuestVram(bUsesGuestVram) {}

    UIVisualStateType visualState() const { return m_visualState; }
    ulong pixelFormat() const { return mPixelFormat; }
    uchar *VRAM() const { return mVRAM; }
    ulong bitsPerPixel() const { return mBitsPerPixel; }
    ulong bytesPerLine() const { return mBytesPerLine; }
    ulong width() const { return mWidth; }
    ulong height() const { return mHeight; }
    double scaleFactor() const { return m_dScaleFactor; }
    QSize scaledSize() const { return m_scaledSize; }
    bool useUnscaledHiDPIOutput() const { return m_fUseUnscaledHiDPIOutput; }
    bool usesGuestVram() const {return mUsesGuestVram;}

private:

    UIVisualStateType m_visualState;
    ulong mPixelFormat;
    uchar *mVRAM;
    ulong mBitsPerPixel;
    ulong mBytesPerLine;
    ulong mWidth;
    ulong mHeight;
    double m_dScaleFactor;
    QSize m_scaledSize;
    bool m_fUseUnscaledHiDPIOutput;
    bool mUsesGuestVram;
};

class VPoxVHWAImage
{
public:
    VPoxVHWAImage ();
    ~VPoxVHWAImage();

    int init(VPoxVHWASettings *aSettings);
#ifdef VPOX_WITH_VIDEOHWACCEL
    uchar *vpoxVRAMAddressFromOffset(uint64_t offset);
    uint64_t vpoxVRAMOffsetFromAddress(uchar* addr);
    uint64_t vpoxVRAMOffset(VPoxVHWASurfaceBase * pSurf);

    void vhwaSaveExec(struct SSMHANDLE * pSSM);
    static void vhwaSaveExecVoid(struct SSMHANDLE * pSSM);
    static int vhwaLoadExec(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);

    int vhwaSurfaceCanCreate(struct VPOXVHWACMD_SURF_CANCREATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceCreate(struct VPOXVHWACMD_SURF_CREATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
#ifdef VPOX_WITH_WDDM
    int vhwaSurfaceGetInfo(struct VPOXVHWACMD_SURF_GETINFO RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
#endif
    int vhwaSurfaceDestroy(struct VPOXVHWACMD_SURF_DESTROY RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceLock(struct VPOXVHWACMD_SURF_LOCK RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceUnlock(struct VPOXVHWACMD_SURF_UNLOCK RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceBlt(struct VPOXVHWACMD_SURF_BLT RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceFlip(struct VPOXVHWACMD_SURF_FLIP RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceColorFill(struct VPOXVHWACMD_SURF_COLORFILL RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceOverlayUpdate(struct VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_GUEST *pCmf);
    int vhwaSurfaceOverlaySetPosition(struct VPOXVHWACMD_SURF_OVERLAY_SETPOSITION RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaSurfaceColorkeySet(struct VPOXVHWACMD_SURF_COLORKEY_SET RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaQueryInfo1(struct VPOXVHWACMD_QUERYINFO1 RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaQueryInfo2(struct VPOXVHWACMD_QUERYINFO2 RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
    int vhwaConstruct(struct VPOXVHWACMD_HH_CONSTRUCT *pCmd);

    void *vramBase() { return mpvVRAM; }
    uint32_t vramSize() { return mcbVRAM; }

    bool hasSurfaces() const;
    bool hasVisibleOverlays();
    QRect overlaysRectUnion();
    QRect overlaysRectIntersection();
#endif

    static const QGLFormat & vpoxGLFormat();

    int reset(VHWACommandList * pCmdList);

    int vpoxFbWidth() {return mDisplay.getVGA()->width(); }
    int vpoxFbHeight() {return mDisplay.getVGA()->height(); }
    bool isInitialized() {return mDisplay.getVGA() != NULL; }

    void resize(const VPoxFBSizeInfo & size);

    class VPoxVHWAGlProgramMngr * vpoxVHWAGetGlProgramMngr() { return mpMngr; }

    VPoxVHWASurfaceBase * vgaSurface() { return mDisplay.getVGA(); }

#ifdef VPOXVHWA_OLD_COORD
    static void doSetupMatrix(const QSize & aSize, bool bInverted);
#endif

    void vpoxDoUpdateViewport(const QRect & aRect);
    void vpoxDoUpdateRect(const QRect * pRect);

    const QRect & vpoxViewport() const {return mViewport;}

#ifdef VPOXVHWA_PROFILE_FPS
    void reportNewFrame() { mbNewFrame = true; }
#endif

    bool performDisplay(bool bForce)
    {
        bForce = mDisplay.performDisplay(bForce | mRepaintNeeded);

#ifdef VPOXVHWA_PROFILE_FPS
        if(mbNewFrame)
        {
            mFPSCounter.frame();
            double fps = mFPSCounter.fps();
            if(!(mFPSCounter.frames() % 31))
            {
                LogRel(("fps: %f\n", fps));
            }
            mbNewFrame = false;
        }
#endif
        return bForce;
    }

    static void pushSettingsAndSetupViewport(const QSize &display, const QRect &viewport)
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        setupMatricies(display, false);
        adjustViewport(display, viewport);
    }

    static void popSettingsAfterSetupViewport()
    {
        glPopAttrib();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

private:
    static void setupMatricies(const QSize &display, bool bInvert);
    static void adjustViewport(const QSize &display, const QRect &viewport);


#ifdef VPOXQGL_DBG_SURF
    void vpoxDoTestSurfaces(void *context);
#endif
#ifdef VPOX_WITH_VIDEOHWACCEL

    void vpoxCheckUpdateAddress(VPoxVHWASurfaceBase * pSurface, uint64_t offset)
    {
        if (pSurface->addressAlocated())
        {
            Assert(!mDisplay.isPrimary(pSurface));
            uchar * addr = vpoxVRAMAddressFromOffset(offset);
            if (addr)
            {
                pSurface->setAddress(addr);
            }
        }
    }

    int vhwaSaveSurface(struct SSMHANDLE * pSSM, VPoxVHWASurfaceBase *pSurf, uint32_t surfCaps);
    static int vhwaLoadSurface(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t cBackBuffers, uint32_t u32Version);
    int vhwaSaveOverlayData(struct SSMHANDLE * pSSM, VPoxVHWASurfaceBase *pSurf, bool bVisible);
    static int vhwaLoadOverlayData(VHWACommandList * pCmdList, struct SSMHANDLE * pSSM, uint32_t u32Version);
    static int vhwaLoadVHWAEnable(VHWACommandList * pCmdList);

    void vhwaDoSurfaceOverlayUpdate(VPoxVHWASurfaceBase *pDstSurf, VPoxVHWASurfaceBase *pSrcSurf,
                                    struct VPOXVHWACMD_SURF_OVERLAY_UPDATE RT_UNTRUSTED_VOLATILE_GUEST *pCmd);
#endif

    VPoxVHWADisplay mDisplay;

    VPoxVHWASurfaceBase* handle2Surface(uint32_t h)
    {
        VPoxVHWASurfaceBase* pSurf = (VPoxVHWASurfaceBase*)mSurfHandleTable.get(h);
        Assert(pSurf);
        return pSurf;
    }

    VPoxVHWAHandleTable mSurfHandleTable;

    bool mRepaintNeeded;

    QRect mViewport;

    VPoxVHWASurfList *mConstructingList;
    int32_t mcRemaining2Contruct;

    class VPoxVHWAGlProgramMngr *mpMngr;

    VPoxVHWASettings *mSettings;

    void    *mpvVRAM;
    uint32_t mcbVRAM;

#ifdef VPOXVHWA_PROFILE_FPS
    VPoxVHWADbgTimer mFPSCounter;
    bool mbNewFrame;
#endif
};

class VPoxGLWgt : public QGLWidget
{
public:
    VPoxGLWgt(VPoxVHWAImage * pImage,
            QWidget* parent, const QGLWidget* shareWidget);

protected:
    void paintGL()
    {
        mpImage->performDisplay(true);
    }
private:
    VPoxVHWAImage * mpImage;
};

class VPoxVHWAFBO
{
public:
    VPoxVHWAFBO() :
            mFBO(0)
    {}

    ~VPoxVHWAFBO()
    {
        if(mFBO)
        {
            vpoxglDeleteFramebuffers(1, &mFBO);
        }
    }

    void init()
    {
        VPOXQGL_CHECKERR(
                vpoxglGenFramebuffers(1, &mFBO);
        );
    }

    void bind()
    {
        VPOXQGL_CHECKERR(
            vpoxglBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        );
    }

    void unbind()
    {
        VPOXQGL_CHECKERR(
            vpoxglBindFramebuffer(GL_FRAMEBUFFER, 0);
        );
    }

    void attachBound(VPoxVHWATexture *pTex)
    {
        VPOXQGL_CHECKERR(
                vpoxglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pTex->texTarget(), pTex->texture(), 0);
        );
    }

private:
    GLuint mFBO;
};

template <class T>
class VPoxVHWATextureImageFBO : public T
{
public:
    VPoxVHWATextureImageFBO(const QRect &size, const VPoxVHWAColorFormat &format, class VPoxVHWAGlProgramMngr * aMgr, VPOXVHWAIMG_TYPE flags) :
            T(size, format, aMgr, flags & (~(VPOXVHWAIMG_FBO | VPOXVHWAIMG_LINEAR))),
            mFBOTex(size, VPoxVHWAColorFormat(32, 0xff0000, 0xff00, 0xff), aMgr, (flags & (~VPOXVHWAIMG_FBO))),
            mpvFBOTexMem(NULL)
    {
    }

    virtual ~VPoxVHWATextureImageFBO()
    {
        if(mpvFBOTexMem)
            free(mpvFBOTexMem);
    }

    virtual void init(uchar *pvMem)
    {
        mFBO.init();
        mpvFBOTexMem = (uchar*)malloc(mFBOTex.memSize());
        mFBOTex.init(mpvFBOTexMem);
        T::init(pvMem);
        mFBO.bind();
        mFBO.attachBound(mFBOTex.component(0));
        mFBO.unbind();
    }

    virtual int createDisplay(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected,
            GLuint *pDisplay, class VPoxVHWAGlProgramVHWA ** ppProgram)
    {
        T::createDisplay(NULL, &mFBOTex.rect(), &rect(),
                NULL, NULL, false,
                pDisplay, ppProgram);

        return mFBOTex.initDisplay(pDst, pDstRect, pSrcRect,
                pDstCKey, pSrcCKey, bNotIntersected);
    }

    virtual void update(const QRect * pRect)
    {
        T::update(pRect);

        VPoxVHWAImage::pushSettingsAndSetupViewport(rect().size(), rect());
        mFBO.bind();
        T::display();
        mFBO.unbind();
        VPoxVHWAImage::popSettingsAfterSetupViewport();
    }

    virtual void display(VPoxVHWATextureImage *pDst, const QRect * pDstRect, const QRect * pSrcRect,
            const VPoxVHWAColorKey * pDstCKey, const VPoxVHWAColorKey * pSrcCKey, bool bNotIntersected)
    {
        mFBOTex.display(pDst, pDstRect, pSrcRect, pDstCKey, pSrcCKey, bNotIntersected);
    }

    virtual void display()
    {
        mFBOTex.display();
    }

    const QRect &rect() { return T::rect(); }
private:
    VPoxVHWAFBO mFBO;
    VPoxVHWATextureImage mFBOTex;
    uchar * mpvFBOTexMem;
};

class VPoxQGLOverlay
{
public:
    VPoxQGLOverlay();
    void init(QWidget *pViewport, QObject *pPostEventObject, CSession * aSession, uint32_t id);
    ~VPoxQGLOverlay()
    {
        if (mpShareWgt)
            delete mpShareWgt;
    }

    void updateAttachment(QWidget *pViewport, QObject *pPostEventObject);

    int onVHWACommand(struct VPOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCommand,
                      int /*VPOXVHWACMD_TYPE*/ enmCmdInt, bool fGuestCmd);

    void onVHWACommandEvent (QEvent * pEvent);

    /**
     * to be called on NotifyUpdate framebuffer call
     * @return true if the request was processed & should not be forwarded to the framebuffer
     * false - otherwise */
    bool onNotifyUpdate (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH);

    void onNotifyUpdateIgnore (ULONG aX, ULONG aY,
                             ULONG aW, ULONG aH)
    {
        Q_UNUSED(aX);
        Q_UNUSED(aY);
        Q_UNUSED(aW);
        Q_UNUSED(aH);
        /* @todo: we actually should not miss notify updates, since we need to update the texture on it */
    }

    void onResizeEventPostprocess (const VPoxFBSizeInfo &re, const QPoint & topLeft);

    void onViewportResized (QResizeEvent * /*re*/)
    {
        vpoxDoCheckUpdateViewport();
        mGlCurrent = false;
    }

    void onViewportScrolled (const QPoint & newTopLeft)
    {
        mContentsTopLeft = newTopLeft;
        vpoxDoCheckUpdateViewport();
        mGlCurrent = false;
    }

    /* not supposed to be called by clients */
    int vhwaLoadExec (struct SSMHANDLE * pSSM, uint32_t u32Version);
    void vhwaSaveExec (struct SSMHANDLE * pSSM);
private:
    int vhwaSurfaceUnlock (struct VPOXVHWACMD_SURF_UNLOCK RT_UNTRUSTED_VOLATILE_GUEST *pCmd);

    void repaintMain();
    void repaintOverlay()
    {
        if(mNeedOverlayRepaint)
        {
            mNeedOverlayRepaint = false;
            performDisplayOverlay();
        }
        if(mNeedSetVisible)
        {
            mNeedSetVisible = false;
            mpOverlayWgt->setVisible (true);
        }
    }
    void repaint()
    {
        repaintOverlay();
        repaintMain();
    }

    void makeCurrent()
    {
        if (!mGlCurrent)
        {
            mGlCurrent = true;
            mpOverlayWgt->makeCurrent();
        }
    }

    void performDisplayOverlay()
    {
        if (mOverlayVisible)
        {
            makeCurrent();
            if (mOverlayImage.performDisplay(false))
                mpOverlayWgt->swapBuffers();
        }
    }

    void vpoxSetGlOn (bool on);
    bool vpoxGetGlOn() { return mGlOn; }
    bool vpoxSynchGl();
    void vpoxDoVHWACmdExec(void RT_UNTRUSTED_VOLATILE_GUEST *pvCmd, int /*VPOXVHWACMD_TYPE*/ enmCmdInt, bool fGuestCmd);
    void vpoxShowOverlay (bool show);
    void vpoxDoCheckUpdateViewport();
    void vpoxDoVHWACmd(void RT_UNTRUSTED_VOLATILE_GUEST *pvCmd, int /*VPOXVHWACMD_TYPE*/ enmCmd, bool fGuestCmd);
    void addMainDirtyRect (const QRect & aRect);
    void vpoxCheckUpdateOverlay (const QRect & rect);
    void processCmd (VPoxVHWACommandElement * pCmd);

    int vhwaConstruct (struct VPOXVHWACMD_HH_CONSTRUCT *pCmd);

    int reset();

    int resetGl();

    void initGl();

    VPoxGLWgt *mpOverlayWgt;
    VPoxVHWAImage mOverlayImage;
    QWidget *mpViewport;
    bool mGlOn;
    bool mOverlayWidgetVisible;
    bool mOverlayVisible;
    bool mGlCurrent;
    bool mProcessingCommands;
    bool mNeedOverlayRepaint;
    bool mNeedSetVisible;
    QRect mOverlayViewport;
    VPoxVHWADirtyRect mMainDirtyRect;

    VPoxVHWACommandElementProcessor mCmdPipe;

    /* this is used in saved state restore to postpone surface restoration
     * till the framebuffer size is restored */
    VHWACommandList mOnResizeCmdList;

    VPoxVHWASettings mSettings;
    CSession * mpSession;

    VPoxFBSizeInfo mSizeInfo;
    VPoxFBSizeInfo mPostponedResize;
    QPoint mContentsTopLeft;

    QGLWidget *mpShareWgt;

    uint32_t m_id;
};

#endif /* defined(VPOX_GUI_USE_QGL) || defined(VPOX_WITH_VIDEOHWACCEL) */

#endif /* !FEQT_INCLUDED_SRC_VPoxFBOverlay_h */
