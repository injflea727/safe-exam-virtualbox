/* $Id: VPoxMPCm.cpp $ */
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

#include "VPoxMPWddm.h"

typedef struct VPOXVIDEOCM_CMD_DR
{
    LIST_ENTRY QueueList;
    PVPOXVIDEOCM_CTX pContext;
    uint32_t cbMaxCmdSize;
    volatile uint32_t cRefs;

    VPOXVIDEOCM_CMD_HDR CmdHdr;
} VPOXVIDEOCM_CMD_DR, *PVPOXVIDEOCM_CMD_DR;

typedef enum
{
    VPOXVIDEOCM_CMD_CTL_KM_TYPE_POST_INVOKE = 1,
    VPOXVIDEOCM_CMD_CTL_KM_TYPE_PRE_INVOKE,
    VPOXVIDEOCM_CMD_CTL_KM_TYPE_DUMMY_32BIT = 0x7fffffff
} VPOXVIDEOCM_CMD_CTL_KM_TYPE;

typedef DECLCALLBACK(VOID) FNVPOXVIDEOCM_CMD_CB(PVPOXVIDEOCM_CTX pContext, struct VPOXVIDEOCM_CMD_CTL_KM *pCmd, PVOID pvContext);
typedef FNVPOXVIDEOCM_CMD_CB *PFNVPOXVIDEOCM_CMD_CB;

typedef struct VPOXVIDEOCM_CMD_CTL_KM
{
    VPOXVIDEOCM_CMD_CTL_KM_TYPE enmType;
    uint32_t u32Reserved;
    PFNVPOXVIDEOCM_CMD_CB pfnCb;
    PVOID pvCb;
} VPOXVIDEOCM_CMD_CTL_KM, *PVPOXVIDEOCM_CMD_CTL_KM;

AssertCompile(VPOXWDDM_ROUNDBOUND(RT_OFFSETOF(VPOXVIDEOCM_CMD_DR, CmdHdr), 8) == RT_OFFSETOF(VPOXVIDEOCM_CMD_DR, CmdHdr));

#define VPOXVIDEOCM_HEADER_SIZE() (VPOXWDDM_ROUNDBOUND(sizeof (VPOXVIDEOCM_CMD_DR), 8))
#define VPOXVIDEOCM_SIZE_FROMBODYSIZE(_s) (VPOXVIDEOCM_HEADER_SIZE() + (_s))
//#define VPOXVIDEOCM_SIZE(_t) (VPOXVIDEOCM_SIZE_FROMBODYSIZE(sizeof (_t)))
#define VPOXVIDEOCM_BODY(_pCmd, _t) ( (_t*)(((uint8_t*)(_pCmd)) + VPOXVIDEOCM_HEADER_SIZE()) )
#define VPOXVIDEOCM_HEAD(_pCmd) ( (PVPOXVIDEOCM_CMD_DR)(((uint8_t*)(_pCmd)) - VPOXVIDEOCM_HEADER_SIZE()) )

#define VPOXVIDEOCM_SENDSIZE_FROMBODYSIZE(_s) ( VPOXVIDEOCM_SIZE_FROMBODYSIZE(_s) - RT_OFFSETOF(VPOXVIDEOCM_CMD_DR, CmdHdr))

//#define VPOXVIDEOCM_BODY_FIELD_OFFSET(_ot, _t, _f) ( (_ot)( VPOXVIDEOCM_BODY(0, uint8_t) + RT_OFFSETOF(_t, _f) ) )

typedef struct VPOXVIDEOCM_SESSION
{
    /* contexts in this session */
    LIST_ENTRY QueueEntry;
    /* contexts in this session */
    LIST_ENTRY ContextList;
    /* commands list  */
    LIST_ENTRY CommandsList;
    /* post process commands list  */
    LIST_ENTRY PpCommandsList;
    /* event used to notify UMD about pending commands */
    PKEVENT pUmEvent;
    /* sync lock */
    KSPIN_LOCK SynchLock;
    /* indicates whether event signaling is needed on cmd add */
    bool bEventNeeded;
} VPOXVIDEOCM_SESSION, *PVPOXVIDEOCM_SESSION;

#define VPOXCMENTRY_2_CMD(_pE) ((PVPOXVIDEOCM_CMD_DR)((uint8_t*)(_pE) - RT_UOFFSETOF(VPOXVIDEOCM_CMD_DR, QueueList)))

