/* $Id: VPoxUtils.h $ */
/** @file
 * VPox Qt GUI - Declarations of utility classes and functions.
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

#ifndef FEQT_INCLUDED_SRC_globals_VPoxUtils_h
#define FEQT_INCLUDED_SRC_globals_VPoxUtils_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMouseEvent>
#include <QWidget>
#include <QTextBrowser>

/* GUI includes: */
#include "UILibraryDefs.h"
#ifdef VPOX_WS_MAC
# include "VPoxUtils-darwin.h"
#endif

/* Other VPox includes: */
#include <iprt/types.h>


/** QObject subclass,
  * allowing to apply string-property value for a certain QObject. */
class SHARED_LIBRARY_STUFF QObjectPropertySetter : public QObject
{
    Q_OBJECT;

public:

    /** Constructs setter for a property with certain @a strName, passing @a pParent to the base-class. */
    QObjectPropertySetter(QObject *pParent, const QString &strName)
        : QObject(pParent), m_strName(strName)
    {}

public slots:

    /** Assigns string property @a strValue. */
    void sltAssignProperty(const QString &strValue)
    {
        parent()->setProperty(m_strName.toLatin1().constData(), strValue);
    }

private:

    /** Holds the property name. */
    const QString m_strName;
};


#endif /* !FEQT_INCLUDED_SRC_globals_VPoxUtils_h */

