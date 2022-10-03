/* $Id: VPoxGLSupportInfo.cpp $ */
/** @file
 * VPox Qt GUI - OpenGL support info used for 2D support detection.
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

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h> /* QGLWidget drags in Windows.h; -Wall forces us to use wrapper. */
# include <iprt/stdint.h>      /* QGLWidget drags in stdint.h; -Wall forces us to use wrapper. */
#endif
#include <QGLWidget>

#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/env.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>

#include <VPox/VPoxGL2D.h>
#include "VPoxFBOverlayCommon.h"
#include <iprt/err.h>

#include <QGLContext>


/*****************/

/* functions */

PFNVPOXVHWA_ACTIVE_TEXTURE vpoxglActiveTexture = NULL;
PFNVPOXVHWA_MULTI_TEX_COORD2I vpoxglMultiTexCoord2i = NULL;
PFNVPOXVHWA_MULTI_TEX_COORD2D vpoxglMultiTexCoord2d = NULL;
PFNVPOXVHWA_MULTI_TEX_COORD2F vpoxglMultiTexCoord2f = NULL;


PFNVPOXVHWA_CREATE_SHADER   vpoxglCreateShader  = NULL;
PFNVPOXVHWA_SHADER_SOURCE   vpoxglShaderSource  = NULL;
PFNVPOXVHWA_COMPILE_SHADER  vpoxglCompileShader = NULL;
PFNVPOXVHWA_DELETE_SHADER   vpoxglDeleteShader  = NULL;

PFNVPOXVHWA_CREATE_PROGRAM  vpoxglCreateProgram = NULL;
PFNVPOXVHWA_ATTACH_SHADER   vpoxglAttachShader  = NULL;
PFNVPOXVHWA_DETACH_SHADER   vpoxglDetachShader  = NULL;
PFNVPOXVHWA_LINK_PROGRAM    vpoxglLinkProgram   = NULL;
PFNVPOXVHWA_USE_PROGRAM     vpoxglUseProgram    = NULL;
PFNVPOXVHWA_DELETE_PROGRAM  vpoxglDeleteProgram = NULL;

PFNVPOXVHWA_IS_SHADER       vpoxglIsShader      = NULL;
PFNVPOXVHWA_GET_SHADERIV    vpoxglGetShaderiv   = NULL;
PFNVPOXVHWA_IS_PROGRAM      vpoxglIsProgram     = NULL;
PFNVPOXVHWA_GET_PROGRAMIV   vpoxglGetProgramiv  = NULL;
PFNVPOXVHWA_GET_ATTACHED_SHADERS vpoxglGetAttachedShaders = NULL;
PFNVPOXVHWA_GET_SHADER_INFO_LOG  vpoxglGetShaderInfoLog   = NULL;
PFNVPOXVHWA_GET_PROGRAM_INFO_LOG vpoxglGetProgramInfoLog  = NULL;

PFNVPOXVHWA_GET_UNIFORM_LOCATION vpoxglGetUniformLocation = NULL;

PFNVPOXVHWA_UNIFORM1F vpoxglUniform1f = NULL;
PFNVPOXVHWA_UNIFORM2F vpoxglUniform2f = NULL;
PFNVPOXVHWA_UNIFORM3F vpoxglUniform3f = NULL;
PFNVPOXVHWA_UNIFORM4F vpoxglUniform4f = NULL;

PFNVPOXVHWA_UNIFORM1I vpoxglUniform1i = NULL;
PFNVPOXVHWA_UNIFORM2I vpoxglUniform2i = NULL;
PFNVPOXVHWA_UNIFORM3I vpoxglUniform3i = NULL;
PFNVPOXVHWA_UNIFORM4I vpoxglUniform4i = NULL;

PFNVPOXVHWA_GEN_BUFFERS vpoxglGenBuffers = NULL;
PFNVPOXVHWA_DELETE_BUFFERS vpoxglDeleteBuffers = NULL;
PFNVPOXVHWA_BIND_BUFFER vpoxglBindBuffer = NULL;
PFNVPOXVHWA_BUFFER_DATA vpoxglBufferData = NULL;
PFNVPOXVHWA_MAP_BUFFER vpoxglMapBuffer = NULL;
PFNVPOXVHWA_UNMAP_BUFFER vpoxglUnmapBuffer = NULL;

