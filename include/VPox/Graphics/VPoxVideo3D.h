/* $Id: VPoxVideo3D.h $ */
/** @file
 * VirtualPox 3D common tooling
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

#ifndef VPOX_INCLUDED_Graphics_VPoxVideo3D_h
#define VPOX_INCLUDED_Graphics_VPoxVideo3D_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#ifndef VPoxTlsRefGetImpl
# ifdef VPoxTlsRefSetImpl
#  error "VPoxTlsRefSetImpl is defined, unexpected!"
# endif
# include <iprt/thread.h>
# define VPoxTlsRefGetImpl(_tls) (RTTlsGet((RTTLS)(_tls)))
# define VPoxTlsRefSetImpl(_tls, _val) (RTTlsSet((RTTLS)(_tls), (_val)))
#else
# ifndef VPoxTlsRefSetImpl
#  error "VPoxTlsRefSetImpl is NOT defined, unexpected!"
# endif
#endif

#ifndef VPoxTlsRefAssertImpl
# define VPoxTlsRefAssertImpl(_a) do {} while (0)
#endif

typedef DECLCALLBACK(void) FNVPOXTLSREFDTOR(void*);
typedef FNVPOXTLSREFDTOR *PFNVPOXTLSREFDTOR;

typedef enum {
    VPOXTLSREFDATA_STATE_UNDEFINED = 0,
    VPOXTLSREFDATA_STATE_INITIALIZED,
    VPOXTLSREFDATA_STATE_TOBE_DESTROYED,
    VPOXTLSREFDATA_STATE_DESTROYING,
    VPOXTLSREFDATA_STATE_32BIT_HACK = 0x7fffffff
} VPOXTLSREFDATA_STATE;

#define VPOXTLSREFDATA \
    volatile int32_t cTlsRefs; \
    VPOXTLSREFDATA_STATE enmTlsRefState; \
    PFNVPOXTLSREFDTOR pfnTlsRefDtor; \

struct VPOXTLSREFDATA_DUMMY
{
    VPOXTLSREFDATA
};

#define VPOXTLSREFDATA_OFFSET(_t) RT_OFFSETOF(_t, cTlsRefs)
#define VPOXTLSREFDATA_ASSERT_OFFSET(_t) RTASSERT_OFFSET_OF(_t, cTlsRefs)
#define VPOXTLSREFDATA_SIZE() (sizeof (struct VPOXTLSREFDATA_DUMMY))
#define VPOXTLSREFDATA_COPY(_pDst, _pSrc) do { \
        (_pDst)->cTlsRefs = (_pSrc)->cTlsRefs; \
        (_pDst)->enmTlsRefState = (_pSrc)->enmTlsRefState; \
        (_pDst)->pfnTlsRefDtor = (_pSrc)->pfnTlsRefDtor; \
    } while (0)

#define VPOXTLSREFDATA_EQUAL(_pDst, _pSrc) ( \
           (_pDst)->cTlsRefs == (_pSrc)->cTlsRefs \
        && (_pDst)->enmTlsRefState == (_pSrc)->enmTlsRefState \
        && (_pDst)->pfnTlsRefDtor == (_pSrc)->pfnTlsRefDtor \
    )


#define VPoxTlsRefInit(_p, _pfnDtor) do { \
        (_p)->cTlsRefs = 1; \
        (_p)->enmTlsRefState = VPOXTLSREFDATA_STATE_INITIALIZED; \
        (_p)->pfnTlsRefDtor = (_pfnDtor); \
    } while (0)

#define VPoxTlsRefIsFunctional(_p) (!!((_p)->enmTlsRefState == VPOXTLSREFDATA_STATE_INITIALIZED))

#define VPoxTlsRefAddRef(_p) do { \
        int cRefs = ASMAtomicIncS32(&(_p)->cTlsRefs); \
        VPoxTlsRefAssertImpl(cRefs > 1 || (_p)->enmTlsRefState == VPOXTLSREFDATA_STATE_DESTROYING); \
        RT_NOREF(cRefs); \
    } while (0)

#define VPoxTlsRefCountGet(_p) (ASMAtomicReadS32(&(_p)->cTlsRefs))

#define VPoxTlsRefRelease(_p) do { \
        int cRefs = ASMAtomicDecS32(&(_p)->cTlsRefs); \
        VPoxTlsRefAssertImpl(cRefs >= 0); \
        if (!cRefs && (_p)->enmTlsRefState != VPOXTLSREFDATA_STATE_DESTROYING /* <- avoid recursion if VPoxTlsRefAddRef/Release is called from dtor */) { \
            (_p)->enmTlsRefState = VPOXTLSREFDATA_STATE_DESTROYING; \
            (_p)->pfnTlsRefDtor((_p)); \
        } \
    } while (0)

