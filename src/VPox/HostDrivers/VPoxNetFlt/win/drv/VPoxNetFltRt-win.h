/* $Id: VPoxNetFltRt-win.h $ */
/** @file
 * VPoxNetFltRt-win.h - Bridged Networking Driver, Windows Specific Code.
 * NetFlt Runtime API
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltRt_win_h
#define VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltRt_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif
DECLHIDDEN(VOID) vpoxNetFltWinUnload(IN PDRIVER_OBJECT DriverObject);

#ifndef VPOXNETADP
# if !defined(VPOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS)
DECLHIDDEN(bool) vpoxNetFltWinMatchPackets(PNDIS_PACKET pPacket1, PNDIS_PACKET pPacket2, const INT cbMatch);
DECLHIDDEN(bool) vpoxNetFltWinMatchPacketAndSG(PNDIS_PACKET pPacket, PINTNETSG pSG, const INT cbMatch);
# endif
#endif

/*************************
 * packet queue API      *
 *************************/


#define LIST_ENTRY_2_PACKET_INFO(pListEntry) \
    ( (PVPOXNETFLT_PACKET_INFO)((uint8_t *)(pListEntry) - RT_UOFFSETOF(VPOXNETFLT_PACKET_INFO, ListEntry)) )

#if !defined(VPOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS)

#define VPOX_SLE_2_PKTRSVD_PT(_pEntry) \
    ( (PVPOXNETFLT_PKTRSVD_PT)((uint8_t *)(_pEntry) - RT_UOFFSETOF(VPOXNETFLT_PKTRSVD_PT, ListEntry)) )

#define VPOX_SLE_2_SENDPACKET(_pEntry) \
    ( (PNDIS_PACKET)((uint8_t *)(VPOX_SLE_2_PKTRSVD_PT(_pEntry)) - RT_UOFFSETOF(NDIS_PACKET, ProtocolReserved)) )

#endif
/**
 * enqueus the packet info to the tail of the queue
 */
DECLINLINE(void) vpoxNetFltWinQuEnqueueTail(PVPOXNETFLT_PACKET_QUEUE pQueue, PVPOXNETFLT_PACKET_INFO pPacketInfo)
{
    InsertTailList(pQueue, &pPacketInfo->ListEntry);
}

DECLINLINE(void) vpoxNetFltWinQuEnqueueHead(PVPOXNETFLT_PACKET_QUEUE pQueue, PVPOXNETFLT_PACKET_INFO pPacketInfo)
{
    Assert(pPacketInfo->pPool);
    InsertHeadList(pQueue, &pPacketInfo->ListEntry);
}

/**
 * enqueus the packet info to the tail of the queue
 */
DECLINLINE(void) vpoxNetFltWinQuInterlockedEnqueueTail(PVPOXNETFLT_INTERLOCKED_PACKET_QUEUE pQueue, PVPOXNETFLT_PACKET_INFO pPacketInfo)
{
    Assert(pPacketInfo->pPool);
    NdisAcquireSpinLock(&pQueue->Lock);
    vpoxNetFltWinQuEnqueueTail(&pQueue->Queue, pPacketInfo);
    NdisReleaseSpinLock(&pQueue->Lock);
}

DECLINLINE(void) vpoxNetFltWinQuInterlockedEnqueueHead(PVPOXNETFLT_INTERLOCKED_PACKET_QUEUE pQueue, PVPOXNETFLT_PACKET_INFO pPacketInfo)
{
    NdisAcquireSpinLock(&pQueue->Lock);
    vpoxNetFltWinQuEnqueueHead(&pQueue->Queue, pPacketInfo);
    NdisReleaseSpinLock(&pQueue->Lock);
}

/**
 * dequeus the packet info from the head of the queue
 */
DECLINLINE(PVPOXNETFLT_PACKET_INFO) vpoxNetFltWinQuDequeueHead(PVPOXNETFLT_PACKET_QUEUE pQueue)
{
    PLIST_ENTRY pListEntry = RemoveHeadList(pQueue);
    if (pListEntry != pQueue)
    {
        PVPOXNETFLT_PACKET_INFO pInfo = LIST_ENTRY_2_PACKET_INFO(pListEntry);
        Assert(pInfo->pPool);
        return pInfo;
    }
    return NULL;
}

DECLINLINE(PVPOXNETFLT_PACKET_INFO) vpoxNetFltWinQuDequeueTail(PVPOXNETFLT_PACKET_QUEUE pQueue)
{
    PLIST_ENTRY pListEntry = RemoveTailList(pQueue);
    if (pListEntry != pQueue)
    {
        PVPOXNETFLT_PACKET_INFO pInfo = LIST_ENTRY_2_PACKET_INFO(pListEntry);
        Assert(pInfo->pPool);
        return pInfo;
    }
    return NULL;
}

DECLINLINE(PVPOXNETFLT_PACKET_INFO) vpoxNetFltWinQuInterlockedDequeueHead(PVPOXNETFLT_INTERLOCKED_PACKET_QUEUE pInterlockedQueue)
{
    PVPOXNETFLT_PACKET_INFO pInfo;
    NdisAcquireSpinLock(&pInterlockedQueue->Lock);
    pInfo = vpoxNetFltWinQuDequeueHead(&pInterlockedQueue->Queue);
    NdisReleaseSpinLock(&pInterlockedQueue->Lock);
    return pInfo;
}