PFNVPOXVHWA_IS_FRAMEBUFFER vpoxglIsFramebuffer = NULL;
PFNVPOXVHWA_BIND_FRAMEBUFFER vpoxglBindFramebuffer = NULL;
PFNVPOXVHWA_DELETE_FRAMEBUFFERS vpoxglDeleteFramebuffers = NULL;
PFNVPOXVHWA_GEN_FRAMEBUFFERS vpoxglGenFramebuffers = NULL;
PFNVPOXVHWA_CHECK_FRAMEBUFFER_STATUS vpoxglCheckFramebufferStatus = NULL;
PFNVPOXVHWA_FRAMEBUFFER_TEXTURE1D vpoxglFramebufferTexture1D = NULL;
PFNVPOXVHWA_FRAMEBUFFER_TEXTURE2D vpoxglFramebufferTexture2D = NULL;
PFNVPOXVHWA_FRAMEBUFFER_TEXTURE3D vpoxglFramebufferTexture3D = NULL;
PFNVPOXVHWA_GET_FRAMEBUFFER_ATTACHMENT_PARAMETRIV vpoxglGetFramebufferAttachmentParameteriv = NULL;

#define VPOXVHWA_GETPROCADDRESS(_c, _t, _n) ((_t)(uintptr_t)(_c).getProcAddress(QString(_n)))

#define VPOXVHWA_PFNINIT_SAME(_c, _t, _v, _rc) \
    do { \
        if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v)) == NULL) \
        { \
            VPOXQGLLOGREL(("ERROR: '%s' function not found\n", "gl"#_v));\
            AssertBreakpoint(); \
            if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"ARB")) == NULL) \
            { \
                VPOXQGLLOGREL(("ERROR: '%s' function not found\n", "gl"#_v"ARB"));\
                AssertBreakpoint(); \
                if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"EXT")) == NULL) \
                { \
                    VPOXQGLLOGREL(("ERROR: '%s' function not found\n", "gl"#_v"EXT"));\
                    AssertBreakpoint(); \
                    (_rc)++; \
                } \
            } \
        } \
    }while(0)

