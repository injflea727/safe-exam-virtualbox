/** @file
 * USBLib - Library for wrapping up the VPoxUSB functionality, Solaris flavor.
 * (DEV,HDrv,Main)
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

#ifndef VPOX_INCLUDED_usblib_solaris_h
#define VPOX_INCLUDED_usblib_solaris_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/cdefs.h>
#include <VPox/usbfilter.h>
#include <VPox/vusb.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/param.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_usblib_solaris    Solaris USB Specifics
 * @ingroup grp_usblib
 * @{
 */

/** @name VPoxUSB specific IOCtls.
 * VPoxUSB uses them for resetting USB devices requests from userland.
 * USBProxyService/Device makes use of them to communicate with VPoxUSB.
 * @{ */

/** Ring-3 request wrapper for big requests.
 *
 * This is necessary because the ioctl number scheme on many Unixy OSes (esp. Solaris)
 * only allows a relatively small size to be encoded into the request. So, for big
 * request this generic form is used instead. */
typedef struct VPOXUSBREQ
{
    /** Magic value (VPOXUSB(MON)_MAGIC). */
    uint32_t    u32Magic;
    /** The size of the data buffer (In & Out). */
    uint32_t    cbData;
    /** Result code of the request filled by driver. */
    int32_t     rc;
    /** The user address of the data buffer. */
    RTR3PTR     pvDataR3;
} VPOXUSBREQ;
/** Pointer to a request wrapper for solaris. */
typedef VPOXUSBREQ *PVPOXUSBREQ;
/** Pointer to a const request wrapper for solaris. */
typedef const VPOXUSBREQ *PCVPOXUSBREQ;

#pragma pack(1)
typedef struct
{
    /* Pointer to the Filter. */
    USBFILTER      Filter;
    /* Where to store the added Filter (Id). */
    uintptr_t      uId;
} VPOXUSBREQ_ADD_FILTER;

typedef struct
{
    /* Pointer to Filter (Id) to be removed. */
    uintptr_t      uId;
} VPOXUSBREQ_REMOVE_FILTER;

typedef struct
{
    /** Whether to re-attach the driver. */
    bool           fReattach;
    /* Physical path of the USB device. */
    char           szDevicePath[1];
} VPOXUSBREQ_RESET_DEVICE;

typedef struct
{
    /* Where to store the instance. */
    int           *pInstance;
    /* Physical path of the USB device. */
    char           szDevicePath[1];
} VPOXUSBREQ_DEVICE_INSTANCE;

typedef struct
{
    /** Where to store the instance. */
    int            Instance;
    /* Where to store the client path. */
    char           szClientPath[MAXPATHLEN];
    /** Device identifier (VendorId:ProductId:Release:StaticPath) */
    char           szDeviceIdent[MAXPATHLEN+48];
    /** Callback from monitor specifying client consumer (VM) credentials */
    DECLR0CALLBACKMEMBER(int, pfnSetConsumerCredentials,(RTPROCESS Process, int Instance, void *pvReserved));
} VPOXUSBREQ_CLIENT_INFO, *PVPOXUSBREQ_CLIENT_INFO;
typedef VPOXUSBREQ_CLIENT_INFO VPOXUSB_CLIENT_INFO;
typedef PVPOXUSBREQ_CLIENT_INFO PVPOXUSB_CLIENT_INFO;

/** Isoc packet descriptor (Must mirror exactly Solaris USBA's usb_isoc_pkt_descr_t) */
typedef struct
{
    ushort_t                cbPkt;              /* Size of the packet */
    ushort_t                cbActPkt;           /* Size of the packet actually transferred */
    VUSBSTATUS              enmStatus;          /* Per frame transfer status */
} VUSBISOC_PKT_DESC;

/** VPoxUSB IOCtls */
typedef struct
{
    void                   *pvUrbR3;            /* Pointer to userland URB (untouched by kernel driver) */
    uint8_t                 bEndpoint;          /* Endpoint address */
    VUSBXFERTYPE            enmType;            /* Xfer type */
    VUSBDIRECTION           enmDir;             /* Xfer direction */
    VUSBSTATUS              enmStatus;          /* URB status */
    bool                    fShortOk;           /* Whether receiving less data than requested is acceptable. */
    size_t                  cbData;             /* Size of the data */
    void                   *pvData;             /* Pointer to the data */
    uint32_t                cIsocPkts;          /* Number of Isoc packets */
    VUSBISOC_PKT_DESC       aIsocPkts[8];       /* Array of Isoc packet descriptors */
} VPOXUSBREQ_URB, *PVPOXUSBREQ_URB;

