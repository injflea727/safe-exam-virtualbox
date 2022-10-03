/* $Id: precomp_vcc.h $ */
/** @file
 * VirtualPox COM - Visual C++ precompiled header for VPoxSVC.
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


#include <iprt/cdefs.h>
#include <iprt/win/winsock2.h>
#include <iprt/win/windows.h>
#include <VPox/cdefs.h>
#include <iprt/types.h>
#include <iprt/cpp/list.h>
#include <iprt/cpp/meta.h>
#include <iprt/cpp/ministring.h>
#include <VPox/com/microatl.h>
#include <VPox/com/com.h>
#include <VPox/com/array.h>
#include <VPox/com/Guid.h>
#include <VPox/com/string.h>

#include "VPox/com/VirtualPox.h"

#if defined(Log) || defined(LogIsEnabled)
# error "Log() from iprt/log.h cannot be defined in the precompiled header!"
#endif

