/* $Id: vpoxext.h $ */
/** @file
 * VPox extension to Wine D3D
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
 */

#ifndef VPOX_INCLUDED_SRC_Graphics_shaderlib_vpoxext_h
#define VPOX_INCLUDED_SRC_Graphics_shaderlib_vpoxext_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VPOX_WINE_WITHOUT_LIBWINE
# ifdef _MSC_VER
#  include <iprt/win/windows.h>
# else
#  include <windows.h>
# endif
#endif

#include <iprt/list.h>

HRESULT VPoxExtCheckInit(void);
HRESULT VPoxExtCheckTerm(void);
#if defined(VPOX_WINE_WITH_SINGLE_CONTEXT) || defined(VPOX_WINE_WITH_SINGLE_SWAPCHAIN_CONTEXT)
# ifndef VPOX_WITH_WDDM
/* Windows destroys HDC created by a given thread when the thread is terminated
 * this leads to a mess-up in Wine & Chromium code in some situations, e.g.
 * D3D device is created in one thread, then the thread is terminated,
 * then device is started to be used in another thread */
HDC VPoxExtGetDC(HWND hWnd);
int VPoxExtReleaseDC(HWND hWnd, HDC hDC);
# endif
/* We need to do a VPoxTlsRefRelease for the current thread context on thread exit to avoid memory leaking
 * Calling VPoxTlsRefRelease may result in a call to context dtor callback, which is supposed to be run under wined3d lock.
 * We can not acquire a wined3d lock in DllMain since this would result in a lock order violation, which may result in a deadlock.
 * In other words, wined3d may internally call Win32 API functions which result in a DLL lock acquisition while holding wined3d lock.
 * So lock order should always be "wined3d lock" -> "dll lock".
 * To avoid possible deadlocks we make an asynchronous call to a worker thread to make a context release from there. */
struct wined3d_context;
void VPoxExtReleaseContextAsync(struct wined3d_context *context);
#endif

/* API for creating & destroying windows */
HRESULT VPoxExtWndDestroy(HWND hWnd, HDC hDC);
HRESULT VPoxExtWndCreate(DWORD width, DWORD height, HWND *phWnd, HDC *phDC);


/* hashmap */
typedef DECLCALLBACK(uint32_t) FNVPOXEXT_HASHMAP_HASH(void *pvKey);
typedef FNVPOXEXT_HASHMAP_HASH *PFNVPOXEXT_HASHMAP_HASH;

typedef DECLCALLBACK(bool) FNVPOXEXT_HASHMAP_EQUAL(void *pvKey1, void *pvKey2);
typedef FNVPOXEXT_HASHMAP_EQUAL *PFNVPOXEXT_HASHMAP_EQUAL;

struct VPOXEXT_HASHMAP;
struct VPOXEXT_HASHMAP_ENTRY;
typedef DECLCALLBACK(bool) FNVPOXEXT_HASHMAP_VISITOR(struct VPOXEXT_HASHMAP *pMap, void *pvKey, struct VPOXEXT_HASHMAP_ENTRY *pValue, void *pvVisitor);
typedef FNVPOXEXT_HASHMAP_VISITOR *PFNVPOXEXT_HASHMAP_VISITOR;

typedef struct VPOXEXT_HASHMAP_ENTRY
{
    RTLISTNODE ListNode;
    void *pvKey;
    uint32_t u32Hash;
} VPOXEXT_HASHMAP_ENTRY, *PVPOXEXT_HASHMAP_ENTRY;

typedef struct VPOXEXT_HASHMAP_BUCKET
{
    RTLISTNODE EntryList;
} VPOXEXT_HASHMAP_BUCKET, *PVPOXEXT_HASHMAP_BUCKET;

#define VPOXEXT_HASHMAP_NUM_BUCKETS 29

typedef struct VPOXEXT_HASHMAP
{
    PFNVPOXEXT_HASHMAP_HASH pfnHash;
    PFNVPOXEXT_HASHMAP_EQUAL pfnEqual;
    uint32_t cEntries;
    VPOXEXT_HASHMAP_BUCKET aBuckets[VPOXEXT_HASHMAP_NUM_BUCKETS];
} VPOXEXT_HASHMAP, *PVPOXEXT_HASHMAP;

