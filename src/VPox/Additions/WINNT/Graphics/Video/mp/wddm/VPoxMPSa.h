/* $Id: VPoxMPSa.h $ */

/** @file
 * Sorted array API
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPSa_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPSa_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/assert.h>

typedef struct CR_SORTARRAY
{
    uint32_t cBufferSize;
    uint32_t cSize;
    uint64_t *pElements;
} CR_SORTARRAY;


#ifndef IN_RING0
# define VPOXSADECL(_type) DECLEXPORT(_type)
#else
# define VPOXSADECL(_type) RTDECL(_type)
#endif


DECLINLINE(uint32_t) CrSaGetSize(const CR_SORTARRAY *pArray)
{
    return pArray->cSize;
}

DECLINLINE(uint64_t) CrSaGetVal(const CR_SORTARRAY *pArray, uint32_t i)
{
    Assert(i < pArray->cSize);
    return pArray->pElements[i];
}

DECLINLINE(const uint64_t*) CrSaGetElements(const CR_SORTARRAY *pArray)
{
    return pArray->pElements;
}

DECLINLINE(void) CrSaClear(CR_SORTARRAY *pArray)
{
    pArray->cSize = 0;
}

VPOXSADECL(int) CrSaInit(CR_SORTARRAY *pArray, uint32_t cInitBuffer);
VPOXSADECL(void) CrSaCleanup(CR_SORTARRAY *pArray);
/*
 * @return true if element is found */
VPOXSADECL(bool) CrSaContains(const CR_SORTARRAY *pArray, uint64_t element);

/*
 * @return VINF_SUCCESS  if element is added
 * VINF_ALREADY_INITIALIZED if element was in array already
 * VERR_NO_MEMORY - no memory
 *  */
VPOXSADECL(int) CrSaAdd(CR_SORTARRAY *pArray, uint64_t element);

/*
 * @return VINF_SUCCESS  if element is removed
 * VINF_ALREADY_INITIALIZED if element was NOT in array
 *  */
VPOXSADECL(int) CrSaRemove(CR_SORTARRAY *pArray, uint64_t element);

/*
 * @return VINF_SUCCESS on success
 * VERR_NO_MEMORY - no memory
 *  */
VPOXSADECL(void) CrSaIntersect(CR_SORTARRAY *pArray1, const CR_SORTARRAY *pArray2);
VPOXSADECL(int) CrSaIntersected(const CR_SORTARRAY *pArray1, const CR_SORTARRAY *pArray2, CR_SORTARRAY *pResult);

/*
 * @return VINF_SUCCESS on success
 * VERR_NO_MEMORY - no memory
 *  */
VPOXSADECL(int) CrSaUnited(const CR_SORTARRAY *pArray1, const CR_SORTARRAY *pArray2, CR_SORTARRAY *pResult);

/*
 * @return VINF_SUCCESS on success
 * VERR_NO_MEMORY - no memory
 *  */
VPOXSADECL(int) CrSaClone(const CR_SORTARRAY *pArray1, CR_SORTARRAY *pResult);

VPOXSADECL(int) CrSaCmp(const CR_SORTARRAY *pArray1, const CR_SORTARRAY *pArray2);

VPOXSADECL(bool) CrSaCovers(const CR_SORTARRAY *pArray1, const CR_SORTARRAY *pArray2);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPSa_h */
