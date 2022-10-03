/* $Id: VPoxMPLegacy.cpp $ */
/** @file
 * VPox WDDM Miniport driver. The legacy VPoxVGA adapter support with 2D software unaccelerated
 * framebuffer operations.
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
#include "common/VPoxMPCommon.h"
#include "common/VPoxMPHGSMI.h"
#ifdef VPOX_WITH_VIDEOHWACCEL
# include "VPoxMPVhwa.h"
#endif
#include "VPoxMPVidPn.h"
#include "VPoxMPLegacy.h"

#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/param.h>
#include <iprt/initterm.h>

#include <VPox/VPoxGuestLib.h>
#include <VPox/VMMDev.h> /* for VMMDevVideoSetVisibleRegion */
#include <VPoxVideo.h>
#include <wingdi.h> /* needed for RGNDATA definition */
#include <VPoxDisplay.h> /* this is from Additions/WINNT/include/ to include escape codes */
#include <VPoxVideoVBE.h>
#include <VPox/Version.h>


/* ddi dma command queue handling */
typedef enum
{
    VPOXVDMADDI_STATE_UNCKNOWN = 0,
    VPOXVDMADDI_STATE_NOT_DX_CMD,
    VPOXVDMADDI_STATE_NOT_QUEUED,
    VPOXVDMADDI_STATE_PENDING,
    VPOXVDMADDI_STATE_SUBMITTED,
    VPOXVDMADDI_STATE_COMPLETED
} VPOXVDMADDI_STATE;

typedef struct VPOXVDMADDI_CMD *PVPOXVDMADDI_CMD;
typedef DECLCALLBACK(VOID) FNVPOXVDMADDICMDCOMPLETE_DPC(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd, PVOID pvContext);
typedef FNVPOXVDMADDICMDCOMPLETE_DPC *PFNVPOXVDMADDICMDCOMPLETE_DPC;

typedef struct VPOXVDMADDI_CMD
{
    LIST_ENTRY QueueEntry;
    VPOXVDMADDI_STATE enmState;
    uint32_t u32NodeOrdinal;
    uint32_t u32FenceId;
    DXGK_INTERRUPT_TYPE enmComplType;
    PFNVPOXVDMADDICMDCOMPLETE_DPC pfnComplete;
    PVOID pvComplete;
} VPOXVDMADDI_CMD, *PVPOXVDMADDI_CMD;

#define VPOXVDMADDI_CMD_FROM_ENTRY(_pEntry) ((PVPOXVDMADDI_CMD)(((uint8_t*)(_pEntry)) - RT_OFFSETOF(VPOXVDMADDI_CMD, QueueEntry)))

typedef struct VPOXWDDM_DMA_ALLOCINFO
{
    PVPOXWDDM_ALLOCATION pAlloc;
    VPOXVIDEOOFFSET offAlloc;
    UINT segmentIdAlloc : 31;
    UINT fWriteOp : 1;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
} VPOXWDDM_DMA_ALLOCINFO, *PVPOXWDDM_DMA_ALLOCINFO;

typedef struct VPOXVDMAPIPE_RECTS
{
    RECT ContextRect;
    VPOXWDDM_RECTS_INFO UpdateRects;
} VPOXVDMAPIPE_RECTS, *PVPOXVDMAPIPE_RECTS;

typedef struct VPOXVDMA_CLRFILL
{
    VPOXWDDM_DMA_ALLOCINFO Alloc;
    UINT Color;
    VPOXWDDM_RECTS_INFO Rects;
} VPOXVDMA_CLRFILL, *PVPOXVDMA_CLRFILL;

typedef struct VPOXVDMA_BLT
{
    VPOXWDDM_DMA_ALLOCINFO SrcAlloc;
    VPOXWDDM_DMA_ALLOCINFO DstAlloc;
    RECT SrcRect;
    VPOXVDMAPIPE_RECTS DstRects;
} VPOXVDMA_BLT, *PVPOXVDMA_BLT;

typedef struct VPOXVDMA_FLIP
{
    VPOXWDDM_DMA_ALLOCINFO Alloc;
} VPOXVDMA_FLIP, *PVPOXVDMA_FLIP;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR
{
    VPOXWDDM_DMA_PRIVATEDATA_BASEHDR BaseHdr;
}VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR, *PVPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_BLT
{
    VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VPOXVDMA_BLT Blt;
} VPOXWDDM_DMA_PRIVATEDATA_BLT, *PVPOXWDDM_DMA_PRIVATEDATA_BLT;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_FLIP
{
    VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VPOXVDMA_FLIP Flip;
} VPOXWDDM_DMA_PRIVATEDATA_FLIP, *PVPOXWDDM_DMA_PRIVATEDATA_FLIP;

typedef struct VPOXWDDM_DMA_PRIVATEDATA_CLRFILL
{
    VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VPOXVDMA_CLRFILL ClrFill;
} VPOXWDDM_DMA_PRIVATEDATA_CLRFILL, *PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL;

typedef enum
{
    VPOXWDDM_HGSMICMD_TYPE_UNDEFINED = 0,
    VPOXWDDM_HGSMICMD_TYPE_CTL       = 1,
} VPOXWDDM_HGSMICMD_TYPE;

VPOXWDDM_HGSMICMD_TYPE vpoxWddmHgsmiGetCmdTypeFromOffset(PVPOXMP_DEVEXT pDevExt, HGSMIOFFSET offCmd)
{
    if (HGSMIAreaContainsOffset(&VPoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx.Heap.area, offCmd))
        return VPOXWDDM_HGSMICMD_TYPE_CTL;
    return VPOXWDDM_HGSMICMD_TYPE_UNDEFINED;
}

VOID vpoxVdmaDdiNodesInit(PVPOXMP_DEVEXT pDevExt)
{
    for (UINT i = 0; i < RT_ELEMENTS(pDevExt->aNodes); ++i)
    {
        pDevExt->aNodes[i].uLastCompletedFenceId = 0;
        PVPOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[i].CmdQueue;
        pQueue->cQueuedCmds = 0;
        InitializeListHead(&pQueue->CmdQueue);
    }
    InitializeListHead(&pDevExt->DpcCmdQueue);
}

static VOID vpoxVdmaDdiCmdNotifyCompletedIrq(PVPOXMP_DEVEXT pDevExt, UINT u32NodeOrdinal, UINT u32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    PVPOXVDMADDI_NODE pNode = &pDevExt->aNodes[u32NodeOrdinal];
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
    switch (enmComplType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
            notify.DmaCompleted.SubmissionFenceId = u32FenceId;
            notify.DmaCompleted.NodeOrdinal = u32NodeOrdinal;
            pNode->uLastCompletedFenceId = u32FenceId;
            break;

        case DXGK_INTERRUPT_DMA_PREEMPTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_PREEMPTED;
            notify.DmaPreempted.PreemptionFenceId = u32FenceId;
            notify.DmaPreempted.NodeOrdinal = u32NodeOrdinal;
            notify.DmaPreempted.LastCompletedFenceId = pNode->uLastCompletedFenceId;
            break;

        case DXGK_INTERRUPT_DMA_FAULTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_FAULTED;
            notify.DmaFaulted.FaultedFenceId = u32FenceId;
            notify.DmaFaulted.Status = STATUS_UNSUCCESSFUL; /** @todo better status ? */
            notify.DmaFaulted.NodeOrdinal = u32NodeOrdinal;
            break;

        default:
            Assert(0);
            break;
    }

    pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
}

static VOID vpoxVdmaDdiCmdProcessCompletedIrq(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    vpoxVdmaDdiCmdNotifyCompletedIrq(pDevExt, pCmd->u32NodeOrdinal, pCmd->u32FenceId, enmComplType);
    switch (enmComplType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            InsertTailList(&pDevExt->DpcCmdQueue, &pCmd->QueueEntry);
            break;
        default:
            AssertFailed();
            break;
    }
}

DECLINLINE(VOID) vpoxVdmaDdiCmdDequeueIrq(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd)
{
    PVPOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[pCmd->u32NodeOrdinal].CmdQueue;
    ASMAtomicDecU32(&pQueue->cQueuedCmds);
    RemoveEntryList(&pCmd->QueueEntry);
}

DECLINLINE(VOID) vpoxVdmaDdiCmdEnqueueIrq(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd)
{
    PVPOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[pCmd->u32NodeOrdinal].CmdQueue;
    ASMAtomicIncU32(&pQueue->cQueuedCmds);
    InsertTailList(&pQueue->CmdQueue, &pCmd->QueueEntry);
}

