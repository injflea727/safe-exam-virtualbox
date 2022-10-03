/* $Id: vpoximgMedia.h $ */

/** @file
 * vpoximgMedia.h
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

#ifndef VPOX_INCLUDED_SRC_vpoximg_mount_vpoximgMedia_h
#define VPOX_INCLUDED_SRC_vpoximg_mount_vpoximgMedia_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

typedef struct MEDIUMINFO
{
    char *name;
    char *uuid;
    char *location;
    char *description;
    char *state;
    char *size;
    char *format;
    int ro;
} MEDIUMINFO;

int vpoximgListVMs(IVirtualPox *pVirtualPox);
char *vpoximgScaledSize(size_t size);

#endif /* !VPOX_INCLUDED_SRC_vpoximg_mount_vpoximgMedia_h */
