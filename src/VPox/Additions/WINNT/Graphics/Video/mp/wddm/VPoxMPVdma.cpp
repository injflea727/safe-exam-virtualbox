/* $Id: VPoxMPVdma.cpp $ */
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
#include "common/VPoxMPCommon.h"
#include "VPoxMPVdma.h"
#ifdef VPOX_WITH_VIDEOHWACCEL
#include "VPoxMPVhwa.h"
#endif
#include <iprt/asm.h>
#include <iprt/mem.h>


static DECLCALLBACK(void *) hgsmiEnvAlloc(void *pvEnv, HGSMISIZE cb)
{
    NOREF(pvEnv);
    return RTMemAlloc(cb);
}

static DECLCALLBACK(void) hgsmiEnvFree(void *pvEnv, void *pv)
{
    NOREF(pvEnv);
    RTMemFree(pv);
}

static HGSMIENV g_hgsmiEnvVdma =
{
    NULL,
    hgsmiEnvAlloc,
    hgsmiEnvFree
};

/* create a DMACommand buffer */
int vpoxVdmaCreate(PVPOXMP_DEVEXT pDevExt, VPOXVDMAINFO *pInfo
        )
{
    RT_NOREF(pDevExt);
    pInfo->fEnabled           = FALSE;

    return VINF_SUCCESS;
}

int vpoxVdmaDisable (PVPOXMP_DEVEXT pDevExt, PVPOXVDMAINFO pInfo)
{
    RT_NOREF(pDevExt);
    Assert(pInfo->fEnabled);
    if (!pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;

    /* ensure nothing else is submitted */
    pInfo->fEnabled        = FALSE;
    return VINF_SUCCESS;
}

int vpoxVdmaEnable (PVPOXMP_DEVEXT pDevExt, PVPOXVDMAINFO pInfo)
{
    RT_NOREF(pDevExt);
    Assert(!pInfo->fEnabled);
    if (pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;
    return VINF_SUCCESS;
}

int vpoxVdmaDestroy (PVPOXMP_DEVEXT pDevExt, PVPOXVDMAINFO pInfo)
{
    int rc = VINF_SUCCESS;
    Assert(!pInfo->fEnabled);
    if (pInfo->fEnabled)
        rc = vpoxVdmaDisable (pDevExt, pInfo);
    return rc;
}
