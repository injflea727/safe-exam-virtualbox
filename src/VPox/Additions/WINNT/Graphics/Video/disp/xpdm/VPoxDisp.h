/* $Id: VPoxDisp.h $ */
/** @file
 * VPox XPDM Display driver
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDisp_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDisp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxDispInternal.h"
#include "VPoxDispVrdpBmp.h"

/* VirtualPox display driver version, could be seen in Control Panel */
#define VPOXDISPDRIVERVERSION 0x01UL

#if (VPOXDISPDRIVERVERSION & (~0xFFUL))
#error VPOXDISPDRIVERVERSION can't be more than 0xFF
#endif

#define VPOXDISP_DEVICE_NAME L"VPoxDisp"

/* Current mode info */
typedef struct _VPOXDISPCURRENTMODE
{
    ULONG ulIndex;                      /* miniport's video mode index */
    ULONG ulWidth, ulHeight;            /* visible screen width and height */
    ULONG ulBitsPerPel;                 /* number of bits per pel */
    LONG  lScanlineStride;              /* distance between scanlines */
    FLONG flMaskR, flMaskG, flMaskB;    /* RGB mask */
    ULONG ulPaletteShift;               /* number of bits we have to shift 888 palette to match device palette */
} VPOXDISPCURRENTMODE, *PVPOXDISPCURRENTMODE;

/* Pointer related info */
typedef struct _VPOXDISPPOINTERINFO
{
    VIDEO_POINTER_CAPABILITIES caps;    /* Pointer capabilities */
    PVIDEO_POINTER_ATTRIBUTES pAttrs;   /* Preallocated buffer to pass pointer shape to miniport driver */
    DWORD  cbAttrs;                     /* Size of pAttrs buffer */
    POINTL orgHotSpot;                  /* Hot spot origin */
} VPOXDISPPOINTERINFO, *PVPOXDISPPOINTERINFO;

/* Surface info */
typedef struct _VPOXDISPSURF
{
    HBITMAP  hBitmap;        /* GDI's handle to framebuffer bitmap */
    SURFOBJ* psoBitmap;      /* lock pointer to framebuffer bitmap */
    HSURF    hSurface;       /* GDI's handle to framebuffer device-managed surface */
    ULONG    ulFormat;       /* Bitmap format, one of BMF_XXBPP */
} VPOXDISPSURF, *PVPOXDISPSURF;

/* VRAM Layout */
typedef struct _VPOXDISPVRAMLAYOUT
{
    ULONG cbVRAM;

    ULONG offFramebuffer, cbFramebuffer;
    ULONG offDDrawHeap, cbDDrawHeap;
    ULONG offVBVABuffer, cbVBVABuffer;
    ULONG offDisplayInfo, cbDisplayInfo;
} VPOXDISPVRAMLAYOUT;

/* HGSMI info */
typedef struct _VPOXDISPHGSMIINFO
{
    BOOL bSupported;               /* HGSMI is supported and enabled */

    HGSMIQUERYCALLBACKS mp;        /* HGSMI miniport's callbacks and context */
    HGSMIGUESTCOMMANDCONTEXT ctx;  /* HGSMI guest context */
} VPOXDISPHGSMIINFO;

/* Saved screen bits information. */
typedef struct _SSB
{
    ULONG ident;   /* 1 based index in the stack = the handle returned by VPoxDispDrvSaveScreenBits (SS_SAVE) */
    BYTE *pBuffer; /* Buffer where screen bits are saved. */
} SSB;

#ifdef VPOX_WITH_DDRAW
/* DirectDraw surface lock information */
typedef struct _VPOXDDLOCKINFO
{
    BOOL bLocked;
    RECTL rect;
} VPOXDDLOCKINFO;
#endif

