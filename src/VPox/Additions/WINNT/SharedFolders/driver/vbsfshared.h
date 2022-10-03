/* $Id: vbsfshared.h $ */
/** @file
 * VirtualPox Windows Guest Shared Folders FSD - Definitions shared with the network provider dll.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsfshared_h
#define GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsfshared_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** The network provider name for shared folders. */
#define MRX_VPOX_PROVIDER_NAME_U                L"VirtualPox Shared Folders"

/** The filesystem name for shared folders. */
#define MRX_VPOX_FILESYS_NAME_U                 L"VPoxSharedFolderFS"

/** The redirector device name. */
#define DD_MRX_VPOX_FS_DEVICE_NAME_U            L"\\Device\\VPoxMiniRdr"

/** Volume label prefix. */
#define VPOX_VOLNAME_PREFIX                     L"VPOX_"
/** Size of volume label prefix. */
#define VPOX_VOLNAME_PREFIX_SIZE                (sizeof(VPOX_VOLNAME_PREFIX) - sizeof(VPOX_VOLNAME_PREFIX[0]))

/** NT path of the symbolic link, which is used by the user mode dll to
 * open the FSD. */
#define DD_MRX_VPOX_USERMODE_SHADOW_DEV_NAME_U  L"\\??\\VPoxMiniRdrDN"
/** Win32 path of the symbolic link, which is used by the user mode dll
 * to open the FSD. */
#define DD_MRX_VPOX_USERMODE_DEV_NAME_U         L"\\\\.\\VPoxMiniRdrDN"

/** @name IOCTL_MRX_VPOX_XXX - VPoxSF IOCTL codes.
 * @{  */
#define IOCTL_MRX_VPOX_ADDCONN          CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 100, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_GETCONN          CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 101, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_DELCONN          CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 102, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_GETLIST          CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 103, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_GETGLOBALLIST    CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 104, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_GETGLOBALCONN    CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 105, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_START            CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 106, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MRX_VPOX_STOP             CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 107, METHOD_BUFFERED, FILE_ANY_ACCESS)
/** @} */

#endif /* !GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsfshared_h */