void* vpoxVideoCmCmdReinitForContext(void *pvCmd, PVPOXVIDEOCM_CTX pContext)
{
    PVPOXVIDEOCM_CMD_DR pHdr = VPOXVIDEOCM_HEAD(pvCmd);
    pHdr->pContext = pContext;
    pHdr->CmdHdr.u64UmData = pContext->u64UmData;
    return pvCmd;
}

void* vpoxVideoCmCmdCreate(PVPOXVIDEOCM_CTX pContext, uint32_t cbSize)
{
    Assert(cbSize);
    if (!cbSize)
        return NULL;

    Assert(VPOXWDDM_ROUNDBOUND(cbSize, 8) == cbSize);
    cbSize = VPOXWDDM_ROUNDBOUND(cbSize, 8);

    Assert(pContext->pSession);
    if (!pContext->pSession)
        return NULL;

    uint32_t cbCmd = VPOXVIDEOCM_SIZE_FROMBODYSIZE(cbSize);
    PVPOXVIDEOCM_CMD_DR pCmd = (PVPOXVIDEOCM_CMD_DR)vpoxWddmMemAllocZero(cbCmd);
    Assert(pCmd);
    if (pCmd)
    {
        InitializeListHead(&pCmd->QueueList);
        pCmd->pContext = pContext;
        pCmd->cbMaxCmdSize = VPOXVIDEOCM_SENDSIZE_FROMBODYSIZE(cbSize);
        pCmd->cRefs = 1;
        pCmd->CmdHdr.u64UmData = pContext->u64UmData;
        pCmd->CmdHdr.cbCmd = pCmd->cbMaxCmdSize;
    }
    return VPOXVIDEOCM_BODY(pCmd, void);
}

static PVPOXVIDEOCM_CMD_CTL_KM vpoxVideoCmCmdCreateKm(PVPOXVIDEOCM_CTX pContext, VPOXVIDEOCM_CMD_CTL_KM_TYPE enmType,
        PFNVPOXVIDEOCM_CMD_CB pfnCb, PVOID pvCb,
        uint32_t cbSize)
{
    PVPOXVIDEOCM_CMD_CTL_KM pCmd = (PVPOXVIDEOCM_CMD_CTL_KM)vpoxVideoCmCmdCreate(pContext, cbSize + sizeof (*pCmd));
    pCmd->enmType = enmType;
    pCmd->pfnCb = pfnCb;
    pCmd->pvCb = pvCb;
    PVPOXVIDEOCM_CMD_DR pHdr = VPOXVIDEOCM_HEAD(pCmd);
    pHdr->CmdHdr.enmType = VPOXVIDEOCM_CMD_TYPE_CTL_KM;
    return pCmd;
}

static DECLCALLBACK(VOID) vpoxVideoCmCmdCbSetEventAndDereference(PVPOXVIDEOCM_CTX pContext, PVPOXVIDEOCM_CMD_CTL_KM pCmd,
                                                                 PVOID pvContext)
{
    RT_NOREF(pContext);
    PKEVENT pEvent = (PKEVENT)pvContext;
    KeSetEvent(pEvent, 0, FALSE);
    ObDereferenceObject(pEvent);
    vpoxVideoCmCmdRelease(pCmd);
}

NTSTATUS vpoxVideoCmCmdSubmitCompleteEvent(PVPOXVIDEOCM_CTX pContext, PKEVENT pEvent)
{
    Assert(pEvent);
    PVPOXVIDEOCM_CMD_CTL_KM pCmd = vpoxVideoCmCmdCreateKm(pContext, VPOXVIDEOCM_CMD_CTL_KM_TYPE_POST_INVOKE,
            vpoxVideoCmCmdCbSetEventAndDereference, pEvent, 0);
    if (!pCmd)
    {
        WARN(("vpoxVideoCmCmdCreateKm failed"));
        return STATUS_NO_MEMORY;
    }

    vpoxVideoCmCmdSubmit(pCmd, VPOXVIDEOCM_SUBMITSIZE_DEFAULT);

    return STATUS_SUCCESS;
}