/* Structure holding driver private device info. */
typedef struct _VPOXDISPDEV
{
    HANDLE hDriver;                          /* Display device handle which was passed to VPoxDispDrvEnablePDEV */
    HDEV   hDevGDI;                          /* GDI's handle for PDEV created in VPoxDispDrvEnablePDEV */

    VPOXDISPCURRENTMODE mode;                /* Current device mode */
    ULONG iDevice;                           /* Miniport's device index */
    POINTL orgDev;                           /* Device origin for DualView (0,0 is primary) */
    POINTL orgDisp;                          /* Display origin in virtual desktop, NT4 only */

    VPOXDISPPOINTERINFO pointer;             /* Pointer info */

    HPALETTE hDefaultPalette;                /* Default palette handle */
    PALETTEENTRY *pPalette;                  /* Palette entries for device managed palette */

    VPOXDISPSURF surface;                    /* Device surface */
    FLONG flDrawingHooks;                    /* Enabled drawing hooks */

    VIDEO_MEMORY_INFORMATION memInfo;        /* Mapped Framebuffer/vram info */
    VPOXDISPVRAMLAYOUT layout;               /* VRAM layout information */

    VPOXDISPHGSMIINFO hgsmi;                 /* HGSMI Info */
    HGSMIQUERYCPORTPROCS vpAPI;              /* Video Port API callbacks and miniport's context */

    VBVABUFFERCONTEXT vbvaCtx;               /* VBVA context */
    VRDPBC            vrdpCache;             /* VRDP bitmap cache */

    ULONG cSSB;                              /* Number of active saved screen bits records in the following array. */
    SSB aSSB[4];                             /* LIFO type stack for saved screen areas. */

#ifdef VPOX_WITH_DDRAW
    VPOXDDLOCKINFO ddpsLock;                 /* Primary surface DirectDraw lock information */
#endif

#ifdef VPOX_WITH_VIDEOHWACCEL
    VPOXDISPVHWAINFO  vhwa;                  /* VHWA Info */
#endif

    BOOL bBitmapCacheDisabled;
} VPOXDISPDEV, *PVPOXDISPDEV;

/* -------------------- Driver callbacks -------------------- */
RT_C_DECLS_BEGIN
ULONG APIENTRY DriverEntry(IN PVOID Context1, IN PVOID Context2);
RT_C_DECLS_END

DHPDEV APIENTRY VPoxDispDrvEnablePDEV(DEVMODEW *pdm, LPWSTR pwszLogAddress,
                                      ULONG cPat, HSURF *phsurfPatterns,
                                      ULONG cjCaps, ULONG *pdevcaps,
                                      ULONG cjDevInfo, DEVINFO  *pdi,
                                      HDEV  hdev, PWSTR pwszDeviceName, HANDLE hDriver);
VOID APIENTRY VPoxDispDrvCompletePDEV(DHPDEV dhpdev, HDEV hdev);
VOID APIENTRY VPoxDispDrvDisablePDEV(DHPDEV dhpdev);
HSURF APIENTRY VPoxDispDrvEnableSurface(DHPDEV dhpdev);
VOID APIENTRY VPoxDispDrvDisableSurface(DHPDEV dhpdev);

BOOL APIENTRY VPoxDispDrvLineTo(SURFOBJ *pso, CLIPOBJ *pco, BRUSHOBJ *pbo,
                                LONG x1, LONG y1, LONG x2, LONG y2, RECTL *prclBounds, MIX mix);
BOOL APIENTRY VPoxDispDrvStrokePath(SURFOBJ *pso, PATHOBJ *ppo, CLIPOBJ *pco, XFORMOBJ *pxo,
                                    BRUSHOBJ  *pbo, POINTL *pptlBrushOrg, LINEATTRS *plineattrs, MIX mix);

BOOL APIENTRY VPoxDispDrvFillPath(SURFOBJ *pso, PATHOBJ *ppo, CLIPOBJ *pco, BRUSHOBJ *pbo, POINTL *pptlBrushOrg,
                                  MIX mix, FLONG flOptions);
BOOL APIENTRY VPoxDispDrvPaint(SURFOBJ *pso, CLIPOBJ *pco, BRUSHOBJ *pbo, POINTL *pptlBrushOrg, MIX mix);

BOOL APIENTRY VPoxDispDrvRealizeBrush(BRUSHOBJ *pbo, SURFOBJ *psoTarget, SURFOBJ *psoPattern, SURFOBJ *psoMask,
                                      XLATEOBJ *pxlo, ULONG iHatch);
ULONG APIENTRY VPoxDispDrvDitherColor(DHPDEV dhpdev, ULONG iMode, ULONG rgb, ULONG *pul);

BOOL APIENTRY VPoxDispDrvBitBlt(SURFOBJ *psoTrg, SURFOBJ *psoSrc, SURFOBJ *psoMask, CLIPOBJ *pco, XLATEOBJ *pxlo,
                                RECTL *prclTrg, POINTL *pptlSrc, POINTL *pptlMask, BRUSHOBJ *pbo, POINTL *pptlBrush,
                                ROP4 rop4);
