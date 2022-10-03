/* $Id: VPox2DHelpers.cpp $ */
/** @file
 * VPox Qt GUI - 2D Video Acceleration helpers implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#if defined(VPOX_GUI_USE_QGL) || defined(VPOX_WITH_VIDEOHWACCEL)

#define LOG_GROUP LOG_GROUP_GUI

// WORKAROUND:
// QGLWidget drags in Windows.h and stdint.h
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
# include <iprt/stdint.h>
#endif

/* Qt includes: */
#include <QGLWidget>

/* GUI includes: */
#include "VPox2DHelpers.h"

/* Other VPox includes: */
#include <VPox/VPoxGL2D.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

static bool g_fVPoxVHWAChecked = false;
static bool g_fVPoxVHWASupported = false;


/*********************************************************************************************************************************
*   Namespace VPox2DHelpers implementation.                                                                                      *
*********************************************************************************************************************************/

bool VPox2DHelpers::isAcceleration2DVideoAvailable()
{
    if (!g_fVPoxVHWAChecked)
    {
        g_fVPoxVHWAChecked = true;
        g_fVPoxVHWASupported = VPoxVHWAInfo::checkVHWASupport();
    }
    return g_fVPoxVHWASupported;
}

quint64 VPox2DHelpers::required2DOffscreenVideoMemory()
{
    /* HDTV == 1920x1080 ~ 2M
     * for the 4:2:2 formats each pixel is 2Bytes
     * so each frame may be 4MiB
     * so for triple-buffering we would need 12 MiB */
    return _1M * 12;
}

#endif /* VPOX_GUI_USE_QGL || VPOX_WITH_VIDEOHWACCEL */