DECLINLINE(void) vpoxVideoCmCmdRetainByHdr(PVPOXVIDEOCM_CMD_DR pHdr)
{
    ASMAtomicIncU32(&pHdr->cRefs);
}

DECLINLINE(void) vpoxVideoCmCmdReleaseByHdr(PVPOXVIDEOCM_CMD_DR pHdr)
{
    uint32_t cRefs = ASMAtomicDecU32(&pHdr->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
        vpoxWddmMemFree(pHdr);
}

static void vpoxVideoCmCmdCancel(PVPOXVIDEOCM_CMD_DR pHdr)
{
    InitializeListHead(&pHdr->QueueList);
    vpoxVideoCmCmdReleaseByHdr(pHdr);
}

static void vpoxVideoCmCmdPostByHdr(PVPOXVIDEOCM_SESSION pSession, PVPOXVIDEOCM_CMD_DR pHdr, uint32_t cbSize)
{
    bool bSignalEvent = false;
    if (cbSize != VPOXVIDEOCM_SUBMITSIZE_DEFAULT)
    {
        cbSize = VPOXVIDEOCM_SENDSIZE_FROMBODYSIZE(cbSize);
        Assert(cbSize <= pHdr->cbMaxCmdSize);
        pHdr->CmdHdr.cbCmd = cbSize;
    }

    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);

    KIRQL OldIrql;
    KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);

    InsertHeadList(&pSession->CommandsList, &pHdr->QueueList);
    if (pSession->bEventNeeded)
    {
        pSession->bEventNeeded = false;
        bSignalEvent = true;
    }

    KeReleaseSpinLock(&pSession->SynchLock, OldIrql);

    if (bSignalEvent)
        KeSetEvent(pSession->pUmEvent, 0, FALSE);
}

void vpoxVideoCmCmdRetain(void *pvCmd)
{
    PVPOXVIDEOCM_CMD_DR pHdr = VPOXVIDEOCM_HEAD(pvCmd);
    vpoxVideoCmCmdRetainByHdr(pHdr);
}

void vpoxVideoCmCmdRelease(void *pvCmd)
{
    PVPOXVIDEOCM_CMD_DR pHdr = VPOXVIDEOCM_HEAD(pvCmd);
    vpoxVideoCmCmdReleaseByHdr(pHdr);
}

/**
 * @param pvCmd memory buffer returned by vpoxVideoCmCmdCreate
 * @param cbSize should be <= cbSize posted to vpoxVideoCmCmdCreate on command creation
 */
void vpoxVideoCmCmdSubmit(void *pvCmd, uint32_t cbSize)
{
    PVPOXVIDEOCM_CMD_DR pHdr = VPOXVIDEOCM_HEAD(pvCmd);
    vpoxVideoCmCmdPostByHdr(pHdr->pContext->pSession, pHdr, cbSize);
}

NTSTATUS vpoxVideoCmCmdVisit(PVPOXVIDEOCM_CTX pContext, BOOLEAN bEntireSession, PFNVPOXVIDEOCMCMDVISITOR pfnVisitor,
                             PVOID pvVisitor)
{
    PVPOXVIDEOCM_SESSION pSession = pContext->pSession;
    PLIST_ENTRY pCurEntry = NULL;
    PVPOXVIDEOCM_CMD_DR pHdr;

    KIRQL OldIrql;
    KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);

    pCurEntry = pSession->CommandsList.Blink;
    do
    {
        if (pCurEntry != &pSession->CommandsList)
        {
            pHdr = VPOXCMENTRY_2_CMD(pCurEntry);
            pCurEntry = pHdr->QueueList.Blink;
            if (bEntireSession || pHdr->pContext == pContext)
            {
                if (pHdr->CmdHdr.enmType == VPOXVIDEOCM_CMD_TYPE_UM)
                {
                    void * pvBody = VPOXVIDEOCM_BODY(pHdr, void);
                    UINT fRet = pfnVisitor(pHdr->pContext, pvBody, pHdr->CmdHdr.cbCmd, pvVisitor);
                    if (fRet & VPOXVIDEOCMCMDVISITOR_RETURN_RMCMD)
                    {
                        RemoveEntryList(&pHdr->QueueList);
                    }
                    if ((fRet & VPOXVIDEOCMCMDVISITOR_RETURN_BREAK))
                        break;
                }
                else
                {
                    WARN(("non-um cmd on visit, skipping"));
                }
            }
        }
        else
        {
            break;
        }
    } while (1);


    KeReleaseSpinLock(&pSession->SynchLock, OldIrql);

    return STATUS_SUCCESS;
}

