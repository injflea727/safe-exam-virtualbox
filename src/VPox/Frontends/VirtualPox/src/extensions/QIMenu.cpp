/* $Id: QIMenu.cpp $ */
/** @file
 * VPox Qt GUI - Qt extensions: QIMenu class implementation.
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

/* GUI includes: */
#include "QIMenu.h"


QIMenu::QIMenu(QWidget *pParent /* = 0 */)
    : QMenu(pParent)
{
}

void QIMenu::sltHighlightFirstAction()
{
#ifdef VPOX_WS_WIN
    /* Windows host requires window-activation: */
    activateWindow();
#endif

    /* Focus next child: */
    QMenu::focusNextChild();
}
