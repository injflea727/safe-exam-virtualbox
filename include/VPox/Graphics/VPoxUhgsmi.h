/* $Id: VPoxUhgsmi.h $ */
/** @file
 * Document me, pretty please.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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

#ifndef VPOX_INCLUDED_Graphics_VPoxUhgsmi_h
#define VPOX_INCLUDED_Graphics_VPoxUhgsmi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>

typedef struct VPOXUHGSMI *PVPOXUHGSMI;

typedef struct VPOXUHGSMI_BUFFER *PVPOXUHGSMI_BUFFER;

typedef union VPOXUHGSMI_BUFFER_TYPE_FLAGS
{
    uint32_t Value;
    struct
    {
        uint32_t fCommand       : 1;
        uint32_t Reserved       : 31;
    } s;
} VPOXUHGSMI_BUFFER_TYPE_FLAGS;

typedef union VPOXUHGSMI_BUFFER_LOCK_FLAGS
{
    uint32_t Value;
    struct
    {
        uint32_t fReadOnly      : 1;
        uint32_t fWriteOnly     : 1;
        uint32_t fDonotWait     : 1;
        uint32_t fDiscard       : 1;
        uint32_t fLockEntire    : 1;
        uint32_t Reserved       : 27;
    } s;
} VPOXUHGSMI_BUFFER_LOCK_FLAGS;

typedef union VPOXUHGSMI_BUFFER_SUBMIT_FLAGS
{
    uint32_t Value;
    struct
    {
        uint32_t fHostReadOnly  : 1;
        uint32_t fHostWriteOnly : 1;
        uint32_t fDoNotRetire   : 1; /**< the buffer will be used in a subsequent command */
        uint32_t fEntireBuffer  : 1;
        uint32_t Reserved       : 28;
    } s;
} VPOXUHGSMI_BUFFER_SUBMIT_FLAGS, *PVPOXUHGSMI_BUFFER_SUBMIT_FLAGS;

/* the caller can specify NULL as a hSynch and specify a valid enmSynchType to make UHGSMI create a proper object itself,
 *  */
typedef DECLCALLBACK(int) FNVPOXUHGSMI_BUFFER_CREATE(PVPOXUHGSMI pHgsmi, uint32_t cbBuf, VPOXUHGSMI_BUFFER_TYPE_FLAGS fType, PVPOXUHGSMI_BUFFER* ppBuf);
typedef FNVPOXUHGSMI_BUFFER_CREATE *PFNVPOXUHGSMI_BUFFER_CREATE;

typedef struct VPOXUHGSMI_BUFFER_SUBMIT
{
    PVPOXUHGSMI_BUFFER pBuf;
    uint32_t offData;
    uint32_t cbData;
    VPOXUHGSMI_BUFFER_SUBMIT_FLAGS fFlags;
} VPOXUHGSMI_BUFFER_SUBMIT, *PVPOXUHGSMI_BUFFER_SUBMIT;

typedef DECLCALLBACK(int) FNVPOXUHGSMI_BUFFER_SUBMIT(PVPOXUHGSMI pHgsmi, PVPOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers);
typedef FNVPOXUHGSMI_BUFFER_SUBMIT *PFNVPOXUHGSMI_BUFFER_SUBMIT;

typedef DECLCALLBACK(int) FNVPOXUHGSMI_BUFFER_DESTROY(PVPOXUHGSMI_BUFFER pBuf);
typedef FNVPOXUHGSMI_BUFFER_DESTROY *PFNVPOXUHGSMI_BUFFER_DESTROY;

typedef DECLCALLBACK(int) FNVPOXUHGSMI_BUFFER_LOCK(PVPOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VPOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock);
typedef FNVPOXUHGSMI_BUFFER_LOCK *PFNVPOXUHGSMI_BUFFER_LOCK;

typedef DECLCALLBACK(int) FNVPOXUHGSMI_BUFFER_UNLOCK(PVPOXUHGSMI_BUFFER pBuf);
typedef FNVPOXUHGSMI_BUFFER_UNLOCK *PFNVPOXUHGSMI_BUFFER_UNLOCK;

typedef struct VPOXUHGSMI
{
    PFNVPOXUHGSMI_BUFFER_CREATE pfnBufferCreate;
    PFNVPOXUHGSMI_BUFFER_SUBMIT pfnBufferSubmit;
    /** User custom data. */
    void *pvUserData;
} VPOXUHGSMI;

typedef struct VPOXUHGSMI_BUFFER
{
    PFNVPOXUHGSMI_BUFFER_LOCK pfnLock;
    PFNVPOXUHGSMI_BUFFER_UNLOCK pfnUnlock;
    PFNVPOXUHGSMI_BUFFER_DESTROY pfnDestroy;

    /* r/o data added for ease of access and simplicity
     * modifying it leads to unpredictable behavior */
    VPOXUHGSMI_BUFFER_TYPE_FLAGS fType;
    uint32_t cbBuffer;
    /** User custom data. */
    void *pvUserData;
} VPOXUHGSMI_BUFFER;

#define VPoxUhgsmiBufferCreate(_pUhgsmi, _cbBuf, _fType, _ppBuf) ((_pUhgsmi)->pfnBufferCreate(_pUhgsmi, _cbBuf, _fType, _ppBuf))
#define VPoxUhgsmiBufferSubmit(_pUhgsmi, _aBuffers, _cBuffers) ((_pUhgsmi)->pfnBufferSubmit(_pUhgsmi, _aBuffers, _cBuffers))

#define VPoxUhgsmiBufferLock(_pBuf, _offLock, _cbLock, _fFlags, _pvLock) ((_pBuf)->pfnLock(_pBuf, _offLock, _cbLock, _fFlags, _pvLock))
#define VPoxUhgsmiBufferUnlock(_pBuf) ((_pBuf)->pfnUnlock(_pBuf))
#define VPoxUhgsmiBufferDestroy(_pBuf) ((_pBuf)->pfnDestroy(_pBuf))

#endif /* !VPOX_INCLUDED_Graphics_VPoxUhgsmi_h */