typedef struct
{
    uint8_t                 bEndpoint;          /* Endpoint address */
} VPOXUSBREQ_CLEAR_EP, *PVPOXUSBREQ_CLEAR_EP;


typedef struct
{
    uint8_t                 bConfigValue;       /* Configuration value */
} VPOXUSBREQ_SET_CONFIG, *PVPOXUSBREQ_SET_CONFIG;
typedef VPOXUSBREQ_SET_CONFIG  VPOXUSBREQ_GET_CONFIG;
typedef PVPOXUSBREQ_SET_CONFIG PVPOXUSBREQ_GET_CONFIG;

typedef struct
{
    uint8_t                 bInterface;         /* Interface number */
    uint8_t                 bAlternate;         /* Alternate setting */
} VPOXUSBREQ_SET_INTERFACE, *PVPOXUSBREQ_SET_INTERFACE;

typedef enum
{
    /** Close device not a reset. */
    VPOXUSB_RESET_LEVEL_CLOSE     = 0,
    /** Hard reset resulting in device replug behaviour. */
    VPOXUSB_RESET_LEVEL_REATTACH  = 2,
    /** Device-level reset. */
    VPOXUSB_RESET_LEVEL_SOFT      = 4
} VPOXUSB_RESET_LEVEL;

typedef struct
{
    VPOXUSB_RESET_LEVEL     ResetLevel;         /* Reset level after closing */
} VPOXUSBREQ_CLOSE_DEVICE, *PVPOXUSBREQ_CLOSE_DEVICE;

typedef struct
{
    uint8_t                 bEndpoint;          /* Endpoint address */
} VPOXUSBREQ_ABORT_PIPE, *PVPOXUSBREQ_ABORT_PIPE;

typedef struct
{
    uint32_t                u32Major;           /* Driver major number */
    uint32_t                u32Minor;           /* Driver minor number */
} VPOXUSBREQ_GET_VERSION, *PVPOXUSBREQ_GET_VERSION;

#pragma pack()

/** The VPOXUSBREQ::u32Magic value for VPoxUSBMon. */
#define VPOXUSBMON_MAGIC           0xba5eba11
/** The VPOXUSBREQ::u32Magic value for VPoxUSB.*/
#define VPOXUSB_MAGIC              0x601fba11
/** The USBLib entry point for userland. */
#define VPOXUSB_DEVICE_NAME        "/dev/vpoxusbmon"

/** The USBMonitor Major version. */
#define VPOXUSBMON_VERSION_MAJOR   2
/** The USBMonitor Minor version. */
#define VPOXUSBMON_VERSION_MINOR   1

/** The USB Major version. */
#define VPOXUSB_VERSION_MAJOR      1
/** The USB Minor version. */
#define VPOXUSB_VERSION_MINOR      1

#ifdef RT_ARCH_AMD64
# define VPOXUSB_IOCTL_FLAG     128
#elif defined(RT_ARCH_X86)
# define VPOXUSB_IOCTL_FLAG     0
#else
# error "dunno which arch this is!"
#endif

/** USB driver name*/
#define VPOXUSB_DRIVER_NAME     "vpoxusb"

/* No automatic buffering, size limited to 255 bytes => use VPOXUSBREQ for everything. */
#define VPOXUSB_IOCTL_CODE(Function, Size)  _IOWRN('V', (Function) | VPOXUSB_IOCTL_FLAG, sizeof(VPOXUSBREQ))
#define VPOXUSB_IOCTL_CODE_FAST(Function)   _IO(   'V', (Function) | VPOXUSB_IOCTL_FLAG)
#define VPOXUSB_IOCTL_STRIP_SIZE(Code)      (Code)

