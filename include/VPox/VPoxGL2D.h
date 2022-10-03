/** @file
 *
 * VPox frontends: Qt GUI ("VirtualPox"):
 * OpenGL support info used for 2D support detection
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VPOX_INCLUDED_VPoxGL2D_h
#define VPOX_INCLUDED_VPoxGL2D_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

typedef char GLchar;

#ifndef GL_COMPILE_STATUS
# define GL_COMPILE_STATUS 0x8b81
#endif
#ifndef GL_LINK_STATUS
# define GL_LINK_STATUS    0x8b82
#endif
#ifndef GL_FRAGMENT_SHADER
# define GL_FRAGMENT_SHADER 0x8b30
#endif
#ifndef GL_VERTEX_SHADER
# define GL_VERTEX_SHADER 0x8b31
#endif

/* GL_ARB_multitexture */
#ifndef GL_TEXTURE0
# define GL_TEXTURE0                    0x84c0
#endif
#ifndef GL_TEXTURE1
# define GL_TEXTURE1                    0x84c1
#endif
#ifndef GL_MAX_TEXTURE_COORDS
# define GL_MAX_TEXTURE_COORDS          0x8871
#endif
#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
# define GL_MAX_TEXTURE_IMAGE_UNITS     0x8872
#endif

#ifndef APIENTRY
# define APIENTRY
#endif

typedef GLvoid (APIENTRY *PFNVPOXVHWA_ACTIVE_TEXTURE) (GLenum texture);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_MULTI_TEX_COORD2I) (GLenum texture, GLint v0, GLint v1);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_MULTI_TEX_COORD2F) (GLenum texture, GLfloat v0, GLfloat v1);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_MULTI_TEX_COORD2D) (GLenum texture, GLdouble v0, GLdouble v1);

/* GL_ARB_texture_rectangle */
#ifndef GL_TEXTURE_RECTANGLE
# define GL_TEXTURE_RECTANGLE 0x84F5
#endif

/* GL_ARB_shader_objects */
/* GL_ARB_fragment_shader */

typedef GLuint (APIENTRY *PFNVPOXVHWA_CREATE_SHADER)  (GLenum type);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_SHADER_SOURCE)  (GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_COMPILE_SHADER) (GLuint shader);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_DELETE_SHADER)  (GLuint shader);

typedef GLuint (APIENTRY *PFNVPOXVHWA_CREATE_PROGRAM) ();
typedef GLvoid (APIENTRY *PFNVPOXVHWA_ATTACH_SHADER)  (GLuint program, GLuint shader);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_DETACH_SHADER)  (GLuint program, GLuint shader);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_LINK_PROGRAM)   (GLuint program);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_USE_PROGRAM)    (GLuint program);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_DELETE_PROGRAM) (GLuint program);

typedef GLboolean (APIENTRY *PFNVPOXVHWA_IS_SHADER)   (GLuint shader);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GET_SHADERIV)   (GLuint shader, GLenum pname, GLint *params);
typedef GLboolean (APIENTRY *PFNVPOXVHWA_IS_PROGRAM)  (GLuint program);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GET_PROGRAMIV)  (GLuint program, GLenum pname, GLint *params);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GET_ATTACHED_SHADERS) (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GET_SHADER_INFO_LOG)  (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GET_PROGRAM_INFO_LOG) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint (APIENTRY *PFNVPOXVHWA_GET_UNIFORM_LOCATION) (GLint programObj, const GLchar *name);

typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM1F)(GLint location, GLfloat v0);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM2F)(GLint location, GLfloat v0, GLfloat v1);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM3F)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM4F)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM1I)(GLint location, GLint v0);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM2I)(GLint location, GLint v0, GLint v1);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM3I)(GLint location, GLint v0, GLint v1, GLint v2);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_UNIFORM4I)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);

/* GL_ARB_pixel_buffer_object*/
#ifndef Q_WS_MAC
/* apears to be defined on mac */
typedef ptrdiff_t GLsizeiptr;
#endif