static BOOLEAN vpoxVdmaDdiCmdCompletedIrq(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    if (VPOXVDMADDI_STATE_NOT_DX_CMD == pCmd->enmState)
    {
        InsertTailList(&pDevExt->DpcCmdQueue, &pCmd->QueueEntry);
        return FALSE;
    }

    PVPOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[pCmd->u32NodeOrdinal].CmdQueue;
    BOOLEAN bQueued = pCmd->enmState > VPOXVDMADDI_STATE_NOT_QUEUED;
    BOOLEAN bComplete = FALSE;
    Assert(!bQueued || pQueue->cQueuedCmds);
    Assert(!bQueued || !IsListEmpty(&pQueue->CmdQueue));
    pCmd->enmState = VPOXVDMADDI_STATE_COMPLETED;
    if (bQueued)
    {
        if (pQueue->CmdQueue.Flink == &pCmd->QueueEntry)
        {
            vpoxVdmaDdiCmdDequeueIrq(pDevExt, pCmd);
            bComplete = TRUE;
        }
    }
    else if (IsListEmpty(&pQueue->CmdQueue))
    {
        bComplete = TRUE;
    }
    else
    {
        vpoxVdmaDdiCmdEnqueueIrq(pDevExt, pCmd);
    }

    if (bComplete)
    {
        vpoxVdmaDdiCmdProcessCompletedIrq(pDevExt, pCmd, enmComplType);

        while (!IsListEmpty(&pQueue->CmdQueue))
        {
            pCmd = VPOXVDMADDI_CMD_FROM_ENTRY(pQueue->CmdQueue.Flink);
            if (pCmd->enmState == VPOXVDMADDI_STATE_COMPLETED)
            {
                vpoxVdmaDdiCmdDequeueIrq(pDevExt, pCmd);
                vpoxVdmaDdiCmdProcessCompletedIrq(pDevExt, pCmd, pCmd->enmComplType);
            }
            else
                break;
        }
    }
    else
    {
        pCmd->enmState = VPOXVDMADDI_STATE_COMPLETED;
        pCmd->enmComplType = enmComplType;
    }

    return bComplete;
}

typedef struct VPOXVDMADDI_CMD_COMPLETED_CB
{
    PVPOXMP_DEVEXT pDevExt;
    PVPOXVDMADDI_CMD pCmd;
    DXGK_INTERRUPT_TYPE enmComplType;
} VPOXVDMADDI_CMD_COMPLETED_CB, *PVPOXVDMADDI_CMD_COMPLETED_CB;

static BOOLEAN vpoxVdmaDdiCmdCompletedCb(PVOID Context)
{
    PVPOXVDMADDI_CMD_COMPLETED_CB pdc = (PVPOXVDMADDI_CMD_COMPLETED_CB)Context;
    PVPOXMP_DEVEXT pDevExt = pdc->pDevExt;
    BOOLEAN bNeedDpc = vpoxVdmaDdiCmdCompletedIrq(pDevExt, pdc->pCmd, pdc->enmComplType);
    pDevExt->bNotifyDxDpc |= bNeedDpc;

    if (bNeedDpc)
    {
        pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
    }

    return bNeedDpc;
}

static NTSTATUS vpoxVdmaDdiCmdCompleted(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    VPOXVDMADDI_CMD_COMPLETED_CB context;
    context.pDevExt = pDevExt;
    context.pCmd = pCmd;
    context.enmComplType = enmComplType;
    BOOLEAN bNeedDps;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vpoxVdmaDdiCmdCompletedCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bNeedDps);
    AssertNtStatusSuccess(Status);
    return Status;
}

DECLINLINE(VOID) vpoxVdmaDdiCmdInit(PVPOXVDMADDI_CMD pCmd, uint32_t u32NodeOrdinal, uint32_t u32FenceId,
                                    PFNVPOXVDMADDICMDCOMPLETE_DPC pfnComplete, PVOID pvComplete)
{
    pCmd->QueueEntry.Blink = NULL;
    pCmd->QueueEntry.Flink = NULL;
    pCmd->enmState = VPOXVDMADDI_STATE_NOT_QUEUED;
    pCmd->u32NodeOrdinal = u32NodeOrdinal;
    pCmd->u32FenceId = u32FenceId;
    pCmd->pfnComplete = pfnComplete;
    pCmd->pvComplete = pvComplete;
}

static DECLCALLBACK(VOID) vpoxVdmaDdiCmdCompletionCbFree(PVPOXMP_DEVEXT pDevExt, PVPOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    RT_NOREF(pDevExt, pvContext);
    vpoxWddmMemFree(pCmd);
}

DECLINLINE(BOOLEAN) vpoxVdmaDdiCmdCanComplete(PVPOXMP_DEVEXT pDevExt, UINT u32NodeOrdinal)
{
    PVPOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[u32NodeOrdinal].CmdQueue;
    return ASMAtomicUoReadU32(&pQueue->cQueuedCmds) == 0;
}

typedef struct VPOXVDMADDI_CMD_COMPLETE_CB
{
    PVPOXMP_DEVEXT pDevExt;
    UINT u32NodeOrdinal;
    uint32_t u32FenceId;
} VPOXVDMADDI_CMD_COMPLETE_CB, *PVPOXVDMADDI_CMD_COMPLETE_CB;

static BOOLEAN vpoxVdmaDdiCmdFenceCompleteCb(PVOID Context)
{
    PVPOXVDMADDI_CMD_COMPLETE_CB pdc = (PVPOXVDMADDI_CMD_COMPLETE_CB)Context;
    PVPOXMP_DEVEXT pDevExt = pdc->pDevExt;

    vpoxVdmaDdiCmdNotifyCompletedIrq(pDevExt, pdc->u32NodeOrdinal, pdc->u32FenceId, DXGK_INTERRUPT_DMA_COMPLETED);

    pDevExt->bNotifyDxDpc = TRUE;
    pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

    return TRUE;
}

static NTSTATUS vpoxVdmaDdiCmdFenceNotifyComplete(PVPOXMP_DEVEXT pDevExt, uint32_t u32NodeOrdinal, uint32_t u32FenceId)
{
    VPOXVDMADDI_CMD_COMPLETE_CB context;
    context.pDevExt = pDevExt;
    context.u32NodeOrdinal = u32NodeOrdinal;
    context.u32FenceId = u32FenceId;
    BOOLEAN bRet;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vpoxVdmaDdiCmdFenceCompleteCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    AssertNtStatusSuccess(Status);
    return Status;
}

static NTSTATUS vpoxVdmaDdiCmdFenceComplete(PVPOXMP_DEVEXT pDevExt, uint32_t u32NodeOrdinal, uint32_t u32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    if (vpoxVdmaDdiCmdCanComplete(pDevExt, u32NodeOrdinal))
        return vpoxVdmaDdiCmdFenceNotifyComplete(pDevExt, u32NodeOrdinal, u32FenceId);

    PVPOXVDMADDI_CMD pCmd = (PVPOXVDMADDI_CMD)vpoxWddmMemAlloc(sizeof (VPOXVDMADDI_CMD));
    Assert(pCmd);
    if (pCmd)
    {
        vpoxVdmaDdiCmdInit(pCmd, u32NodeOrdinal, u32FenceId, vpoxVdmaDdiCmdCompletionCbFree, NULL);
        NTSTATUS Status = vpoxVdmaDdiCmdCompleted(pDevExt, pCmd, enmComplType);
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
        vpoxWddmMemFree(pCmd);
        return Status;
    }
    return STATUS_NO_MEMORY;
}