#define VPOXVHWA_PFNINIT(_c, _t, _v, _f,_rc) \
    do { \
        if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_f)) == NULL) \
        { \
            VPOXQGLLOGREL(("ERROR: '%s' function is not found\n", "gl"#_f));\
            AssertBreakpoint(); \
            (_rc)++; \
        } \
    }while(0)

#define VPOXVHWA_PFNINIT_OBJECT_ARB(_c, _t, _v, _rc) \
        do { \
            if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"ObjectARB")) == NULL) \
            { \
                VPOXQGLLOGREL(("ERROR: '%s' function is not found\n", "gl"#_v"ObjectARB"));\
                AssertBreakpoint(); \
                (_rc)++; \
            } \
        }while(0)

#define VPOXVHWA_PFNINIT_ARB(_c, _t, _v, _rc) \
        do { \
            if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"ARB")) == NULL) \
            { \
                VPOXQGLLOGREL(("ERROR: '%s' function is not found\n", "gl"#_v"ARB"));\
                AssertBreakpoint(); \
                (_rc)++; \
            } \
        }while(0)

#define VPOXVHWA_PFNINIT_EXT(_c, _t, _v, _rc) \
        do { \
            if((vpoxgl##_v = VPOXVHWA_GETPROCADDRESS(_c, _t, "gl"#_v"EXT")) == NULL) \
            { \
                VPOXQGLLOGREL(("ERROR: '%s' function is not found\n", "gl"#_v"EXT"));\
                AssertBreakpoint(); \
                (_rc)++; \
            } \
        }while(0)

static int vpoxVHWAGlParseSubver(const GLubyte * ver, const GLubyte ** pNext, bool bSpacePrefixAllowed)
{
    int val = 0;

    for(;;++ver)
    {
        if(*ver >= '0' && *ver <= '9')
        {
            if(!val)
            {
                if(*ver == '0')
                    continue;
            }
            else
            {
                val *= 10;
            }
            val += *ver - '0';
        }
        else if(*ver == '.')
        {
            *pNext = ver+1;
            break;
        }
        else if(*ver == '\0')
        {
            *pNext = NULL;
            break;
        }
        else if(*ver == ' ' || *ver == '\t' ||  *ver == 0x0d || *ver == 0x0a)
        {
            if(bSpacePrefixAllowed)
            {
                if(!val)
                {
                    continue;
                }
            }

            /* treat this as the end ov version string */
            *pNext = NULL;
            break;
        }
        else
        {
            Assert(0);
            val = -1;
            break;
        }
    }

    return val;
}

/* static */
int VPoxGLInfo::parseVersion(const GLubyte * ver)
{
    int iVer = vpoxVHWAGlParseSubver(ver, &ver, true);
    if(iVer)
    {
        iVer <<= 16;
        if(ver)
        {
            int tmp = vpoxVHWAGlParseSubver(ver, &ver, false);
            if(tmp >= 0)
            {
                iVer |= tmp << 8;
                if(ver)
                {
                    tmp = vpoxVHWAGlParseSubver(ver, &ver, false);
                    if(tmp >= 0)
                    {
                        iVer |= tmp;
                    }
                    else
                    {
                        Assert(0);
                        iVer = -1;
                    }
                }
            }
            else
            {
                Assert(0);
                iVer = -1;
            }
        }
    }
    return iVer;
}

void VPoxGLInfo::init(const QGLContext * pContext)
{
    if(mInitialized)
        return;

    mInitialized = true;

    if (!QGLFormat::hasOpenGL())
    {
        VPOXQGLLOGREL (("no gl support available\n"));
        return;
    }

//    pContext->makeCurrent();

    const GLubyte * str;
    VPOXQGL_CHECKERR(
            str = glGetString(GL_VERSION);
            );

    if(str)
    {
        VPOXQGLLOGREL (("gl version string: 0%s\n", str));

        mGLVersion = parseVersion (str);
        Assert(mGLVersion > 0);
        if(mGLVersion < 0)
        {
            mGLVersion = 0;
        }
        else
        {
            VPOXQGLLOGREL (("gl version: 0x%x\n", mGLVersion));
            VPOXQGL_CHECKERR(
                    str = glGetString (GL_EXTENSIONS);
                    );

            VPOXQGLLOGREL (("gl extensions: %s\n", str));

            const char * pos = strstr((const char *)str, "GL_ARB_multitexture");
            m_GL_ARB_multitexture = pos != NULL;
            VPOXQGLLOGREL (("GL_ARB_multitexture: %d\n", m_GL_ARB_multitexture));

            pos = strstr((const char *)str, "GL_ARB_shader_objects");
            m_GL_ARB_shader_objects = pos != NULL;
            VPOXQGLLOGREL (("GL_ARB_shader_objects: %d\n", m_GL_ARB_shader_objects));

            pos = strstr((const char *)str, "GL_ARB_fragment_shader");
            m_GL_ARB_fragment_shader = pos != NULL;
            VPOXQGLLOGREL (("GL_ARB_fragment_shader: %d\n", m_GL_ARB_fragment_shader));

            pos = strstr((const char *)str, "GL_ARB_pixel_buffer_object");
            m_GL_ARB_pixel_buffer_object = pos != NULL;
            VPOXQGLLOGREL (("GL_ARB_pixel_buffer_object: %d\n", m_GL_ARB_pixel_buffer_object));

            pos = strstr((const char *)str, "GL_ARB_texture_rectangle");
            m_GL_ARB_texture_rectangle = pos != NULL;
            VPOXQGLLOGREL (("GL_ARB_texture_rectangle: %d\n", m_GL_ARB_texture_rectangle));

            pos = strstr((const char *)str, "GL_EXT_texture_rectangle");
            m_GL_EXT_texture_rectangle = pos != NULL;
            VPOXQGLLOGREL (("GL_EXT_texture_rectangle: %d\n", m_GL_EXT_texture_rectangle));

            pos = strstr((const char *)str, "GL_NV_texture_rectangle");
            m_GL_NV_texture_rectangle = pos != NULL;
            VPOXQGLLOGREL (("GL_NV_texture_rectangle: %d\n", m_GL_NV_texture_rectangle));

            pos = strstr((const char *)str, "GL_ARB_texture_non_power_of_two");
            m_GL_ARB_texture_non_power_of_two = pos != NULL;
            VPOXQGLLOGREL (("GL_ARB_texture_non_power_of_two: %d\n", m_GL_ARB_texture_non_power_of_two));

            pos = strstr((const char *)str, "GL_EXT_framebuffer_object");
            m_GL_EXT_framebuffer_object = pos != NULL;
            VPOXQGLLOGREL (("GL_EXT_framebuffer_object: %d\n", m_GL_EXT_framebuffer_object));


            initExtSupport(*pContext);
        }
    }
    else
    {
        VPOXQGLLOGREL (("failed to make the context current, treating as unsupported\n"));
    }
}

void VPoxGLInfo::initExtSupport(const QGLContext & context)
{
    int rc = 0;
    do
    {
        rc = 0;
        mMultiTexNumSupported = 1; /* default, 1 means not supported */
        if(mGLVersion >= 0x010201) /* ogl >= 1.2.1 */
        {
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_ACTIVE_TEXTURE, ActiveTexture, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_MULTI_TEX_COORD2I, MultiTexCoord2i, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_MULTI_TEX_COORD2D, MultiTexCoord2d, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_MULTI_TEX_COORD2F, MultiTexCoord2f, rc);
        }
        else if(m_GL_ARB_multitexture)
        {
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_ACTIVE_TEXTURE, ActiveTexture, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_MULTI_TEX_COORD2I, MultiTexCoord2i, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_MULTI_TEX_COORD2D, MultiTexCoord2d, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_MULTI_TEX_COORD2F, MultiTexCoord2f, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        GLint maxCoords, maxUnits;
        glGetIntegerv(GL_MAX_TEXTURE_COORDS, &maxCoords);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);

        VPOXQGLLOGREL(("Max Tex Coords (%d), Img Units (%d)\n", maxCoords, maxUnits));
        /* take the minimum of those */
        if(maxUnits < maxCoords)
            maxCoords = maxUnits;
        if(maxUnits < 2)
        {
            VPOXQGLLOGREL(("Max Tex Coord or Img Units < 2 disabling MultiTex support\n"));
            break;
        }

        mMultiTexNumSupported = maxUnits;
    }while(0);


    do
    {
        rc = 0;
        mPBOSupported = false;

        if(m_GL_ARB_pixel_buffer_object)
        {
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_GEN_BUFFERS, GenBuffers, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_DELETE_BUFFERS, DeleteBuffers, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_BIND_BUFFER, BindBuffer, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_BUFFER_DATA, BufferData, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_MAP_BUFFER, MapBuffer, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNMAP_BUFFER, UnmapBuffer, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        mPBOSupported = true;
    } while(0);

    do
    {
        rc = 0;
        mFragmentShaderSupported = false;

        if(mGLVersion >= 0x020000)  /* if ogl >= 2.0*/
        {
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_CREATE_SHADER, CreateShader, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_SHADER_SOURCE, ShaderSource, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_COMPILE_SHADER, CompileShader, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_DELETE_SHADER, DeleteShader, rc);

            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_CREATE_PROGRAM, CreateProgram, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_ATTACH_SHADER, AttachShader, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_DETACH_SHADER, DetachShader, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_LINK_PROGRAM, LinkProgram, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_USE_PROGRAM, UseProgram, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_DELETE_PROGRAM, DeleteProgram, rc);

            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_IS_SHADER, IsShader, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_GET_SHADERIV, GetShaderiv, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_IS_PROGRAM, IsProgram, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_GET_PROGRAMIV, GetProgramiv, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_GET_ATTACHED_SHADERS, GetAttachedShaders,  rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_GET_SHADER_INFO_LOG, GetShaderInfoLog, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_GET_PROGRAM_INFO_LOG, GetProgramInfoLog, rc);

            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_GET_UNIFORM_LOCATION, GetUniformLocation, rc);

            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM1F, Uniform1f, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM2F, Uniform2f, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM3F, Uniform3f, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM4F, Uniform4f, rc);

            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM1I, Uniform1i, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM2I, Uniform2i, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM3I, Uniform3i, rc);
            VPOXVHWA_PFNINIT_SAME(context, PFNVPOXVHWA_UNIFORM4I, Uniform4i, rc);
        }
        else if(m_GL_ARB_shader_objects && m_GL_ARB_fragment_shader)
        {
            VPOXVHWA_PFNINIT_OBJECT_ARB(context, PFNVPOXVHWA_CREATE_SHADER, CreateShader, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_SHADER_SOURCE, ShaderSource, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_COMPILE_SHADER, CompileShader, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_DELETE_SHADER, DeleteShader, DeleteObjectARB, rc);

            VPOXVHWA_PFNINIT_OBJECT_ARB(context, PFNVPOXVHWA_CREATE_PROGRAM, CreateProgram, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_ATTACH_SHADER, AttachShader, AttachObjectARB, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_DETACH_SHADER, DetachShader, DetachObjectARB, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_LINK_PROGRAM, LinkProgram, rc);
            VPOXVHWA_PFNINIT_OBJECT_ARB(context, PFNVPOXVHWA_USE_PROGRAM, UseProgram, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_DELETE_PROGRAM, DeleteProgram, DeleteObjectARB, rc);

        /// @todo    VPOXVHWA_PFNINIT(PFNVPOXVHWA_IS_SHADER, IsShader, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_GET_SHADERIV, GetShaderiv, GetObjectParameterivARB, rc);
        /// @todo    VPOXVHWA_PFNINIT(PFNVPOXVHWA_IS_PROGRAM, IsProgram, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_GET_PROGRAMIV, GetProgramiv, GetObjectParameterivARB, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_GET_ATTACHED_SHADERS, GetAttachedShaders, GetAttachedObjectsARB, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_GET_SHADER_INFO_LOG, GetShaderInfoLog, GetInfoLogARB, rc);
            VPOXVHWA_PFNINIT(context, PFNVPOXVHWA_GET_PROGRAM_INFO_LOG, GetProgramInfoLog, GetInfoLogARB, rc);

            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_GET_UNIFORM_LOCATION, GetUniformLocation, rc);

            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM1F, Uniform1f, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM2F, Uniform2f, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM3F, Uniform3f, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM4F, Uniform4f, rc);

            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM1I, Uniform1i, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM2I, Uniform2i, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM3I, Uniform3i, rc);
            VPOXVHWA_PFNINIT_ARB(context, PFNVPOXVHWA_UNIFORM4I, Uniform4i, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        mFragmentShaderSupported = true;
    } while(0);

    do
    {
        rc = 0;
        mFBOSupported = false;

        if(m_GL_EXT_framebuffer_object)
        {
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_IS_FRAMEBUFFER, IsFramebuffer, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_BIND_FRAMEBUFFER, BindFramebuffer, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_DELETE_FRAMEBUFFERS, DeleteFramebuffers, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_GEN_FRAMEBUFFERS, GenFramebuffers, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_CHECK_FRAMEBUFFER_STATUS, CheckFramebufferStatus, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_FRAMEBUFFER_TEXTURE1D, FramebufferTexture1D, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_FRAMEBUFFER_TEXTURE2D, FramebufferTexture2D, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_FRAMEBUFFER_TEXTURE3D, FramebufferTexture3D, rc);
            VPOXVHWA_PFNINIT_EXT(context, PFNVPOXVHWA_GET_FRAMEBUFFER_ATTACHMENT_PARAMETRIV, GetFramebufferAttachmentParameteriv, rc);
        }
        else
        {
            break;
        }

        if(RT_FAILURE(rc))
            break;

        mFBOSupported = true;
    } while(0);

    if(m_GL_ARB_texture_rectangle || m_GL_EXT_texture_rectangle || m_GL_NV_texture_rectangle)
    {
        mTextureRectangleSupported = true;
    }
    else
    {
        mTextureRectangleSupported = false;
    }

    mTextureNP2Supported = m_GL_ARB_texture_non_power_of_two;
}

