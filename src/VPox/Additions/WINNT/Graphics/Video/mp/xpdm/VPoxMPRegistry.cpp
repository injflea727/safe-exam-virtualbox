/* $Id: VPoxMPRegistry.cpp $ */
/** @file
 * VPox XPDM Miniport registry related functions
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

#include "common/VPoxMPCommon.h"

static VP_STATUS
VPoxMPQueryNamedValueCB(PVOID HwDeviceExtension, PVOID Context, PWSTR ValueName, PVOID ValueData, ULONG ValueLength)
{
    RT_NOREF(HwDeviceExtension, ValueName);
    PAGED_CODE();

    if (!ValueLength || !Context)
    {
        WARN(("failed due to invalid parameters"));
        return ERROR_INVALID_PARAMETER;
    }

    *(uint32_t *)Context = *(uint32_t *)ValueData;

    return NO_ERROR;
}


VP_STATUS VPoxMPCmnRegInit(IN PVPOXMP_DEVEXT pExt, OUT VPOXMPCMNREGISTRY *pReg)
{
    *pReg = pExt->pPrimary;
    return NO_ERROR;
}

VP_STATUS VPoxMPCmnRegFini(IN VPOXMPCMNREGISTRY Reg)
{
    RT_NOREF(Reg);
    return NO_ERROR;
}

VP_STATUS VPoxMPCmnRegSetDword(IN VPOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t Val)
{
    return VideoPortSetRegistryParameters(Reg, pName, &Val, sizeof(Val));
}

VP_STATUS VPoxMPCmnRegQueryDword(IN VPOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t *pVal)
{
    VP_STATUS rc;

    rc = VideoPortGetRegistryParameters(Reg, pName, FALSE, VPoxMPQueryNamedValueCB, pVal);
    if (rc!=NO_ERROR && pVal)
    {
        *pVal = 0;
    }
    return rc;
}