DECLINLINE(PVPOXNETFLT_PACKET_INFO) vpoxNetFltWinQuInterlockedDequeueTail(PVPOXNETFLT_INTERLOCKED_PACKET_QUEUE pInterlockedQueue)
{
    PVPOXNETFLT_PACKET_INFO pInfo;
    NdisAcquireSpinLock(&pInterlockedQueue->Lock);
    pInfo = vpoxNetFltWinQuDequeueTail(&pInterlockedQueue->Queue);
    NdisReleaseSpinLock(&pInterlockedQueue->Lock);
    return pInfo;
}

DECLINLINE(void) vpoxNetFltWinQuDequeue(PVPOXNETFLT_PACKET_INFO pInfo)
{
    RemoveEntryList(&pInfo->ListEntry);
}

DECLINLINE(void) vpoxNetFltWinQuInterlockedDequeue(PVPOXNETFLT_INTERLOCKED_PACKET_QUEUE pInterlockedQueue, PVPOXNETFLT_PACKET_INFO pInfo)
{
    NdisAcquireSpinLock(&pInterlockedQueue->Lock);
    vpoxNetFltWinQuDequeue(pInfo);
    NdisReleaseSpinLock(&pInterlockedQueue->Lock);
}

/**
 * allocates the packet info from the pool
 */
DECLINLINE(PVPOXNETFLT_PACKET_INFO) vpoxNetFltWinPpAllocPacketInfo(PVPOXNETFLT_PACKET_INFO_POOL pPool)
{
    return vpoxNetFltWinQuInterlockedDequeueHead(&pPool->Queue);
}

/**
 * returns the packet info to the pool
 */
DECLINLINE(void) vpoxNetFltWinPpFreePacketInfo(PVPOXNETFLT_PACKET_INFO pInfo)
{
    PVPOXNETFLT_PACKET_INFO_POOL pPool = pInfo->pPool;
    vpoxNetFltWinQuInterlockedEnqueueHead(&pPool->Queue, pInfo);
}

/** initializes the packet queue */
#define INIT_PACKET_QUEUE(_pQueue) InitializeListHead((_pQueue))

/** initializes the packet queue */
#define INIT_INTERLOCKED_PACKET_QUEUE(_pQueue) \
    { \
        INIT_PACKET_QUEUE(&(_pQueue)->Queue); \
        NdisAllocateSpinLock(&(_pQueue)->Lock); \
    }

/** delete the packet queue */
#define FINI_INTERLOCKED_PACKET_QUEUE(_pQueue) NdisFreeSpinLock(&(_pQueue)->Lock)

/** returns the packet the packet info contains */
#define GET_PACKET_FROM_INFO(_pPacketInfo) (ASMAtomicUoReadPtr((void * volatile *)&(_pPacketInfo)->pPacket))

/** assignes the packet to the packet info */
#define SET_PACKET_TO_INFO(_pPacketInfo, _pPacket) (ASMAtomicUoWritePtr(&(_pPacketInfo)->pPacket, (_pPacket)))

/** returns the flags the packet info contains */
#define GET_FLAGS_FROM_INFO(_pPacketInfo) (ASMAtomicUoReadU32((volatile uint32_t *)&(_pPacketInfo)->fFlags))

/** sets flags to the packet info */
#define SET_FLAGS_TO_INFO(_pPacketInfo, _fFlags) (ASMAtomicUoWriteU32((volatile uint32_t *)&(_pPacketInfo)->fFlags, (_fFlags)))

#ifdef VPOXNETFLT_NO_PACKET_QUEUE
DECLHIDDEN(bool) vpoxNetFltWinPostIntnet(PVPOXNETFLTINS pInstance, PVOID pvPacket, const UINT fFlags);
#else
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinQuEnqueuePacket(PVPOXNETFLTINS pInstance, PVOID pPacket, const UINT fPacketFlags);
DECLHIDDEN(void) vpoxNetFltWinQuFiniPacketQueue(PVPOXNETFLTINS pInstance);
DECLHIDDEN(NTSTATUS) vpoxNetFltWinQuInitPacketQueue(PVPOXNETFLTINS pInstance);
#endif /* #ifndef VPOXNETFLT_NO_PACKET_QUEUE */


#ifndef VPOXNETADP
/**
 * searches the list entry in a single-linked list
 */
DECLINLINE(bool) vpoxNetFltWinSearchListEntry(PVPOXNETFLT_SINGLE_LIST pList, PSINGLE_LIST_ENTRY pEntry2Search, bool bRemove)
{
    PSINGLE_LIST_ENTRY pHead = &pList->Head;
    PSINGLE_LIST_ENTRY pCur;
    PSINGLE_LIST_ENTRY pPrev;
    for (pCur = pHead->Next, pPrev = pHead; pCur; pPrev = pCur, pCur = pCur->Next)
    {
        if (pEntry2Search == pCur)
        {
            if (bRemove)
            {
                pPrev->Next = pCur->Next;
                if (pCur == pList->pTail)
                {
                    pList->pTail = pPrev;
                }
            }
            return true;
        }
    }
    return false;
}

#if !defined(VPOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS)

DECLINLINE(PNDIS_PACKET) vpoxNetFltWinSearchPacket(PVPOXNETFLT_SINGLE_LIST pList, PNDIS_PACKET pPacket2Search, int cbMatch, bool bRemove)
{
    PSINGLE_LIST_ENTRY pHead = &pList->Head;
    PSINGLE_LIST_ENTRY pCur;
    PSINGLE_LIST_ENTRY pPrev;
    PNDIS_PACKET pCurPacket;
    for (pCur = pHead->Next, pPrev = pHead; pCur; pPrev = pCur, pCur = pCur->Next)
    {
        pCurPacket = VPOX_SLE_2_SENDPACKET(pCur);
        if (pCurPacket == pPacket2Search || vpoxNetFltWinMatchPackets(pPacket2Search, pCurPacket, cbMatch))
        {
            if (bRemove)
            {
                pPrev->Next = pCur->Next;
                if (pCur == pList->pTail)
                {
                    pList->pTail = pPrev;
                }
            }
            return pCurPacket;
        }
    }
    return NULL;
}

