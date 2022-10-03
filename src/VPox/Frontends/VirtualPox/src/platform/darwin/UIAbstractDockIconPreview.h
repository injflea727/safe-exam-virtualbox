/* $Id: UIAbstractDockIconPreview.h $ */
/** @file
 * VPox Qt GUI - Abstract class for the dock icon preview.
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

#ifndef FEQT_INCLUDED_SRC_platform_darwin_UIAbstractDockIconPreview_h
#define FEQT_INCLUDED_SRC_platform_darwin_UIAbstractDockIconPreview_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* System includes */
#include <ApplicationServices/ApplicationServices.h>

/* VPox includes */
#include "VPoxUtils-darwin.h"

class UIFrameBuffer;
class UISession;

class QPixmap;

class UIAbstractDockIconPreview
{
public:
    UIAbstractDockIconPreview(UISession *pSession, const QPixmap& overlayImage);
    virtual ~UIAbstractDockIconPreview() {}

    virtual void updateDockOverlay() = 0;
    virtual void updateDockPreview(CGImageRef VMImage) = 0;
    virtual void updateDockPreview(UIFrameBuffer *pFrameBuffer);

    virtual void setOriginalSize(int /* aWidth */, int /* aHeight */) {}
};

class UIAbstractDockIconPreviewHelper
{
public:
    UIAbstractDockIconPreviewHelper(UISession *pSession, const QPixmap& overlayImage);
    virtual ~UIAbstractDockIconPreviewHelper();
    void initPreviewImages();
    void drawOverlayIcons(CGContextRef context);

    void* currentPreviewWindowId() const;

    /* Flipping is necessary cause the drawing context in Mac OS X is flipped by 180 degree */
    inline CGRect flipRect(CGRect rect) const { return ::darwinFlipCGRect(rect, m_dockIconRect); }
    inline CGRect centerRect(CGRect rect) const { return ::darwinCenterRectTo(rect, m_dockIconRect); }
    inline CGRect centerRectTo(CGRect rect, const CGRect& toRect) const { return ::darwinCenterRectTo(rect, toRect); }

    /* Private member vars */
    UISession *m_pSession;
    const CGRect m_dockIconRect;

    CGImageRef m_overlayImage;
    CGImageRef m_dockMonitor;
    CGImageRef m_dockMonitorGlossy;

    CGRect m_updateRect;
    CGRect m_monitorRect;
};

#endif /* !FEQT_INCLUDED_SRC_platform_darwin_UIAbstractDockIconPreview_h */

