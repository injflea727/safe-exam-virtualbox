/* $Id: wddm_screen.h $ */
/** @file
 * VirtualPox Windows Guest Mesa3D - VMSVGA hardware driver.
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

#ifndef GA_INCLUDED_SRC_3D_win_VPoxSVGA_wddm_screen_h
#define GA_INCLUDED_SRC_3D_win_VPoxSVGA_wddm_screen_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPoxGaDriver.h>

#include "vmw_screen.h"

struct vmw_winsys_screen_wddm
{
    struct vmw_winsys_screen base;

    const WDDMGalliumDriverEnv *pEnv;
    VPOXGAHWINFOSVGA HwInfo;
};

#endif /* !GA_INCLUDED_SRC_3D_win_VPoxSVGA_wddm_screen_h */

