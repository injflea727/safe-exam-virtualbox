/* $Id: VPoxUSBFilterMgr.cpp $ */
/** @file
 * VirtualPox Ring-0 USB Filter Manager.
 */

/*
 * Copyright (C) 2007-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VPox/usbfilter.h>
#include "VPoxUSBFilterMgr.h"

#include <iprt/err.h>
#include <iprt/handletable.h>
#include <iprt/mem.h>
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK
# include <iprt/spinlock.h>
#else
# include <iprt/semaphore.h>
#endif
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** @def VPOXUSBFILTERMGR_LOCK
 * Locks the filter list. Careful with scoping since this may
 * create a temporary variable. Don't call twice in the same function.
 */

/** @def VPOXUSBFILTERMGR_UNLOCK
 * Unlocks the filter list.
 */
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK

# define VPOXUSBFILTERMGR_LOCK() \
    RTSpinlockAcquire(g_Spinlock)

# define VPOXUSBFILTERMGR_UNLOCK() \
    RTSpinlockRelease(g_Spinlock)

#else

# define VPOXUSBFILTERMGR_LOCK() \
    do { int rc2 = RTSemFastMutexRequest(g_Mtx); AssertRC(rc2); } while (0)

# define VPOXUSBFILTERMGR_UNLOCK() \
    do { int rc2 = RTSemFastMutexRelease(g_Mtx); AssertRC(rc2); } while (0)

#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to an VPoxUSB filter. */
typedef struct VPOXUSBFILTER *PVPOXUSBFILTER;
/** Pointer to PVPOXUSBFILTER. */
typedef PVPOXUSBFILTER *PPVPOXUSBFILTER;

/**
 * VPoxUSB internal filter representation.
 */
typedef struct VPOXUSBFILTER
{
    /** The core filter. */
    USBFILTER             Core;
    /** The filter owner. */
    VPOXUSBFILTER_CONTEXT Owner;
    /** The filter Id. */
    uint32_t              uHnd;
    /** Pointer to the next filter in the list. */
    PVPOXUSBFILTER        pNext;
} VPOXUSBFILTER;

/**
 * VPoxUSB filter list.
 */
typedef struct VPOXUSBFILTERLIST
{
    /** The head pointer. */
    PVPOXUSBFILTER      pHead;
    /** The tail pointer. */
    PVPOXUSBFILTER      pTail;
} VPOXUSBFILTERLIST;
/** Pointer to a VPOXUSBFILTERLIST. */
typedef VPOXUSBFILTERLIST *PVPOXUSBFILTERLIST;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK
/** Spinlock protecting the filter lists. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
#else
/** Mutex protecting the filter lists. */
static RTSEMFASTMUTEX       g_Mtx = NIL_RTSEMFASTMUTEX;
#endif
/** The per-type filter lists.
 * @remark The first entry is empty (USBFILTERTYPE_INVALID). */
static VPOXUSBFILTERLIST    g_aLists[USBFILTERTYPE_END];
/** The handle table to match handles to the right filter. */
static RTHANDLETABLE        g_hHndTableFilters = NIL_RTHANDLETABLE;



/**
 * Initializes the VPoxUSB filter manager.
 *
 * @returns IPRT status code.
 */
