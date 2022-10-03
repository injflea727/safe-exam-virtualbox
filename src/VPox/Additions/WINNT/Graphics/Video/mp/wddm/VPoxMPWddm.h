/* $Id: VPoxMPWddm.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPWddm_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPWddm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOX_WDDM_DRIVERNAME L"VPoxWddm"

#ifndef DEBUG_misha
# ifdef Assert
#  error "VPoxMPWddm.h must be included first."
# endif
# define RT_NO_STRICT
#endif
#include "common/VPoxMPUtils.h"
#include "common/VPoxMPDevExt.h"
#include "../../common/VPoxVideoTools.h"

//#define VPOXWDDM_DEBUG_VIDPN

#define VPOXWDDM_CFG_DRV_DEFAULT                        0
#define VPOXWDDM_CFG_DRV_SECONDARY_TARGETS_CONNECTED    1

#define VPOXWDDM_CFG_DRVTARGET_CONNECTED                1

#define VPOXWDDM_CFG_LOG_UM_BACKDOOR 0x00000001
#define VPOXWDDM_CFG_LOG_UM_DBGPRINT 0x00000002
#define VPOXWDDM_CFG_STR_LOG_UM L"VPoxLogUm"
#define VPOXWDDM_CFG_STR_RATE L"RefreshRate"

#define VPOXWDDM_REG_DRV_FLAGS_NAME L"VPoxFlags"
#define VPOXWDDM_REG_DRV_DISPFLAGS_PREFIX L"VPoxDispFlags"

#define VPOXWDDM_REG_DRVKEY_PREFIX L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\"

#define VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Video\\"
#define VPOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY L"\\Video"


#define VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Control\\VIDEO\\"
#define VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7 L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\UnitedVideo\\CONTROL\\VIDEO\\"
#define VPOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN10_17763 L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\UnitedVideo\\CONTROL\\VIDEO\\"

#define VPOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELX L"Attach.RelativeX"
#define VPOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELY L"Attach.RelativeY"
#define VPOXWDDM_REG_DISPLAYSETTINGS_ATTACH_DESKTOP L"Attach.ToDesktop"

extern DWORD g_VPoxLogUm;
extern DWORD g_RefreshRate;

RT_C_DECLS_BEGIN
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
RT_C_DECLS_END

PVOID vpoxWddmMemAlloc(IN SIZE_T cbSize);
PVOID vpoxWddmMemAllocZero(IN SIZE_T cbSize);
VOID vpoxWddmMemFree(PVOID pvMem);

NTSTATUS vpoxWddmCallIsr(PVPOXMP_DEVEXT pDevExt);

DECLINLINE(PVPOXWDDM_RESOURCE) vpoxWddmResourceForAlloc(PVPOXWDDM_ALLOCATION pAlloc)
{
#if 0
    if(pAlloc->iIndex == VPOXWDDM_ALLOCATIONINDEX_VOID)
        return NULL;
    PVPOXWDDM_RESOURCE pRc = (PVPOXWDDM_RESOURCE)(((uint8_t*)pAlloc) - RT_OFFSETOF(VPOXWDDM_RESOURCE, aAllocations[pAlloc->iIndex]));
    return pRc;
#else
    return pAlloc->pResource;
#endif
}

VOID vpoxWddmAllocationDestroy(PVPOXWDDM_ALLOCATION pAllocation);

DECLINLINE(BOOLEAN) vpoxWddmAddrSetVram(PVPOXWDDM_ADDR pAddr, UINT SegmentId, VPOXVIDEOOFFSET offVram)
{
    if (pAddr->SegmentId == SegmentId && pAddr->offVram == offVram)
        return FALSE;

    pAddr->SegmentId = SegmentId;
    pAddr->offVram = offVram;
    return TRUE;
}

DECLINLINE(bool) vpoxWddmAddrVramEqual(const VPOXWDDM_ADDR *pAddr1, const VPOXWDDM_ADDR *pAddr2)
{
    return pAddr1->SegmentId == pAddr2->SegmentId && pAddr1->offVram == pAddr2->offVram;
}

DECLINLINE(VPOXVIDEOOFFSET) vpoxWddmVramAddrToOffset(PVPOXMP_DEVEXT pDevExt, PHYSICAL_ADDRESS Addr)
{
    PVPOXMP_COMMON pCommon = VPoxCommonFromDeviceExt(pDevExt);
    AssertRelease(pCommon->phVRAM.QuadPart <= Addr.QuadPart);
    return (VPOXVIDEOOFFSET)Addr.QuadPart - pCommon->phVRAM.QuadPart;
}

DECLINLINE(VOID) vpoxWddmAssignPrimary(PVPOXWDDM_SOURCE pSource, PVPOXWDDM_ALLOCATION pAllocation,
                                       D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    RT_NOREF(srcId);

    /* vpoxWddmAssignPrimary can not be run in reentrant order, so safely do a direct unlocked check here */
    if (pSource->pPrimaryAllocation == pAllocation)
        return;

    if (pSource->pPrimaryAllocation)
    {
        PVPOXWDDM_ALLOCATION pOldAlloc = pSource->pPrimaryAllocation;
        /* clear the visibility info fo the current primary */
        pOldAlloc->bVisible = FALSE;
        pOldAlloc->bAssigned = FALSE;
        Assert(pOldAlloc->AllocData.SurfDesc.VidPnSourceId == srcId);
    }

    if (pAllocation)
    {
        Assert(pAllocation->AllocData.SurfDesc.VidPnSourceId == srcId);
        pAllocation->bAssigned = TRUE;
        pAllocation->bVisible = pSource->bVisible;

        if (pSource->AllocData.hostID != pAllocation->AllocData.hostID)
        {
            pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_LOCATION; /* force guest->host notification */
            pSource->AllocData.hostID = pAllocation->AllocData.hostID;
        }

        if (!vpoxWddmAddrVramEqual(&pSource->AllocData.Addr, &pAllocation->AllocData.Addr))
        {
            if (!pAllocation->AllocData.hostID)
                pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_LOCATION; /* force guest->host notification */

            pSource->AllocData.Addr = pAllocation->AllocData.Addr;
        }
    }
    else
    {
        pSource->u8SyncState &= ~VPOXWDDM_HGSYNC_F_SYNCED_LOCATION; /* force guest->host notification */
        /*ensure we do not refer to the deleted host id */
        pSource->AllocData.hostID = 0;
    }

    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->AllocationLock, &OldIrql);
    pSource->pPrimaryAllocation = pAllocation;
    KeReleaseSpinLock(&pSource->AllocationLock, OldIrql);
}