void vpoxVideoCmCtxInitEmpty(PVPOXVIDEOCM_CTX pContext)
{
    InitializeListHead(&pContext->SessionEntry);
    pContext->pSession = NULL;
    pContext->u64UmData = 0ULL;
}

static void vpoxVideoCmSessionCtxAddLocked(PVPOXVIDEOCM_SESSION pSession, PVPOXVIDEOCM_CTX pContext)
{
    InsertHeadList(&pSession->ContextList, &pContext->SessionEntry);
    pContext->pSession = pSession;
}

void vpoxVideoCmSessionCtxAdd(PVPOXVIDEOCM_SESSION pSession, PVPOXVIDEOCM_CTX pContext)
{
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);

    vpoxVideoCmSessionCtxAddLocked(pSession, pContext);

    KeReleaseSpinLock(&pSession->SynchLock, OldIrql);
}

void vpoxVideoCmSessionSignalEvent(PVPOXVIDEOCM_SESSION pSession)
{
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    if (pSession->pUmEvent)
        KeSetEvent(pSession->pUmEvent, 0, FALSE);
}

static void vpoxVideoCmSessionDestroyLocked(PVPOXVIDEOCM_SESSION pSession)
{
    /* signal event so that user-space client can figure out the context is destroyed
     * in case the context destroyal is caused by Graphics device reset or miniport driver update */
    KeSetEvent(pSession->pUmEvent, 0, FALSE);
    ObDereferenceObject(pSession->pUmEvent);
    Assert(IsListEmpty(&pSession->ContextList));
    Assert(IsListEmpty(&pSession->CommandsList));
    Assert(IsListEmpty(&pSession->PpCommandsList));
    RemoveEntryList(&pSession->QueueEntry);
    vpoxWddmMemFree(pSession);
}

static void vpoxVideoCmSessionCtxPpList(PVPOXVIDEOCM_CTX pContext, PLIST_ENTRY pHead)
{
    LIST_ENTRY *pCur;
    for (pCur = pHead->Flink; pCur != pHead; pCur = pHead->Flink)
    {
        RemoveEntryList(pCur);
        PVPOXVIDEOCM_CMD_DR pHdr = VPOXCMENTRY_2_CMD(pCur);
        PVPOXVIDEOCM_CMD_CTL_KM pCmd = VPOXVIDEOCM_BODY(pHdr, VPOXVIDEOCM_CMD_CTL_KM);
        pCmd->pfnCb(pContext, pCmd, pCmd->pvCb);
    }
}

static void vpoxVideoCmSessionCtxDetachCmdsLocked(PLIST_ENTRY pEntriesHead, PVPOXVIDEOCM_CTX pContext, PLIST_ENTRY pDstHead)
{
    LIST_ENTRY *pCur;
    LIST_ENTRY *pPrev;
    pCur = pEntriesHead->Flink;
    pPrev = pEntriesHead;
    while (pCur != pEntriesHead)
    {
        PVPOXVIDEOCM_CMD_DR pCmd = VPOXCMENTRY_2_CMD(pCur);
        if (pCmd->pContext == pContext)
        {
            RemoveEntryList(pCur);
            InsertTailList(pDstHead, pCur);
            pCur = pPrev;
            /* pPrev - remains unchanged */
        }
        else
        {
            pPrev = pCur;
        }
        pCur = pCur->Flink;
    }
}
/**
 * @return true iff the given session is destroyed
 */