DECLINLINE(PNDIS_PACKET) vpoxNetFltWinSearchPacketBySG(PVPOXNETFLT_SINGLE_LIST pList, PINTNETSG pSG, int cbMatch, bool bRemove)
{
    PSINGLE_LIST_ENTRY pHead = &pList->Head;
    PSINGLE_LIST_ENTRY pCur;
    PSINGLE_LIST_ENTRY pPrev;
    PNDIS_PACKET pCurPacket;
    for (pCur = pHead->Next, pPrev = pHead; pCur; pPrev = pCur, pCur = pCur->Next)
    {
        pCurPacket = VPOX_SLE_2_SENDPACKET(pCur);
        if (vpoxNetFltWinMatchPacketAndSG(pCurPacket, pSG, cbMatch))
        {
            if (bRemove)
            {
                pPrev->Next = pCur->Next;
                if (pCur == pList->pTail)
                {
                    pList->pTail = pPrev;
                }
            }
            return pCurPacket;
        }
    }
    return NULL;
}

#endif /* #if !defined(VPOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS) */

DECLINLINE(bool) vpoxNetFltWinSListIsEmpty(PVPOXNETFLT_SINGLE_LIST pList)
{
    return !pList->Head.Next;
}

DECLINLINE(void) vpoxNetFltWinPutTail(PVPOXNETFLT_SINGLE_LIST pList, PSINGLE_LIST_ENTRY pEntry)
{
    pList->pTail->Next = pEntry;
    pList->pTail = pEntry;
    pEntry->Next = NULL;
}

DECLINLINE(void) vpoxNetFltWinPutHead(PVPOXNETFLT_SINGLE_LIST pList, PSINGLE_LIST_ENTRY pEntry)
{
    pEntry->Next = pList->Head.Next;
    pList->Head.Next = pEntry;
    if (!pEntry->Next)
        pList->pTail = pEntry;
}

DECLINLINE(PSINGLE_LIST_ENTRY) vpoxNetFltWinGetHead(PVPOXNETFLT_SINGLE_LIST pList)
{
    PSINGLE_LIST_ENTRY pEntry = pList->Head.Next;
    if (pEntry && pEntry == pList->pTail)
    {
        pList->Head.Next = NULL;
        pList->pTail = &pList->Head;
    }
    return pEntry;
}

DECLINLINE(bool) vpoxNetFltWinInterlockedSearchListEntry(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PSINGLE_LIST_ENTRY pEntry2Search, bool bRemove)
{
    bool bFound;
    NdisAcquireSpinLock(&pList->Lock);
    bFound = vpoxNetFltWinSearchListEntry(&pList->List, pEntry2Search, bRemove);
    NdisReleaseSpinLock(&pList->Lock);
    return bFound;
}

#if !defined(VPOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS)

DECLINLINE(PNDIS_PACKET) vpoxNetFltWinInterlockedSearchPacket(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PNDIS_PACKET pPacket2Search, int cbMatch, bool bRemove)
{
    PNDIS_PACKET pFound;
    NdisAcquireSpinLock(&pList->Lock);
    pFound = vpoxNetFltWinSearchPacket(&pList->List, pPacket2Search, cbMatch, bRemove);
    NdisReleaseSpinLock(&pList->Lock);
    return pFound;
}

DECLINLINE(PNDIS_PACKET) vpoxNetFltWinInterlockedSearchPacketBySG(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PINTNETSG pSG, int cbMatch, bool bRemove)
{
    PNDIS_PACKET pFound;
    NdisAcquireSpinLock(&pList->Lock);
    pFound = vpoxNetFltWinSearchPacketBySG(&pList->List, pSG, cbMatch, bRemove);
    NdisReleaseSpinLock(&pList->Lock);
    return pFound;
}
#endif /* #if !defined(VPOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS) */

DECLINLINE(void) vpoxNetFltWinInterlockedPutTail(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PSINGLE_LIST_ENTRY pEntry)
{
    NdisAcquireSpinLock(&pList->Lock);
    vpoxNetFltWinPutTail(&pList->List, pEntry);
    NdisReleaseSpinLock(&pList->Lock);
}

DECLINLINE(void) vpoxNetFltWinInterlockedPutHead(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PSINGLE_LIST_ENTRY pEntry)
{
    NdisAcquireSpinLock(&pList->Lock);
    vpoxNetFltWinPutHead(&pList->List, pEntry);
    NdisReleaseSpinLock(&pList->Lock);
}

DECLINLINE(PSINGLE_LIST_ENTRY) vpoxNetFltWinInterlockedGetHead(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList)
{
    PSINGLE_LIST_ENTRY pEntry;
    NdisAcquireSpinLock(&pList->Lock);
    pEntry = vpoxNetFltWinGetHead(&pList->List);
    NdisReleaseSpinLock(&pList->Lock);
    return pEntry;
}

# if defined(DEBUG_NETFLT_PACKETS) || !defined(VPOX_LOOPBACK_USEFLAGS)
DECLINLINE(void) vpoxNetFltWinLbPutSendPacket(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket, bool bFromIntNet)
{
    PVPOXNETFLT_PKTRSVD_PT pSrv = (PVPOXNETFLT_PKTRSVD_PT)pPacket->ProtocolReserved;
    pSrv->bFromIntNet = bFromIntNet;
    vpoxNetFltWinInterlockedPutHead(&pNetFlt->u.s.WinIf.SendPacketQueue, &pSrv->ListEntry);
}