#define VPOXUSBMON_IOCTL_ADD_FILTER         VPOXUSB_IOCTL_CODE(1, (sizeof(VPoxUSBAddFilterReq)))
#define VPOXUSBMON_IOCTL_REMOVE_FILTER      VPOXUSB_IOCTL_CODE(2, (sizeof(VPoxUSBRemoveFilterReq)))
#define VPOXUSBMON_IOCTL_RESET_DEVICE       VPOXUSB_IOCTL_CODE(3, (sizeof(VPOXUSBREQ_RESET_DEVICE)))
#define VPOXUSBMON_IOCTL_DEVICE_INSTANCE    VPOXUSB_IOCTL_CODE(4, (sizeof(VPOXUSBREQ_DEVICE_INSTANCE)))
#define VPOXUSBMON_IOCTL_CLIENT_INFO        VPOXUSB_IOCTL_CODE(5, (sizeof(VPOXUSBREQ_CLIENT_PATH)))
#define VPOXUSBMON_IOCTL_GET_VERSION        VPOXUSB_IOCTL_CODE(6, (sizeof(VPOXUSBREQ_GET_VERSION)))

/* VPoxUSB ioctls */
#define VPOXUSB_IOCTL_SEND_URB              VPOXUSB_IOCTL_CODE(20, (sizeof(VPOXUSBREQ_URB)))            /* 1072146796 */
#define VPOXUSB_IOCTL_REAP_URB              VPOXUSB_IOCTL_CODE(21, (sizeof(VPOXUSBREQ_URB)))            /* 1072146795 */
#define VPOXUSB_IOCTL_CLEAR_EP              VPOXUSB_IOCTL_CODE(22, (sizeof(VPOXUSBREQ_CLEAR_EP)))       /* 1072146794 */
#define VPOXUSB_IOCTL_SET_CONFIG            VPOXUSB_IOCTL_CODE(23, (sizeof(VPOXUSBREQ_SET_CONFIG)))     /* 1072146793 */
#define VPOXUSB_IOCTL_SET_INTERFACE         VPOXUSB_IOCTL_CODE(24, (sizeof(VPOXUSBREQ_SET_INTERFACE)))  /* 1072146792 */
#define VPOXUSB_IOCTL_CLOSE_DEVICE          VPOXUSB_IOCTL_CODE(25, (sizeof(VPOXUSBREQ_CLOSE_DEVICE)))   /* 1072146791 0xc0185699 */
#define VPOXUSB_IOCTL_ABORT_PIPE            VPOXUSB_IOCTL_CODE(26, (sizeof(VPOXUSBREQ_ABORT_PIPE)))     /* 1072146790 */
#define VPOXUSB_IOCTL_GET_CONFIG            VPOXUSB_IOCTL_CODE(27, (sizeof(VPOXUSBREQ_GET_CONFIG)))     /* 1072146789 */
#define VPOXUSB_IOCTL_GET_VERSION           VPOXUSB_IOCTL_CODE(28, (sizeof(VPOXUSBREQ_GET_VERSION)))    /* 1072146788 */

/** @} */

/* USBLibHelper data for resetting the device. */
typedef struct VPOXUSBHELPERDATA_RESET
{
    /** Path of the USB device. */
    const char  *pszDevicePath;
    /** Re-enumerate or not. */
    bool        fHardReset;
} VPOXUSBHELPERDATA_RESET;
typedef VPOXUSBHELPERDATA_RESET *PVPOXUSBHELPERDATA_RESET;
typedef const VPOXUSBHELPERDATA_RESET *PCVPOXUSBHELPERDATA_RESET;

/* USBLibHelper data for device hijacking. */
typedef struct VPOXUSBHELPERDATA_ALIAS
{
    /** Vendor ID. */
    uint16_t        idVendor;
    /** Product ID. */
    uint16_t        idProduct;
    /** Revision, integer part. */
    uint16_t        bcdDevice;
    /** Path of the USB device. */
    const char      *pszDevicePath;
} VPOXUSBHELPERDATA_ALIAS;
typedef VPOXUSBHELPERDATA_ALIAS *PVPOXUSBHELPERDATA_ALIAS;
typedef const VPOXUSBHELPERDATA_ALIAS *PCVPOXUSBHELPERDATA_ALIAS;

USBLIB_DECL(int) USBLibResetDevice(char *pszDevicePath, bool fReattach);
USBLIB_DECL(int) USBLibDeviceInstance(char *pszDevicePath, int *pInstance);
USBLIB_DECL(int) USBLibGetClientInfo(char *pszDeviceIdent, char **ppszClientPath, int *pInstance);
USBLIB_DECL(int) USBLibAddDeviceAlias(PUSBDEVICE pDevice);
USBLIB_DECL(int) USBLibRemoveDeviceAlias(PUSBDEVICE pDevice);
/*USBLIB_DECL(int) USBLibConfigureDevice(PUSBDEVICE pDevice);*/

/** @} */
RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_usblib_solaris_h */