#ifndef GL_READ_ONLY
# define GL_READ_ONLY                   0x88B8
#endif
#ifndef GL_WRITE_ONLY
# define GL_WRITE_ONLY                  0x88B9
#endif
#ifndef GL_READ_WRITE
# define GL_READ_WRITE                  0x88BA
#endif
#ifndef GL_STREAM_DRAW
# define GL_STREAM_DRAW                 0x88E0
#endif
#ifndef GL_STREAM_READ
# define GL_STREAM_READ                 0x88E1
#endif
#ifndef GL_STREAM_COPY
# define GL_STREAM_COPY                 0x88E2
#endif
#ifndef GL_DYNAMIC_DRAW
# define GL_DYNAMIC_DRAW                0x88E8
#endif

#ifndef GL_PIXEL_PACK_BUFFER
# define GL_PIXEL_PACK_BUFFER           0x88EB
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
# define GL_PIXEL_UNPACK_BUFFER         0x88EC
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
# define GL_PIXEL_PACK_BUFFER_BINDING   0x88ED
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
# define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif

typedef GLvoid (APIENTRY *PFNVPOXVHWA_GEN_BUFFERS)(GLsizei n, GLuint *buffers);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_DELETE_BUFFERS)(GLsizei n, const GLuint *buffers);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_BIND_BUFFER)(GLenum target, GLuint buffer);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_BUFFER_DATA)(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRY *PFNVPOXVHWA_MAP_BUFFER)(GLenum target, GLenum access);
typedef GLboolean (APIENTRY *PFNVPOXVHWA_UNMAP_BUFFER)(GLenum target);

/* GL_EXT_framebuffer_object */
#ifndef GL_FRAMEBUFFER
# define GL_FRAMEBUFFER                0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
# define GL_COLOR_ATTACHMENT0          0x8CE0
#endif

typedef GLboolean (APIENTRY *PFNVPOXVHWA_IS_FRAMEBUFFER)(GLuint framebuffer);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_BIND_FRAMEBUFFER)(GLenum target, GLuint framebuffer);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_DELETE_FRAMEBUFFERS)(GLsizei n, const GLuint *framebuffers);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GEN_FRAMEBUFFERS)(GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRY *PFNVPOXVHWA_CHECK_FRAMEBUFFER_STATUS)(GLenum target);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_FRAMEBUFFER_TEXTURE1D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_FRAMEBUFFER_TEXTURE2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_FRAMEBUFFER_TEXTURE3D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef GLvoid (APIENTRY *PFNVPOXVHWA_GET_FRAMEBUFFER_ATTACHMENT_PARAMETRIV)(GLenum target, GLenum attachment, GLenum pname, GLint *params);


/*****************/

/* functions */

/* @todo: move those to VPoxGLInfo class instance members ??? */
extern PFNVPOXVHWA_ACTIVE_TEXTURE vpoxglActiveTexture;
extern PFNVPOXVHWA_MULTI_TEX_COORD2I vpoxglMultiTexCoord2i;
extern PFNVPOXVHWA_MULTI_TEX_COORD2D vpoxglMultiTexCoord2d;
extern PFNVPOXVHWA_MULTI_TEX_COORD2F vpoxglMultiTexCoord2f;


extern PFNVPOXVHWA_CREATE_SHADER   vpoxglCreateShader;
extern PFNVPOXVHWA_SHADER_SOURCE   vpoxglShaderSource;
extern PFNVPOXVHWA_COMPILE_SHADER  vpoxglCompileShader;
extern PFNVPOXVHWA_DELETE_SHADER   vpoxglDeleteShader;