DECLINLINE(bool) vpoxNetFltWinLbIsFromIntNet(PNDIS_PACKET pPacket)
{
    PVPOXNETFLT_PKTRSVD_PT pSrv = (PVPOXNETFLT_PKTRSVD_PT)pPacket->ProtocolReserved;
    return pSrv->bFromIntNet;
}

DECLINLINE(PNDIS_PACKET) vpoxNetFltWinLbSearchLoopBack(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket, bool bRemove)
{
    return vpoxNetFltWinInterlockedSearchPacket(&pNetFlt->u.s.WinIf.SendPacketQueue, pPacket, VPOXNETFLT_PACKETMATCH_LENGTH, bRemove);
}

DECLINLINE(PNDIS_PACKET) vpoxNetFltWinLbSearchLoopBackBySG(PVPOXNETFLTINS pNetFlt, PINTNETSG pSG, bool bRemove)
{
    return vpoxNetFltWinInterlockedSearchPacketBySG(&pNetFlt->u.s.WinIf.SendPacketQueue, pSG, VPOXNETFLT_PACKETMATCH_LENGTH, bRemove);
}

DECLINLINE(bool) vpoxNetFltWinLbRemoveSendPacket(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket)
{
    PVPOXNETFLT_PKTRSVD_PT pSrv = (PVPOXNETFLT_PKTRSVD_PT)pPacket->ProtocolReserved;
    bool bRet = vpoxNetFltWinInterlockedSearchListEntry(&pNetFlt->u.s.WinIf.SendPacketQueue, &pSrv->ListEntry, true);
#ifdef DEBUG_misha
    Assert(bRet == (pNetFlt->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE));
#endif
    return bRet;
}

# endif

#endif

#ifdef DEBUG_misha
DECLHIDDEN(bool) vpoxNetFltWinCheckMACs(PNDIS_PACKET pPacket, PRTMAC pDst, PRTMAC pSrc);
DECLHIDDEN(bool) vpoxNetFltWinCheckMACsSG(PINTNETSG pSG, PRTMAC pDst, PRTMAC pSrc);
extern RTMAC g_vpoxNetFltWinVerifyMACBroadcast;
extern RTMAC g_vpoxNetFltWinVerifyMACGuest;

# define VPOXNETFLT_LBVERIFY(_pnf, _p) \
    do { \
        Assert(!vpoxNetFltWinCheckMACs(_p, NULL, &g_vpoxNetFltWinVerifyMACGuest)); \
        Assert(!vpoxNetFltWinCheckMACs(_p, NULL, &(_pnf)->u.s.MacAddr)); \
    } while (0)

# define VPOXNETFLT_LBVERIFYSG(_pnf, _p) \
    do { \
        Assert(!vpoxNetFltWinCheckMACsSG(_p, NULL, &g_vpoxNetFltWinVerifyMACGuest)); \
        Assert(!vpoxNetFltWinCheckMACsSG(_p, NULL, &(_pnf)->u.s.MacAddr)); \
    } while (0)

#else
# define VPOXNETFLT_LBVERIFY(_pnf, _p) do { } while (0)
# define VPOXNETFLT_LBVERIFYSG(_pnf, _p) do { } while (0)
#endif

/** initializes the list */
#define INIT_SINGLE_LIST(_pList) \
    { \
        (_pList)->Head.Next = NULL; \
        (_pList)->pTail = &(_pList)->Head; \
    }

/** initializes the list */
#define INIT_INTERLOCKED_SINGLE_LIST(_pList) \
    do { \
        INIT_SINGLE_LIST(&(_pList)->List); \
        NdisAllocateSpinLock(&(_pList)->Lock); \
    } while (0)

/** delete the packet queue */
#define FINI_INTERLOCKED_SINGLE_LIST(_pList) \
    do { \
        Assert(vpoxNetFltWinSListIsEmpty(&(_pList)->List)); \
        NdisFreeSpinLock(&(_pList)->Lock) \
    } while (0)


/**************************************************************************
 * PVPOXNETFLTINS , WinIf reference/dereference (i.e. retain/release) API *
 **************************************************************************/


DECLHIDDEN(void) vpoxNetFltWinWaitDereference(PVPOXNETFLT_WINIF_DEVICE pState);

DECLINLINE(void) vpoxNetFltWinReferenceModeNetFlt(PVPOXNETFLTINS pIns)
{
    ASMAtomicIncU32((volatile uint32_t *)&pIns->u.s.cModeNetFltRefs);
}

DECLINLINE(void) vpoxNetFltWinReferenceModePassThru(PVPOXNETFLTINS pIns)
{
    ASMAtomicIncU32((volatile uint32_t *)&pIns->u.s.cModePassThruRefs);
}

DECLINLINE(void) vpoxNetFltWinIncReferenceModeNetFlt(PVPOXNETFLTINS pIns, uint32_t v)
{
    ASMAtomicAddU32((volatile uint32_t *)&pIns->u.s.cModeNetFltRefs, v);
}

DECLINLINE(void) vpoxNetFltWinIncReferenceModePassThru(PVPOXNETFLTINS pIns, uint32_t v)
{
    ASMAtomicAddU32((volatile uint32_t *)&pIns->u.s.cModePassThruRefs, v);
}

DECLINLINE(void) vpoxNetFltWinDereferenceModeNetFlt(PVPOXNETFLTINS pIns)
{
    ASMAtomicDecU32((volatile uint32_t *)&pIns->u.s.cModeNetFltRefs);
}

DECLINLINE(void) vpoxNetFltWinDereferenceModePassThru(PVPOXNETFLTINS pIns)
{
    ASMAtomicDecU32((volatile uint32_t *)&pIns->u.s.cModePassThruRefs);
}