bool vpoxVideoCmSessionCtxRemoveLocked(PVPOXVIDEOCM_SESSION pSession, PVPOXVIDEOCM_CTX pContext)
{
    bool bDestroy;
    LIST_ENTRY RemainedList;
    LIST_ENTRY RemainedPpList;
    LIST_ENTRY *pCur;
    InitializeListHead(&RemainedList);
    InitializeListHead(&RemainedPpList);
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);

    pContext->pSession = NULL;
    RemoveEntryList(&pContext->SessionEntry);
    bDestroy = !!(IsListEmpty(&pSession->ContextList));
    /* ensure there are no commands left for the given context */
    if (bDestroy)
    {
        vpoxVideoLeDetach(&pSession->CommandsList, &RemainedList);
        vpoxVideoLeDetach(&pSession->PpCommandsList, &RemainedPpList);
    }
    else
    {
        vpoxVideoCmSessionCtxDetachCmdsLocked(&pSession->CommandsList, pContext, &RemainedList);
        vpoxVideoCmSessionCtxDetachCmdsLocked(&pSession->PpCommandsList, pContext, &RemainedPpList);
    }

    KeReleaseSpinLock(&pSession->SynchLock, OldIrql);

    for (pCur = RemainedList.Flink; pCur != &RemainedList; pCur = RemainedList.Flink)
    {
        RemoveEntryList(pCur);
        PVPOXVIDEOCM_CMD_DR pCmd = VPOXCMENTRY_2_CMD(pCur);
        vpoxVideoCmCmdCancel(pCmd);
    }

    vpoxVideoCmSessionCtxPpList(pContext, &RemainedPpList);

    if (bDestroy)
    {
        vpoxVideoCmSessionDestroyLocked(pSession);
    }

    return bDestroy;
}

/* the session gets destroyed once the last context is removed from it */
NTSTATUS vpoxVideoCmSessionCreateLocked(PVPOXVIDEOCM_MGR pMgr, PVPOXVIDEOCM_SESSION *ppSession, PKEVENT pUmEvent,
                                        PVPOXVIDEOCM_CTX pContext)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PVPOXVIDEOCM_SESSION pSession = (PVPOXVIDEOCM_SESSION)vpoxWddmMemAllocZero(sizeof (VPOXVIDEOCM_SESSION));
    Assert(pSession);
    if (pSession)
    {
        InitializeListHead(&pSession->ContextList);
        InitializeListHead(&pSession->CommandsList);
        InitializeListHead(&pSession->PpCommandsList);
        pSession->pUmEvent = pUmEvent;
        Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
        KeInitializeSpinLock(&pSession->SynchLock);
        pSession->bEventNeeded = true;
        vpoxVideoCmSessionCtxAddLocked(pSession, pContext);
        InsertHeadList(&pMgr->SessionList, &pSession->QueueEntry);
        *ppSession = pSession;
        return STATUS_SUCCESS;
//        vpoxWddmMemFree(pSession);
    }
    else
    {
        Status = STATUS_NO_MEMORY;
    }
    return Status;
}

#define VPOXCMENTRY_2_SESSION(_pE) ((PVPOXVIDEOCM_SESSION)((uint8_t*)(_pE) - RT_UOFFSETOF(VPOXVIDEOCM_SESSION, QueueEntry)))

NTSTATUS vpoxVideoCmCtxAdd(PVPOXVIDEOCM_MGR pMgr, PVPOXVIDEOCM_CTX pContext, HANDLE hUmEvent, uint64_t u64UmData)
{
    PKEVENT pUmEvent = NULL;
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    NTSTATUS Status = ObReferenceObjectByHandle(hUmEvent, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
        (PVOID*)&pUmEvent,
        NULL);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        KIRQL OldIrql;
        KeAcquireSpinLock(&pMgr->SynchLock, &OldIrql);

        bool bFound = false;
        PVPOXVIDEOCM_SESSION pSession = NULL;
        for (PLIST_ENTRY pEntry = pMgr->SessionList.Flink; pEntry != &pMgr->SessionList; pEntry = pEntry->Flink)
        {
            pSession = VPOXCMENTRY_2_SESSION(pEntry);
            if (pSession->pUmEvent == pUmEvent)
            {
                bFound = true;
                break;
            }
        }

        pContext->u64UmData = u64UmData;

        if (!bFound)
        {
            Status = vpoxVideoCmSessionCreateLocked(pMgr, &pSession, pUmEvent, pContext);
            AssertNtStatusSuccess(Status);
        }
        else
        {
            /* Status = */vpoxVideoCmSessionCtxAdd(pSession, pContext);
            /*AssertNtStatusSuccess(Status);*/
        }

        KeReleaseSpinLock(&pMgr->SynchLock, OldIrql);

        if (Status == STATUS_SUCCESS)
        {
            return STATUS_SUCCESS;
        }

        ObDereferenceObject(pUmEvent);
    }
    return Status;
}

