/* $Id: VPoxDrvTool.h $ */
/** @file
 * Windows Driver R0 Tooling.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_cmn_VPoxDrvTool_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_cmn_VPoxDrvTool_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/cdefs.h>
#include <iprt/stdint.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/nt/wdm.h>


RT_C_DECLS_BEGIN

#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VPOXDRVTOOL
#  define VPOXDRVTOOL_DECL(a_Type) DECLEXPORT(a_Type)
# else
#  define VPOXDRVTOOL_DECL(a_Type) DECLIMPORT(a_Type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VPOXDRVTOOL_DECL(a_Type) a_Type VPOXCALL
#endif

VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolRegOpenKeyU(OUT PHANDLE phKey, IN PUNICODE_STRING pName, IN ACCESS_MASK fAccess);
VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess);
VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolRegCloseKey(IN HANDLE hKey);
VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PULONG pDword);
VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT ULONG val);

VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolIoPostAsync(PDEVICE_OBJECT pDevObj, PIRP pIrp, PKEVENT pEvent);
VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolIoPostSync(PDEVICE_OBJECT pDevObj, PIRP pIrp);
VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolIoPostSyncWithTimeout(PDEVICE_OBJECT pDevObj, PIRP pIrp, ULONG dwTimeoutMs);
DECLINLINE(NTSTATUS) VPoxDrvToolIoComplete(PIRP pIrp, NTSTATUS Status, ULONG ulInfo)
{
    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information = ulInfo;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return Status;
}

typedef struct VPOXDRVTOOL_REF
{
    volatile uint32_t cRefs;
} VPOXDRVTOOL_REF, *PVPOXDRVTOOL_REF;

DECLINLINE(void) VPoxDrvToolRefInit(PVPOXDRVTOOL_REF pRef)
{
    pRef->cRefs = 1;
}

DECLINLINE(uint32_t) VPoxDrvToolRefRetain(PVPOXDRVTOOL_REF pRef)
{
    Assert(pRef->cRefs);
    Assert(pRef->cRefs < UINT32_MAX / 2);
    return ASMAtomicIncU32(&pRef->cRefs);
}

DECLINLINE(uint32_t) VPoxDrvToolRefRelease(PVPOXDRVTOOL_REF pRef)
{
    uint32_t cRefs = ASMAtomicDecU32(&pRef->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    return cRefs;
}

VPOXDRVTOOL_DECL(VOID) VPoxDrvToolRefWaitEqual(PVPOXDRVTOOL_REF pRef, uint32_t u32Val);

VPOXDRVTOOL_DECL(NTSTATUS) VPoxDrvToolStrCopy(PUNICODE_STRING pDst, CONST PUNICODE_STRING pSrc);
VPOXDRVTOOL_DECL(VOID) VPoxDrvToolStrFree(PUNICODE_STRING pStr);

RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_cmn_VPoxDrvTool_h */

