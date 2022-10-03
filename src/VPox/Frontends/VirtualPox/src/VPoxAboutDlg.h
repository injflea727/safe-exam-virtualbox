/* $Id: VPoxAboutDlg.h $ */
/** @file
 * VPox Qt GUI - VPoxAboutDlg class declaration.
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

#ifndef FEQT_INCLUDED_SRC_VPoxAboutDlg_h
#define FEQT_INCLUDED_SRC_VPoxAboutDlg_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QPixmap>

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QEvent;
class QLabel;
class QVBoxLayout;

/** QIDialog extension
  * used to show the About-VirtualPox dialog. */
class SHARED_LIBRARY_STUFF VPoxAboutDlg : public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructs dialog passing @a pParent to the base-class.
      * @param  strVersion  Brings the version number of VirtualPox. */
    VPoxAboutDlg(QWidget *pParent, const QString &strVersion);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares main-layout. */
    void prepareMainLayout();
    /** Prepares label. */
    void prepareLabel();
    /** Prepares close-button. */
    void prepareCloseButton();

    /** Holds the pseudo-parent widget reference. */
    QObject *m_pPseudoParent;

    /** Holds the About-VirtualPox text. */
    QString  m_strAboutText;
    /** Holds the VirtualPox version number. */
    QString  m_strVersion;

    /** Holds the About-VirtualPox image. */
    QPixmap  m_pixmap;
    /** Holds the About-VirtualPox dialog size. */
    QSize    m_size;

    /** Holds About-VirtualPox main-layout instance. */
    QVBoxLayout *m_pMainLayout;
    /** Holds About-VirtualPox text-label instance. */
    QLabel      *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_VPoxAboutDlg_h */