NTSTATUS vpoxVideoCmCtxRemove(PVPOXVIDEOCM_MGR pMgr, PVPOXVIDEOCM_CTX pContext)
{
    PVPOXVIDEOCM_SESSION pSession = pContext->pSession;
    if (!pSession)
        return STATUS_SUCCESS;

    KIRQL OldIrql;
    KeAcquireSpinLock(&pMgr->SynchLock, &OldIrql);

    vpoxVideoCmSessionCtxRemoveLocked(pSession, pContext);

    KeReleaseSpinLock(&pMgr->SynchLock, OldIrql);

    return STATUS_SUCCESS;
}

NTSTATUS vpoxVideoCmInit(PVPOXVIDEOCM_MGR pMgr)
{
    KeInitializeSpinLock(&pMgr->SynchLock);
    InitializeListHead(&pMgr->SessionList);
    return STATUS_SUCCESS;
}

NTSTATUS vpoxVideoCmTerm(PVPOXVIDEOCM_MGR pMgr)
{
    RT_NOREF(pMgr);
    Assert(IsListEmpty(&pMgr->SessionList));
    return STATUS_SUCCESS;
}

NTSTATUS vpoxVideoCmSignalEvents(PVPOXVIDEOCM_MGR pMgr)
{
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    PVPOXVIDEOCM_SESSION pSession = NULL;

    KIRQL OldIrql;
    KeAcquireSpinLock(&pMgr->SynchLock, &OldIrql);

    for (PLIST_ENTRY pEntry = pMgr->SessionList.Flink; pEntry != &pMgr->SessionList; pEntry = pEntry->Flink)
    {
        pSession = VPOXCMENTRY_2_SESSION(pEntry);
        vpoxVideoCmSessionSignalEvent(pSession);
    }

    KeReleaseSpinLock(&pMgr->SynchLock, OldIrql);

    return STATUS_SUCCESS;
}

VOID vpoxVideoCmProcessKm(PVPOXVIDEOCM_CTX pContext, PVPOXVIDEOCM_CMD_CTL_KM pCmd)
{
    PVPOXVIDEOCM_SESSION pSession = pContext->pSession;

    switch (pCmd->enmType)
    {
        case VPOXVIDEOCM_CMD_CTL_KM_TYPE_PRE_INVOKE:
        {
            pCmd->pfnCb(pContext, pCmd, pCmd->pvCb);
            break;
        }

        case VPOXVIDEOCM_CMD_CTL_KM_TYPE_POST_INVOKE:
        {
            PVPOXVIDEOCM_CMD_DR pHdr = VPOXVIDEOCM_HEAD(pCmd);
            KIRQL OldIrql;
            KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);
            InsertTailList(&pSession->PpCommandsList, &pHdr->QueueList);
            KeReleaseSpinLock(&pSession->SynchLock, OldIrql);
            break;
        }

        default:
        {
            WARN(("unsupported cmd type %d", pCmd->enmType));
            break;
        }
    }
}

