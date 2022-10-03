/* $Id: VPoxMPVdma.h $ */
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVdma_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVdma_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#include <VPoxVideo.h>
#include <HGSMI.h>

typedef struct _VPOXMP_DEVEXT *PVPOXMP_DEVEXT;

/* DMA commands are currently submitted over HGSMI */
typedef struct VPOXVDMAINFO
{
    BOOL      fEnabled;
} VPOXVDMAINFO, *PVPOXVDMAINFO;

int vpoxVdmaCreate (PVPOXMP_DEVEXT pDevExt, VPOXVDMAINFO *pInfo
        );
int vpoxVdmaDisable(PVPOXMP_DEVEXT pDevExt, PVPOXVDMAINFO pInfo);
int vpoxVdmaEnable(PVPOXMP_DEVEXT pDevExt, PVPOXVDMAINFO pInfo);
int vpoxVdmaDestroy(PVPOXMP_DEVEXT pDevExt, PVPOXVDMAINFO pInfo);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPVdma_h */
