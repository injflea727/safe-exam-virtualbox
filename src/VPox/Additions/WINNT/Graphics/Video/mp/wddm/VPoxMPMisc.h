/* $Id: VPoxMPMisc.h $ */
/** @file
 * VPox WDDM Miniport driver
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPMisc_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPMisc_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "../../common/VPoxVideoTools.h"

DECLINLINE(void) vpoxVideoLeDetach(LIST_ENTRY *pList, LIST_ENTRY *pDstList)
{
    if (IsListEmpty(pList))
    {
        InitializeListHead(pDstList);
    }
    else
    {
        *pDstList = *pList;
        Assert(pDstList->Flink->Blink == pList);
        Assert(pDstList->Blink->Flink == pList);
        /* pDstList->Flink & pDstList->Blink point to the "real| entries, never to pList
         * since we've checked IsListEmpty(pList) above */
        pDstList->Flink->Blink = pDstList;
        pDstList->Blink->Flink = pDstList;
        InitializeListHead(pList);
    }
}

typedef uint32_t VPOXWDDM_HANDLE;
#define VPOXWDDM_HANDLE_INVALID 0UL

typedef struct VPOXWDDM_HTABLE
{
    uint32_t cData;
    uint32_t iNext2Search;
    uint32_t cSize;
    PVOID *paData;
} VPOXWDDM_HTABLE, *PVPOXWDDM_HTABLE;

typedef struct VPOXWDDM_HTABLE_ITERATOR
{
    PVPOXWDDM_HTABLE pTbl;
    uint32_t iCur;
    uint32_t cLeft;
} VPOXWDDM_HTABLE_ITERATOR, *PVPOXWDDM_HTABLE_ITERATOR;

VOID vpoxWddmHTableIterInit(PVPOXWDDM_HTABLE pTbl, PVPOXWDDM_HTABLE_ITERATOR pIter);
PVOID vpoxWddmHTableIterNext(PVPOXWDDM_HTABLE_ITERATOR pIter, VPOXWDDM_HANDLE *phHandle);
BOOL vpoxWddmHTableIterHasNext(PVPOXWDDM_HTABLE_ITERATOR pIter);
PVOID vpoxWddmHTableIterRemoveCur(PVPOXWDDM_HTABLE_ITERATOR pIter);
NTSTATUS vpoxWddmHTableCreate(PVPOXWDDM_HTABLE pTbl, uint32_t cSize);
VOID vpoxWddmHTableDestroy(PVPOXWDDM_HTABLE pTbl);
NTSTATUS vpoxWddmHTableRealloc(PVPOXWDDM_HTABLE pTbl, uint32_t cNewSize);
VPOXWDDM_HANDLE vpoxWddmHTablePut(PVPOXWDDM_HTABLE pTbl, PVOID pvData);
PVOID vpoxWddmHTableRemove(PVPOXWDDM_HTABLE pTbl, VPOXWDDM_HANDLE hHandle);
PVOID vpoxWddmHTableGet(PVPOXWDDM_HTABLE pTbl, VPOXWDDM_HANDLE hHandle);

NTSTATUS vpoxWddmRegQueryDisplaySettingsKeyName(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult);
NTSTATUS vpoxWddmRegOpenDisplaySettingsKey(IN PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, OUT PHANDLE phKey);
NTSTATUS vpoxWddmRegDisplaySettingsQueryRelX(HANDLE hKey, int * pResult);
NTSTATUS vpoxWddmRegDisplaySettingsQueryRelY(HANDLE hKey, int * pResult);
NTSTATUS vpoxWddmDisplaySettingsQueryPos(IN PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, POINT * pPos);
void vpoxWddmDisplaySettingsCheckPos(IN PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId);
NTSTATUS vpoxWddmRegQueryVideoGuidString(PVPOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult);

NTSTATUS vpoxWddmRegQueryDrvKeyName(PVPOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult);

NTSTATUS vpoxWddmRegOpenKeyEx(OUT PHANDLE phKey, IN HANDLE hRootKey, IN PWCHAR pName, IN ACCESS_MASK fAccess);
NTSTATUS vpoxWddmRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess);
NTSTATUS vpoxWddmRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PDWORD pDword);
NTSTATUS vpoxWddmRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, IN DWORD val);

NTSTATUS vpoxWddmRegDrvFlagsSet(PVPOXMP_DEVEXT pDevExt, DWORD fVal);
DWORD vpoxWddmRegDrvFlagsGet(PVPOXMP_DEVEXT pDevExt, DWORD fDefault);

UNICODE_STRING* vpoxWddmVGuidGet(PVPOXMP_DEVEXT pDevExt);
VOID vpoxWddmVGuidFree(PVPOXMP_DEVEXT pDevExt);

#define VPOXWDDM_MM_VOID 0xffffffffUL