DECLINLINE(VPOXVIDEOOFFSET) vpoxWddmAddrFramOffset(const VPOXWDDM_ADDR *pAddr)
{
    return (pAddr->offVram != VPOXVIDEOOFFSET_VOID && pAddr->SegmentId) ?
            (pAddr->SegmentId == 1 ? pAddr->offVram : 0)
            : VPOXVIDEOOFFSET_VOID;
}

DECLINLINE(int) vpoxWddmScreenInfoInit(VBVAINFOSCREEN RT_UNTRUSTED_VOLATILE_HOST *pScreen,
                                       const VPOXWDDM_ALLOC_DATA *pAllocData, const POINT * pVScreenPos, uint16_t fFlags)
{
    VPOXVIDEOOFFSET offVram = vpoxWddmAddrFramOffset(&pAllocData->Addr);
    if (offVram == VPOXVIDEOOFFSET_VOID && !(fFlags & (VBVA_SCREEN_F_DISABLED | VBVA_SCREEN_F_BLANK2)))
    {
        WARN(("offVram == VPOXVIDEOOFFSET_VOID"));
        return VERR_INVALID_PARAMETER;
    }

    pScreen->u32ViewIndex    = pAllocData->SurfDesc.VidPnSourceId;
    pScreen->i32OriginX      = pVScreenPos->x;
    pScreen->i32OriginY      = pVScreenPos->y;
    pScreen->u32StartOffset  = (uint32_t)offVram;
    pScreen->u32LineSize     = pAllocData->SurfDesc.pitch;
    pScreen->u32Width        = pAllocData->SurfDesc.width;
    pScreen->u32Height       = pAllocData->SurfDesc.height;
    pScreen->u16BitsPerPixel = (uint16_t)pAllocData->SurfDesc.bpp;
    pScreen->u16Flags        = fFlags;

    return VINF_SUCCESS;
}

bool vpoxWddmGhDisplayCheckSetInfoFromSource(PVPOXMP_DEVEXT pDevExt, PVPOXWDDM_SOURCE pSource);

#define VPOXWDDM_IS_DISPLAYONLY() (g_VPoxDisplayOnly)

# define VPOXWDDM_IS_FB_ALLOCATION(_pDevExt, _pAlloc) ((_pAlloc)->bAssigned)

# define VPOXWDDM_FB_ALLOCATION(_pDevExt, _pSrc) ((_pSrc)->pPrimaryAllocation)

#define VPOXWDDM_CTXLOCK_INIT(_p) do { \
        KeInitializeSpinLock(&(_p)->ContextLock); \
    } while (0)
#define VPOXWDDM_CTXLOCK_DATA KIRQL _ctxLockOldIrql;
#define VPOXWDDM_CTXLOCK_LOCK(_p) do { \
        KeAcquireSpinLock(&(_p)->ContextLock, &_ctxLockOldIrql); \
    } while (0)
#define VPOXWDDM_CTXLOCK_UNLOCK(_p) do { \
        KeReleaseSpinLock(&(_p)->ContextLock, _ctxLockOldIrql); \
    } while (0)

DECLINLINE(PVPOXWDDM_ALLOCATION) vpoxWddmGetAllocationFromAllocList(DXGK_ALLOCATIONLIST *pAllocList)
{
    PVPOXWDDM_OPENALLOCATION pOa = (PVPOXWDDM_OPENALLOCATION)pAllocList->hDeviceSpecificAllocation;
    return pOa->pAllocation;
}

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPWddm_h */