extern PFNVPOXVHWA_CREATE_PROGRAM  vpoxglCreateProgram;
extern PFNVPOXVHWA_ATTACH_SHADER   vpoxglAttachShader;
extern PFNVPOXVHWA_DETACH_SHADER   vpoxglDetachShader;
extern PFNVPOXVHWA_LINK_PROGRAM    vpoxglLinkProgram;
extern PFNVPOXVHWA_USE_PROGRAM     vpoxglUseProgram;
extern PFNVPOXVHWA_DELETE_PROGRAM  vpoxglDeleteProgram;

extern PFNVPOXVHWA_IS_SHADER       vpoxglIsShader;
extern PFNVPOXVHWA_GET_SHADERIV    vpoxglGetShaderiv;
extern PFNVPOXVHWA_IS_PROGRAM      vpoxglIsProgram;
extern PFNVPOXVHWA_GET_PROGRAMIV   vpoxglGetProgramiv;
extern PFNVPOXVHWA_GET_ATTACHED_SHADERS vpoxglGetAttachedShaders;
extern PFNVPOXVHWA_GET_SHADER_INFO_LOG  vpoxglGetShaderInfoLog;
extern PFNVPOXVHWA_GET_PROGRAM_INFO_LOG vpoxglGetProgramInfoLog;

extern PFNVPOXVHWA_GET_UNIFORM_LOCATION vpoxglGetUniformLocation;

extern PFNVPOXVHWA_UNIFORM1F vpoxglUniform1f;
extern PFNVPOXVHWA_UNIFORM2F vpoxglUniform2f;
extern PFNVPOXVHWA_UNIFORM3F vpoxglUniform3f;
extern PFNVPOXVHWA_UNIFORM4F vpoxglUniform4f;

extern PFNVPOXVHWA_UNIFORM1I vpoxglUniform1i;
extern PFNVPOXVHWA_UNIFORM2I vpoxglUniform2i;
extern PFNVPOXVHWA_UNIFORM3I vpoxglUniform3i;
extern PFNVPOXVHWA_UNIFORM4I vpoxglUniform4i;

extern PFNVPOXVHWA_GEN_BUFFERS vpoxglGenBuffers;
extern PFNVPOXVHWA_DELETE_BUFFERS vpoxglDeleteBuffers;
extern PFNVPOXVHWA_BIND_BUFFER vpoxglBindBuffer;
extern PFNVPOXVHWA_BUFFER_DATA vpoxglBufferData;
extern PFNVPOXVHWA_MAP_BUFFER vpoxglMapBuffer;
extern PFNVPOXVHWA_UNMAP_BUFFER vpoxglUnmapBuffer;

extern PFNVPOXVHWA_IS_FRAMEBUFFER vpoxglIsFramebuffer;
extern PFNVPOXVHWA_BIND_FRAMEBUFFER vpoxglBindFramebuffer;
extern PFNVPOXVHWA_DELETE_FRAMEBUFFERS vpoxglDeleteFramebuffers;
extern PFNVPOXVHWA_GEN_FRAMEBUFFERS vpoxglGenFramebuffers;
extern PFNVPOXVHWA_CHECK_FRAMEBUFFER_STATUS vpoxglCheckFramebufferStatus;
extern PFNVPOXVHWA_FRAMEBUFFER_TEXTURE1D vpoxglFramebufferTexture1D;
extern PFNVPOXVHWA_FRAMEBUFFER_TEXTURE2D vpoxglFramebufferTexture2D;
extern PFNVPOXVHWA_FRAMEBUFFER_TEXTURE3D vpoxglFramebufferTexture3D;
extern PFNVPOXVHWA_GET_FRAMEBUFFER_ATTACHMENT_PARAMETRIV vpoxglGetFramebufferAttachmentParameteriv;


