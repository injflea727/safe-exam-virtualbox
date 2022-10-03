/* $Id: DevPcBios.h $ */
/** @file
 * DevPcBios - PC BIOS Device, header shared with the BIOS code.
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

#ifndef VPOX_INCLUDED_SRC_PC_DevPcBios_h
#define VPOX_INCLUDED_SRC_PC_DevPcBios_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @def VPOX_DMI_TABLE_BASE */
#define VPOX_DMI_TABLE_BASE         0xe1000
#define VPOX_DMI_TABLE_VER          0x25

/** def VPOX_DMI_TABLE_SIZE
 *
 * The size should be at least 16-byte aligned for a proper alignment of
 * the MPS table.
 */
#define VPOX_DMI_TABLE_SIZE         768

/** def VPOX_DMI_TABLE_SIZE
 *
 * The size should be at least 16-byte aligned for a proper alignment of
 * the MPS table.
 */
#define VPOX_DMI_HDR_SIZE           32

/** @def VPOX_LANBOOT_SEG
 *
 * Should usually start right after the DMI BIOS page
 */
#define VPOX_LANBOOT_SEG            0xe200

#define VPOX_SMBIOS_MAJOR_VER       2
#define VPOX_SMBIOS_MINOR_VER       5
#define VPOX_SMBIOS_MAXSS           0xff   /* Not very accurate */

#endif /* !VPOX_INCLUDED_SRC_PC_DevPcBios_h */