DECLINLINE(void) vpoxNetFltWinDecReferenceModeNetFlt(PVPOXNETFLTINS pIns, uint32_t v)
{
    Assert(v);
    ASMAtomicAddU32((volatile uint32_t *)&pIns->u.s.cModeNetFltRefs, (uint32_t)(-((int32_t)v)));
}

DECLINLINE(void) vpoxNetFltWinDecReferenceModePassThru(PVPOXNETFLTINS pIns, uint32_t v)
{
    Assert(v);
    ASMAtomicAddU32((volatile uint32_t *)&pIns->u.s.cModePassThruRefs, (uint32_t)(-((int32_t)v)));
}

DECLINLINE(void) vpoxNetFltWinSetPowerState(PVPOXNETFLT_WINIF_DEVICE pState, NDIS_DEVICE_POWER_STATE State)
{
    ASMAtomicUoWriteU32((volatile uint32_t *)&pState->PowerState, State);
}

DECLINLINE(NDIS_DEVICE_POWER_STATE) vpoxNetFltWinGetPowerState(PVPOXNETFLT_WINIF_DEVICE pState)
{
    return (NDIS_DEVICE_POWER_STATE)ASMAtomicUoReadU32((volatile uint32_t *)&pState->PowerState);
}

DECLINLINE(void) vpoxNetFltWinSetOpState(PVPOXNETFLT_WINIF_DEVICE pState, VPOXNETDEVOPSTATE State)
{
    ASMAtomicUoWriteU32((volatile uint32_t *)&pState->OpState, State);
}

DECLINLINE(VPOXNETDEVOPSTATE) vpoxNetFltWinGetOpState(PVPOXNETFLT_WINIF_DEVICE pState)
{
    return (VPOXNETDEVOPSTATE)ASMAtomicUoReadU32((volatile uint32_t *)&pState->OpState);
}

DECLINLINE(bool) vpoxNetFltWinDoReferenceDevice(PVPOXNETFLT_WINIF_DEVICE pState)
{
    if (vpoxNetFltWinGetPowerState(pState) == NdisDeviceStateD0 && vpoxNetFltWinGetOpState(pState) == kVPoxNetDevOpState_Initialized)
    {
        /** @todo r=bird: Since this is a volatile member, why don't you declare it as
         *        such and save yourself all the casting? */
        ASMAtomicIncU32((uint32_t volatile *)&pState->cReferences);
        return true;
    }
    return false;
}

#ifndef VPOXNETADP
DECLINLINE(bool) vpoxNetFltWinDoReferenceDevices(PVPOXNETFLT_WINIF_DEVICE pState1, PVPOXNETFLT_WINIF_DEVICE pState2)
{
    if (vpoxNetFltWinGetPowerState(pState1) == NdisDeviceStateD0
            && vpoxNetFltWinGetOpState(pState1) == kVPoxNetDevOpState_Initialized
            && vpoxNetFltWinGetPowerState(pState2) == NdisDeviceStateD0
            && vpoxNetFltWinGetOpState(pState2) == kVPoxNetDevOpState_Initialized)
    {
        ASMAtomicIncU32((uint32_t volatile *)&pState1->cReferences);
        ASMAtomicIncU32((uint32_t volatile *)&pState2->cReferences);
        return true;
    }
    return false;
}
#endif

DECLINLINE(void) vpoxNetFltWinDereferenceDevice(PVPOXNETFLT_WINIF_DEVICE pState)
{
    ASMAtomicDecU32((uint32_t volatile *)&pState->cReferences);
    /** @todo r=bird: Add comment explaining why these cannot hit 0 or why
     *        reference are counted  */
}

#ifndef VPOXNETADP
DECLINLINE(void) vpoxNetFltWinDereferenceDevices(PVPOXNETFLT_WINIF_DEVICE pState1, PVPOXNETFLT_WINIF_DEVICE pState2)
{
    ASMAtomicDecU32((uint32_t volatile *)&pState1->cReferences);
    ASMAtomicDecU32((uint32_t volatile *)&pState2->cReferences);
}
#endif

DECLINLINE(void) vpoxNetFltWinDecReferenceDevice(PVPOXNETFLT_WINIF_DEVICE pState, uint32_t v)
{
    Assert(v);
    ASMAtomicAddU32((uint32_t volatile *)&pState->cReferences, (uint32_t)(-((int32_t)v)));
}

#ifndef VPOXNETADP
DECLINLINE(void) vpoxNetFltWinDecReferenceDevices(PVPOXNETFLT_WINIF_DEVICE pState1, PVPOXNETFLT_WINIF_DEVICE pState2, uint32_t v)
{
    ASMAtomicAddU32((uint32_t volatile *)&pState1->cReferences, (uint32_t)(-((int32_t)v)));
    ASMAtomicAddU32((uint32_t volatile *)&pState2->cReferences, (uint32_t)(-((int32_t)v)));
}
#endif

DECLINLINE(bool) vpoxNetFltWinDoIncReferenceDevice(PVPOXNETFLT_WINIF_DEVICE pState, uint32_t v)
{
    Assert(v);
    if (vpoxNetFltWinGetPowerState(pState) == NdisDeviceStateD0 && vpoxNetFltWinGetOpState(pState) == kVPoxNetDevOpState_Initialized)
    {
        ASMAtomicAddU32((uint32_t volatile *)&pState->cReferences, v);
        return true;
    }
    return false;
}

