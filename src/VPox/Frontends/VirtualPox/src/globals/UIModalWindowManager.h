/* $Id: UIModalWindowManager.h $ */
/** @file
 * VPox Qt GUI - UIModalWindowManager class declaration.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIModalWindowManager_h
#define FEQT_INCLUDED_SRC_globals_UIModalWindowManager_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QWidget>
#include <QList>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QObject subclass which contains a stack(s) of guarded-pointer(s) to the current top-level
  * modal-window(s) which could be used to determine parents for new top-level modal-dialog(s). */
class SHARED_LIBRARY_STUFF UIModalWindowManager : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about stack changed. */
    void sigStackChanged();

public:

    /** Creates the static singleton instance. */
    static void create();
    /** Destroys the static singleton instance. */
    static void destroy();

    /** Returns actual top-level parent window for a passed @a pPossibleParentWidget. */
    QWidget *realParentWindow(QWidget *pPossibleParentWidget);
    /** Returns whether passed @a pWindow is in the modal window stack. */
    bool isWindowInTheModalWindowStack(QWidget *pWindow);
    /** Returns whether passed @a pWindow is on the top of the modal window stack. */
    bool isWindowOnTheTopOfTheModalWindowStack(QWidget *pWindow);

    /** Registers new parent @a pWindow above the passed @a pParentWindow or as separate stack. */
    void registerNewParent(QWidget *pWindow, QWidget *pParentWindow = 0);

    /** Defines the main application @a pWindow shown. */
    void setMainWindowShown(QWidget *pWindow) { m_pMainWindowShown = pWindow; }
    /** Returns the main application window shown. */
    QWidget *mainWindowShown() const { return m_pMainWindowShown; }
#ifdef VPOX_GUI_WITH_NETWORK_MANAGER
    /** Returns network manager or main window shown. */
    QWidget* networkManagerOrMainWindowShown() const;
#endif

private slots:

    /** Removes window with base-class @a pObject pointer from the stack. */
    void sltRemoveFromStack(QObject *pObject);

private:

    /** Constructs Modal Window Manager instance. */
    UIModalWindowManager();
    /** Destructs Modal Window Manager instance. */
    ~UIModalWindowManager();

    /** Returns whether stack contains @a pParentWindow at all or @a fAsTheTopOfStack. */
    bool contains(QWidget *pParentWindow, bool fAsTheTopOfStack = false);

    /** WORKAROUND: Preprocess (show) real parent for a passed @a pParent. */
    static void preprocessRealParent(QWidget *pParent);

    /** Holds the list of the top-level window stacks. */
    QList<QList<QWidget*> > m_windows;

    /** Holds the main application window shown. */
    QWidget *m_pMainWindowShown;

    /** Holds the static singleton instance. */
    static UIModalWindowManager *s_pInstance;
    /** Returns the static singleton instance. */
    static UIModalWindowManager *instance();
    /** Allows friend-access for static singleton instance. */
    friend UIModalWindowManager &windowManager();
};

/** Singleton Modal Window Manager 'official' name. */
inline UIModalWindowManager &windowManager() { return *(UIModalWindowManager::instance()); }

#endif /* !FEQT_INCLUDED_SRC_globals_UIModalWindowManager_h */

