/* $Id: VPoxVideoTools.h $ */
/** @file
 * VPox Video tooling
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_common_VPoxVideoTools_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_common_VPoxVideoTools_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/assert.h>

typedef struct VPOXVTLIST_ENTRY
{
    struct VPOXVTLIST_ENTRY *pNext;
} VPOXVTLIST_ENTRY, *PVPOXVTLIST_ENTRY;

typedef struct VPOXVTLIST
{
    PVPOXVTLIST_ENTRY pFirst;
    PVPOXVTLIST_ENTRY pLast;
} VPOXVTLIST, *PVPOXVTLIST;

DECLINLINE(bool) vpoxVtListIsEmpty(PVPOXVTLIST pList)
{
    return !pList->pFirst;
}

DECLINLINE(void) vpoxVtListInit(PVPOXVTLIST pList)
{
    pList->pFirst = pList->pLast = NULL;
}

DECLINLINE(void) vpoxVtListPut(PVPOXVTLIST pList, PVPOXVTLIST_ENTRY pFirst, PVPOXVTLIST_ENTRY pLast)
{
    Assert(pFirst);
    Assert(pLast);
    pLast->pNext = NULL;
    if (pList->pLast)
    {
        Assert(pList->pFirst);
        pList->pLast->pNext = pFirst;
        pList->pLast = pLast;
    }
    else
    {
        Assert(!pList->pFirst);
        pList->pFirst = pFirst;
        pList->pLast = pLast;
    }
}

#define vpoxVtListPutTail vpoxVtListPut

DECLINLINE(void) vpoxVtListPutHead(PVPOXVTLIST pList, PVPOXVTLIST_ENTRY pFirst, PVPOXVTLIST_ENTRY pLast)
{
    Assert(pFirst);
    Assert(pLast);
    pLast->pNext = pList->pFirst;
    if (!pList->pLast)
    {
        Assert(!pList->pFirst);
        pList->pLast = pLast;
    }
    else
    {
        Assert(pList->pFirst);
    }
    pList->pFirst = pFirst;
}

DECLINLINE(void) vpoxVtListPutEntryHead(PVPOXVTLIST pList, PVPOXVTLIST_ENTRY pEntry)
{
    vpoxVtListPutHead(pList, pEntry, pEntry);
}

DECLINLINE(void) vpoxVtListPutEntryTail(PVPOXVTLIST pList, PVPOXVTLIST_ENTRY pEntry)
{
    vpoxVtListPutTail(pList, pEntry, pEntry);
}

DECLINLINE(void) vpoxVtListCat(PVPOXVTLIST pList1, PVPOXVTLIST pList2)
{
    vpoxVtListPut(pList1, pList2->pFirst, pList2->pLast);
    pList2->pFirst = pList2->pLast = NULL;
}

DECLINLINE(void) vpoxVtListDetach(PVPOXVTLIST pList, PVPOXVTLIST_ENTRY *ppFirst, PVPOXVTLIST_ENTRY *ppLast)
{
    *ppFirst = pList->pFirst;
    if (ppLast)
        *ppLast = pList->pLast;
    pList->pFirst = NULL;
    pList->pLast = NULL;
}

DECLINLINE(void) vpoxVtListDetach2List(PVPOXVTLIST pList, PVPOXVTLIST pDstList)
{
    vpoxVtListDetach(pList, &pDstList->pFirst, &pDstList->pLast);
}

DECLINLINE(void) vpoxVtListDetachEntries(PVPOXVTLIST pList, PVPOXVTLIST_ENTRY pBeforeDetach, PVPOXVTLIST_ENTRY pLast2Detach)
{
    if (pBeforeDetach)
    {
        pBeforeDetach->pNext = pLast2Detach->pNext;
        if (!pBeforeDetach->pNext)
            pList->pLast = pBeforeDetach;
    }
    else
    {
        pList->pFirst = pLast2Detach->pNext;
        if (!pList->pFirst)
            pList->pLast = NULL;
    }
    pLast2Detach->pNext = NULL;
}

DECLINLINE(void) vpoxWddmRectUnite(RECT *pR, const RECT *pR2Unite)
{
    pR->left = RT_MIN(pR->left, pR2Unite->left);
    pR->top = RT_MIN(pR->top, pR2Unite->top);
    pR->right = RT_MAX(pR->right, pR2Unite->right);
    pR->bottom = RT_MAX(pR->bottom, pR2Unite->bottom);
}

DECLINLINE(bool) vpoxWddmRectIntersection(const RECT *a, const RECT *b, RECT *rect)
{
    Assert(a);
    Assert(b);
    Assert(rect);
    rect->left = RT_MAX(a->left, b->left);
    rect->right = RT_MIN(a->right, b->right);
    rect->top = RT_MAX(a->top, b->top);
    rect->bottom = RT_MIN(a->bottom, b->bottom);
    return (rect->right>rect->left) && (rect->bottom>rect->top);
}

DECLINLINE(bool) vpoxWddmRectIsEqual(const RECT *pRect1, const RECT *pRect2)
{
    Assert(pRect1);
    Assert(pRect2);
    if (pRect1->left != pRect2->left)
        return false;
    if (pRect1->top != pRect2->top)
        return false;
    if (pRect1->right != pRect2->right)
        return false;
    if (pRect1->bottom != pRect2->bottom)
        return false;
    return true;
}

DECLINLINE(bool) vpoxWddmRectIsCoveres(const RECT *pRect, const RECT *pCovered)
{
    Assert(pRect);
    Assert(pCovered);
    if (pRect->left > pCovered->left)
        return false;
    if (pRect->top > pCovered->top)
        return false;
    if (pRect->right < pCovered->right)
        return false;
    if (pRect->bottom < pCovered->bottom)
        return false;
    return true;
}

DECLINLINE(bool) vpoxWddmRectIsEmpty(const RECT * pRect)
{
    return pRect->left == pRect->right-1 && pRect->top == pRect->bottom-1;
}

DECLINLINE(bool) vpoxWddmRectIsIntersect(const RECT * pRect1, const RECT * pRect2)
{
    return !((pRect1->left < pRect2->left && pRect1->right <= pRect2->left)
            || (pRect2->left < pRect1->left && pRect2->right <= pRect1->left)
            || (pRect1->top < pRect2->top && pRect1->bottom <= pRect2->top)
            || (pRect2->top < pRect1->top && pRect2->bottom <= pRect1->top));
}

DECLINLINE(void) vpoxWddmRectUnited(RECT * pDst, const RECT * pRect1, const RECT * pRect2)
{
    pDst->left = RT_MIN(pRect1->left, pRect2->left);
    pDst->top = RT_MIN(pRect1->top, pRect2->top);
    pDst->right = RT_MAX(pRect1->right, pRect2->right);
    pDst->bottom = RT_MAX(pRect1->bottom, pRect2->bottom);
}

DECLINLINE(void) vpoxWddmRectTranslate(RECT * pRect, int x, int y)
{
    pRect->left   += x;
    pRect->top    += y;
    pRect->right  += x;
    pRect->bottom += y;
}

DECLINLINE(void) vpoxWddmRectMove(RECT * pRect, int x, int y)
{
    LONG w = pRect->right - pRect->left;
    LONG h = pRect->bottom - pRect->top;
    pRect->left   = x;
    pRect->top    = y;
    pRect->right  = w + x;
    pRect->bottom = h + y;
}

DECLINLINE(void) vpoxWddmRectTranslated(RECT *pDst, const RECT * pRect, int x, int y)
{
    *pDst = *pRect;
    vpoxWddmRectTranslate(pDst, x, y);
}

DECLINLINE(void) vpoxWddmRectMoved(RECT *pDst, const RECT * pRect, int x, int y)
{
    *pDst = *pRect;
    vpoxWddmRectMove(pDst, x, y);
}

typedef struct VPOXPOINT3D
{
    UINT x;
    UINT y;
    UINT z;
} VPOXPOINT3D, *PVPOXPOINT3D;

typedef struct VPOXBOX3D
{
    UINT Left;
    UINT Top;
    UINT Right;
    UINT Bottom;
    UINT Front;
    UINT Back;
} VPOXBOX3D, *PVPOXBOX3D;

DECLINLINE(void) vpoxWddmBoxTranslate(VPOXBOX3D * pBox, int x, int y, int z)
{
    pBox->Left   += x;
    pBox->Top    += y;
    pBox->Right  += x;
    pBox->Bottom += y;
    pBox->Front  += z;
    pBox->Back   += z;
}

DECLINLINE(void) vpoxWddmBoxMove(VPOXBOX3D * pBox, int x, int y, int z)
{
    LONG w = pBox->Right - pBox->Left;
    LONG h = pBox->Bottom - pBox->Top;
    LONG d = pBox->Back - pBox->Front;
    pBox->Left   = x;
    pBox->Top    = y;
    pBox->Right  = w + x;
    pBox->Bottom = h + y;
    pBox->Front  = z;
    pBox->Back   = d + z;
}

#define VPOXWDDM_BOXDIV_U(_v, _d, _nz) do { \
        UINT tmp = (_v) / (_d); \
        if (!tmp && (_v) && (_nz)) \
            (_v) = 1; \
        else \
            (_v) = tmp; \
    } while (0)

DECLINLINE(void) vpoxWddmBoxDivide(VPOXBOX3D * pBox, int div, bool fDontReachZero)
{
    VPOXWDDM_BOXDIV_U(pBox->Left, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pBox->Top, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pBox->Right, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pBox->Bottom, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pBox->Front, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pBox->Back, div, fDontReachZero);
}

DECLINLINE(void) vpoxWddmPoint3DDivide(VPOXPOINT3D * pPoint, int div, bool fDontReachZero)
{
    VPOXWDDM_BOXDIV_U(pPoint->x, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pPoint->y, div, fDontReachZero);
    VPOXWDDM_BOXDIV_U(pPoint->y, div, fDontReachZero);
}

DECLINLINE(void) vpoxWddmBoxTranslated(VPOXBOX3D * pDst, const VPOXBOX3D * pBox, int x, int y, int z)
{
    *pDst = *pBox;
    vpoxWddmBoxTranslate(pDst, x, y, z);
}

DECLINLINE(void) vpoxWddmBoxMoved(VPOXBOX3D * pDst, const VPOXBOX3D * pBox, int x, int y, int z)
{
    *pDst = *pBox;
    vpoxWddmBoxMove(pDst, x, y, z);
}

DECLINLINE(void) vpoxWddmBoxDivided(VPOXBOX3D * pDst, const VPOXBOX3D * pBox, int div, bool fDontReachZero)
{
    *pDst = *pBox;
    vpoxWddmBoxDivide(pDst, div, fDontReachZero);
}

DECLINLINE(void) vpoxWddmPoint3DDivided(VPOXPOINT3D * pDst, const VPOXPOINT3D * pPoint, int div, bool fDontReachZero)
{
    *pDst = *pPoint;
    vpoxWddmPoint3DDivide(pDst, div, fDontReachZero);
}

/* the dirty rect info is valid */
#define VPOXWDDM_DIRTYREGION_F_VALID      0x00000001
#define VPOXWDDM_DIRTYREGION_F_RECT_VALID 0x00000002

