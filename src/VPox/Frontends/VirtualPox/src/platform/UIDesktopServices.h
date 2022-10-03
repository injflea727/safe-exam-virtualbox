/* $Id: UIDesktopServices.h $ */
/** @file
 * VPox Qt GUI - Desktop Services..
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

#ifndef FEQT_INCLUDED_SRC_platform_UIDesktopServices_h
#define FEQT_INCLUDED_SRC_platform_UIDesktopServices_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes */
#include <QUuid>

/** Name of the executable (image) used to start VMs. */
#define VPOX_GUI_VMRUNNER_IMAGE "VirtualPoxVM"

/* Qt forward declarations */
class QString;

class UIDesktopServices
{
public:
    static bool createMachineShortcut(const QString &strSrcFile, const QString &strDstPath, const QString &strName, const QUuid &uUuid);
    static bool openInFileManager(const QString &strFile);
};

#endif /* !FEQT_INCLUDED_SRC_platform_UIDesktopServices_h */

