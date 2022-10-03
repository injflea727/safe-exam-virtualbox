/* $Id: tstUSBLinux.h $ */
/** @file
 * VirtualPox USB Proxy Service class, test version for Linux hosts.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_SRC_testcase_tstUSBLinux_h
#define MAIN_INCLUDED_SRC_testcase_tstUSBLinux_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

typedef int HRESULT;
enum { S_OK = 0, E_NOTIMPL = 1 };

#include <VPox/usb.h>
#include <VPox/usbfilter.h>

#include <VPox/err.h>

#ifdef VPOX_USB_WITH_SYSFS
# include <libhal.h>
#endif

#include <stdio.h>
/**
 * The Linux hosted USB Proxy Service.
 */
class USBProxyServiceLinux
{
public:
    USBProxyServiceLinux()
        : mLastError(VINF_SUCCESS)
    {}

    HRESULT initSysfs(void);
    PUSBDEVICE getDevicesFromSysfs(void);
    int getLastError(void)
    {
        return mLastError;
    }

private:
    int start(void) { return VINF_SUCCESS; }
    static void freeDevice(PUSBDEVICE) {}  /* We don't care about leaks in a test. */
    int usbProbeInterfacesFromLibhal(const char *pszHalUuid, PUSBDEVICE pDev);
    int mLastError;
#  ifdef VPOX_USB_WITH_SYSFS
    /** Our connection to DBus for getting information from hal.  This will be
     * NULL if the initialisation failed. */
    DBusConnection *mDBusConnection;
    /** Handle to libhal. */
    LibHalContext *mLibHalContext;
#  endif
};

#endif /* !MAIN_INCLUDED_SRC_testcase_tstUSBLinux_h */

