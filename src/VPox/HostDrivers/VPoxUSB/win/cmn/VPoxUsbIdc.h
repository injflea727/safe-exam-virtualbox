/* $Id: VPoxUsbIdc.h $ */
/** @file
 * Windows USB Proxy - Monitor Driver communication interface.
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

#ifndef VPOX_INCLUDED_SRC_VPoxUSB_win_cmn_VPoxUsbIdc_h
#define VPOX_INCLUDED_SRC_VPoxUSB_win_cmn_VPoxUsbIdc_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOXUSBIDC_VERSION_MAJOR 1
#define VPOXUSBIDC_VERSION_MINOR 0

#define VPOXUSBIDC_INTERNAL_IOCTL_GET_VERSION         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x618, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define VPOXUSBIDC_INTERNAL_IOCTL_PROXY_STARTUP       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x619, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define VPOXUSBIDC_INTERNAL_IOCTL_PROXY_TEARDOWN      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x61A, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define VPOXUSBIDC_INTERNAL_IOCTL_PROXY_STATE_CHANGE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x61B, METHOD_NEITHER, FILE_WRITE_ACCESS)

typedef struct
{
    uint32_t        u32Major;
    uint32_t        u32Minor;
} VPOXUSBIDC_VERSION, *PVPOXUSBIDC_VERSION;

typedef void *HVPOXUSBIDCDEV;

/* the initial device state is USBDEVICESTATE_HELD_BY_PROXY */
typedef struct VPOXUSBIDC_PROXY_STARTUP
{
    union
    {
        /* in: device PDO */
        PDEVICE_OBJECT pPDO;
        /* out: device handle to be used for subsequent USBSUP_PROXY_XXX calls */
        HVPOXUSBIDCDEV hDev;
    } u;
} VPOXUSBIDC_PROXY_STARTUP, *PVPOXUSBIDC_PROXY_STARTUP;

typedef struct VPOXUSBIDC_PROXY_TEARDOWN
{
    HVPOXUSBIDCDEV hDev;
} VPOXUSBIDC_PROXY_TEARDOWN, *PVPOXUSBIDC_PROXY_TEARDOWN;

typedef enum
{
    VPOXUSBIDC_PROXY_STATE_UNKNOWN = 0,
    VPOXUSBIDC_PROXY_STATE_IDLE,
    VPOXUSBIDC_PROXY_STATE_INITIAL = VPOXUSBIDC_PROXY_STATE_IDLE,
    VPOXUSBIDC_PROXY_STATE_USED_BY_GUEST
} VPOXUSBIDC_PROXY_STATE;

typedef struct VPOXUSBIDC_PROXY_STATE_CHANGE
{
    HVPOXUSBIDCDEV hDev;
    VPOXUSBIDC_PROXY_STATE enmState;
} VPOXUSBIDC_PROXY_STATE_CHANGE, *PVPOXUSBIDC_PROXY_STATE_CHANGE;

NTSTATUS VPoxUsbIdcInit();
VOID VPoxUsbIdcTerm();
NTSTATUS VPoxUsbIdcProxyStarted(PDEVICE_OBJECT pPDO, HVPOXUSBIDCDEV *phDev);
NTSTATUS VPoxUsbIdcProxyStopped(HVPOXUSBIDCDEV hDev);

#endif /* !VPOX_INCLUDED_SRC_VPoxUSB_win_cmn_VPoxUsbIdc_h */