NTSTATUS vpoxVdmaGgDmaBltPerform(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_ALLOC_DATA pSrcAlloc, RECT* pSrcRect,
        PVPOXWDDM_ALLOC_DATA pDstAlloc, RECT* pDstRect)
{
    uint8_t* pvVramBase = pDevExt->pvVisibleVram;
    /* we do not support stretching */
    uint32_t srcWidth = pSrcRect->right - pSrcRect->left;
    uint32_t srcHeight = pSrcRect->bottom - pSrcRect->top;
    uint32_t dstWidth = pDstRect->right - pDstRect->left;
    uint32_t dstHeight = pDstRect->bottom - pDstRect->top;
    Assert(srcHeight == dstHeight);
    Assert(dstWidth == srcWidth);
    Assert(pDstAlloc->Addr.offVram != VPOXVIDEOOFFSET_VOID);
    Assert(pSrcAlloc->Addr.offVram != VPOXVIDEOOFFSET_VOID);
    D3DDDIFORMAT enmSrcFormat, enmDstFormat;

    enmSrcFormat = pSrcAlloc->SurfDesc.format;
    enmDstFormat = pDstAlloc->SurfDesc.format;

    if (pDstAlloc->Addr.SegmentId && pDstAlloc->Addr.SegmentId != 1)
    {
        WARN(("request to collor blit invalid allocation"));
        return STATUS_INVALID_PARAMETER;
    }

    if (pSrcAlloc->Addr.SegmentId && pSrcAlloc->Addr.SegmentId != 1)
    {
        WARN(("request to collor blit invalid allocation"));
        return STATUS_INVALID_PARAMETER;
    }

    if (enmSrcFormat != enmDstFormat)
    {
        /* just ignore the alpha component
         * this is ok since our software-based stuff can not handle alpha channel in any way */
        enmSrcFormat = vpoxWddmFmtNoAlphaFormat(enmSrcFormat);
        enmDstFormat = vpoxWddmFmtNoAlphaFormat(enmDstFormat);
        if (enmSrcFormat != enmDstFormat)
        {
            WARN(("color conversion src(%d), dst(%d) not supported!", pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format));
            return STATUS_INVALID_PARAMETER;
        }
    }
    if (srcHeight != dstHeight)
            return STATUS_INVALID_PARAMETER;
    if (srcWidth != dstWidth)
            return STATUS_INVALID_PARAMETER;
    if (pDstAlloc->Addr.offVram == VPOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;
    if (pSrcAlloc->Addr.offVram == VPOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    uint8_t *pvDstSurf = pDstAlloc->Addr.SegmentId ? pvVramBase + pDstAlloc->Addr.offVram : (uint8_t*)pDstAlloc->Addr.pvMem;
    uint8_t *pvSrcSurf = pSrcAlloc->Addr.SegmentId ? pvVramBase + pSrcAlloc->Addr.offVram : (uint8_t*)pSrcAlloc->Addr.pvMem;

    if (pDstAlloc->SurfDesc.width == dstWidth
            && pSrcAlloc->SurfDesc.width == srcWidth
            && pSrcAlloc->SurfDesc.width == pDstAlloc->SurfDesc.width)
    {
        Assert(!pDstRect->left);
        Assert(!pSrcRect->left);
        uint32_t cbDstOff = vpoxWddmCalcOffXYrd(0 /* x */, pDstRect->top, pDstAlloc->SurfDesc.pitch, pDstAlloc->SurfDesc.format);
        uint32_t cbSrcOff = vpoxWddmCalcOffXYrd(0 /* x */, pSrcRect->top, pSrcAlloc->SurfDesc.pitch, pSrcAlloc->SurfDesc.format);
        uint32_t cbSize = vpoxWddmCalcSize(pDstAlloc->SurfDesc.pitch, dstHeight, pDstAlloc->SurfDesc.format);
        memcpy(pvDstSurf + cbDstOff, pvSrcSurf + cbSrcOff, cbSize);
    }
    else
    {
        uint32_t cbDstLine =  vpoxWddmCalcRowSize(pDstRect->left, pDstRect->right, pDstAlloc->SurfDesc.format);
        uint32_t offDstStart = vpoxWddmCalcOffXYrd(pDstRect->left, pDstRect->top, pDstAlloc->SurfDesc.pitch, pDstAlloc->SurfDesc.format);
        Assert(cbDstLine <= pDstAlloc->SurfDesc.pitch);
        uint32_t cbDstSkip = pDstAlloc->SurfDesc.pitch;
        uint8_t * pvDstStart = pvDstSurf + offDstStart;

        uint32_t cbSrcLine = vpoxWddmCalcRowSize(pSrcRect->left, pSrcRect->right, pSrcAlloc->SurfDesc.format);
        uint32_t offSrcStart = vpoxWddmCalcOffXYrd(pSrcRect->left, pSrcRect->top, pSrcAlloc->SurfDesc.pitch, pSrcAlloc->SurfDesc.format);
        Assert(cbSrcLine <= pSrcAlloc->SurfDesc.pitch); NOREF(cbSrcLine);
        uint32_t cbSrcSkip = pSrcAlloc->SurfDesc.pitch;
        const uint8_t * pvSrcStart = pvSrcSurf + offSrcStart;

        uint32_t cRows = vpoxWddmCalcNumRows(pDstRect->top, pDstRect->bottom, pDstAlloc->SurfDesc.format);

        Assert(cbDstLine == cbSrcLine);

        for (uint32_t i = 0; i < cRows; ++i)
        {
            memcpy(pvDstStart, pvSrcStart, cbDstLine);
            pvDstStart += cbDstSkip;
            pvSrcStart += cbSrcSkip;
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS vpoxVdmaGgDmaColorFill(PVPOXMP_DEVEXT pDevExt, VPOXVDMA_CLRFILL *pCF)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    Assert (pDevExt->pvVisibleVram);
    if (pDevExt->pvVisibleVram)
    {
        PVPOXWDDM_ALLOCATION pAlloc = pCF->Alloc.pAlloc;
        if (pAlloc->AllocData.Addr.SegmentId && pAlloc->AllocData.Addr.SegmentId != 1)
        {
            WARN(("request to collor fill invalid allocation"));
            return STATUS_INVALID_PARAMETER;
        }

        VPOXVIDEOOFFSET offVram = vpoxWddmAddrFramOffset(&pAlloc->AllocData.Addr);
        if (offVram != VPOXVIDEOOFFSET_VOID)
        {
            RECT UnionRect = {0};
            uint8_t *pvMem = pDevExt->pvVisibleVram + offVram;
            UINT bpp = pAlloc->AllocData.SurfDesc.bpp;
            Assert(bpp);
            Assert(((bpp * pAlloc->AllocData.SurfDesc.width) >> 3) == pAlloc->AllocData.SurfDesc.pitch);
            switch (bpp)
            {
                case 32:
                {
                    uint8_t bytestPP = bpp >> 3;
                    for (UINT i = 0; i < pCF->Rects.cRects; ++i)
                    {
                        RECT *pRect = &pCF->Rects.aRects[i];
                        for (LONG ir = pRect->top; ir < pRect->bottom; ++ir)
                        {
                            uint32_t * pvU32Mem = (uint32_t*)(pvMem + (ir * pAlloc->AllocData.SurfDesc.pitch) + (pRect->left * bytestPP));
                            uint32_t cRaw = pRect->right - pRect->left;
                            Assert(pRect->left >= 0);
                            Assert(pRect->right <= (LONG)pAlloc->AllocData.SurfDesc.width);
                            Assert(pRect->top >= 0);
                            Assert(pRect->bottom <= (LONG)pAlloc->AllocData.SurfDesc.height);
                            for (UINT j = 0; j < cRaw; ++j)
                            {
                                *pvU32Mem = pCF->Color;
                                ++pvU32Mem;
                            }
                        }
                        vpoxWddmRectUnited(&UnionRect, &UnionRect, pRect);
                    }
                    Status = STATUS_SUCCESS;
                    break;
                }
                case 16:
                case 8:
                default:
                    AssertBreakpoint();
                    break;
            }

            if (Status == STATUS_SUCCESS)
            {
                if (pAlloc->AllocData.SurfDesc.VidPnSourceId != D3DDDI_ID_UNINITIALIZED
                        && VPOXWDDM_IS_FB_ALLOCATION(pDevExt, pAlloc)
                        && pAlloc->bVisible
                        )
                {
                    if (!vpoxWddmRectIsEmpty(&UnionRect))
                    {
                        PVPOXWDDM_SOURCE pSource = &pDevExt->aSources[pCF->Alloc.pAlloc->AllocData.SurfDesc.VidPnSourceId];
                        uint32_t cUnlockedVBVADisabled = ASMAtomicReadU32(&pDevExt->cUnlockedVBVADisabled);
                        if (!cUnlockedVBVADisabled)
                        {
                            VPOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &UnionRect);
                        }
                        else
                        {
                            VPOXVBVA_OP_WITHLOCK(ReportDirtyRect, pDevExt, pSource, &UnionRect);
                        }
                    }
                }
                else
                {
                    AssertBreakpoint();
                }
            }
        }
        else
            WARN(("invalid offVram"));
    }

    return Status;
}

static void vpoxVdmaBltDirtyRectsUpdate(PVPOXMP_DEVEXT pDevExt, VPOXWDDM_SOURCE *pSource, uint32_t cRects, const RECT *paRects)
{
    if (!cRects)
    {
        WARN(("vpoxVdmaBltDirtyRectsUpdate: no rects specified"));
        return;
    }

    RECT rect;
    rect = paRects[0];
    for (UINT i = 1; i < cRects; ++i)
    {
        vpoxWddmRectUnited(&rect, &rect, &paRects[i]);
    }

    uint32_t cUnlockedVBVADisabled = ASMAtomicReadU32(&pDevExt->cUnlockedVBVADisabled);
    if (!cUnlockedVBVADisabled)
    {
        VPOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &rect);
    }
    else
    {
        VPOXVBVA_OP_WITHLOCK_ATDPC(ReportDirtyRect, pDevExt, pSource, &rect);
    }
}

/*
 * @return on success the number of bytes the command contained, otherwise - VERR_xxx error code
 */
static NTSTATUS vpoxVdmaGgDmaBlt(PVPOXMP_DEVEXT pDevExt, PVPOXVDMA_BLT pBlt)
{
    /* we do not support stretching for now */
    Assert(pBlt->SrcRect.right - pBlt->SrcRect.left == pBlt->DstRects.ContextRect.right - pBlt->DstRects.ContextRect.left);
    Assert(pBlt->SrcRect.bottom - pBlt->SrcRect.top == pBlt->DstRects.ContextRect.bottom - pBlt->DstRects.ContextRect.top);
    if (pBlt->SrcRect.right - pBlt->SrcRect.left != pBlt->DstRects.ContextRect.right - pBlt->DstRects.ContextRect.left)
        return STATUS_INVALID_PARAMETER;
    if (pBlt->SrcRect.bottom - pBlt->SrcRect.top != pBlt->DstRects.ContextRect.bottom - pBlt->DstRects.ContextRect.top)
        return STATUS_INVALID_PARAMETER;
    Assert(pBlt->DstRects.UpdateRects.cRects);

    NTSTATUS Status = STATUS_SUCCESS;

    if (pBlt->DstRects.UpdateRects.cRects)
    {
        for (uint32_t i = 0; i < pBlt->DstRects.UpdateRects.cRects; ++i)
        {
            RECT SrcRect;
            vpoxWddmRectTranslated(&SrcRect, &pBlt->DstRects.UpdateRects.aRects[i], -pBlt->DstRects.ContextRect.left, -pBlt->DstRects.ContextRect.top);

            Status = vpoxVdmaGgDmaBltPerform(pDevExt, &pBlt->SrcAlloc.pAlloc->AllocData, &SrcRect,
                    &pBlt->DstAlloc.pAlloc->AllocData, &pBlt->DstRects.UpdateRects.aRects[i]);
            AssertNtStatusSuccess(Status);
            if (Status != STATUS_SUCCESS)
                return Status;
        }
    }
    else
    {
        Status = vpoxVdmaGgDmaBltPerform(pDevExt, &pBlt->SrcAlloc.pAlloc->AllocData, &pBlt->SrcRect,
                &pBlt->DstAlloc.pAlloc->AllocData, &pBlt->DstRects.ContextRect);
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
            return Status;
    }

    return Status;
}

static NTSTATUS vpoxVdmaProcessBltCmd(PVPOXMP_DEVEXT pDevExt, VPOXWDDM_CONTEXT *pContext, VPOXWDDM_DMA_PRIVATEDATA_BLT *pBlt)
{
    RT_NOREF(pContext);
    NTSTATUS Status = STATUS_SUCCESS;
    PVPOXWDDM_ALLOCATION pDstAlloc = pBlt->Blt.DstAlloc.pAlloc;
//    PVPOXWDDM_ALLOCATION pSrcAlloc = pBlt->Blt.SrcAlloc.pAlloc;
    {
        /* the allocations contain a real data in VRAM, do blitting */
        vpoxVdmaGgDmaBlt(pDevExt, &pBlt->Blt);

        if (pDstAlloc->bAssigned && pDstAlloc->bVisible)
        {
            /* Only for visible framebuffer allocations. */
            VPOXWDDM_SOURCE *pSource = &pDevExt->aSources[pDstAlloc->AllocData.SurfDesc.VidPnSourceId];
            /* Assert but otherwise ignore wrong allocations. */
            AssertReturn(pDstAlloc->AllocData.SurfDesc.VidPnSourceId < VPOX_VIDEO_MAX_SCREENS, STATUS_SUCCESS);
            AssertReturn(pSource->pPrimaryAllocation == pDstAlloc, STATUS_SUCCESS);
            vpoxVdmaBltDirtyRectsUpdate(pDevExt, pSource, pBlt->Blt.DstRects.UpdateRects.cRects, pBlt->Blt.DstRects.UpdateRects.aRects);
        }
    }
    return Status;
}

static NTSTATUS vpoxVdmaProcessFlipCmd(PVPOXMP_DEVEXT pDevExt, VPOXWDDM_CONTEXT *pContext, VPOXWDDM_DMA_PRIVATEDATA_FLIP *pFlip)
{
    RT_NOREF(pContext);
    NTSTATUS Status = STATUS_SUCCESS;
    PVPOXWDDM_ALLOCATION pAlloc = pFlip->Flip.Alloc.pAlloc;
    VPOXWDDM_SOURCE *pSource = &pDevExt->aSources[pAlloc->AllocData.SurfDesc.VidPnSourceId];
    vpoxWddmAssignPrimary(pSource, pAlloc, pAlloc->AllocData.SurfDesc.VidPnSourceId);
    {
        WARN(("unexpected flip request"));
    }

    return Status;
}

static NTSTATUS vpoxVdmaProcessClrFillCmd(PVPOXMP_DEVEXT pDevExt, VPOXWDDM_CONTEXT *pContext, VPOXWDDM_DMA_PRIVATEDATA_CLRFILL *pCF)
{
    RT_NOREF(pContext);
    NTSTATUS Status = STATUS_SUCCESS;
//    PVPOXWDDM_ALLOCATION pAlloc = pCF->ClrFill.Alloc.pAlloc;
    {
        Status = vpoxVdmaGgDmaColorFill(pDevExt, &pCF->ClrFill);
        if (!NT_SUCCESS(Status))
            WARN(("vpoxVdmaGgDmaColorFill failed Status 0x%x", Status));
    }

    return Status;
}

static void vpoxWddmPatchLocationInit(D3DDDI_PATCHLOCATIONLIST *pPatchLocationListOut, UINT idx, UINT offPatch)
{
    memset(pPatchLocationListOut, 0, sizeof (*pPatchLocationListOut));
    pPatchLocationListOut->AllocationIndex = idx;
    pPatchLocationListOut->PatchOffset = offPatch;
}

static void vpoxWddmPopulateDmaAllocInfo(PVPOXWDDM_DMA_ALLOCINFO pInfo, PVPOXWDDM_ALLOCATION pAlloc, DXGK_ALLOCATIONLIST *pDmaAlloc)
{
    pInfo->pAlloc = pAlloc;
    if (pDmaAlloc->SegmentId)
    {
        pInfo->offAlloc = (VPOXVIDEOOFFSET)pDmaAlloc->PhysicalAddress.QuadPart;
        pInfo->segmentIdAlloc = pDmaAlloc->SegmentId;
    }
    else
        pInfo->segmentIdAlloc = 0;
    pInfo->srcId = pAlloc->AllocData.SurfDesc.VidPnSourceId;
}

/*
 *
 * DxgkDdi
 *
 */

NTSTATUS
APIENTRY
DxgkDdiBuildPagingBufferLegacy(
    CONST HANDLE  hAdapter,
    DXGKARG_BUILDPAGINGBUFFER*  pBuildPagingBuffer)
{
    /* DxgkDdiBuildPagingBuffer should be made pageable. */
    PAGED_CODE();

    vpoxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    RT_NOREF(hAdapter);
//    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)hAdapter;

    LOGF(("ENTER, context(0x%x)", hAdapter));

    uint32_t cbCmdDma = 0;

    /** @todo */
    switch (pBuildPagingBuffer->Operation)
    {
        case DXGK_OPERATION_TRANSFER:
        {
            cbCmdDma = VPOXWDDM_DUMMY_DMABUFFER_SIZE;
#ifdef VPOX_WITH_VDMA
            PVPOXWDDM_ALLOCATION pAlloc = (PVPOXWDDM_ALLOCATION)pBuildPagingBuffer->Transfer.hAllocation;
            Assert(pAlloc);
            if (pAlloc
                    && !pAlloc->fRcFlags.Overlay /* overlay surfaces actually contain a valid data */
                    && pAlloc->enmType != VPOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE  /* shadow primary - also */
                    && pAlloc->enmType != VPOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER /* hgsmi buffer - also */
                    )
            {
                /* we do not care about the others for now */
                Status = STATUS_SUCCESS;
                break;
            }
            UINT cbCmd = VPOXVDMACMD_SIZE(VPOXVDMACMD_DMA_BPB_TRANSFER);
            VPOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_HOST *pDr = vpoxVdmaCBufDrCreate(&pDevExt->u.primary.Vdma, cbCmd);
            Assert(pDr);
            if (pDr)
            {
                SIZE_T cbTransfered = 0;
                SIZE_T cbTransferSize = pBuildPagingBuffer->Transfer.TransferSize;
                VPOXVDMACMD RT_UNTRUSTED_VOLATILE_HOST *pHdr = VPOXVDMACBUF_DR_TAIL(pDr, VPOXVDMACMD);
                do
                {
                    // vpoxVdmaCBufDrCreate zero initializes the pDr
                    pDr->fFlags = VPOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
                    pDr->cbBuf = cbCmd;
                    pDr->rc = VERR_NOT_IMPLEMENTED;

                    pHdr->enmType = VPOXVDMACMD_TYPE_DMA_BPB_TRANSFER;
                    pHdr->u32CmdSpecific = 0;
                    VPOXVDMACMD_DMA_BPB_TRANSFER RT_UNTRUSTED_VOLATILE_HOST *pBody
                        = VPOXVDMACMD_BODY(pHdr, VPOXVDMACMD_DMA_BPB_TRANSFER);
//                    pBody->cbTransferSize = (uint32_t)pBuildPagingBuffer->Transfer.TransferSize;
                    pBody->fFlags = 0;
                    SIZE_T cSrcPages = (cbTransferSize + 0xfff ) >> 12;
                    SIZE_T cDstPages = cSrcPages;

                    if (pBuildPagingBuffer->Transfer.Source.SegmentId)
                    {
                        uint64_t off = pBuildPagingBuffer->Transfer.Source.SegmentAddress.QuadPart;
                        off += pBuildPagingBuffer->Transfer.TransferOffset + cbTransfered;
                        pBody->Src.offVramBuf = off;
                        pBody->fFlags |= VPOXVDMACMD_DMA_BPB_TRANSFER_F_SRC_VRAMOFFSET;
                    }
                    else
                    {
                        UINT index = pBuildPagingBuffer->Transfer.MdlOffset + (UINT)(cbTransfered>>12);
                        pBody->Src.phBuf = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Source.pMdl)[index] << PAGE_SHIFT;
                        PFN_NUMBER num = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Source.pMdl)[index];
                        cSrcPages = 1;
                        for (UINT i = 1; i < ((cbTransferSize - cbTransfered + 0xfff) >> 12); ++i)
                        {
                            PFN_NUMBER cur = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Source.pMdl)[index+i];
                            if(cur != ++num)
                            {
                                cSrcPages+= i-1;
                                break;
                            }
                        }
                    }

                    if (pBuildPagingBuffer->Transfer.Destination.SegmentId)
                    {
                        uint64_t off = pBuildPagingBuffer->Transfer.Destination.SegmentAddress.QuadPart;
                        off += pBuildPagingBuffer->Transfer.TransferOffset;
                        pBody->Dst.offVramBuf = off + cbTransfered;
                        pBody->fFlags |= VPOXVDMACMD_DMA_BPB_TRANSFER_F_DST_VRAMOFFSET;
                    }
                    else
                    {
                        UINT index = pBuildPagingBuffer->Transfer.MdlOffset + (UINT)(cbTransfered>>12);
                        pBody->Dst.phBuf = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Destination.pMdl)[index] << PAGE_SHIFT;
                        PFN_NUMBER num = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Destination.pMdl)[index];
                        cDstPages = 1;
                        for (UINT i = 1; i < ((cbTransferSize - cbTransfered + 0xfff) >> 12); ++i)
                        {
                            PFN_NUMBER cur = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Destination.pMdl)[index+i];
                            if(cur != ++num)
                            {
                                cDstPages+= i-1;
                                break;
                            }
                        }
                    }

                    SIZE_T cbCurTransfer;
                    cbCurTransfer = RT_MIN(cbTransferSize - cbTransfered, (SIZE_T)cSrcPages << PAGE_SHIFT);
                    cbCurTransfer = RT_MIN(cbCurTransfer, (SIZE_T)cDstPages << PAGE_SHIFT);

                    pBody->cbTransferSize = (UINT)cbCurTransfer;
                    Assert(!(cbCurTransfer & 0xfff));

                    int rc = vpoxVdmaCBufDrSubmitSynch(pDevExt, &pDevExt->u.primary.Vdma, pDr);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        Status = STATUS_SUCCESS;
                        cbTransfered += cbCurTransfer;
                    }
                    else
                        Status = STATUS_UNSUCCESSFUL;
                } while (cbTransfered < cbTransferSize);
                Assert(cbTransfered == cbTransferSize);
                vpoxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
            }
            else
            {
                /** @todo try flushing.. */
                LOGREL(("vpoxVdmaCBufDrCreate returned NULL"));
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }
#endif /* #ifdef VPOX_WITH_VDMA */
            break;
        }
        case DXGK_OPERATION_FILL:
        {
            cbCmdDma = VPOXWDDM_DUMMY_DMABUFFER_SIZE;
            Assert(pBuildPagingBuffer->Fill.FillPattern == 0);
            /*PVPOXWDDM_ALLOCATION pAlloc = (PVPOXWDDM_ALLOCATION)pBuildPagingBuffer->Fill.hAllocation; - unused. Incomplete code? */
//            pBuildPagingBuffer->pDmaBuffer = (uint8_t*)pBuildPagingBuffer->pDmaBuffer + VPOXVDMACMD_SIZE(VPOXVDMACMD_DMA_BPB_FILL);
            break;
        }
        case DXGK_OPERATION_DISCARD_CONTENT:
        {
//            AssertBreakpoint();
            break;
        }
        default:
        {
            WARN(("unsupported op (%d)", pBuildPagingBuffer->Operation));
            break;
        }
    }

    if (cbCmdDma)
    {
        pBuildPagingBuffer->pDmaBuffer = ((uint8_t*)pBuildPagingBuffer->pDmaBuffer) + cbCmdDma;
    }

    LOGF(("LEAVE, context(0x%x)", hAdapter));

    return Status;

}

