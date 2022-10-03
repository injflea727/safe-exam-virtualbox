/* $Id: VPoxMPShgsmi.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPShgsmi_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPShgsmi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <VPoxVideo.h>

#include "common/VPoxMPUtils.h"

typedef struct VPOXSHGSMI
{
    KSPIN_LOCK HeapLock;
    HGSMIHEAP Heap;
} VPOXSHGSMI, *PVPOXSHGSMI;

typedef DECLCALLBACK(void) FNVPOXSHGSMICMDCOMPLETION(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvCmd, void *pvContext);
typedef FNVPOXSHGSMICMDCOMPLETION *PFNVPOXSHGSMICMDCOMPLETION;

typedef DECLCALLBACK(PFNVPOXSHGSMICMDCOMPLETION) FNVPOXSHGSMICMDCOMPLETION_IRQ(PVPOXSHGSMI pHeap,
                                                                               void RT_UNTRUSTED_VOLATILE_HOST *pvCmd,
                                                                               void *pvContext, void **ppvCompletion);
typedef FNVPOXSHGSMICMDCOMPLETION_IRQ *PFNVPOXSHGSMICMDCOMPLETION_IRQ;


const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *
    VPoxSHGSMICommandPrepAsynchEvent(PVPOXSHGSMI pHeap, PVOID pvBuff, RTSEMEVENT hEventSem);
const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *
    VPoxSHGSMICommandPrepSynch(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvCmd);
const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *
    VPoxSHGSMICommandPrepAsynch(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuff,
                                PFNVPOXSHGSMICMDCOMPLETION pfnCompletion, void RT_UNTRUSTED_VOLATILE_HOST *pvCompletion,
                                uint32_t fFlags);
const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *
    VPoxSHGSMICommandPrepAsynchIrq(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuff,
                                   PFNVPOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags);

void VPoxSHGSMICommandDoneAsynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader);
int  VPoxSHGSMICommandDoneSynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader);
void VPoxSHGSMICommandCancelAsynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader);
void VPoxSHGSMICommandCancelSynch(PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader);

DECLINLINE(HGSMIOFFSET) VPoxSHGSMICommandOffset(const PVPOXSHGSMI pHeap, const VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_HOST *pHeader)
{
    return HGSMIHeapBufferOffset(&pHeap->Heap, (void*)pHeader);
}

/* allows getting VRAM offset of arbitrary pointer within the SHGSMI command
 * if invalid pointer is passed in, behavior is undefined */
DECLINLINE(HGSMIOFFSET) VPoxSHGSMICommandPtrOffset(const PVPOXSHGSMI pHeap, const void RT_UNTRUSTED_VOLATILE_HOST *pvPtr)
{
    return HGSMIPointerToOffset(&pHeap->Heap.area, (const HGSMIBUFFERHEADER RT_UNTRUSTED_VOLATILE_HOST *)pvPtr);
}

int VPoxSHGSMIInit(PVPOXSHGSMI pHeap, void *pvBase, HGSMISIZE cbArea, HGSMIOFFSET offBase, const HGSMIENV *pEnv);
void VPoxSHGSMITerm(PVPOXSHGSMI pHeap);
void RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMIHeapAlloc(PVPOXSHGSMI pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo);
void VPoxSHGSMIHeapFree(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer);
void RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMIHeapBufferAlloc(PVPOXSHGSMI pHeap, HGSMISIZE cbData);
void VPoxSHGSMIHeapBufferFree(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer);
void RT_UNTRUSTED_VOLATILE_HOST *VPoxSHGSMICommandAlloc(PVPOXSHGSMI pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo);
void VPoxSHGSMICommandFree(PVPOXSHGSMI pHeap, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer);
int VPoxSHGSMICommandProcessCompletion(PVPOXSHGSMI pHeap, VPOXSHGSMIHEADER* pCmd, bool bIrq, struct VPOXVTLIST * pPostProcessList);
int VPoxSHGSMICommandPostprocessCompletion(PVPOXSHGSMI pHeap, struct VPOXVTLIST * pPostProcessList);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPShgsmi_h */