void VPoxVHWAInfo::init(const QGLContext * pContext)
{
    if(mInitialized)
        return;

    mInitialized = true;

    mglInfo.init(pContext);

    if(mglInfo.isFragmentShaderSupported() && mglInfo.isTextureRectangleSupported())
    {
        uint32_t num = 0;
        mFourccSupportedList[num++] = FOURCC_AYUV;
        mFourccSupportedList[num++] = FOURCC_UYVY;
        mFourccSupportedList[num++] = FOURCC_YUY2;
        if(mglInfo.getMultiTexNumSupported() >= 4)
        {
            /* YV12 currently requires 3 units (for each color component)
             * + 1 unit for dst texture for color-keying + 3 units for each color component
             * TODO: we could store YV12 data in one texture to eliminate this requirement*/
            mFourccSupportedList[num++] = FOURCC_YV12;
        }

        Assert(num <= VPOXVHWA_NUMFOURCC);
        mFourccSupportedCount = num;
    }
    else
    {
        mFourccSupportedCount = 0;
    }
}

bool VPoxVHWAInfo::isVHWASupported() const
{
    if(mglInfo.getGLVersion() <= 0)
    {
        /* error occurred while gl info initialization */
        VPOXQGLLOGREL(("2D not supported: gl version info not initialized properly\n"));
        return false;
    }

#ifndef DEBUGVHWASTRICT
    /* in case we do not support shaders & multitexturing we can not support dst colorkey,
     * no sense to report Video Acceleration supported */
    if(!mglInfo.isFragmentShaderSupported())
    {
        VPOXQGLLOGREL(("2D not supported: fragment shader unsupported\n"));
        return false;
    }
#endif
    if(mglInfo.getMultiTexNumSupported() < 2)
    {
        VPOXQGLLOGREL(("2D not supported: multitexture unsupported\n"));
        return false;
    }

    /* color conversion now supported only GL_TEXTURE_RECTANGLE
     * in this case only stretching is accelerated
     * report as unsupported, TODO: probably should report as supported for stretch acceleration */
    if(!mglInfo.isTextureRectangleSupported())
    {
        VPOXQGLLOGREL(("2D not supported: texture rectangle unsupported\n"));
        return false;
    }

    VPOXQGLLOGREL(("2D is supported!\n"));
    return true;
}

