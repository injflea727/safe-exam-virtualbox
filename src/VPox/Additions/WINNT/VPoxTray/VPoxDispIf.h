/* $Id: VPoxDispIf.h $ */
/** @file
 * VPoxTray - Display Settings Interface abstraction for XPDM & WDDM
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

#ifndef GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDispIf_h
#define GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDispIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

#ifdef VPOX_WITH_WDDM
# define D3DKMDT_SPECIAL_MULTIPLATFORM_TOOL
# include <d3dkmthk.h>
# include <VPoxDispKmt.h>
#endif

#include <VPoxDisplay.h>

typedef enum
{
    VPOXDISPIF_MODE_UNKNOWN  = 0,
    VPOXDISPIF_MODE_XPDM_NT4 = 1,
    VPOXDISPIF_MODE_XPDM
#ifdef VPOX_WITH_WDDM
    , VPOXDISPIF_MODE_WDDM
    , VPOXDISPIF_MODE_WDDM_W7
#endif
} VPOXDISPIF_MODE;
/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us
 * NOTE: DispIf makes no checks whether the display driver is actually a VPox driver,
 * it just switches between using different backend OS API based on the VPoxDispIfSwitchMode call
 * It's caller's responsibility to initiate it to work in the correct mode */
typedef struct VPOXDISPIF
{
    VPOXDISPIF_MODE enmMode;
    /* with WDDM the approach is to call into WDDM miniport driver via PFND3DKMT API provided by the GDI,
     * The PFND3DKMT is supposed to be used by the OpenGL ICD according to MSDN, so this approach is a bit hacky */
    union
    {
        struct
        {
            LONG (WINAPI *pfnChangeDisplaySettingsEx)(LPCSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
        } xpdm;
#ifdef VPOX_WITH_WDDM
        struct
        {
            /* ChangeDisplaySettingsEx does not exist in NT. ResizeDisplayDevice uses the function. */
            LONG (WINAPI *pfnChangeDisplaySettingsEx)(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
            /* EnumDisplayDevices does not exist in NT. isVPoxDisplayDriverActive et al. are using these functions. */
            BOOL (WINAPI *pfnEnumDisplayDevices)(IN LPCSTR lpDevice, IN DWORD iDevNum, OUT PDISPLAY_DEVICEA lpDisplayDevice, IN DWORD dwFlags);

            VPOXDISPKMT_CALLBACKS KmtCallbacks;
        } wddm;
#endif
    } modeData;
} VPOXDISPIF, *PVPOXDISPIF;
typedef const struct VPOXDISPIF *PCVPOXDISPIF;

/* initializes the DispIf
 * Initially the DispIf is configured to work in XPDM mode
 * call VPoxDispIfSwitchMode to switch the mode to WDDM */
DWORD VPoxDispIfInit(PVPOXDISPIF pIf);
DWORD VPoxDispIfSwitchMode(PVPOXDISPIF pIf, VPOXDISPIF_MODE enmMode, VPOXDISPIF_MODE *penmOldMode);
DECLINLINE(VPOXDISPIF_MODE) VPoxDispGetMode(PVPOXDISPIF pIf) { return pIf->enmMode; }
DWORD VPoxDispIfTerm(PVPOXDISPIF pIf);
DWORD VPoxDispIfEscape(PCVPOXDISPIF const pIf, PVPOXDISPIFESCAPE pEscape, int cbData);
DWORD VPoxDispIfEscapeInOut(PCVPOXDISPIF const pIf, PVPOXDISPIFESCAPE pEscape, int cbData);
DWORD VPoxDispIfResizeModes(PCVPOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes);
DWORD VPoxDispIfCancelPendingResize(PCVPOXDISPIF const pIf);
DWORD VPoxDispIfResizeStarted(PCVPOXDISPIF const pIf);

BOOL VPoxDispIfResizeDisplayWin7(PCVPOXDISPIF const pIf, uint32_t cDispDef, const VMMDevDisplayDef *paDispDef);

typedef struct VPOXDISPIF_SEAMLESS
{
    PCVPOXDISPIF pIf;

    union
    {
#ifdef VPOX_WITH_WDDM
        struct
        {
            VPOXDISPKMT_ADAPTER Adapter;
# ifdef VPOX_DISPIF_WITH_OPCONTEXT
            VPOXDISPKMT_DEVICE Device;
            VPOXDISPKMT_CONTEXT Context;
# endif
        } wddm;
#endif
    } modeData;
} VPOXDISPIF_SEAMLESS;

DECLINLINE(bool) VPoxDispIfSeamlesIsValid(VPOXDISPIF_SEAMLESS *pSeamless)
{
    return !!pSeamless->pIf;
}

DWORD VPoxDispIfSeamlessCreate(PCVPOXDISPIF const pIf, VPOXDISPIF_SEAMLESS *pSeamless, HANDLE hEvent);
DWORD VPoxDispIfSeamlessTerm(VPOXDISPIF_SEAMLESS *pSeamless);
DWORD VPoxDispIfSeamlessSubmit(VPOXDISPIF_SEAMLESS *pSeamless, VPOXDISPIFESCAPE *pData, int cbData);

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDispIf_h */

