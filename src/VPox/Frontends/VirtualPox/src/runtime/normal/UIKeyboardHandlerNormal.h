/* $Id: UIKeyboardHandlerNormal.h $ */
/** @file
 * VPox Qt GUI - UIKeyboardHandlerNormal class declaration.
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

#ifndef FEQT_INCLUDED_SRC_runtime_normal_UIKeyboardHandlerNormal_h
#define FEQT_INCLUDED_SRC_runtime_normal_UIKeyboardHandlerNormal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIKeyboardHandler.h"

/** UIKeyboardHandler reimplementation
  * providing machine-logic with PopupMenu keyboard handler. */
class UIKeyboardHandlerNormal : public UIKeyboardHandler
{
    Q_OBJECT;

protected:

    /** Normal keyboard-handler constructor. */
    UIKeyboardHandlerNormal(UIMachineLogic *pMachineLogic);
    /** Normal keyboard-handler destructor. */
    virtual ~UIKeyboardHandlerNormal();

private:

#ifndef VPOX_WS_MAC
    /** General event-filter. */
    bool eventFilter(QObject *pWatched, QEvent *pEvent);
#endif /* !VPOX_WS_MAC */

    /* Friend class: */
    friend class UIKeyboardHandler;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_normal_UIKeyboardHandlerNormal_h */
