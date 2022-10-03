/* $Id: VPoxSeamless.h $ */
/** @file
 * VPoxSeamless - Seamless windows
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

#ifndef GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxSeamless_h
#define GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxSeamless_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

void VPoxSeamlessEnable();
void VPoxSeamlessDisable();
void VPoxSeamlessCheckWindows(bool fForce);

void VPoxSeamlessSetSupported(BOOL fSupported);

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxSeamless_h */

