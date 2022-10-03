/* $Id: VPoxServiceToolBox.h $ */
/** @file
 * VPoxService - Toolbox header for sharing defines between toolbox binary and VPoxService.
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

#ifndef GA_INCLUDED_SRC_common_VPoxService_VPoxServiceToolBox_h
#define GA_INCLUDED_SRC_common_VPoxService_VPoxServiceToolBox_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/GuestHost/GuestControl.h>

RT_C_DECLS_BEGIN
extern bool                     VGSvcToolboxMain(int argc, char **argv, RTEXITCODE *prcExit);
extern int                      VGSvcToolboxExitCodeConvertToRc(const char *pszTool, RTEXITCODE rcExit);
RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_common_VPoxService_VPoxServiceToolBox_h */