typedef struct VPOXWDDM_DIRTYREGION
{
    uint32_t fFlags; /* <-- see VPOXWDDM_DIRTYREGION_F_xxx flags above */
    RECT Rect;
} VPOXWDDM_DIRTYREGION, *PVPOXWDDM_DIRTYREGION;

DECLINLINE(void) vpoxWddmDirtyRegionAddRect(PVPOXWDDM_DIRTYREGION pInfo, const RECT *pRect)
{
    if (!(pInfo->fFlags & VPOXWDDM_DIRTYREGION_F_VALID))
    {
        pInfo->fFlags = VPOXWDDM_DIRTYREGION_F_VALID;
        if (pRect)
        {
            pInfo->fFlags |= VPOXWDDM_DIRTYREGION_F_RECT_VALID;
            pInfo->Rect = *pRect;
        }
    }
    else if (!!(pInfo->fFlags & VPOXWDDM_DIRTYREGION_F_RECT_VALID))
    {
        if (pRect)
            vpoxWddmRectUnite(&pInfo->Rect, pRect);
        else
            pInfo->fFlags &= ~VPOXWDDM_DIRTYREGION_F_RECT_VALID;
    }
}

DECLINLINE(void) vpoxWddmDirtyRegionUnite(PVPOXWDDM_DIRTYREGION pInfo, const VPOXWDDM_DIRTYREGION *pInfo2)
{
    if (pInfo2->fFlags & VPOXWDDM_DIRTYREGION_F_VALID)
    {
        if (pInfo2->fFlags & VPOXWDDM_DIRTYREGION_F_RECT_VALID)
            vpoxWddmDirtyRegionAddRect(pInfo, &pInfo2->Rect);
        else
            vpoxWddmDirtyRegionAddRect(pInfo, NULL);
    }
}

DECLINLINE(void) vpoxWddmDirtyRegionClear(PVPOXWDDM_DIRTYREGION pInfo)
{
    pInfo->fFlags = 0;
}

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_common_VPoxVideoTools_h */