#ifndef VPOXNETADP
DECLINLINE(bool) vpoxNetFltWinDoIncReferenceDevices(PVPOXNETFLT_WINIF_DEVICE pState1, PVPOXNETFLT_WINIF_DEVICE pState2, uint32_t v)
{
    if (vpoxNetFltWinGetPowerState(pState1) == NdisDeviceStateD0
            && vpoxNetFltWinGetOpState(pState1) == kVPoxNetDevOpState_Initialized
            && vpoxNetFltWinGetPowerState(pState2) == NdisDeviceStateD0
            && vpoxNetFltWinGetOpState(pState2) == kVPoxNetDevOpState_Initialized)
    {
        ASMAtomicAddU32((uint32_t volatile *)&pState1->cReferences, v);
        ASMAtomicAddU32((uint32_t volatile *)&pState2->cReferences, v);
        return true;
    }
    return false;
}
#endif


DECLINLINE(bool) vpoxNetFltWinReferenceWinIfNetFlt(PVPOXNETFLTINS pNetFlt, bool * pbNetFltActive)
{
    RTSpinlockAcquire((pNetFlt)->hSpinlock);
#ifndef VPOXNETADP
    if (!vpoxNetFltWinDoReferenceDevices(&pNetFlt->u.s.WinIf.MpState, &pNetFlt->u.s.WinIf.PtState))
#else
    if (!vpoxNetFltWinDoReferenceDevice(&pNetFlt->u.s.WinIf.MpState))
#endif
    {
        RTSpinlockRelease((pNetFlt)->hSpinlock);
        *pbNetFltActive = false;
        return false;
    }

    if (pNetFlt->enmTrunkState != INTNETTRUNKIFSTATE_ACTIVE)
    {
        vpoxNetFltWinReferenceModePassThru(pNetFlt);
        RTSpinlockRelease((pNetFlt)->hSpinlock);
        *pbNetFltActive = false;
        return true;
    }

    vpoxNetFltRetain((pNetFlt), true /* fBusy */);
    vpoxNetFltWinReferenceModeNetFlt(pNetFlt);
    RTSpinlockRelease((pNetFlt)->hSpinlock);

    *pbNetFltActive = true;
    return true;
}

DECLINLINE(bool) vpoxNetFltWinIncReferenceWinIfNetFlt(PVPOXNETFLTINS pNetFlt, uint32_t v, bool *pbNetFltActive)
{
    uint32_t i;

    Assert(v);
    if (!v)
    {
        *pbNetFltActive = false;
        return false;
    }

    RTSpinlockAcquire((pNetFlt)->hSpinlock);
#ifndef VPOXNETADP
    if (!vpoxNetFltWinDoIncReferenceDevices(&pNetFlt->u.s.WinIf.MpState, &pNetFlt->u.s.WinIf.PtState, v))
#else
    if (!vpoxNetFltWinDoIncReferenceDevice(&pNetFlt->u.s.WinIf.MpState, v))
#endif
    {
        RTSpinlockRelease(pNetFlt->hSpinlock);
        *pbNetFltActive = false;
        return false;
    }

    if (pNetFlt->enmTrunkState != INTNETTRUNKIFSTATE_ACTIVE)
    {
        vpoxNetFltWinIncReferenceModePassThru(pNetFlt, v);

        RTSpinlockRelease((pNetFlt)->hSpinlock);
        *pbNetFltActive = false;
        return true;
    }

    vpoxNetFltRetain(pNetFlt, true /* fBusy */);

    vpoxNetFltWinIncReferenceModeNetFlt(pNetFlt, v);

    RTSpinlockRelease(pNetFlt->hSpinlock);

    /* we have marked it as busy, so can do the res references outside the lock */
    for (i = 0; i < v-1; i++)
    {
        vpoxNetFltRetain(pNetFlt, true /* fBusy */);
    }

    *pbNetFltActive = true;

    return true;
}

DECLINLINE(void) vpoxNetFltWinDecReferenceNetFlt(PVPOXNETFLTINS pNetFlt, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
    {
        vpoxNetFltRelease(pNetFlt, true);
    }

    vpoxNetFltWinDecReferenceModeNetFlt(pNetFlt, n);
}

DECLINLINE(void) vpoxNetFltWinDereferenceNetFlt(PVPOXNETFLTINS pNetFlt)
{
    vpoxNetFltRelease(pNetFlt, true);

    vpoxNetFltWinDereferenceModeNetFlt(pNetFlt);
}

DECLINLINE(void) vpoxNetFltWinDecReferenceWinIf(PVPOXNETFLTINS pNetFlt, uint32_t v)
{
#ifdef VPOXNETADP
    vpoxNetFltWinDecReferenceDevice(&pNetFlt->u.s.WinIf.MpState, v);
#else
    vpoxNetFltWinDecReferenceDevices(&pNetFlt->u.s.WinIf.MpState, &pNetFlt->u.s.WinIf.PtState, v);
#endif
}

DECLINLINE(void) vpoxNetFltWinDereferenceWinIf(PVPOXNETFLTINS pNetFlt)
{
#ifdef VPOXNETADP
    vpoxNetFltWinDereferenceDevice(&pNetFlt->u.s.WinIf.MpState);
#else
    vpoxNetFltWinDereferenceDevices(&pNetFlt->u.s.WinIf.MpState, &pNetFlt->u.s.WinIf.PtState);
#endif
}