int VPoxUSBFilterInit(void)
{
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK
    int rc = RTSpinlockCreate(&g_Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VPoxUSBFilter");
#else
    int rc = RTSemFastMutexCreate(&g_Mtx);
#endif
    if (RT_SUCCESS(rc))
    {
        uint32_t fFlags;
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK
        fFlags = RTHANDLETABLE_FLAGS_LOCKED_IRQ_SAFE;
#else
        fFlags = RTHANDLETABLE_FLAGS_LOCKED;
#endif
        rc = RTHandleTableCreateEx(&g_hHndTableFilters, fFlags, 1 /* uBase */, 8192 /* cMax */,
                                   NULL, NULL);
        if (RT_SUCCESS(rc))
        {
            /* not really required, but anyway... */
            for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
                g_aLists[i].pHead = g_aLists[i].pTail = NULL;
        }
        else
        {
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK
            RTSpinlockDestroy(g_Spinlock);
            g_Spinlock = NIL_RTSPINLOCK;
#else
            RTSemFastMutexDestroy(g_Mtx);
            g_Mtx = NIL_RTSEMFASTMUTEX;
#endif
        }
    }
    return rc;
}


/**
 * Internal worker that frees a filter.
 *
 * @param   pFilter     The filter to free.
 */
static void vpoxUSBFilterFree(PVPOXUSBFILTER pFilter)
{
    USBFilterDelete(&pFilter->Core);
    pFilter->Owner = VPOXUSBFILTER_CONTEXT_NIL;
    pFilter->pNext = NULL;
    RTMemFree(pFilter);
}


/**
 * Terminates the VPoxUSB filter manager.
 */
void VPoxUSBFilterTerm(void)
{
#ifdef VPOXUSBFILTERMGR_USB_SPINLOCK
    RTSpinlockDestroy(g_Spinlock);
    g_Spinlock = NIL_RTSPINLOCK;
#else
    RTSemFastMutexDestroy(g_Mtx);
    g_Mtx = NIL_RTSEMFASTMUTEX;
#endif

    for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
    {
        PVPOXUSBFILTER pCur = g_aLists[i].pHead;
        g_aLists[i].pHead = g_aLists[i].pTail = NULL;
        while (pCur)
        {
            PVPOXUSBFILTER pNext = pCur->pNext;
            RTHandleTableFree(g_hHndTableFilters, pCur->uHnd);
            vpoxUSBFilterFree(pCur);
            pCur = pNext;
        }
    }

    RTHandleTableDestroy(g_hHndTableFilters, NULL, NULL);
}


/**
 * Adds a new filter.
 *
 * The filter will be validate, duplicated and added.
 *
 * @returns IPRT status code.
 * @param   pFilter     The filter.
 * @param   Owner       The filter owner. Must be non-zero.
 * @param   puId        Where to store the filter ID.
 */
int VPoxUSBFilterAdd(PCUSBFILTER pFilter, VPOXUSBFILTER_CONTEXT Owner, uintptr_t *puId)
{
    /*
     * Validate input.
     */
    int rc = USBFilterValidate(pFilter);
    if (RT_FAILURE(rc))
        return rc;
    if (!Owner || Owner == VPOXUSBFILTER_CONTEXT_NIL)
        return VERR_INVALID_PARAMETER;
    if (!VALID_PTR(puId))
        return VERR_INVALID_POINTER;

    /*
     * Allocate a new filter.
     */
    PVPOXUSBFILTER pNew = (PVPOXUSBFILTER)RTMemAlloc(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;
    memcpy(&pNew->Core, pFilter, sizeof(pNew->Core));
    pNew->Owner = Owner;
    pNew->pNext = NULL;

    rc = RTHandleTableAlloc(g_hHndTableFilters, pNew, &pNew->uHnd);
    if (RT_SUCCESS(rc))
    {
        *puId = pNew->uHnd;

        /*
         * Insert it.
         */
        PVPOXUSBFILTERLIST pList = &g_aLists[pFilter->enmType];

        VPOXUSBFILTERMGR_LOCK();

        if (pList->pTail)
            pList->pTail->pNext = pNew;
        else
            pList->pHead = pNew;
        pList->pTail = pNew;

        VPOXUSBFILTERMGR_UNLOCK();
    }
    else
        RTMemFree(pNew);

    return rc;
}


/**
 * Removes an existing filter.
 *
 * The filter will be validate, duplicated and added.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if successfully removed.
 * @retval  VERR_FILE_NOT_FOUND if the specified filter/owner cannot be found.
 *
 * @param   Owner       The filter owner.
 * @param   uId         The ID of the filter that's to be removed.
 *                      Returned by VPoxUSBFilterAdd().
 */
int VPoxUSBFilterRemove(VPOXUSBFILTER_CONTEXT Owner, uintptr_t uId)
{
    /*
     * Validate input.
     */
    if (!uId || uId != (uint32_t)uId)
        return VERR_INVALID_PARAMETER;
    if (!Owner || Owner == VPOXUSBFILTER_CONTEXT_NIL)
        return VERR_INVALID_PARAMETER;

    /*
     * Locate and unlink it.
     */
    uint32_t uHnd = (uint32_t)uId;
    PVPOXUSBFILTER pCur = NULL;

    VPOXUSBFILTERMGR_LOCK();

    for (unsigned i = USBFILTERTYPE_FIRST; !pCur && i < RT_ELEMENTS(g_aLists); i++)
    {
        PVPOXUSBFILTER pPrev = NULL;
        pCur = g_aLists[i].pHead;
        while (pCur)
        {
            if (    pCur->uHnd == uHnd
                &&  pCur->Owner == Owner)
            {
                PVPOXUSBFILTER pNext = pCur->pNext;
                if (pPrev)
                    pPrev->pNext = pNext;
                else
                    g_aLists[i].pHead = pNext;
                if (!pNext)
                    g_aLists[i].pTail = pPrev;
                break;
            }

            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    VPOXUSBFILTERMGR_UNLOCK();

    /*
     * Free it (if found).
     */
    if (pCur)
    {
        void *pv = RTHandleTableFree(g_hHndTableFilters, pCur->uHnd);
        Assert(pv == pCur); NOREF(pv);
        vpoxUSBFilterFree(pCur);
        return VINF_SUCCESS;
    }

    return VERR_FILE_NOT_FOUND;
}

VPOXUSBFILTER_CONTEXT VPoxUSBFilterGetOwner(uintptr_t uId)
{
    Assert(uId);
    /*
     * Validate input.
     */
    if (!uId || uId != (uint32_t)uId)
        return VPOXUSBFILTER_CONTEXT_NIL;

    /*
     * Result.
     */
    VPOXUSBFILTER_CONTEXT Owner = VPOXUSBFILTER_CONTEXT_NIL;

    VPOXUSBFILTERMGR_LOCK();

    PVPOXUSBFILTER pCur = (PVPOXUSBFILTER)RTHandleTableLookup(g_hHndTableFilters, (uint32_t)uId);
    if (pCur)
        Owner = pCur->Owner;

    Assert(Owner != VPOXUSBFILTER_CONTEXT_NIL);

    VPOXUSBFILTERMGR_UNLOCK();

    return Owner;
}

/**
 * Removes all filters belonging to the specified owner.
 *
 * This is typically called when an owner disconnects or
 * terminates unexpectedly.
 *
 * @param   Owner       The owner
 */
void VPoxUSBFilterRemoveOwner(VPOXUSBFILTER_CONTEXT Owner)
{
    /*
     * Collect the filters that should be freed.
     */
    PVPOXUSBFILTER pToFree = NULL;

    VPOXUSBFILTERMGR_LOCK();

    for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
    {
        PVPOXUSBFILTER pPrev = NULL;
        PVPOXUSBFILTER pCur = g_aLists[i].pHead;
        while (pCur)
        {
            if (pCur->Owner == Owner)
            {
                PVPOXUSBFILTER pNext = pCur->pNext;
                if (pPrev)
                    pPrev->pNext = pNext;
                else
                    g_aLists[i].pHead = pNext;
                if (!pNext)
                    g_aLists[i].pTail = pPrev;

                pCur->pNext = pToFree;
                pToFree = pCur;

                pCur = pNext;
            }
            else
            {
                pPrev = pCur;
                pCur = pCur->pNext;
            }
        }
    }

    VPOXUSBFILTERMGR_UNLOCK();

    /*
     * Free any filters we've found.
     */
    while (pToFree)
    {
        PVPOXUSBFILTER pNext = pToFree->pNext;
        void *pv = RTHandleTableFree(g_hHndTableFilters, pToFree->uHnd);
        Assert(pv == pToFree); NOREF(pv);
        vpoxUSBFilterFree(pToFree);
        pToFree = pNext;
    }
}

/**
 * Match the specified device against the filters.
 * Unlike the VPoxUSBFilterMatch, returns Owner also if exclude filter is matched
 *
 * @returns Owner on if matched, VPOXUSBFILTER_CONTEXT_NIL it not matched.
 * @param   pDevice             The device data as a filter structure.
 *                              See USBFilterMatch for how to construct this.
 * @param   puId                Where to store the filter id (optional).
 * @param   fRemoveFltIfOneShot Whether or not to remove one-shot filters on
 *                              match.
 * @param   pfFilter            Where to store whether the device must be filtered or not
 * @param   pfIsOneShot         Where to return whetehr the match was a one-shot
 *                              filter or not.  Optional.
 *
 */
VPOXUSBFILTER_CONTEXT VPoxUSBFilterMatchEx(PCUSBFILTER pDevice, uintptr_t *puId,
                                           bool fRemoveFltIfOneShot, bool *pfFilter, bool *pfIsOneShot)
{
    /*
     * Validate input.
     */
    int rc = USBFilterValidate(pDevice);
    AssertRCReturn(rc, VPOXUSBFILTER_CONTEXT_NIL);

    *pfFilter = false;
    if (puId)
        *puId = 0;

    /*
     * Search the lists for a match.
     * (The lists are ordered by priority.)
     */
    VPOXUSBFILTERMGR_LOCK();

    for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
    {
        PVPOXUSBFILTER pPrev = NULL;
        PVPOXUSBFILTER pCur = g_aLists[i].pHead;
        while (pCur)
        {
            if (USBFilterMatch(&pCur->Core, pDevice))
            {
                /*
                 * Take list specific actions and return.
                 *
                 * The code does NOT implement the case where there are two or more
                 * filter clients, and one of them is releasing a device that's
                 * requested by some of the others. It's just too much work for a
                 * situation that noone will encounter.
                 */
                if (puId)
                    *puId = pCur->uHnd;
                VPOXUSBFILTER_CONTEXT Owner = pCur->Owner;
                *pfFilter = !!(i != USBFILTERTYPE_IGNORE
                            && i != USBFILTERTYPE_ONESHOT_IGNORE);

                if (    i == USBFILTERTYPE_ONESHOT_IGNORE
                    ||  i == USBFILTERTYPE_ONESHOT_CAPTURE)
                {
                    if (fRemoveFltIfOneShot)
                    {
                        /* unlink */
                        PVPOXUSBFILTER pNext = pCur->pNext;
                        if (pPrev)
                            pPrev->pNext = pNext;
                        else
                            g_aLists[i].pHead = pNext;
                        if (!pNext)
                            g_aLists[i].pTail = pPrev;
                    }
                }

                VPOXUSBFILTERMGR_UNLOCK();

                if (    i == USBFILTERTYPE_ONESHOT_IGNORE
                    ||  i == USBFILTERTYPE_ONESHOT_CAPTURE)
                {
                    if (fRemoveFltIfOneShot)
                    {
                        void *pv = RTHandleTableFree(g_hHndTableFilters, pCur->uHnd);
                        Assert(pv == pCur); NOREF(pv);
                        vpoxUSBFilterFree(pCur);
                    }
                    if (pfIsOneShot)
                        *pfIsOneShot = true;
                }
                else
                {
                    if (pfIsOneShot)
                        *pfIsOneShot = false;
                }
                return Owner;
            }

            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    VPOXUSBFILTERMGR_UNLOCK();
    return VPOXUSBFILTER_CONTEXT_NIL;
}

/**
 * Match the specified device against the filters.
 *
 * @returns Owner on if matched, VPOXUSBFILTER_CONTEXT_NIL it not matched.
 * @param   pDevice     The device data as a filter structure.
 *                      See USBFilterMatch for how to construct this.
 * @param   puId        Where to store the filter id (optional).
 */
VPOXUSBFILTER_CONTEXT VPoxUSBFilterMatch(PCUSBFILTER pDevice, uintptr_t *puId)
{
    bool fFilter = false;
    VPOXUSBFILTER_CONTEXT Owner = VPoxUSBFilterMatchEx(pDevice, puId,
                                    true, /* remove filter is it's a one-shot*/
                                    &fFilter, NULL /* bool * fIsOneShot */);
    if (fFilter)
    {
        Assert(Owner != VPOXUSBFILTER_CONTEXT_NIL);
        return Owner;
    }
    return VPOXUSBFILTER_CONTEXT_NIL;
}

