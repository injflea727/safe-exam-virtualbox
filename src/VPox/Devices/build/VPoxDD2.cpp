/* $Id: VPoxDD2.cpp $ */
/** @file
 * VPoxDD2 - Built-in drivers & devices part 2.
 *
 * These drivers and devices are in separate modules because of LGPL.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV
#include <VPox/vmm/pdm.h>
#include <VPox/version.h>
#include <iprt/errcore.h>

#include <VPox/log.h>
#include <iprt/assert.h>

#include "VPoxDD2.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
const void *g_apvVPoxDDDependencies2[] =
{
    (void *)&g_abPcBiosBinary386,
    (void *)&g_abPcBiosBinary286,
    (void *)&g_abPcBiosBinary8086,
    (void *)&g_abVgaBiosBinary386,
    (void *)&g_abVgaBiosBinary286,
    (void *)&g_abVgaBiosBinary8086,
#ifdef VPOX_WITH_PXE_ROM
    (void *)&g_abNetBiosBinary,
#endif
};


/**
 * Register builtin devices.
 *
 * @returns VPox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VPox version number.
 */
extern "C" DECLEXPORT(int) VPoxDevicesRegister(PPDMDEVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VPoxDevicesRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VPOX_VERSION, ("u32Version=%#x VPOX_VERSION=%#x\n", u32Version, VPOX_VERSION));

#ifndef VPOX_WITH_NEW_LPC_DEVICE
    int rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceLPC);
    if (RT_FAILURE(rc))
        return rc;
#else
    RT_NOREF(pCallbacks);
#endif

    return VINF_SUCCESS;
}