typedef struct VPOXWDDM_MM
{
    RTL_BITMAP BitMap;
    UINT cPages;
    UINT cAllocs;
    PULONG pBuffer;
} VPOXWDDM_MM, *PVPOXWDDM_MM;

NTSTATUS vpoxMmInit(PVPOXWDDM_MM pMm, UINT cPages);
ULONG vpoxMmAlloc(PVPOXWDDM_MM pMm, UINT cPages);
VOID vpoxMmFree(PVPOXWDDM_MM pMm, UINT iPage, UINT cPages);
NTSTATUS vpoxMmTerm(PVPOXWDDM_MM pMm);

typedef struct VPOXVIDEOCM_ALLOC_MGR
{
    /* synch lock */
    FAST_MUTEX Mutex;
    VPOXWDDM_HTABLE AllocTable;
    VPOXWDDM_MM Mm;
//    PHYSICAL_ADDRESS PhData;
    uint8_t *pvData;
    uint32_t offData;
    uint32_t cbData;
} VPOXVIDEOCM_ALLOC_MGR, *PVPOXVIDEOCM_ALLOC_MGR;

typedef struct VPOXVIDEOCM_ALLOC_CONTEXT
{
    PVPOXVIDEOCM_ALLOC_MGR pMgr;
    /* synch lock */
    FAST_MUTEX Mutex;
    VPOXWDDM_HTABLE AllocTable;
} VPOXVIDEOCM_ALLOC_CONTEXT, *PVPOXVIDEOCM_ALLOC_CONTEXT;

NTSTATUS vpoxVideoAMgrCreate(PVPOXMP_DEVEXT pDevExt, PVPOXVIDEOCM_ALLOC_MGR pMgr, uint32_t offData, uint32_t cbData);
NTSTATUS vpoxVideoAMgrDestroy(PVPOXMP_DEVEXT pDevExt, PVPOXVIDEOCM_ALLOC_MGR pMgr);

NTSTATUS vpoxVideoAMgrCtxCreate(PVPOXVIDEOCM_ALLOC_MGR pMgr, PVPOXVIDEOCM_ALLOC_CONTEXT pCtx);
NTSTATUS vpoxVideoAMgrCtxDestroy(PVPOXVIDEOCM_ALLOC_CONTEXT pCtx);

NTSTATUS vpoxVideoAMgrCtxAllocCreate(PVPOXVIDEOCM_ALLOC_CONTEXT pContext, PVPOXVIDEOCM_UM_ALLOC pUmAlloc);
NTSTATUS vpoxVideoAMgrCtxAllocDestroy(PVPOXVIDEOCM_ALLOC_CONTEXT pContext, VPOXDISP_KMHANDLE hSesionHandle);

VOID vpoxWddmSleep(uint32_t u32Val);
VOID vpoxWddmCounterU32Wait(uint32_t volatile * pu32, uint32_t u32Val);

NTSTATUS vpoxUmdDumpBuf(PVPOXDISPIFESCAPE_DBGDUMPBUF pBuf, uint32_t cbBuffer);

#if 0
/* wine shrc handle -> allocation map */
VOID vpoxShRcTreeInit(PVPOXMP_DEVEXT pDevExt);
VOID vpoxShRcTreeTerm(PVPOXMP_DEVEXT pDevExt);
BOOLEAN vpoxShRcTreePut(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pAlloc);
PVPOXWDDM_ALLOCATION vpoxShRcTreeGet(PVPOXMP_DEVEXT pDevExt, HANDLE hSharedRc);
BOOLEAN vpoxShRcTreeRemove(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOCATION pAlloc);
#endif

NTSTATUS vpoxWddmDrvCfgInit(PUNICODE_STRING pRegStr);

NTSTATUS VPoxWddmSlEnableVSyncNotification(PVPOXMP_DEVEXT pDevExt, BOOLEAN fEnable);
NTSTATUS VPoxWddmSlGetScanLine(PVPOXMP_DEVEXT pDevExt, DXGKARG_GETSCANLINE *pSl);
NTSTATUS VPoxWddmSlInit(PVPOXMP_DEVEXT pDevExt);
NTSTATUS VPoxWddmSlTerm(PVPOXMP_DEVEXT pDevExt);

void vpoxWddmDiInitDefault(DXGK_DISPLAY_INFORMATION *pInfo, PHYSICAL_ADDRESS PhAddr, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId);
void vpoxWddmDiToAllocData(PVPOXMP_DEVEXT pDevExt, const DXGK_DISPLAY_INFORMATION *pInfo, struct VPOXWDDM_ALLOC_DATA *pAllocData);
void vpoxWddmDmSetupDefaultVramLocation(PVPOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID ModifiedVidPnSourceId, struct VPOXWDDM_SOURCE *paSources);

char const *vpoxWddmAllocTypeString(PVPOXWDDM_ALLOCATION pAlloc);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPMisc_h */
