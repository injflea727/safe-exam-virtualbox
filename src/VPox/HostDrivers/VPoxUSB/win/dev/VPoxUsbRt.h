/* $Id: VPoxUsbRt.h $ */
/** @file
 * VPox USB R0 runtime
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_dev_VPoxUsbRt_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_dev_VPoxUsbRt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxUsbCmn.h"
#include "../cmn/VPoxUsbIdc.h"

#define VPOXUSBRT_MAX_CFGS 4

typedef struct VPOXUSB_PIPE_INFO {
    UCHAR       EndpointAddress;
    ULONG       NextScheduledFrame;
} VPOXUSB_PIPE_INFO;

typedef struct VPOXUSB_IFACE_INFO {
    USBD_INTERFACE_INFORMATION      *pInterfaceInfo;
    VPOXUSB_PIPE_INFO               *pPipeInfo;
} VPOXUSB_IFACE_INFO;

typedef struct VPOXUSB_RT
{
    UNICODE_STRING                  IfName;

    HANDLE                          hPipe0;
    HANDLE                          hConfiguration;
    uint32_t                        uConfigValue;

    uint32_t                        uNumInterfaces;
    USB_DEVICE_DESCRIPTOR           *devdescr;
    USB_CONFIGURATION_DESCRIPTOR    *cfgdescr[VPOXUSBRT_MAX_CFGS];

    VPOXUSB_IFACE_INFO              *pVBIfaceInfo;

    uint16_t                        idVendor, idProduct, bcdDevice;
    char                            szSerial[MAX_USB_SERIAL_STRING];
    BOOLEAN                         fIsHighSpeed;

    HVPOXUSBIDCDEV                  hMonDev;
    PFILE_OBJECT                    pOwner;
} VPOXUSB_RT, *PVPOXUSB_RT;

typedef struct VPOXUSBRT_IDC
{
    PDEVICE_OBJECT pDevice;
    PFILE_OBJECT pFile;
} VPOXUSBRT_IDC, *PVPOXUSBRT_IDC;

DECLHIDDEN(NTSTATUS) vpoxUsbRtGlobalsInit();
DECLHIDDEN(VOID) vpoxUsbRtGlobalsTerm();

DECLHIDDEN(NTSTATUS) vpoxUsbRtInit(PVPOXUSBDEV_EXT pDevExt);
DECLHIDDEN(VOID) vpoxUsbRtClear(PVPOXUSBDEV_EXT pDevExt);
DECLHIDDEN(NTSTATUS) vpoxUsbRtRm(PVPOXUSBDEV_EXT pDevExt);
DECLHIDDEN(NTSTATUS) vpoxUsbRtStart(PVPOXUSBDEV_EXT pDevExt);

DECLHIDDEN(NTSTATUS) vpoxUsbRtDispatch(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp);
DECLHIDDEN(NTSTATUS) vpoxUsbRtCreate(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp);
DECLHIDDEN(NTSTATUS) vpoxUsbRtClose(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp);

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_dev_VPoxUsbRt_h */