/**
 * DxgkDdiPresent
 */
NTSTATUS
APIENTRY
DxgkDdiPresentLegacy(
    CONST HANDLE  hContext,
    DXGKARG_PRESENT  *pPresent)
{
    RT_NOREF(hContext);
    PAGED_CODE();

//    LOGF(("ENTER, hContext(0x%x)", hContext));

    vpoxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
#ifdef VPOX_STRICT
    PVPOXWDDM_CONTEXT pContext = (PVPOXWDDM_CONTEXT)hContext;
    PVPOXWDDM_DEVICE pDevice = pContext->pDevice;
    PVPOXMP_DEVEXT pDevExt = pDevice->pAdapter;
#endif

    Assert(pPresent->DmaBufferPrivateDataSize >= sizeof (VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR));
    if (pPresent->DmaBufferPrivateDataSize < sizeof (VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR))
    {
        LOGREL(("Present->DmaBufferPrivateDataSize(%d) < sizeof VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR (%d)", pPresent->DmaBufferPrivateDataSize , sizeof (VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR)));
        /** @todo can this actually happen? what status tu return? */
        return STATUS_INVALID_PARAMETER;
    }

    PVPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR pPrivateData = (PVPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR)pPresent->pDmaBufferPrivateData;
    pPrivateData->BaseHdr.fFlags.Value = 0;
    /*uint32_t cContexts2D = ASMAtomicReadU32(&pDevExt->cContexts2D); - unused */

    if (pPresent->Flags.Blt)
    {
        Assert(pPresent->Flags.Value == 1); /* only Blt is set, we do not support anything else for now */
        DXGK_ALLOCATIONLIST *pSrc =  &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
        DXGK_ALLOCATIONLIST *pDst =  &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];
        PVPOXWDDM_ALLOCATION pSrcAlloc = vpoxWddmGetAllocationFromAllocList(pSrc);
        if (!pSrcAlloc)
        {
            /* this should not happen actually */
            WARN(("failed to get Src Allocation info for hDeviceSpecificAllocation(0x%x)",pSrc->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
            goto done;
        }

        PVPOXWDDM_ALLOCATION pDstAlloc = vpoxWddmGetAllocationFromAllocList(pDst);
        if (!pDstAlloc)
        {
            /* this should not happen actually */
            WARN(("failed to get Dst Allocation info for hDeviceSpecificAllocation(0x%x)",pDst->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
            goto done;
        }


        UINT cbCmd = pPresent->DmaBufferPrivateDataSize;
        pPrivateData->BaseHdr.enmCmd = VPOXVDMACMD_TYPE_DMA_PRESENT_BLT;

        PVPOXWDDM_DMA_PRIVATEDATA_BLT pBlt = (PVPOXWDDM_DMA_PRIVATEDATA_BLT)pPrivateData;

        vpoxWddmPopulateDmaAllocInfo(&pBlt->Blt.SrcAlloc, pSrcAlloc, pSrc);
        vpoxWddmPopulateDmaAllocInfo(&pBlt->Blt.DstAlloc, pDstAlloc, pDst);

        ASSERT_WARN(!pSrcAlloc->fRcFlags.SharedResource, ("Shared Allocatoin used in Present!"));

        pBlt->Blt.SrcRect = pPresent->SrcRect;
        pBlt->Blt.DstRects.ContextRect = pPresent->DstRect;
        pBlt->Blt.DstRects.UpdateRects.cRects = 0;
        UINT cbHead = RT_UOFFSETOF(VPOXWDDM_DMA_PRIVATEDATA_BLT, Blt.DstRects.UpdateRects.aRects[0]);
        Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);
        UINT cbRects = (pPresent->SubRectCnt - pPresent->MultipassOffset) * sizeof (RECT);
        pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VPOXWDDM_DUMMY_DMABUFFER_SIZE;
        Assert(pPresent->DmaSize >= VPOXWDDM_DUMMY_DMABUFFER_SIZE);
        cbCmd -= cbHead;
        Assert(cbCmd < UINT32_MAX/2);
        Assert(cbCmd > sizeof (RECT));
        if (cbCmd >= cbRects)
        {
            cbCmd -= cbRects;
            memcpy(&pBlt->Blt.DstRects.UpdateRects.aRects[0], &pPresent->pDstSubRects[pPresent->MultipassOffset], cbRects);
            pBlt->Blt.DstRects.UpdateRects.cRects += cbRects/sizeof (RECT);

            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbHead + cbRects;
        }
        else
        {
            UINT cbFitingRects = (cbCmd/sizeof (RECT)) * sizeof (RECT);
            Assert(cbFitingRects);
            memcpy(&pBlt->Blt.DstRects.UpdateRects.aRects[0], &pPresent->pDstSubRects[pPresent->MultipassOffset], cbFitingRects);
            cbCmd -= cbFitingRects;
            pPresent->MultipassOffset += cbFitingRects/sizeof (RECT);
            pBlt->Blt.DstRects.UpdateRects.cRects += cbFitingRects/sizeof (RECT);
            Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);

            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbHead + cbFitingRects;
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

        memset(pPresent->pPatchLocationListOut, 0, 2*sizeof (D3DDDI_PATCHLOCATIONLIST));
        pPresent->pPatchLocationListOut->PatchOffset = 0;
        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
        ++pPresent->pPatchLocationListOut;
        pPresent->pPatchLocationListOut->PatchOffset = 4;
        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
        ++pPresent->pPatchLocationListOut;
    }
    else if (pPresent->Flags.Flip)
    {
        Assert(pPresent->Flags.Value == 4); /* only Blt is set, we do not support anything else for now */
        Assert(pContext->enmType == VPOXWDDM_CONTEXT_TYPE_CUSTOM_3D);
        DXGK_ALLOCATIONLIST *pSrc =  &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
        PVPOXWDDM_ALLOCATION pSrcAlloc = vpoxWddmGetAllocationFromAllocList(pSrc);

        if (!pSrcAlloc)
        {
            /* this should not happen actually */
            WARN(("failed to get pSrc Allocation info for hDeviceSpecificAllocation(0x%x)",pSrc->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
            goto done;
        }

        Assert(pDevExt->cContexts3D);
        pPrivateData->BaseHdr.enmCmd = VPOXVDMACMD_TYPE_DMA_PRESENT_FLIP;
        PVPOXWDDM_DMA_PRIVATEDATA_FLIP pFlip = (PVPOXWDDM_DMA_PRIVATEDATA_FLIP)pPrivateData;

        vpoxWddmPopulateDmaAllocInfo(&pFlip->Flip.Alloc, pSrcAlloc, pSrc);

        UINT cbCmd = sizeof (VPOXWDDM_DMA_PRIVATEDATA_FLIP);
        pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbCmd;
        pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VPOXWDDM_DUMMY_DMABUFFER_SIZE;
        Assert(pPresent->DmaSize >= VPOXWDDM_DUMMY_DMABUFFER_SIZE);

        memset(pPresent->pPatchLocationListOut, 0, sizeof (D3DDDI_PATCHLOCATIONLIST));
        pPresent->pPatchLocationListOut->PatchOffset = 0;
        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
        ++pPresent->pPatchLocationListOut;
    }
    else if (pPresent->Flags.ColorFill)
    {
        Assert(pContext->enmType == VPOXWDDM_CONTEXT_TYPE_CUSTOM_2D);
        Assert(pPresent->Flags.Value == 2); /* only ColorFill is set, we do not support anything else for now */
        DXGK_ALLOCATIONLIST *pDst =  &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];
        PVPOXWDDM_ALLOCATION pDstAlloc = vpoxWddmGetAllocationFromAllocList(pDst);
        if (!pDstAlloc)
        {

            /* this should not happen actually */
            WARN(("failed to get pDst Allocation info for hDeviceSpecificAllocation(0x%x)",pDst->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
            goto done;
        }

        UINT cbCmd = pPresent->DmaBufferPrivateDataSize;
        pPrivateData->BaseHdr.enmCmd = VPOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL;
        PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateData;

        vpoxWddmPopulateDmaAllocInfo(&pCF->ClrFill.Alloc, pDstAlloc, pDst);

        pCF->ClrFill.Color = pPresent->Color;
        pCF->ClrFill.Rects.cRects = 0;
        UINT cbHead = RT_UOFFSETOF(VPOXWDDM_DMA_PRIVATEDATA_CLRFILL, ClrFill.Rects.aRects[0]);
        Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);
        UINT cbRects = (pPresent->SubRectCnt - pPresent->MultipassOffset) * sizeof (RECT);
        pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VPOXWDDM_DUMMY_DMABUFFER_SIZE;
        Assert(pPresent->DmaSize >= VPOXWDDM_DUMMY_DMABUFFER_SIZE);
        cbCmd -= cbHead;
        Assert(cbCmd < UINT32_MAX/2);
        Assert(cbCmd > sizeof (RECT));
        if (cbCmd >= cbRects)
        {
            cbCmd -= cbRects;
            memcpy(&pCF->ClrFill.Rects.aRects[pPresent->MultipassOffset], pPresent->pDstSubRects, cbRects);
            pCF->ClrFill.Rects.cRects += cbRects/sizeof (RECT);

            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbHead + cbRects;
        }
        else
        {
            UINT cbFitingRects = (cbCmd/sizeof (RECT)) * sizeof (RECT);
            Assert(cbFitingRects);
            memcpy(&pCF->ClrFill.Rects.aRects[0], pPresent->pDstSubRects, cbFitingRects);
            cbCmd -= cbFitingRects;
            pPresent->MultipassOffset += cbFitingRects/sizeof (RECT);
            pCF->ClrFill.Rects.cRects += cbFitingRects/sizeof (RECT);
            Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);

            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbHead + cbFitingRects;
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

        memset(pPresent->pPatchLocationListOut, 0, sizeof (D3DDDI_PATCHLOCATIONLIST));
        pPresent->pPatchLocationListOut->PatchOffset = 0;
        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
        ++pPresent->pPatchLocationListOut;
    }
    else
    {
        WARN(("cmd NOT IMPLEMENTED!! Flags(0x%x)", pPresent->Flags.Value));
        Status = STATUS_NOT_SUPPORTED;
    }

done:
//    LOGF(("LEAVE, hContext(0x%x), Status(0x%x)", hContext, Status));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiRenderLegacy(
    CONST HANDLE  hContext,
    DXGKARG_RENDER  *pRender)
{
    RT_NOREF(hContext);
//    LOGF(("ENTER, hContext(0x%x)", hContext));

    if (pRender->DmaBufferPrivateDataSize < sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        WARN(("Present->DmaBufferPrivateDataSize(%d) < sizeof VPOXWDDM_DMA_PRIVATEDATA_BASEHDR (%d)",
                pRender->DmaBufferPrivateDataSize , sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }
    if (pRender->CommandLength < sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        WARN(("Present->DmaBufferPrivateDataSize(%d) < sizeof VPOXWDDM_DMA_PRIVATEDATA_BASEHDR (%d)",
                pRender->DmaBufferPrivateDataSize , sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }
    if (pRender->DmaSize < pRender->CommandLength)
    {
        WARN(("pRender->DmaSize(%d) < pRender->CommandLength(%d)",
                pRender->DmaSize, pRender->CommandLength));
        return STATUS_INVALID_PARAMETER;
    }
    if (pRender->PatchLocationListOutSize < pRender->PatchLocationListInSize)
    {
        WARN(("pRender->PatchLocationListOutSize(%d) < pRender->PatchLocationListInSize(%d)",
                pRender->PatchLocationListOutSize, pRender->PatchLocationListInSize));
        return STATUS_INVALID_PARAMETER;
    }
    if (pRender->AllocationListSize != pRender->PatchLocationListInSize)
    {
        WARN(("pRender->AllocationListSize(%d) != pRender->PatchLocationListInSize(%d)",
                pRender->AllocationListSize, pRender->PatchLocationListInSize));
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_SUCCESS;

    __try
    {
        PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR pInputHdr = (PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR)pRender->pCommand;
        switch (pInputHdr->enmCmd)
        {
            case VPOXVDMACMD_TYPE_DMA_NOP:
            {
                PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR pPrivateData = (PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR)pRender->pDmaBufferPrivateData;
                pPrivateData->enmCmd = VPOXVDMACMD_TYPE_DMA_NOP;
                pRender->pDmaBufferPrivateData = (uint8_t*)pRender->pDmaBufferPrivateData + sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR);
                pRender->pDmaBuffer = ((uint8_t*)pRender->pDmaBuffer) + pRender->CommandLength;
                for (UINT i = 0; i < pRender->PatchLocationListInSize; ++i)
                {
                    UINT offPatch = i * 4;
                    if (offPatch + 4 > pRender->CommandLength)
                    {
                        WARN(("wrong offPatch"));
                        return STATUS_INVALID_PARAMETER;
                    }
                    if (offPatch != pRender->pPatchLocationListIn[i].PatchOffset)
                    {
                        WARN(("wrong PatchOffset"));
                        return STATUS_INVALID_PARAMETER;
                    }
                    if (i != pRender->pPatchLocationListIn[i].AllocationIndex)
                    {
                        WARN(("wrong AllocationIndex"));
                        return STATUS_INVALID_PARAMETER;
                    }
                    vpoxWddmPatchLocationInit(&pRender->pPatchLocationListOut[i], i, offPatch);
                }
                break;
            }
            default:
            {
                WARN(("unsupported command %d", pInputHdr->enmCmd));
                return STATUS_INVALID_PARAMETER;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = STATUS_INVALID_PARAMETER;
        WARN(("invalid parameter"));
    }
//    LOGF(("LEAVE, hContext(0x%x)", hContext));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiPatchLegacy(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PATCH*  pPatch)
{
    RT_NOREF(hAdapter);
    /* DxgkDdiPatch should be made pageable. */
    PAGED_CODE();

    NTSTATUS Status = STATUS_SUCCESS;

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vpoxVDbgBreakFv();

    /* Value == 2 is Present
     * Value == 4 is RedirectedPresent
     * we do not expect any other flags to be set here */
//    Assert(pPatch->Flags.Value == 2 || pPatch->Flags.Value == 4);
    if (pPatch->DmaBufferPrivateDataSubmissionEndOffset - pPatch->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        Assert(pPatch->DmaBufferPrivateDataSize >= sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR));
        VPOXWDDM_DMA_PRIVATEDATA_BASEHDR *pPrivateDataBase = (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR*)((uint8_t*)pPatch->pDmaBufferPrivateData + pPatch->DmaBufferPrivateDataSubmissionStartOffset);
        switch (pPrivateDataBase->enmCmd)
        {
            case VPOXVDMACMD_TYPE_DMA_PRESENT_BLT:
            {
                PVPOXWDDM_DMA_PRIVATEDATA_BLT pBlt = (PVPOXWDDM_DMA_PRIVATEDATA_BLT)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 2);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_SOURCE_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pSrcAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pSrcAllocationList->SegmentId);
                pBlt->Blt.SrcAlloc.segmentIdAlloc = pSrcAllocationList->SegmentId;
                pBlt->Blt.SrcAlloc.offAlloc = (VPOXVIDEOOFFSET)pSrcAllocationList->PhysicalAddress.QuadPart;

                pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart + 1];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_DESTINATION_INDEX);
                Assert(pPatchList->PatchOffset == 4);
                const DXGK_ALLOCATIONLIST *pDstAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pDstAllocationList->SegmentId);
                pBlt->Blt.DstAlloc.segmentIdAlloc = pDstAllocationList->SegmentId;
                pBlt->Blt.DstAlloc.offAlloc = (VPOXVIDEOOFFSET)pDstAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VPOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
            {
                PVPOXWDDM_DMA_PRIVATEDATA_FLIP pFlip = (PVPOXWDDM_DMA_PRIVATEDATA_FLIP)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 1);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_SOURCE_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pSrcAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pSrcAllocationList->SegmentId);
                pFlip->Flip.Alloc.segmentIdAlloc = pSrcAllocationList->SegmentId;
                pFlip->Flip.Alloc.offAlloc = (VPOXVIDEOOFFSET)pSrcAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VPOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
            {
                PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 1);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_DESTINATION_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pDstAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pDstAllocationList->SegmentId);
                pCF->ClrFill.Alloc.segmentIdAlloc = pDstAllocationList->SegmentId;
                pCF->ClrFill.Alloc.offAlloc = (VPOXVIDEOOFFSET)pDstAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VPOXVDMACMD_TYPE_DMA_NOP:
                break;
            case VPOXVDMACMD_TYPE_CHROMIUM_CMD:
            {
                uint8_t * pPrivateBuf = (uint8_t*)pPrivateDataBase;
                for (UINT i = pPatch->PatchLocationListSubmissionStart; i < pPatch->PatchLocationListSubmissionLength; ++i)
                {
                    const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[i];
                    Assert(pPatchList->AllocationIndex < pPatch->AllocationListSize);
                    const DXGK_ALLOCATIONLIST *pAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                    Assert(pAllocationList->SegmentId);
                    if (pAllocationList->SegmentId)
                    {
                        DXGK_ALLOCATIONLIST *pAllocation2Patch = (DXGK_ALLOCATIONLIST*)(pPrivateBuf + pPatchList->PatchOffset);
                        pAllocation2Patch->SegmentId = pAllocationList->SegmentId;
                        pAllocation2Patch->PhysicalAddress.QuadPart = pAllocationList->PhysicalAddress.QuadPart + pPatchList->AllocationOffset;
                        Assert(!(pAllocationList->PhysicalAddress.QuadPart & 0xfffUL)); /* <- just a check to ensure allocation offset does not go here */
                    }
                }
                break;
            }
            default:
            {
                AssertBreakpoint();
                uint8_t *pBuf = ((uint8_t *)pPatch->pDmaBuffer) + pPatch->DmaBufferSubmissionStartOffset;
                for (UINT i = pPatch->PatchLocationListSubmissionStart; i < pPatch->PatchLocationListSubmissionLength; ++i)
                {
                    const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[i];
                    Assert(pPatchList->AllocationIndex < pPatch->AllocationListSize);
                    const DXGK_ALLOCATIONLIST *pAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                    if (pAllocationList->SegmentId)
                    {
                        Assert(pPatchList->PatchOffset < (pPatch->DmaBufferSubmissionEndOffset - pPatch->DmaBufferSubmissionStartOffset));
                        *((VPOXVIDEOOFFSET*)(pBuf+pPatchList->PatchOffset)) = (VPOXVIDEOOFFSET)pAllocationList->PhysicalAddress.QuadPart;
                    }
                    else
                    {
                        /* sanity */
                        if (pPatch->Flags.Value == 2 || pPatch->Flags.Value == 4)
                            Assert(i == 0);
                    }
                }
                break;
            }
        }
    }
    else if (pPatch->DmaBufferPrivateDataSubmissionEndOffset == pPatch->DmaBufferPrivateDataSubmissionStartOffset)
    {
        /* this is a NOP, just return success */
//        LOG(("null data size, treating as NOP"));
        return STATUS_SUCCESS;
    }
    else
    {
        WARN(("DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR) (%d)",
                pPatch->DmaBufferPrivateDataSubmissionEndOffset,
                pPatch->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }

    LOGF(("LEAVE, context(0x%x)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiSubmitCommandLegacy(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SUBMITCOMMAND*  pSubmitCommand)
{
    /* DxgkDdiSubmitCommand runs at dispatch, should not be pageable. */
    NTSTATUS Status = STATUS_SUCCESS;

//    LOGF(("ENTER, context(0x%x)", hAdapter));

    vpoxVDbgBreakFv();

    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)hAdapter;
    PVPOXWDDM_CONTEXT pContext = (PVPOXWDDM_CONTEXT)pSubmitCommand->hContext;
    PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR pPrivateDataBase = NULL;
    VPOXVDMACMD_TYPE enmCmd = VPOXVDMACMD_TYPE_UNDEFINED;
    Assert(pContext);
    Assert(pContext->pDevice);
    Assert(pContext->pDevice->pAdapter == pDevExt);
    Assert(!pSubmitCommand->DmaBufferSegmentId);

    /* the DMA command buffer is located in system RAM, the host will need to pick it from there */
    //BufInfo.fFlags = 0; /* see VPOXVDMACBUF_FLAG_xx */
    if (pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        pPrivateDataBase = (PVPOXWDDM_DMA_PRIVATEDATA_BASEHDR)((uint8_t*)pSubmitCommand->pDmaBufferPrivateData + pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset);
        Assert(pPrivateDataBase);
        enmCmd = pPrivateDataBase->enmCmd;
    }
    else if (pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset == pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset)
    {
        enmCmd = VPOXVDMACMD_TYPE_DMA_NOP;
    }
    else
    {
        WARN(("DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR) (%d)",
                pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset,
                pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VPOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }

    switch (enmCmd)
    {
        case VPOXVDMACMD_TYPE_DMA_PRESENT_BLT:
        {
            VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR *pPrivateData = (VPOXWDDM_DMA_PRIVATEDATA_PRESENTHDR*)pPrivateDataBase;
            PVPOXWDDM_DMA_PRIVATEDATA_BLT pBlt = (PVPOXWDDM_DMA_PRIVATEDATA_BLT)pPrivateData;
            PVPOXWDDM_ALLOCATION pDstAlloc = pBlt->Blt.DstAlloc.pAlloc;
            PVPOXWDDM_ALLOCATION pSrcAlloc = pBlt->Blt.SrcAlloc.pAlloc;
            BOOLEAN fSrcChanged;
            BOOLEAN fDstChanged;

            fDstChanged = vpoxWddmAddrSetVram(&pDstAlloc->AllocData.Addr, pBlt->Blt.DstAlloc.segmentIdAlloc, pBlt->Blt.DstAlloc.offAlloc);
            fSrcChanged = vpoxWddmAddrSetVram(&pSrcAlloc->AllocData.Addr, pBlt->Blt.SrcAlloc.segmentIdAlloc, pBlt->Blt.SrcAlloc.offAlloc);

            if (VPOXWDDM_IS_FB_ALLOCATION(pDevExt, pDstAlloc))
            {
                Assert(pDstAlloc->AllocData.SurfDesc.VidPnSourceId < VPOX_VIDEO_MAX_SCREENS);
            }

            Status = vpoxVdmaProcessBltCmd(pDevExt, pContext, pBlt);
            if (!NT_SUCCESS(Status))
                WARN(("vpoxVdmaProcessBltCmd failed, Status 0x%x", Status));

            Status = vpoxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId,
                    NT_SUCCESS(Status) ? DXGK_INTERRUPT_DMA_COMPLETED : DXGK_INTERRUPT_DMA_FAULTED);
            break;
        }
        case VPOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
        {
            VPOXWDDM_DMA_PRIVATEDATA_FLIP *pFlip = (VPOXWDDM_DMA_PRIVATEDATA_FLIP*)pPrivateDataBase;
            PVPOXWDDM_ALLOCATION pAlloc = pFlip->Flip.Alloc.pAlloc;
            VPOXWDDM_SOURCE *pSource = &pDevExt->aSources[pAlloc->AllocData.SurfDesc.VidPnSourceId];
            vpoxWddmAddrSetVram(&pAlloc->AllocData.Addr, pFlip->Flip.Alloc.segmentIdAlloc, pFlip->Flip.Alloc.offAlloc);
            vpoxWddmAssignPrimary(pSource, pAlloc, pAlloc->AllocData.SurfDesc.VidPnSourceId);
            vpoxWddmGhDisplayCheckSetInfoFromSource(pDevExt, pSource);

            Status = vpoxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId,
                    NT_SUCCESS(Status) ? DXGK_INTERRUPT_DMA_COMPLETED : DXGK_INTERRUPT_DMA_FAULTED);
            break;
        }
        case VPOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
        {
            PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVPOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateDataBase;
            vpoxWddmAddrSetVram(&pCF->ClrFill.Alloc.pAlloc->AllocData.Addr, pCF->ClrFill.Alloc.segmentIdAlloc, pCF->ClrFill.Alloc.offAlloc);

            Status = vpoxVdmaProcessClrFillCmd(pDevExt, pContext, pCF);
            if (!NT_SUCCESS(Status))
                WARN(("vpoxVdmaProcessClrFillCmd failed, Status 0x%x", Status));

            Status = vpoxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId,
                    NT_SUCCESS(Status) ? DXGK_INTERRUPT_DMA_COMPLETED : DXGK_INTERRUPT_DMA_FAULTED);
            break;
        }
        case VPOXVDMACMD_TYPE_DMA_NOP:
        {
            Status = vpoxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            AssertNtStatusSuccess(Status);
            break;
        }
        default:
        {
            WARN(("unexpected command %d", enmCmd));
            break;
        }
    }
//    LOGF(("LEAVE, context(0x%x)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiPreemptCommandLegacy(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PREEMPTCOMMAND*  pPreemptCommand)
{
    RT_NOREF(hAdapter, pPreemptCommand);
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertFailed();
    /** @todo fixme: implement */

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

typedef struct VPOXWDDM_QUERYCURFENCE_CB
{
    PVPOXMP_DEVEXT pDevExt;
    ULONG MessageNumber;
    ULONG uLastCompletedCmdFenceId;
} VPOXWDDM_QUERYCURFENCE_CB, *PVPOXWDDM_QUERYCURFENCE_CB;

static BOOLEAN vpoxWddmQueryCurrentFenceCb(PVOID Context)
{
    PVPOXWDDM_QUERYCURFENCE_CB pdc = (PVPOXWDDM_QUERYCURFENCE_CB)Context;
    PVPOXMP_DEVEXT pDevExt = pdc->pDevExt;
    BOOL bRc = DxgkDdiInterruptRoutineLegacy(pDevExt, pdc->MessageNumber);
    pdc->uLastCompletedCmdFenceId = pDevExt->u.primary.uLastCompletedPagingBufferCmdFenceId;
    return bRc;
}

NTSTATUS
APIENTRY
DxgkDdiQueryCurrentFenceLegacy(
    CONST HANDLE  hAdapter,
    DXGKARG_QUERYCURRENTFENCE*  pCurrentFence)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vpoxVDbgBreakF();

    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)hAdapter;
    VPOXWDDM_QUERYCURFENCE_CB context = {0};
    context.pDevExt = pDevExt;
    BOOLEAN bRet;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vpoxWddmQueryCurrentFenceCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        pCurrentFence->CurrentFence = context.uLastCompletedCmdFenceId;
    }

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

BOOLEAN DxgkDdiInterruptRoutineLegacy(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG MessageNumber
    )
{
    RT_NOREF(MessageNumber);
//    LOGF(("ENTER, context(0x%p), msg(0x%x)", MiniportDeviceContext, MessageNumber));

    vpoxVDbgBreakFv();

    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)MiniportDeviceContext;
    BOOLEAN bOur = FALSE;
    BOOLEAN bNeedDpc = FALSE;
    if (VPoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags) /* If HGSMI is enabled at all. */
    {
        VPOXVTLIST CtlList;
        vpoxVtListInit(&CtlList);

#ifdef VPOX_WITH_VIDEOHWACCEL
        VPOXVTLIST VhwaCmdList;
        vpoxVtListInit(&VhwaCmdList);
#endif

        uint32_t flags = VPoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags->u32HostFlags;
        bOur = (flags & HGSMIHOSTFLAGS_IRQ);

        if (bOur)
            VPoxHGSMIClearIrq(&VPoxCommonFromDeviceExt(pDevExt)->hostCtx);

        do
        {
            if (flags & HGSMIHOSTFLAGS_GCOMMAND_COMPLETED)
            {
                /* read the command offset */
                HGSMIOFFSET offCmd = VBVO_PORT_READ_U32(VPoxCommonFromDeviceExt(pDevExt)->guestCtx.port);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    VPOXWDDM_HGSMICMD_TYPE enmType = vpoxWddmHgsmiGetCmdTypeFromOffset(pDevExt, offCmd);
                    PVPOXVTLIST pList;
                    PVPOXSHGSMI pHeap;
                    switch (enmType)
                    {
                        case VPOXWDDM_HGSMICMD_TYPE_CTL:
                            pList = &CtlList;
                            pHeap = &VPoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx;
                            break;
                        default:
                            AssertBreakpoint();
                            pList = NULL;
                            pHeap = NULL;
                            break;
                    }

                    if (pHeap)
                    {
                        uint16_t chInfo;
                        uint8_t RT_UNTRUSTED_VOLATILE_GUEST *pvCmd =
                            HGSMIBufferDataAndChInfoFromOffset(&pHeap->Heap.area, offCmd, &chInfo);
                        Assert(pvCmd);
                        if (pvCmd)
                        {
                            switch (chInfo)
                            {
#ifdef VPOX_WITH_VIDEOHWACCEL
                                case VBVA_VHWA_CMD:
                                {
                                    vpoxVhwaPutList(&VhwaCmdList, (VPOXVHWACMD*)pvCmd);
                                    break;
                                }
#endif /* # ifdef VPOX_WITH_VIDEOHWACCEL */
                                default:
                                    AssertBreakpoint();
                            }
                        }
                    }
                }
            }
            else if (flags & HGSMIHOSTFLAGS_COMMANDS_PENDING)
            {
                AssertBreakpoint();
                /** @todo FIXME: implement !!! */
            }
            else
                break;

            flags = VPoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags->u32HostFlags;
        } while (1);

        if (!vpoxVtListIsEmpty(&CtlList))
        {
            vpoxVtListCat(&pDevExt->CtlList, &CtlList);
            bNeedDpc = TRUE;
        }
#ifdef VPOX_WITH_VIDEOHWACCEL
        if (!vpoxVtListIsEmpty(&VhwaCmdList))
        {
            vpoxVtListCat(&pDevExt->VhwaCmdList, &VhwaCmdList);
            bNeedDpc = TRUE;
        }
#endif

        if (pDevExt->bNotifyDxDpc)
        {
            bNeedDpc = TRUE;
        }

        if (bOur)
        {
            if (flags & HGSMIHOSTFLAGS_VSYNC)
            {
                Assert(0);
                DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
                for (UINT i = 0; i < (UINT)VPoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
                {
                    PVPOXWDDM_TARGET pTarget = &pDevExt->aTargets[i];
                    if (pTarget->fConnected)
                    {
                        memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
                        notify.InterruptType = DXGK_INTERRUPT_CRTC_VSYNC;
                        notify.CrtcVsync.VidPnTargetId = i;
                        pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
                        bNeedDpc = TRUE;
                    }
                }
            }

            if (pDevExt->bNotifyDxDpc)
            {
                bNeedDpc = TRUE;
            }

#if 0 //def DEBUG_misha
            /* this is not entirely correct since host may concurrently complete some commands and raise a new IRQ while we are here,
             * still this allows to check that the host flags are correctly cleared after the ISR */
            Assert(VPoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags);
            uint32_t flags = VPoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags->u32HostFlags;
            Assert(flags == 0);
#endif
        }

        if (bNeedDpc)
        {
            pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
        }
    }

//    LOGF(("LEAVE, context(0x%p), bOur(0x%x)", MiniportDeviceContext, (ULONG)bOur));

    return bOur;
}


typedef struct VPOXWDDM_DPCDATA
{
    VPOXVTLIST CtlList;
#ifdef VPOX_WITH_VIDEOHWACCEL
    VPOXVTLIST VhwaCmdList;
#endif
    LIST_ENTRY CompletedDdiCmdQueue;
    BOOL bNotifyDpc;
} VPOXWDDM_DPCDATA, *PVPOXWDDM_DPCDATA;

typedef struct VPOXWDDM_GETDPCDATA_CONTEXT
{
    PVPOXMP_DEVEXT pDevExt;
    VPOXWDDM_DPCDATA data;
} VPOXWDDM_GETDPCDATA_CONTEXT, *PVPOXWDDM_GETDPCDATA_CONTEXT;

BOOLEAN vpoxWddmGetDPCDataCallback(PVOID Context)
{
    PVPOXWDDM_GETDPCDATA_CONTEXT pdc = (PVPOXWDDM_GETDPCDATA_CONTEXT)Context;
    PVPOXMP_DEVEXT pDevExt = pdc->pDevExt;
    vpoxVtListDetach2List(&pDevExt->CtlList, &pdc->data.CtlList);
#ifdef VPOX_WITH_VIDEOHWACCEL
    vpoxVtListDetach2List(&pDevExt->VhwaCmdList, &pdc->data.VhwaCmdList);
#endif

    pdc->data.bNotifyDpc = pDevExt->bNotifyDxDpc;
    pDevExt->bNotifyDxDpc = FALSE;

    ASMAtomicWriteU32(&pDevExt->fCompletingCommands, 0);

    return TRUE;
}

VOID DxgkDdiDpcRoutineLegacy(
    IN CONST PVOID  MiniportDeviceContext
    )
{
//    LOGF(("ENTER, context(0x%p)", MiniportDeviceContext));

    vpoxVDbgBreakFv();

    PVPOXMP_DEVEXT pDevExt = (PVPOXMP_DEVEXT)MiniportDeviceContext;

    VPOXWDDM_GETDPCDATA_CONTEXT context = {0};
    BOOLEAN bRet;

    context.pDevExt = pDevExt;

    /* get DPC data at IRQL */
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vpoxWddmGetDPCDataCallback,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    AssertNtStatusSuccess(Status); NOREF(Status);

    if (!vpoxVtListIsEmpty(&context.data.CtlList))
    {
        int rc = VPoxSHGSMICommandPostprocessCompletion (&VPoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, &context.data.CtlList);
        AssertRC(rc);
    }
#ifdef VPOX_WITH_VIDEOHWACCEL
    if (!vpoxVtListIsEmpty(&context.data.VhwaCmdList))
    {
        vpoxVhwaCompletionListProcess(pDevExt, &context.data.VhwaCmdList);
    }
#endif

//    LOGF(("LEAVE, context(0x%p)", MiniportDeviceContext));
}
