/* $Id: VPoxMPVbva.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVbva_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVbva_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/cdefs.h>  /* for VPOXCALL */

typedef struct VPOXVBVAINFO
{
    VBVABUFFERCONTEXT Vbva;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
    KSPIN_LOCK Lock;
} VPOXVBVAINFO;

int vpoxVbvaEnable(PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva);
int vpoxVbvaDisable(PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva);
int vpoxVbvaDestroy(PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva);
int vpoxVbvaCreate(PVPOXMP_DEVEXT pDevExt, VPOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vpoxVbvaReportDirtyRect(PVPOXMP_DEVEXT pDevExt, struct VPOXWDDM_SOURCE *pSrc, RECT *pRectOrig);

#define VPOXVBVA_OP(_op, _pdext, _psrc, _arg) \
        do { \
            if (VPoxVBVABufferBeginUpdate(&(_psrc)->Vbva.Vbva, &VPoxCommonFromDeviceExt(_pdext)->guestCtx)) \
            { \
                vpoxVbva##_op(_pdext, _psrc, _arg); \
                VPoxVBVABufferEndUpdate(&(_psrc)->Vbva.Vbva); \
            } \
        } while (0)

#define VPOXVBVA_OP_WITHLOCK_ATDPC(_op, _pdext, _psrc, _arg) \
        do { \
            Assert(KeGetCurrentIrql() == DISPATCH_LEVEL); \
            KeAcquireSpinLockAtDpcLevel(&(_psrc)->Vbva.Lock);  \
            VPOXVBVA_OP(_op, _pdext, _psrc, _arg);        \
            KeReleaseSpinLockFromDpcLevel(&(_psrc)->Vbva.Lock);\
        } while (0)

#define VPOXVBVA_OP_WITHLOCK(_op, _pdext, _psrc, _arg) \
        do { \
            KIRQL OldIrql; \
            KeAcquireSpinLock(&(_psrc)->Vbva.Lock, &OldIrql);  \
            VPOXVBVA_OP(_op, _pdext, _psrc, _arg);        \
            KeReleaseSpinLock(&(_psrc)->Vbva.Lock, OldIrql);   \
        } while (0)

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVbva_h */
