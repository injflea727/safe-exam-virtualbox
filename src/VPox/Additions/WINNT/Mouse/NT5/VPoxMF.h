/* $Id: VPoxMF.h $ */
/** @file
 * VPox Mouse Filter Driver - Internal Header.
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

#ifndef GA_INCLUDED_SRC_WINNT_Mouse_NT5_VPoxMF_h
#define GA_INCLUDED_SRC_WINNT_Mouse_NT5_VPoxMF_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#define LOG_GROUP LOG_GROUP_DRV_MOUSE
#include <VPox/log.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include "../common/VPoxMouseLog.h"
#include <iprt/nt/ntddk.h>
RT_C_DECLS_BEGIN
#include <ntddmou.h>
#include <ntddkbd.h>
#include <ntdd8042.h>
RT_C_DECLS_END
#include <VPox/VMMDev.h> /* for VMMDevReqMouseStatus */

#define IOCTL_INTERNAL_MOUSE_CONNECT CTL_CODE(FILE_DEVICE_MOUSE, 0x0080, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef VOID (*PFNSERVICECB)(PDEVICE_OBJECT DeviceObject, PMOUSE_INPUT_DATA InputDataStart,
                             PMOUSE_INPUT_DATA InputDataEnd, PULONG InputDataConsumed);

typedef struct _INTERNAL_MOUSE_CONNECT_DATA
{
    PDEVICE_OBJECT pDO;
    PFNSERVICECB pfnServiceCB;
} INTERNAL_MOUSE_CONNECT_DATA, *PINTERNAL_MOUSE_CONNECT_DATA;

typedef struct _VPOXMOUSE_DEVEXT
{
    LIST_ENTRY ListEntry;
    PDEVICE_OBJECT pdoMain;           /* PDO passed to VPoxDrvAddDevice */
    PDEVICE_OBJECT pdoSelf;           /* our PDO created in VPoxDrvAddDevice*/
    PDEVICE_OBJECT pdoParent;         /* Highest PDO in chain before we've attached our filter */

    BOOLEAN bHostMouse;               /* Indicates if we're filtering the chain with emulated i8042 PS/2 adapter */

    INTERNAL_MOUSE_CONNECT_DATA OriginalConnectData; /* Original connect data intercepted in IOCTL_INTERNAL_MOUSE_CONNECT */
    VMMDevReqMouseStatus       *pSCReq;              /* Preallocated request to use in pfnServiceCB */

    IO_REMOVE_LOCK RemoveLock;
} VPOXMOUSE_DEVEXT, *PVPOXMOUSE_DEVEXT;

/* Interface functions */
RT_C_DECLS_BEGIN
 NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
RT_C_DECLS_END

NTSTATUS VPoxDrvAddDevice(IN PDRIVER_OBJECT Driver, IN PDEVICE_OBJECT PDO);
VOID VPoxDrvUnload(IN PDRIVER_OBJECT Driver);

/* IRP handlers */
NTSTATUS VPoxIrpPassthrough(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VPoxIrpInternalIOCTL(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VPoxIrpPnP(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS VPoxIrpPower(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

/* Internal functions */
void VPoxMouFltInitGlobals(void);
void VPoxMouFltDeleteGlobals(void);
void VPoxDeviceAdded(PVPOXMOUSE_DEVEXT pDevExt);
void VPoxInformHost(PVPOXMOUSE_DEVEXT pDevExt);
void VPoxDeviceRemoved(PVPOXMOUSE_DEVEXT pDevExt);

VOID VPoxDrvNotifyServiceCB(PVPOXMOUSE_DEVEXT pDevExt, PMOUSE_INPUT_DATA InputDataStart, PMOUSE_INPUT_DATA InputDataEnd, PULONG  InputDataConsumed);

#endif /* !GA_INCLUDED_SRC_WINNT_Mouse_NT5_VPoxMF_h */