DECLINLINE(bool) vpoxNetFltWinIncReferenceWinIf(PVPOXNETFLTINS pNetFlt, uint32_t v)
{
    Assert(v);
    if (!v)
    {
        return false;
    }

    RTSpinlockAcquire(pNetFlt->hSpinlock);
#ifdef VPOXNETADP
    if (vpoxNetFltWinDoIncReferenceDevice(&pNetFlt->u.s.WinIf.MpState, v))
#else
    if (vpoxNetFltWinDoIncReferenceDevices(&pNetFlt->u.s.WinIf.MpState, &pNetFlt->u.s.WinIf.PtState, v))
#endif
    {
        RTSpinlockRelease(pNetFlt->hSpinlock);
        return true;
    }

    RTSpinlockRelease(pNetFlt->hSpinlock);
    return false;
}

DECLINLINE(bool) vpoxNetFltWinReferenceWinIf(PVPOXNETFLTINS pNetFlt)
{
    RTSpinlockAcquire(pNetFlt->hSpinlock);
#ifdef VPOXNETADP
    if (vpoxNetFltWinDoReferenceDevice(&pNetFlt->u.s.WinIf.MpState))
#else
    if (vpoxNetFltWinDoReferenceDevices(&pNetFlt->u.s.WinIf.MpState, &pNetFlt->u.s.WinIf.PtState))
#endif
    {
        RTSpinlockRelease(pNetFlt->hSpinlock);
        return true;
    }

    RTSpinlockRelease(pNetFlt->hSpinlock);
    return false;
}

/***********************************************
 * methods for accessing the network card info *
 ***********************************************/

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinGetMacAddress(PVPOXNETFLTINS pNetFlt, PRTMAC pMac);
DECLHIDDEN(bool) vpoxNetFltWinIsPromiscuous(PVPOXNETFLTINS pNetFlt);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinSetPromiscuous(PVPOXNETFLTINS pNetFlt, bool bYes);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinQueryPhysicalMedium(PVPOXNETFLTINS pNetFlt, NDIS_PHYSICAL_MEDIUM * pMedium);

/*********************
 * mem alloc API     *
 *********************/

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinMemAlloc(PVOID* ppMemBuf, UINT cbLength);

DECLHIDDEN(void) vpoxNetFltWinMemFree(PVOID pMemBuf);

/* convenience method used which allocates and initializes the PINTNETSG containing one
 * segment referring the buffer of size cbBufSize
 * the allocated PINTNETSG should be freed with the vpoxNetFltWinMemFree.
 *
 * This is used when our ProtocolReceive callback is called and we have to return the indicated NDIS_PACKET
 * on a callback exit. This is why we allocate the PINTNETSG and put the packet info there and enqueue it
 * for the packet queue */
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinAllocSG(UINT cbBufSize, PINTNETSG *ppSG);

/************************
 * WinIf init/fini API *
 ************************/
#if defined(VPOXNETADP)
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtInitBind(PVPOXNETFLTINS *ppNetFlt, NDIS_HANDLE hMiniportAdapter, PNDIS_STRING pBindToMiniportName /* actually this is our miniport name*/, NDIS_HANDLE hWrapperConfigurationContext);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtInitWinIf(PVPOXNETFLTWIN pWinIf);
#else
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtInitBind(PVPOXNETFLTINS *ppNetFlt, PNDIS_STRING pOurMiniportName, PNDIS_STRING pBindToMiniportName);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtInitWinIf(PVPOXNETFLTWIN pWinIf, PNDIS_STRING pOurDeviceName);
#endif

DECLHIDDEN(VOID) vpoxNetFltWinPtFiniWinIf(PVPOXNETFLTWIN pWinIf);

/************************************
 * Execute Job at passive level API *
 ************************************/

typedef VOID (*PFNVPOXNETFLT_JOB_ROUTINE) (PVOID pContext);

DECLHIDDEN(VOID) vpoxNetFltWinJobSynchExecAtPassive(PFNVPOXNETFLT_JOB_ROUTINE pfnRoutine, PVOID pContext);

/*******************************
 * Ndis Packets processing API *
 *******************************/
DECLHIDDEN(PNDIS_PACKET) vpoxNetFltWinNdisPacketFromSG(PVPOXNETFLTINS pNetFlt, PINTNETSG pSG, PVOID pBufToFree, bool bToWire, bool bCopyMemory);

DECLHIDDEN(void) vpoxNetFltWinFreeSGNdisPacket(PNDIS_PACKET pPacket, bool bFreeMem);

#ifdef DEBUG_NETFLT_PACKETS
#define DBG_CHECK_PACKETS(_p1, _p2) \
    {   \
        bool _b = vpoxNetFltWinMatchPackets(_p1, _p2, -1);  \
        Assert(_b);  \
    }

#define DBG_CHECK_PACKET_AND_SG(_p, _sg) \
    {   \
        bool _b = vpoxNetFltWinMatchPacketAndSG(_p, _sg, -1);  \
        Assert(_b);  \
    }

#define DBG_CHECK_SGS(_sg1, _sg2) \
    {   \
        bool _b = vpoxNetFltWinMatchSGs(_sg1, _sg2, -1);  \
        Assert(_b);  \
    }

#else
#define DBG_CHECK_PACKETS(_p1, _p2)
#define DBG_CHECK_PACKET_AND_SG(_p, _sg)
#define DBG_CHECK_SGS(_sg1, _sg2)
#endif

/**
 * Ndis loops back broadcast packets posted to the wire by IntNet
 * This routine is used in the mechanism of preventing this looping
 *
 * @param pAdapt
 * @param pPacket
 * @param bOnRecv true is we are receiving the packet from the wire
 * false otherwise (i.e. the packet is from the host)
 *
 * @return true if the packet is a looped back one, false otherwise
 */
#ifdef VPOX_LOOPBACK_USEFLAGS
DECLINLINE(bool) vpoxNetFltWinIsLoopedBackPacket(PNDIS_PACKET pPacket)
{
    return (NdisGetPacketFlags(pPacket) & g_fPacketIsLoopedBack) == g_fPacketIsLoopedBack;
}
#endif

