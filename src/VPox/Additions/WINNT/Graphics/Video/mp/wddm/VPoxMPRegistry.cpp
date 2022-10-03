/* $Id: VPoxMPRegistry.cpp $ */
/** @file
 * VPox WDDM Miniport registry related functions
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

VP_STATUS VPoxMPCmnRegInit(IN PVPOXMP_DEVEXT pExt, OUT VPOXMPCMNREGISTRY *pReg)
{
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vpoxWddmRegQueryDrvKeyName(pExt, cbBuf, Buf, &cbBuf);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vpoxWddmRegOpenKey(pReg, Buf, GENERIC_READ | GENERIC_WRITE);
        AssertNtStatusSuccess(Status);
        if(Status == STATUS_SUCCESS)
            return NO_ERROR;
    }

    /* fall-back to make the subsequent VPoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *pReg = NULL;

    return ERROR_INVALID_PARAMETER;
}

VP_STATUS VPoxMPCmnRegFini(IN VPOXMPCMNREGISTRY Reg)
{
    if (!Reg)
    {
        return ERROR_INVALID_PARAMETER;
    }

    NTSTATUS Status = ZwClose(Reg);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

VP_STATUS VPoxMPCmnRegQueryDword(IN VPOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t *pVal)
{
    /* seems like the new code assumes the Reg functions zeroes up the value on failure */
    *pVal = 0;

    if (!Reg)
    {
        return ERROR_INVALID_PARAMETER;
    }

    NTSTATUS Status = vpoxWddmRegQueryValueDword(Reg, pName, (PDWORD)pVal);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

VP_STATUS VPoxMPCmnRegSetDword(IN VPOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t Val)
{
    if (!Reg)
    {
        return ERROR_INVALID_PARAMETER;
    }

    NTSTATUS Status = vpoxWddmRegSetValueDword(Reg, pName, Val);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}
