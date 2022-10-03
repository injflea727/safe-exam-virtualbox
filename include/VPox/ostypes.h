/** @file
 * VirtualPox - Global Guest Operating System definition.
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

#ifndef VPOX_INCLUDED_ostypes_h
#define VPOX_INCLUDED_ostypes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN

/**
 * Global list of guest operating system types.
 *
 * They are grouped into families. A family identifer is always has
 * mod 0x10000 == 0. New entries can be added, however other components
 * depend on the values (e.g. the Qt GUI and guest additions) so the
 * existing values MUST stay the same.
 *
 * Note: distinguish between 32 & 64 bits guest OSes by checking bit 8 (mod 0x100)
 */
typedef enum VPOXOSTYPE
{
    VPOXOSTYPE_Unknown          = 0,
    VPOXOSTYPE_Unknown_x64      = 0x00100,
    /** @name DOS and it's descendants
     * @{ */
    VPOXOSTYPE_DOS              = 0x10000,
    VPOXOSTYPE_Win31            = 0x15000,
    VPOXOSTYPE_Win9x            = 0x20000,
    VPOXOSTYPE_Win95            = 0x21000,
    VPOXOSTYPE_Win98            = 0x22000,
    VPOXOSTYPE_WinMe            = 0x23000,
    VPOXOSTYPE_WinNT            = 0x30000,
    VPOXOSTYPE_WinNT_x64        = 0x30100,
    VPOXOSTYPE_WinNT3x          = 0x30800,
    VPOXOSTYPE_WinNT4           = 0x31000,
    VPOXOSTYPE_Win2k            = 0x32000,
    VPOXOSTYPE_WinXP            = 0x33000,
    VPOXOSTYPE_WinXP_x64        = 0x33100,
    VPOXOSTYPE_Win2k3           = 0x34000,
    VPOXOSTYPE_Win2k3_x64       = 0x34100,
    VPOXOSTYPE_WinVista         = 0x35000,
    VPOXOSTYPE_WinVista_x64     = 0x35100,
    VPOXOSTYPE_Win2k8           = 0x36000,
    VPOXOSTYPE_Win2k8_x64       = 0x36100,
    VPOXOSTYPE_Win7             = 0x37000,
    VPOXOSTYPE_Win7_x64         = 0x37100,
    VPOXOSTYPE_Win8             = 0x38000,
    VPOXOSTYPE_Win8_x64         = 0x38100,
    VPOXOSTYPE_Win2k12_x64      = 0x39100,
    VPOXOSTYPE_Win81            = 0x3A000,
    VPOXOSTYPE_Win81_x64        = 0x3A100,
    VPOXOSTYPE_Win10            = 0x3B000,
    VPOXOSTYPE_Win10_x64        = 0x3B100,
    VPOXOSTYPE_Win2k16_x64      = 0x3C100,
    VPOXOSTYPE_Win2k19_x64      = 0x3D100,
    VPOXOSTYPE_Win11_x64        = 0x3E100,
    VPOXOSTYPE_OS2              = 0x40000,
    VPOXOSTYPE_OS2Warp3         = 0x41000,
    VPOXOSTYPE_OS2Warp4         = 0x42000,
    VPOXOSTYPE_OS2Warp45        = 0x43000,
    VPOXOSTYPE_ECS              = 0x44000,
    VPOXOSTYPE_ArcaOS           = 0x45000,
    VPOXOSTYPE_OS21x            = 0x48000,
    /** @} */
    /** @name Unixy related OSes
     * @{ */
    VPOXOSTYPE_Linux            = 0x50000,
    VPOXOSTYPE_Linux_x64        = 0x50100,
    VPOXOSTYPE_Linux22          = 0x51000,
    VPOXOSTYPE_Linux24          = 0x52000,
    VPOXOSTYPE_Linux24_x64      = 0x52100,
    VPOXOSTYPE_Linux26          = 0x53000,
    VPOXOSTYPE_Linux26_x64      = 0x53100,
    VPOXOSTYPE_ArchLinux        = 0x54000,
    VPOXOSTYPE_ArchLinux_x64    = 0x54100,
    VPOXOSTYPE_Debian           = 0x55000,
    VPOXOSTYPE_Debian_x64       = 0x55100,
    VPOXOSTYPE_OpenSUSE         = 0x56000,
    VPOXOSTYPE_OpenSUSE_x64     = 0x56100,
    VPOXOSTYPE_FedoraCore       = 0x57000,
    VPOXOSTYPE_FedoraCore_x64   = 0x57100,
    VPOXOSTYPE_Gentoo           = 0x58000,
    VPOXOSTYPE_Gentoo_x64       = 0x58100,
    VPOXOSTYPE_Mandriva         = 0x59000,
    VPOXOSTYPE_Mandriva_x64     = 0x59100,
    VPOXOSTYPE_RedHat           = 0x5A000,
    VPOXOSTYPE_RedHat_x64       = 0x5A100,
    VPOXOSTYPE_Turbolinux       = 0x5B000,
    VPOXOSTYPE_Turbolinux_x64   = 0x5B100,
    VPOXOSTYPE_Ubuntu           = 0x5C000,
    VPOXOSTYPE_Ubuntu_x64       = 0x5C100,
    VPOXOSTYPE_Xandros          = 0x5D000,
    VPOXOSTYPE_Xandros_x64      = 0x5D100,
    VPOXOSTYPE_Oracle           = 0x5E000,
    VPOXOSTYPE_Oracle_x64       = 0x5E100,
    VPOXOSTYPE_FreeBSD          = 0x60000,
    VPOXOSTYPE_FreeBSD_x64      = 0x60100,
    VPOXOSTYPE_OpenBSD          = 0x61000,
    VPOXOSTYPE_OpenBSD_x64      = 0x61100,
    VPOXOSTYPE_NetBSD           = 0x62000,
    VPOXOSTYPE_NetBSD_x64       = 0x62100,
    VPOXOSTYPE_Netware          = 0x70000,
    VPOXOSTYPE_Solaris          = 0x80000,  // Solaris 10U7 (5/09) and earlier
    VPOXOSTYPE_Solaris_x64      = 0x80100,  // Solaris 10U7 (5/09) and earlier
    VPOXOSTYPE_Solaris10U8_or_later     = 0x80001,
    VPOXOSTYPE_Solaris10U8_or_later_x64 = 0x80101,
    VPOXOSTYPE_OpenSolaris      = 0x81000,
    VPOXOSTYPE_OpenSolaris_x64  = 0x81100,
    VPOXOSTYPE_Solaris11_x64    = 0x82100,
    VPOXOSTYPE_L4               = 0x90000,
    VPOXOSTYPE_QNX              = 0xA0000,
    VPOXOSTYPE_MacOS            = 0xB0000,
    VPOXOSTYPE_MacOS_x64        = 0xB0100,
    VPOXOSTYPE_MacOS106         = 0xB2000,
    VPOXOSTYPE_MacOS106_x64     = 0xB2100,
    VPOXOSTYPE_MacOS107_x64     = 0xB3100,
    VPOXOSTYPE_MacOS108_x64     = 0xB4100,
    VPOXOSTYPE_MacOS109_x64     = 0xB5100,
    VPOXOSTYPE_MacOS1010_x64    = 0xB6100,
    VPOXOSTYPE_MacOS1011_x64    = 0xB7100,
    VPOXOSTYPE_MacOS1012_x64    = 0xB8100,
    VPOXOSTYPE_MacOS1013_x64    = 0xB9100,
    /** @} */
    /** @name Other OSes and stuff
     * @{ */
    VPOXOSTYPE_JRockitVE        = 0xC0000,
    VPOXOSTYPE_Haiku            = 0xD0000,
    VPOXOSTYPE_Haiku_x64        = 0xD0100,
    VPOXOSTYPE_VPoxBS_x64       = 0xE0100,
    /** @} */

/** The bit number which indicates 64-bit or 32-bit. */
#define VPOXOSTYPE_x64_BIT       8
    /** The mask which indicates 64-bit. */
    VPOXOSTYPE_x64              = 1 << VPOXOSTYPE_x64_BIT,

    /** The usual 32-bit hack. */
    VPOXOSTYPE_32BIT_HACK = 0x7fffffff
} VPOXOSTYPE;


/**
 * Global list of guest OS families.
 */
typedef enum VPOXOSFAMILY
{
    VPOXOSFAMILY_Unknown          = 0,
    VPOXOSFAMILY_Windows32        = 1,
    VPOXOSFAMILY_Windows64        = 2,
    VPOXOSFAMILY_Linux32          = 3,
    VPOXOSFAMILY_Linux64          = 4,
    VPOXOSFAMILY_FreeBSD32        = 5,
    VPOXOSFAMILY_FreeBSD64        = 6,
    VPOXOSFAMILY_Solaris32        = 7,
    VPOXOSFAMILY_Solaris64        = 8,
    VPOXOSFAMILY_MacOSX32         = 9,
    VPOXOSFAMILY_MacOSX64         = 10,
    /** The usual 32-bit hack. */
    VPOXOSFAMILY_32BIT_HACK = 0x7fffffff
} VPOXOSFAMILY;

RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_ostypes_h */