void VPoxExtHashInit(PVPOXEXT_HASHMAP pMap, PFNVPOXEXT_HASHMAP_HASH pfnHash, PFNVPOXEXT_HASHMAP_EQUAL pfnEqual);
PVPOXEXT_HASHMAP_ENTRY VPoxExtHashPut(PVPOXEXT_HASHMAP pMap, void *pvKey, PVPOXEXT_HASHMAP_ENTRY pEntry);
PVPOXEXT_HASHMAP_ENTRY VPoxExtHashGet(PVPOXEXT_HASHMAP pMap, void *pvKey);
PVPOXEXT_HASHMAP_ENTRY VPoxExtHashRemove(PVPOXEXT_HASHMAP pMap, void *pvKey);
void* VPoxExtHashRemoveEntry(PVPOXEXT_HASHMAP pMap, PVPOXEXT_HASHMAP_ENTRY pEntry);
void VPoxExtHashVisit(PVPOXEXT_HASHMAP pMap, PFNVPOXEXT_HASHMAP_VISITOR pfnVisitor, void *pvVisitor);
void VPoxExtHashCleanup(PVPOXEXT_HASHMAP pMap, PFNVPOXEXT_HASHMAP_VISITOR pfnVisitor, void *pvVisitor);

DECLINLINE(uint32_t) VPoxExtHashSize(PVPOXEXT_HASHMAP pMap)
{
    return pMap->cEntries;
}

DECLINLINE(void*) VPoxExtHashEntryKey(PVPOXEXT_HASHMAP_ENTRY pEntry)
{
    return pEntry->pvKey;
}

struct VPOXEXT_HASHCACHE_ENTRY;
typedef DECLCALLBACK(void) FNVPOXEXT_HASHCACHE_CLEANUP_ENTRY(void *pvKey, struct VPOXEXT_HASHCACHE_ENTRY *pEntry);
typedef FNVPOXEXT_HASHCACHE_CLEANUP_ENTRY *PFNVPOXEXT_HASHCACHE_CLEANUP_ENTRY;

typedef struct VPOXEXT_HASHCACHE_ENTRY
{
    VPOXEXT_HASHMAP_ENTRY MapEntry;
    uint32_t u32Usage;
} VPOXEXT_HASHCACHE_ENTRY, *PVPOXEXT_HASHCACHE_ENTRY;

typedef struct VPOXEXT_HASHCACHE
{
    VPOXEXT_HASHMAP Map;
    uint32_t cMaxElements;
    PFNVPOXEXT_HASHCACHE_CLEANUP_ENTRY pfnCleanupEntry;
} VPOXEXT_HASHCACHE, *PVPOXEXT_HASHCACHE;

#define VPOXEXT_HASHCACHE_FROM_MAP(_pMap) RT_FROM_MEMBER((_pMap), VPOXEXT_HASHCACHE, Map)
#define VPOXEXT_HASHCACHE_ENTRY_FROM_MAP(_pEntry) RT_FROM_MEMBER((_pEntry), VPOXEXT_HASHCACHE_ENTRY, MapEntry)

DECLINLINE(void) VPoxExtCacheInit(PVPOXEXT_HASHCACHE pCache, uint32_t cMaxElements,
        PFNVPOXEXT_HASHMAP_HASH pfnHash,
        PFNVPOXEXT_HASHMAP_EQUAL pfnEqual,
        PFNVPOXEXT_HASHCACHE_CLEANUP_ENTRY pfnCleanupEntry)
{
    VPoxExtHashInit(&pCache->Map, pfnHash, pfnEqual);
    pCache->cMaxElements = cMaxElements;
    pCache->pfnCleanupEntry = pfnCleanupEntry;
}

DECLINLINE(PVPOXEXT_HASHCACHE_ENTRY) VPoxExtCacheGet(PVPOXEXT_HASHCACHE pCache, void *pvKey)
{
    PVPOXEXT_HASHMAP_ENTRY pEntry = VPoxExtHashRemove(&pCache->Map, pvKey);
    return VPOXEXT_HASHCACHE_ENTRY_FROM_MAP(pEntry);
}

DECLINLINE(void) VPoxExtCachePut(PVPOXEXT_HASHCACHE pCache, void *pvKey, PVPOXEXT_HASHCACHE_ENTRY pEntry)
{
    PVPOXEXT_HASHMAP_ENTRY pOldEntry = VPoxExtHashPut(&pCache->Map, pvKey, &pEntry->MapEntry);
    PVPOXEXT_HASHCACHE_ENTRY pOld;
    if (!pOldEntry)
        return;
    pOld = VPOXEXT_HASHCACHE_ENTRY_FROM_MAP(pOldEntry);
    if (pOld != pEntry)
        pCache->pfnCleanupEntry(pvKey, pOld);
}

void VPoxExtCacheCleanup(PVPOXEXT_HASHCACHE pCache);

DECLINLINE(void) VPoxExtCacheTerm(PVPOXEXT_HASHCACHE pCache)
{
    VPoxExtCacheCleanup(pCache);
}

#endif /* !VPOX_INCLUDED_SRC_Graphics_shaderlib_vpoxext_h */