/* static */
bool VPoxVHWAInfo::checkVHWASupport()
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
    static char pszVPoxPath[RTPATH_MAX];
    const char *papszArgs[] = { NULL, "-test", "2D", NULL};
    int rc;
    RTPROCESS Process;
    RTPROCSTATUS ProcStatus;
    uint64_t StartTS;

    rc = RTPathExecDir(pszVPoxPath, RTPATH_MAX); AssertRCReturn(rc, false);
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    rc = RTPathAppend(pszVPoxPath, RTPATH_MAX, "VPoxTestOGL.exe");
#else
    rc = RTPathAppend(pszVPoxPath, RTPATH_MAX, "VPoxTestOGL");
#endif
    papszArgs[0] = pszVPoxPath;         /* argv[0] */
    AssertRCReturn(rc, false);

    rc = RTProcCreate(pszVPoxPath, papszArgs, RTENV_DEFAULT, 0, &Process);
    if (RT_FAILURE(rc))
    {
        VPOXQGLLOGREL(("2D support test failed: failed to create a test process\n"));
        return false;
    }

    StartTS = RTTimeMilliTS();

    while (1)
    {
        rc = RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
        if (rc != VERR_PROCESS_RUNNING)
            break;

        if (RTTimeMilliTS() - StartTS > 30*1000 /* 30 sec */)
        {
            RTProcTerminate(Process);
            RTThreadSleep(100);
            RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
            VPOXQGLLOGREL(("2D support test failed: the test did not complete within 30 sec\n"));
            return false;
        }
        RTThreadSleep(100);
    }

    if (RT_SUCCESS(rc))
    {
        if ((ProcStatus.enmReason==RTPROCEXITREASON_NORMAL) && (ProcStatus.iStatus==0))
        {
            VPOXQGLLOGREL(("2D support test succeeded\n"));
            return true;
        }
    }

    VPOXQGLLOGREL(("2D support test failed: err code (%Rra)\n", rc));

    return false;
#else
    /** @todo test & enable external app approach*/
    VPoxGLTmpContext ctx;
    const QGLContext *pContext = ctx.makeCurrent();
    Assert(pContext);
    if(pContext)
    {
        VPoxVHWAInfo info;
        info.init(pContext);
        return info.isVHWASupported();
    }
    return false;
#endif
}

VPoxGLTmpContext::VPoxGLTmpContext()
{
    if(QGLFormat::hasOpenGL())
    {
        mWidget = new QGLWidget();
    }
    else
    {
        mWidget = NULL;
    }
}

VPoxGLTmpContext::~VPoxGLTmpContext()
{
    if(mWidget)
        delete mWidget;
}

const class QGLContext * VPoxGLTmpContext::makeCurrent()
{
    if(mWidget)
    {
        mWidget->makeCurrent();
        return mWidget->context();
    }
    return NULL;
}

