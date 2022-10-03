/* $Id: VPoxMPShgsmi.cpp $ */
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
#include <iprt/semaphore.h>

/* SHGSMI */
DECLINLINE(void) vpoxSHGSMICommandRetain(VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

void vpoxSHGSMICommandFree(PVPOXSHGSMI pHeap, VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    VPoxSHGSMIHeapFree(pHeap, pCmd);
}

DECLINLINE(void) vpoxSHGSMICommandRelease(PVPOXSHGSMI pHeap, VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if(!cRefs)
        vpoxSHGSMICommandFree(pHeap, pCmd);
}

static DECLCALLBACK(void) vpoxSHGSMICompletionSetEvent(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvCmd, void *pvContext)
{
    RT_NOREF(pHeap, pvCmd);
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

DECLCALLBACK(void) vpoxSHGSMICompletionCommandRelease(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvCmd, void *pvContext)
{
    RT_NOREF(pvContext);
    vpoxSHGSMICommandRelease(pHeap, VPoxSHGSMIBufferHeader(pvCmd));
}

/* do not wait for completion */
DECLINLINE(const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *)
vpoxSHGSMICommandPrepAsynch(PVPOXSHGSMI pHeap, VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    RT_NOREF(pHeap);
    /* ensure the command is not removed until we're processing it */
    vpoxSHGSMICommandRetain(pHeader);
    return pHeader;
}

DECLINLINE(void) vpoxSHGSMICommandDoneAsynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    if(!(ASMAtomicReadU32((volatile uint32_t *)&pHeader->fFlags) & VPOXSHGSMI_FLAG_HG_ASYNCH))
    {
        PFNVPOXSHGSMICMDCOMPLETION pfnCompletion = (PFNVPOXSHGSMICMDCOMPLETION)pHeader->u64Info1;
        if (pfnCompletion)
            pfnCompletion(pHeap, VPoxSHGSMIBufferData(pHeader), (PVOID)pHeader->u64Info2);
    }

    vpoxSHGSMICommandRelease(pHeap, (PVPOXSHGSMIHEADER)pHeader);
}

const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *
VPoxSHGSMICommandPrepAsynchEvent(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuff, RTSEMEVENT hEventSem)
{
    VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader = VPoxSHGSMIBufferHeader(pvBuff);
    pHeader->u64Info1 = (uint64_t)vpoxSHGSMICompletionSetEvent;
    pHeader->u64Info2 = (uintptr_t)hEventSem;
    pHeader->fFlags   = VPOXSHGSMI_FLAG_GH_ASYNCH_IRQ;

    return vpoxSHGSMICommandPrepAsynch(pHeap, pHeader);
}

const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMICommandPrepSynch(PVPOXSHGSMI pHeap,
                                                                              void  RT_UNTRUSTED_VOLATILE_HOST *pCmd)
{
    RTSEMEVENT hEventSem;
    int rc = RTSemEventCreate(&hEventSem);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
    {
        return VPoxSHGSMICommandPrepAsynchEvent(pHeap, pCmd, hEventSem);
    }
    return NULL;
}

void VPoxSHGSMICommandDoneAsynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    vpoxSHGSMICommandDoneAsynch(pHeap, pHeader);
}

int VPoxSHGSMICommandDoneSynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    VPoxSHGSMICommandDoneAsynch(pHeap, pHeader);
    RTSEMEVENT hEventSem = (RTSEMEVENT)pHeader->u64Info2;
    int rc = RTSemEventWait(hEventSem, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        RTSemEventDestroy(hEventSem);
    return rc;
}

void VPoxSHGSMICommandCancelAsynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    vpoxSHGSMICommandRelease(pHeap, (PVPOXSHGSMIHEADER)pHeader);
}

void VPoxSHGSMICommandCancelSynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    VPoxSHGSMICommandCancelAsynch(pHeap, pHeader);
    RTSEMEVENT hEventSem = (RTSEMEVENT)pHeader->u64Info2;
    RTSemEventDestroy(hEventSem);
}

const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *
VPoxSHGSMICommandPrepAsynch(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuff,
                            PFNVPOXSHGSMICMDCOMPLETION pfnCompletion,
                            void RT_UNTRUSTED_VOLATILE_HOST *pvCompletion, uint32_t fFlags)
{
    fFlags &= ~VPOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
    VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader = VPoxSHGSMIBufferHeader(pvBuff);
    pHeader->u64Info1 = (uintptr_t)pfnCompletion;
    pHeader->u64Info2 = (uintptr_t)pvCompletion;
    pHeader->fFlags = fFlags;

    return vpoxSHGSMICommandPrepAsynch(pHeap, pHeader);
}

const VPOXSHGSMIHEADER  RT_UNTRUSTED_VOLATILE_HOST *
VPoxSHGSMICommandPrepAsynchIrq(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuff,
                               PFNVPOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags |= VPOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ | VPOXSHGSMI_FLAG_GH_ASYNCH_IRQ;
    VPOXSHGSMIHEADER  RT_UNTRUSTED_VOLATILE_HOST *pHeader = VPoxSHGSMIBufferHeader(pvBuff);
    pHeader->u64Info1 = (uintptr_t)pfnCompletion;
    pHeader->u64Info2 = (uintptr_t)pvCompletion;
    /* we must assign rather than or because flags field does not get zeroed on command creation */
    pHeader->fFlags = fFlags;

    return vpoxSHGSMICommandPrepAsynch(pHeap, pHeader);
}

void RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMIHeapAlloc(PVPOXSHGSMI pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo)
{
    KIRQL OldIrql;
    void RT_UNTRUSTED_VOLATILE_HOST *pvData;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    pvData = HGSMIHeapAlloc(&pHeap->Heap, cbData, u8Channel, u16ChannelInfo);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
    if (!pvData)
        WARN(("HGSMIHeapAlloc failed!"));
    return pvData;
}

void VPoxSHGSMIHeapFree(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer)
{
    KIRQL OldIrql;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    HGSMIHeapFree(&pHeap->Heap, pvBuffer);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
}

void RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMIHeapBufferAlloc(PVPOXSHGSMI pHeap, HGSMISIZE cbData)
{
    KIRQL OldIrql;
    void RT_UNTRUSTED_VOLATILE_HOST * pvData;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    pvData = HGSMIHeapBufferAlloc(&pHeap->Heap, cbData);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
    if (!pvData)
        WARN(("HGSMIHeapAlloc failed!"));
    return pvData;
}

void VPoxSHGSMIHeapBufferFree(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer)
{
    KIRQL OldIrql;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    HGSMIHeapBufferFree(&pHeap->Heap, pvBuffer);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
}

int VPoxSHGSMIInit(PVPOXSHGSMI pHeap, void *pvBase, HGSMISIZE cbArea, HGSMIOFFSET offBase,
                   const HGSMIENV *pEnv)
{
    KeInitializeSpinLock(&pHeap->HeapLock);
    return HGSMIHeapSetup(&pHeap->Heap, pvBase, cbArea, offBase, pEnv);
}

void VPoxSHGSMITerm(PVPOXSHGSMI pHeap)
{
    HGSMIHeapDestroy(&pHeap->Heap);
}

void RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMICommandAlloc(PVPOXSHGSMI pHeap, HGSMISIZE cbData, uint8_t u8Channel,
                                                        uint16_t u16ChannelInfo)
{
    /* Issue the flush command. */
    VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader =
        (VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *)VPoxSHGSMIHeapAlloc(pHeap, cbData + sizeof(VPOXSHGSMIHEADER),
                                                                           u8Channel, u16ChannelInfo);
    Assert(pHeader);
    if (pHeader)
    {
        pHeader->cRefs = 1;
        return VPoxSHGSMIBufferData(pHeader);
    }
    return NULL;
}

void VPoxSHGSMICommandFree(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer)
{
    VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader = VPoxSHGSMIBufferHeader(pvBuffer);
    vpoxSHGSMICommandRelease(pHeap, pHeader);
}

#define VPOXSHGSMI_CMD2LISTENTRY(_pCmd) ((PVPOXVTLIST_ENTRY)&(_pCmd)->pvNext)
#define VPOXSHGSMI_LISTENTRY2CMD(_pEntry) ( (PVPOXSHGSMIHEADER)((uint8_t *)(_pEntry) - RT_UOFFSETOF(VPOXSHGSMIHEADER, pvNext)) )

int VPoxSHGSMICommandProcessCompletion(PVPOXSHGSMI pHeap, VPOXSHGSMIHEADER *pCur, bool bIrq, PVPOXVTLIST pPostProcessList)
{
    int rc = VINF_SUCCESS;

    do
    {
        if (pCur->fFlags & VPOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ)
        {
            Assert(bIrq);

            PFNVPOXSHGSMICMDCOMPLETION pfnCompletion = NULL;
            void *pvCompletion;
            PFNVPOXSHGSMICMDCOMPLETION_IRQ pfnCallback = (PFNVPOXSHGSMICMDCOMPLETION_IRQ)pCur->u64Info1;
            void *pvCallback = (void*)pCur->u64Info2;

            pfnCompletion = pfnCallback(pHeap, VPoxSHGSMIBufferData(pCur), pvCallback, &pvCompletion);
            if (pfnCompletion)
            {
                pCur->u64Info1 = (uintptr_t)pfnCompletion;
                pCur->u64Info2 = (uintptr_t)pvCompletion;
                pCur->fFlags &= ~VPOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
            }
            else
            {
                /* nothing to do with this command */
                break;
            }
        }

        if (!bIrq)
        {
            PFNVPOXSHGSMICMDCOMPLETION pfnCallback = (PFNVPOXSHGSMICMDCOMPLETION)pCur->u64Info1;
            void *pvCallback = (void*)pCur->u64Info2;
            pfnCallback(pHeap, VPoxSHGSMIBufferData(pCur), pvCallback);
        }
        else
            vpoxVtListPut(pPostProcessList, VPOXSHGSMI_CMD2LISTENTRY(pCur), VPOXSHGSMI_CMD2LISTENTRY(pCur));
    } while (0);


    return rc;
}

int VPoxSHGSMICommandPostprocessCompletion (PVPOXSHGSMI pHeap, PVPOXVTLIST pPostProcessList)
{
    PVPOXVTLIST_ENTRY pNext, pCur;
    for (pCur = pPostProcessList->pFirst; pCur; pCur = pNext)
    {
        /* need to save next since the command may be released in a pfnCallback and thus its data might be invalid */
        pNext = pCur->pNext;
        PVPOXSHGSMIHEADER pCmd = VPOXSHGSMI_LISTENTRY2CMD(pCur);
        PFNVPOXSHGSMICMDCOMPLETION pfnCallback = (PFNVPOXSHGSMICMDCOMPLETION)pCmd->u64Info1;
        void *pvCallback = (void*)pCmd->u64Info2;
        pfnCallback(pHeap, VPoxSHGSMIBufferData(pCmd), pvCallback);
    }

    return VINF_SUCCESS;
}