#define VPoxTlsRefMarkDestroy(_p) do { \
        (_p)->enmTlsRefState = VPOXTLSREFDATA_STATE_TOBE_DESTROYED; \
    } while (0)

#define VPoxTlsRefGetCurrent(_t, _Tsd) ((_t*) VPoxTlsRefGetImpl((_Tsd)))

#define VPoxTlsRefGetCurrentFunctional(_val, _t, _Tsd) do { \
       _t * cur = VPoxTlsRefGetCurrent(_t, _Tsd); \
       if (!cur || VPoxTlsRefIsFunctional(cur)) { \
           (_val) = cur; \
       } else { \
           VPoxTlsRefSetCurrent(_t, _Tsd, NULL); \
           (_val) = NULL; \
       } \
   } while (0)

#define VPoxTlsRefSetCurrent(_t, _Tsd, _p) do { \
        _t * oldCur = VPoxTlsRefGetCurrent(_t, _Tsd); \
        if (oldCur != (_p)) { \
            VPoxTlsRefSetImpl((_Tsd), (_p)); \
            if (oldCur) { \
                VPoxTlsRefRelease(oldCur); \
            } \
            if ((_p)) { \
                VPoxTlsRefAddRef((_t*)(_p)); \
            } \
        } \
    } while (0)


/* host 3D->Fe[/Qt] notification mechanism defines */
typedef enum
{
    VPOX3D_NOTIFY_TYPE_TEST_FUNCTIONAL = 3,
    VPOX3D_NOTIFY_TYPE_3DDATA_VISIBLE  = 4,
    VPOX3D_NOTIFY_TYPE_3DDATA_HIDDEN   = 5,

    VPOX3D_NOTIFY_TYPE_HW_SCREEN_IS_SUPPORTED = 100,
    VPOX3D_NOTIFY_TYPE_HW_SCREEN_CREATED      = 101,
    VPOX3D_NOTIFY_TYPE_HW_SCREEN_DESTROYED    = 102,
    VPOX3D_NOTIFY_TYPE_HW_SCREEN_UPDATE_BEGIN = 103,
    VPOX3D_NOTIFY_TYPE_HW_SCREEN_UPDATE_END   = 104,

    VPOX3D_NOTIFY_TYPE_HW_OVERLAY_CREATED   = 200,
    VPOX3D_NOTIFY_TYPE_HW_OVERLAY_DESTROYED = 201,
    VPOX3D_NOTIFY_TYPE_HW_OVERLAY_GET_ID    = 202,

    VPOX3D_NOTIFY_TYPE_32BIT_HACK = 0x7fffffff
} VPOX3D_NOTIFY_TYPE;

typedef struct VPOX3DNOTIFY
{
    VPOX3D_NOTIFY_TYPE enmNotification;
    int32_t  iDisplay;
    uint32_t u32Reserved;
    uint32_t cbData;
    uint8_t  au8Data[sizeof(uint64_t)];
} VPOX3DNOTIFY;

#endif /* !VPOX_INCLUDED_Graphics_VPoxVideo3D_h */
