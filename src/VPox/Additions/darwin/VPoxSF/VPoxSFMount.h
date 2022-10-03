/* $Id: VPoxSFMount.h $ */
/** @file
 * VPoxSF - Darwin Shared Folders, mount interface.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_darwin_VPoxSF_VPoxSFMount_h
#define GA_INCLUDED_SRC_darwin_VPoxSF_VPoxSFMount_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

/** The shared folders file system name.   */
#define VPOXSF_DARWIN_FS_NAME "vpoxsf"

/**
 * Mount information that gets passed from userland on mount.
 */
typedef struct VPOXSFDRWNMOUNTINFO
{
    /** Magic value (VPOXSFDRWNMOUNTINFO_MAGIC).   */
    uint32_t    u32Magic;
    /** The shared folder name.   */
    char        szFolder[260];
} VPOXSFDRWNMOUNTINFO;
typedef VPOXSFDRWNMOUNTINFO *PVPOXSFDRWNMOUNTINFO;
/** Magic value for VPOXSFDRWNMOUNTINFO::u32Magic.   */
#define VPOXSFDRWNMOUNTINFO_MAGIC     UINT32_C(0xc001cafe)

#endif /* !GA_INCLUDED_SRC_darwin_VPoxSF_VPoxSFMount_h */