class VPoxGLInfo
{
public:
    VPoxGLInfo() :
        mGLVersion(0),
        mFragmentShaderSupported(false),
        mTextureRectangleSupported(false),
        mTextureNP2Supported(false),
        mPBOSupported(false),
        mFBOSupported(false),
        mMultiTexNumSupported(1), /* 1 would mean it is not supported */
        m_GL_ARB_multitexture(false),
        m_GL_ARB_shader_objects(false),
        m_GL_ARB_fragment_shader(false),
        m_GL_ARB_pixel_buffer_object(false),
        m_GL_ARB_texture_rectangle(false),
        m_GL_EXT_texture_rectangle(false),
        m_GL_NV_texture_rectangle(false),
        m_GL_ARB_texture_non_power_of_two(false),
        m_GL_EXT_framebuffer_object(false),
        mInitialized(false)
    {}

    void init(const class QGLContext * pContext);

    bool isInitialized() const { return mInitialized; }

    int getGLVersion() const { return mGLVersion; }
    bool isFragmentShaderSupported() const { return mFragmentShaderSupported; }
    bool isTextureRectangleSupported() const { return mTextureRectangleSupported; }
    bool isTextureNP2Supported() const { return mTextureNP2Supported; }
    bool isPBOSupported() const { return mPBOSupported; }
    /* some ATI drivers do not seem to support non-zero offsets when dealing with PBOs
     * @todo: add a check for that, always unsupported currently */
    bool isPBOOffsetSupported() const { return false; }
    bool isFBOSupported() const { return mFBOSupported; }
    /* 1 would mean it is not supported */
    int getMultiTexNumSupported() const { return mMultiTexNumSupported; }

    static int parseVersion(const GLubyte * ver);
private:
    void initExtSupport(const class QGLContext & context);

    int mGLVersion;
    bool mFragmentShaderSupported;
    bool mTextureRectangleSupported;
    bool mTextureNP2Supported;
    bool mPBOSupported;
    bool mFBOSupported;
    int mMultiTexNumSupported; /* 1 would mean it is not supported */

    bool m_GL_ARB_multitexture;
    bool m_GL_ARB_shader_objects;
    bool m_GL_ARB_fragment_shader;
    bool m_GL_ARB_pixel_buffer_object;
    bool m_GL_ARB_texture_rectangle;
    bool m_GL_EXT_texture_rectangle;
    bool m_GL_NV_texture_rectangle;
    bool m_GL_ARB_texture_non_power_of_two;
    bool m_GL_EXT_framebuffer_object;

    bool mInitialized;
};

class VPoxGLTmpContext
{
public:
    VPoxGLTmpContext();
    ~VPoxGLTmpContext();

    const class QGLContext * makeCurrent();
private:
    class QGLWidget * mWidget;
};


#define VPOXQGL_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))

#define FOURCC_AYUV VPOXQGL_MAKEFOURCC('A', 'Y', 'U', 'V')
#define FOURCC_UYVY VPOXQGL_MAKEFOURCC('U', 'Y', 'V', 'Y')
#define FOURCC_YUY2 VPOXQGL_MAKEFOURCC('Y', 'U', 'Y', '2')
#define FOURCC_YV12 VPOXQGL_MAKEFOURCC('Y', 'V', '1', '2')
#define VPOXVHWA_NUMFOURCC 4

class VPoxVHWAInfo
{
public:
    VPoxVHWAInfo() :
        mFourccSupportedCount(0),
        mInitialized(false)
    {}

    VPoxVHWAInfo(const VPoxGLInfo & glInfo) :
        mglInfo(glInfo),
        mFourccSupportedCount(0),
        mInitialized(false)
    {}

    void init(const class QGLContext * pContext);

    bool isInitialized() const { return mInitialized; }

    const VPoxGLInfo & getGlInfo() const { return mglInfo; }

    bool isVHWASupported() const;

    int getFourccSupportedCount() const { return mFourccSupportedCount; }
    const uint32_t * getFourccSupportedList() const { return mFourccSupportedList; }

    static bool checkVHWASupport();
private:
    VPoxGLInfo mglInfo;
    uint32_t mFourccSupportedList[VPOXVHWA_NUMFOURCC];
    int mFourccSupportedCount;

    bool mInitialized;
};

#endif /* !VPOX_INCLUDED_VPoxGL2D_h */