NTSTATUS vpoxVideoCmEscape(PVPOXVIDEOCM_CTX pContext, PVPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD pCmd, uint32_t cbCmd)
{
    Assert(cbCmd >= sizeof (VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD));
    if (cbCmd < sizeof (VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD))
        return STATUS_BUFFER_TOO_SMALL;

    PVPOXVIDEOCM_SESSION pSession = pContext->pSession;
    PVPOXVIDEOCM_CMD_DR pHdr;
    LIST_ENTRY DetachedList;
    LIST_ENTRY DetachedPpList;
    PLIST_ENTRY pCurEntry = NULL;
    uint32_t cbRemainingCmds = 0;
    uint32_t cbRemainingFirstCmd = 0;
    uint32_t cbData = cbCmd - sizeof (VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD);
    uint8_t * pvData = ((uint8_t *)pCmd) + sizeof (VPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD);
    bool bDetachMode = true;
    InitializeListHead(&DetachedList);
    InitializeListHead(&DetachedPpList);
//    PVPOXWDDM_GETVPOXVIDEOCMCMD_HDR *pvCmd

    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);

    vpoxVideoCmSessionCtxDetachCmdsLocked(&pSession->PpCommandsList, pContext, &DetachedPpList);

    do
    {
        if (bDetachMode)
        {
            if (!IsListEmpty(&pSession->CommandsList))
            {
                Assert(!pCurEntry);
                pHdr = VPOXCMENTRY_2_CMD(pSession->CommandsList.Blink);
                Assert(pHdr->CmdHdr.cbCmd);
                uint32_t cbUserCmd = pHdr->CmdHdr.enmType == VPOXVIDEOCM_CMD_TYPE_UM ? pHdr->CmdHdr.cbCmd : 0;
                if (cbData >= cbUserCmd)
                {
                    RemoveEntryList(&pHdr->QueueList);
                    InsertHeadList(&DetachedList, &pHdr->QueueList);
                    cbData -= cbUserCmd;
                }
                else
                {
                    Assert(cbUserCmd);
                    cbRemainingFirstCmd = cbUserCmd;
                    cbRemainingCmds = cbUserCmd;
                    pCurEntry = pHdr->QueueList.Blink;
                    bDetachMode = false;
                }
            }
            else
            {
                pSession->bEventNeeded = true;
                break;
            }
        }
        else
        {
            Assert(pCurEntry);
            if (pCurEntry != &pSession->CommandsList)
            {
                pHdr = VPOXCMENTRY_2_CMD(pCurEntry);
                uint32_t cbUserCmd = pHdr->CmdHdr.enmType == VPOXVIDEOCM_CMD_TYPE_UM ? pHdr->CmdHdr.cbCmd : 0;
                Assert(cbRemainingFirstCmd);
                cbRemainingCmds += cbUserCmd;
                pCurEntry = pHdr->QueueList.Blink;
            }
            else
            {
                Assert(cbRemainingFirstCmd);
                Assert(cbRemainingCmds);
                break;
            }
        }
    } while (1);

    KeReleaseSpinLock(&pSession->SynchLock, OldIrql);

    vpoxVideoCmSessionCtxPpList(pContext, &DetachedPpList);

    pCmd->Hdr.cbCmdsReturned = 0;
    for (pCurEntry = DetachedList.Blink; pCurEntry != &DetachedList; pCurEntry = DetachedList.Blink)
    {
        pHdr = VPOXCMENTRY_2_CMD(pCurEntry);
        RemoveEntryList(pCurEntry);
        switch (pHdr->CmdHdr.enmType)
        {
            case VPOXVIDEOCM_CMD_TYPE_UM:
            {
                memcpy(pvData, &pHdr->CmdHdr, pHdr->CmdHdr.cbCmd);
                pvData += pHdr->CmdHdr.cbCmd;
                pCmd->Hdr.cbCmdsReturned += pHdr->CmdHdr.cbCmd;
                vpoxVideoCmCmdReleaseByHdr(pHdr);
                break;
            }

            case VPOXVIDEOCM_CMD_TYPE_CTL_KM:
            {
                vpoxVideoCmProcessKm(pContext, VPOXVIDEOCM_BODY(pHdr, VPOXVIDEOCM_CMD_CTL_KM));
                break;
            }

            default:
            {
                WARN(("unsupported cmd type %d", pHdr->CmdHdr.enmType));
                break;
            }
        }
    }

    pCmd->Hdr.cbRemainingCmds = cbRemainingCmds;
    pCmd->Hdr.cbRemainingFirstCmd = cbRemainingFirstCmd;
    pCmd->Hdr.u32Reserved = 0;

    return STATUS_SUCCESS;
}

static BOOLEAN vpoxVideoCmHasUncompletedCmdsLocked(PVPOXVIDEOCM_MGR pMgr)
{
    PVPOXVIDEOCM_SESSION pSession = NULL;
    for (PLIST_ENTRY pEntry = pMgr->SessionList.Flink; pEntry != &pMgr->SessionList; pEntry = pEntry->Flink)
    {
        pSession = VPOXCMENTRY_2_SESSION(pEntry);
        KIRQL OldIrql;
        KeAcquireSpinLock(&pSession->SynchLock, &OldIrql);

        if (pSession->bEventNeeded)
        {
            /* commands still being processed */
            KeReleaseSpinLock(&pSession->SynchLock, OldIrql);
            return TRUE;
        }
        KeReleaseSpinLock(&pSession->SynchLock, OldIrql);
    }
    return FALSE;
}
