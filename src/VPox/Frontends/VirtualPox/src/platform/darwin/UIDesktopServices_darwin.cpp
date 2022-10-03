/* $Id: UIDesktopServices_darwin.cpp $ */
/** @file
 * VPox Qt GUI - Qt GUI - Utility Classes and Functions specific to darwin..
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

/* VPox includes */
#include "UIDesktopServices.h"
#include "UIDesktopServices_darwin_p.h"
#include "VPoxUtils-darwin.h"

/* Qt includes */
#include <QString>

bool UIDesktopServices::createMachineShortcut(const QString &strSrcFile, const QString &strDstPath, const QString &strName, const QUuid &uUuid)
{
    return ::darwinCreateMachineShortcut(::darwinToNativeString(strSrcFile.toUtf8().constData()),
                                         ::darwinToNativeString(strDstPath.toUtf8().constData()),
                                         ::darwinToNativeString(strName.toUtf8().constData()),
                                         ::darwinToNativeString(uUuid.toString().toUtf8().constData()));
}

bool UIDesktopServices::openInFileManager(const QString &strFile)
{
    return ::darwinOpenInFileManager(::darwinToNativeString(strFile.toUtf8().constData()));
}

