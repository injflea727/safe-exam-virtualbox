/* $Id: VPox2DHelpers.h $ */
/** @file
 * VPox Qt GUI - 2D Video Acceleration helpers declarations.
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

#ifndef FEQT_INCLUDED_SRC_VPox2DHelpers_h
#define FEQT_INCLUDED_SRC_VPox2DHelpers_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if defined(VPOX_GUI_USE_QGL) || defined(VPOX_WITH_VIDEOHWACCEL)

/* Qt includes: */
#include <QtGlobal>

/** 2D Video Acceleration Helpers */
namespace VPox2DHelpers
{
    /** Returns whether 2D Video Acceleration is available. */
    bool isAcceleration2DVideoAvailable();

    /** Returns 2D offscreen video memory required for 2D Video Acceleration. */
    quint64 required2DOffscreenVideoMemory();
};

#endif /* VPOX_GUI_USE_QGL || VPOX_WITH_VIDEOHWACCEL */

#endif /* !FEQT_INCLUDED_SRC_VPox2DHelpers_h */