BOOL APIENTRY VPoxDispDrvStretchBlt(SURFOBJ *psoDest, SURFOBJ *psoSrc, SURFOBJ *psoMask, CLIPOBJ *pco, XLATEOBJ *pxlo,
                                    COLORADJUSTMENT *pca, POINTL *pptlHTOrg, RECTL *prclDest, RECTL *prclSrc,
                                    POINTL *pptlMask, ULONG iMode);
BOOL APIENTRY VPoxDispDrvCopyBits(SURFOBJ *psoDest, SURFOBJ *psoSrc, CLIPOBJ *pco, XLATEOBJ *pxlo,
                                  RECTL *prclDest, POINTL *pptlSrc);

ULONG APIENTRY VPoxDispDrvSetPointerShape(SURFOBJ *pso, SURFOBJ *psoMask, SURFOBJ *psoColor, XLATEOBJ *pxlo,
                                          LONG xHot, LONG yHot, LONG x, LONG y, RECTL *prcl, FLONG fl);
VOID APIENTRY VPoxDispDrvMovePointer(SURFOBJ *pso, LONG x, LONG y, RECTL *prcl);

BOOL APIENTRY VPoxDispDrvAssertMode(DHPDEV dhpdev, BOOL bEnable);
VOID APIENTRY VPoxDispDrvDisableDriver();
BOOL APIENTRY VPoxDispDrvTextOut(SURFOBJ *pso, STROBJ *pstro, FONTOBJ *pfo, CLIPOBJ *pco,
                                 RECTL *prclExtra, RECTL *prclOpaque, BRUSHOBJ *pboFore,
                                 BRUSHOBJ *pboOpaque, POINTL *pptlOrg, MIX mix);
BOOL APIENTRY VPoxDispDrvSetPalette(DHPDEV dhpdev, PALOBJ *ppalo, FLONG fl, ULONG iStart, ULONG cColors);
ULONG APIENTRY VPoxDispDrvEscape(SURFOBJ *pso, ULONG iEsc, ULONG cjIn, PVOID pvIn, ULONG cjOut, PVOID pvOut);
ULONG_PTR APIENTRY VPoxDispDrvSaveScreenBits(SURFOBJ *pso, ULONG iMode, ULONG_PTR ident, RECTL *prcl);
ULONG APIENTRY VPoxDispDrvGetModes(HANDLE hDriver, ULONG cjSize, DEVMODEW *pdm);
BOOL APIENTRY VPoxDispDrvOffset(SURFOBJ* pso, LONG x, LONG y, FLONG flReserved);

VOID APIENTRY VPoxDispDrvNotify(SURFOBJ *pso, ULONG iType, PVOID pvData);

#ifdef VPOX_WITH_DDRAW
BOOL APIENTRY VPoxDispDrvGetDirectDrawInfo(DHPDEV dhpdev, DD_HALINFO *pHalInfo, DWORD *pdwNumHeaps,
                                           VIDEOMEMORY *pvmList, DWORD *pdwNumFourCCCodes, DWORD *pdwFourCC);
BOOL APIENTRY VPoxDispDrvEnableDirectDraw(DHPDEV dhpdev, DD_CALLBACKS *pCallBacks,
                                          DD_SURFACECALLBACKS *pSurfaceCallBacks,
                                          DD_PALETTECALLBACKS *pPaletteCallBacks);
VOID APIENTRY VPoxDispDrvDisableDirectDraw(DHPDEV  dhpdev);
HBITMAP APIENTRY VPoxDispDrvDeriveSurface(DD_DIRECTDRAW_GLOBAL *pDirectDraw, DD_SURFACE_LOCAL *pSurface);
#endif /*#ifdef VPOX_WITH_DDRAW*/

/* -------------------- Internal helpers -------------------- */
DECLINLINE(SURFOBJ) *getSurfObj(SURFOBJ *pso)
{
    if (pso)
    {
        PVPOXDISPDEV pDev = (PVPOXDISPDEV)pso->dhpdev;

        if (pDev && pDev->surface.psoBitmap && pso->hsurf == pDev->surface.hSurface)
        {
            /* Convert the device PSO to the bitmap PSO which can be passed to Eng*. */
            pso = pDev->surface.psoBitmap;
        }
    }

    return pso;
}

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDisp_h */