/**************************************************************
 * utility methods for ndis packet creation/initialization    *
 **************************************************************/

#define VPOXNETFLT_OOB_INIT(_p) \
    { \
        NdisZeroMemory(NDIS_OOB_DATA_FROM_PACKET(_p), sizeof(NDIS_PACKET_OOB_DATA)); \
        NDIS_SET_PACKET_HEADER_SIZE(_p, VPOXNETFLT_PACKET_ETHEADER_SIZE); \
    }

#ifndef VPOXNETADP

DECLINLINE(NDIS_STATUS) vpoxNetFltWinCopyPacketInfoOnRecv(PNDIS_PACKET pDstPacket, PNDIS_PACKET pSrcPacket, bool bForceStatusResources)
{
    NDIS_STATUS Status = bForceStatusResources ? NDIS_STATUS_RESOURCES : NDIS_GET_PACKET_STATUS(pSrcPacket);
    NDIS_SET_PACKET_STATUS(pDstPacket, Status);

    NDIS_PACKET_FIRST_NDIS_BUFFER(pDstPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pSrcPacket);
    NDIS_PACKET_LAST_NDIS_BUFFER(pDstPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pSrcPacket);

    NdisGetPacketFlags(pDstPacket) = NdisGetPacketFlags(pSrcPacket);

    NDIS_SET_ORIGINAL_PACKET(pDstPacket, NDIS_GET_ORIGINAL_PACKET(pSrcPacket));
    NDIS_SET_PACKET_HEADER_SIZE(pDstPacket, NDIS_GET_PACKET_HEADER_SIZE(pSrcPacket));

    return Status;
}

DECLINLINE(void) vpoxNetFltWinCopyPacketInfoOnSend(PNDIS_PACKET pDstPacket, PNDIS_PACKET pSrcPacket)
{
    NDIS_PACKET_FIRST_NDIS_BUFFER(pDstPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pSrcPacket);
    NDIS_PACKET_LAST_NDIS_BUFFER(pDstPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pSrcPacket);

    NdisGetPacketFlags(pDstPacket) = NdisGetPacketFlags(pSrcPacket);

    NdisMoveMemory(NDIS_OOB_DATA_FROM_PACKET(pDstPacket),
                    NDIS_OOB_DATA_FROM_PACKET(pSrcPacket),
                    sizeof (NDIS_PACKET_OOB_DATA));

    NdisIMCopySendPerPacketInfo(pDstPacket, pSrcPacket);

    PVOID pMediaSpecificInfo = NULL;
    UINT fMediaSpecificInfoSize = 0;

    NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(pSrcPacket, &pMediaSpecificInfo, &fMediaSpecificInfoSize);

    if (pMediaSpecificInfo || fMediaSpecificInfoSize)
    {
        NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(pDstPacket, pMediaSpecificInfo, fMediaSpecificInfoSize);
    }
}

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPrepareSendPacket(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket, PNDIS_PACKET *ppMyPacket);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPrepareRecvPacket(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket, PNDIS_PACKET *ppMyPacket, bool bDpr);
#endif

DECLHIDDEN(void) vpoxNetFltWinSleep(ULONG milis);

#define MACS_EQUAL(_m1, _m2) \
    ((_m1).au16[0] == (_m2).au16[0] \
        && (_m1).au16[1] == (_m2).au16[1] \
        && (_m1).au16[2] == (_m2).au16[2])


DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinDetachFromInterface(PVPOXNETFLTINS pNetFlt, bool bOnUnbind);
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinCopyString(PNDIS_STRING pDst, PNDIS_STRING pSrc);


/**
 * Sets the enmState member atomically.
 *
 * Used for all updates.
 *
 * @param   pThis           The instance.
 * @param   enmNewState     The new value.
 */
DECLINLINE(void) vpoxNetFltWinSetWinIfState(PVPOXNETFLTINS pNetFlt, VPOXNETFLT_WINIFSTATE enmNewState)
{
    ASMAtomicWriteU32((uint32_t volatile *)&pNetFlt->u.s.WinIf.enmState, enmNewState);
}

/**
 * Gets the enmState member atomically.
 *
 * Used for all reads.
 *
 * @returns The enmState value.
 * @param   pThis           The instance.
 */
DECLINLINE(VPOXNETFLT_WINIFSTATE) vpoxNetFltWinGetWinIfState(PVPOXNETFLTINS pNetFlt)
{
    return (VPOXNETFLT_WINIFSTATE)ASMAtomicUoReadU32((uint32_t volatile *)&pNetFlt->u.s.WinIf.enmState);
}

/* reference the driver module to prevent driver unload */
DECLHIDDEN(void) vpoxNetFltWinDrvReference();
/* dereference the driver module to prevent driver unload */
DECLHIDDEN(void) vpoxNetFltWinDrvDereference();


#ifndef VPOXNETADP
# define VPOXNETFLT_PROMISCUOUS_SUPPORTED(_pNetFlt) (!(_pNetFlt)->fDisablePromiscuous)
#else
# define STATISTIC_INCREASE(_s) ASMAtomicIncU32((uint32_t volatile *)&(_s));

DECLHIDDEN(void) vpoxNetFltWinGenerateMACAddress(RTMAC *pMac);
DECLHIDDEN(int) vpoxNetFltWinMAC2NdisString(RTMAC *pMac, PNDIS_STRING pNdisString);
DECLHIDDEN(int) vpoxNetFltWinMACFromNdisString(RTMAC *pMac, PNDIS_STRING pNdisString);

#endif
#endif /* !VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetFltRt_win_h */
