/* $Id: SHGSMIHost.h $ */
/** @file
 * Missing description
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VPOX_INCLUDED_SRC_Graphics_HGSMI_SHGSMIHost_h
#define VPOX_INCLUDED_SRC_Graphics_HGSMI_SHGSMIHost_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HGSMIHost.h"

int  VPoxSHGSMICommandComplete(PHGSMIINSTANCE pIns, void RT_UNTRUSTED_VOLATILE_GUEST *pvData);
void VPoxSHGSMICommandMarkAsynchCompletion(void RT_UNTRUSTED_VOLATILE_GUEST *pvData);

#endif /* !VPOX_INCLUDED_SRC_Graphics_HGSMI_SHGSMIHost_h */

