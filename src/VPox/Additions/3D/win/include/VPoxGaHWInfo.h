/* $Id: VPoxGaHWInfo.h $ */
/** @file
 * VirtualPox Windows Guest Mesa3D - Gallium driver interface.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_3D_WIN_VPoxGaHWInfo_h
#define GA_INCLUDED_3D_WIN_VPoxGaHWInfo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>

#include <VPoxGaHwSVGA.h>

/* Gallium virtual hardware supported by the miniport. */
#define VPOX_GA_HW_TYPE_UNKNOWN 0
#define VPOX_GA_HW_TYPE_VMSVGA  1

/*
 * VPOXGAHWINFO contains information about the virtual hardware, which is passed
 * to the user mode Gallium driver. The driver can not query the info at the initialization time,
 * therefore we send the complete info to the driver.
 *
 * VPOXGAHWINFO struct goes both to 32 and 64 bit user mode binaries, take care of alignment.
 */
#pragma pack(1)
typedef struct VPOXGAHWINFO
{
    uint32_t u32HwType; /* VPOX_GA_HW_TYPE_* */
    uint32_t u32Reserved;
    union
    {
        VPOXGAHWINFOSVGA svga;
        uint8_t au8Raw[65536];
    } u;
} VPOXGAHWINFO;
#pragma pack()

AssertCompile(RT_SIZEOFMEMB(VPOXGAHWINFO, u) <= RT_SIZEOFMEMB(VPOXGAHWINFO, u.au8Raw));

#endif /* !GA_INCLUDED_3D_WIN_VPoxGaHWInfo_h */
