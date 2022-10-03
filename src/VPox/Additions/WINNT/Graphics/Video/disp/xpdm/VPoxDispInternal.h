/* $Id: VPoxDispInternal.h $ */
/** @file
 * VPox XPDM Display driver, internal header
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispInternal_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#define LOG_GROUP LOG_GROUP_DRV_DISPLAY
#include <VPox/log.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/win/windef.h>
#include <wingdi.h>
#include <winddi.h>
#include <ntddvdeo.h>
#undef CO_E_NOTINITIALIZED
#include <winerror.h>
#include <devioctl.h>
#define VPOX_VIDEO_LOG_NAME "VPoxDisp"
#include "common/VPoxVideoLog.h"
#include "common/xpdm/VPoxVideoPortAPI.h"
#include "common/xpdm/VPoxVideoIOCTL.h"
#include <HGSMI.h>
#include <VPoxVideo.h>
#include <VPoxVideoGuest.h>
#include <VPoxDisplay.h>

typedef struct _VPOXDISPDEV *PVPOXDISPDEV;

#ifdef VPOX_WITH_VIDEOHWACCEL
# include "VPoxDispVHWA.h"
#endif

/* 4bytes tag passed to EngAllocMem.
 * Note: chars are reverse order.
 */
#define MEM_ALLOC_TAG 'bvDD'

/* Helper macros */
#define VPOX_WARN_WINERR(_winerr)                     \
    do {                                              \
        if ((_winerr) != NO_ERROR)                    \
        {                                             \
            WARN(("winerr(%#x)!=NO_ERROR", _winerr)); \
        }                                             \
    } while (0)

#define VPOX_CHECK_WINERR_RETRC(_winerr, _rc)         \
    do {                                              \
        if ((_winerr) != NO_ERROR)                    \
        {                                             \
            WARN(("winerr(%#x)!=NO_ERROR", _winerr)); \
            return (_rc);                             \
        }                                             \
    } while (0)

#define VPOX_WARNRC_RETV(_rc, _ret)            \
    do {                                       \
        if (RT_FAILURE(_rc))                   \
        {                                      \
            WARN(("RT_FAILURE rc(%#x)", _rc)); \
            return (_ret);                     \
        }                                      \
    } while (0)

#define VPOX_WARNRC_RETRC(_rc) VPOX_WARNRC_RETV(_rc, _rc)

#define VPOX_WARNRC(_rc)                       \
    do {                                       \
        if (RT_FAILURE(_rc))                   \
        {                                      \
            WARN(("RT_FAILURE rc(%#x)", _rc)); \
        }                                      \
    } while (0)

#define VPOX_WARNRC_NOBP(_rc)                       \
    do {                                       \
        if (RT_FAILURE(_rc))                   \
        {                                      \
            WARN_NOBP(("RT_FAILURE rc(%#x)", _rc)); \
        }                                      \
    } while (0)


#define VPOX_WARN_IOCTLCB_RETRC(_ioctl, _cbreturned, _cbexpected, _rc)                   \
    do {                                                                                 \
        if ((_cbreturned)!=(_cbexpected))                                                \
        {                                                                                \
            WARN((_ioctl " returned %d, expected %d bytes!", _cbreturned, _cbexpected)); \
            return (_rc);                                                                \
        }                                                                                \
    } while (0)

#define abs(_v) ( ((_v)>0) ? (_v) : (-(_v)) )

typedef struct _CLIPRECTS {
    ULONG  c;
    RECTL  arcl[64];
} CLIPRECTS;

typedef struct _VRDPCLIPRECTS
{
    RECTL rclDstOrig; /* Original bounding rectangle. */
    RECTL rclDst;     /* Bounding rectangle of all rects. */
    CLIPRECTS rects;  /* Rectangles to update. */
} VRDPCLIPRECTS;

/* Mouse pointer related functions */
int VPoxDispInitPointerCaps(PVPOXDISPDEV pDev, DEVINFO *pDevInfo);
int VPoxDispInitPointerAttrs(PVPOXDISPDEV pDev);

/* Palette related functions */
int VPoxDispInitPalette(PVPOXDISPDEV pDev, DEVINFO *pDevInfo);
void VPoxDispDestroyPalette(PVPOXDISPDEV pDev);
int VPoxDispSetPalette8BPP(PVPOXDISPDEV pDev);

/* VBVA related */
int VPoxDispVBVAInit(PVPOXDISPDEV pDev);
void VPoxDispVBVAHostCommandComplete(PVPOXDISPDEV pDev, VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);

void vrdpReportDirtyRect(PVPOXDISPDEV pDev, RECTL *prcl);
void vbvaReportDirtyRect(PVPOXDISPDEV pDev, RECTL *prcl);

#ifdef VPOX_VBVA_ADJUST_RECT
void vrdpAdjustRect (SURFOBJ *pso, RECTL *prcl);
BOOL vbvaFindChangedRect(SURFOBJ *psoDest, SURFOBJ *psoSrc, RECTL *prclDest, POINTL *pptlSrc);
#endif /* VPOX_VBVA_ADJUST_RECT */

#define VRDP_TEXT_MAX_GLYPH_SIZE 0x100
#define VRDP_TEXT_MAX_GLYPHS     0xfe
BOOL vrdpReportText(PVPOXDISPDEV pDev, VRDPCLIPRECTS *pClipRects, STROBJ *pstro, FONTOBJ *pfo,
                    RECTL *prclOpaque, ULONG ulForeRGB, ULONG ulBackRGB);

BOOL vrdpReportOrderGeneric(PVPOXDISPDEV pDev, const VRDPCLIPRECTS *pClipRects,
                             const void *pvOrder, unsigned cbOrder, unsigned code);

BOOL VPoxDispIsScreenSurface(SURFOBJ *pso);
void VPoxDispDumpPSO(SURFOBJ *pso, char *s);

BOOL vrdpDrvRealizeBrush(BRUSHOBJ *pbo, SURFOBJ *psoTarget, SURFOBJ *psoPattern, SURFOBJ *psoMask,
                         XLATEOBJ *pxlo, ULONG iHatch);
void vrdpReset(PVPOXDISPDEV pDev);

DECLINLINE(int) format2BytesPerPixel(const SURFOBJ *pso)
{
    switch (pso->iBitmapFormat)
    {
        case BMF_16BPP: return 2;
        case BMF_24BPP: return 3;
        case BMF_32BPP: return 4;
    }

    return 0;
}

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispInternal_h */
