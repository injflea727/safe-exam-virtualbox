/* $Id: VPoxDispVHWA.h $ */
/** @file
 * VPox XPDM Display driver, helper functions which interacts with our miniport driver
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispVHWA_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispVHWA_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxDisp.h"

#ifdef VPOX_WITH_VIDEOHWACCEL
typedef struct _VPOXDISPVHWAINFO
{
    uint32_t caps;
    uint32_t caps2;
    uint32_t colorKeyCaps;
    uint32_t stretchCaps;
    uint32_t surfaceCaps;
    uint32_t numOverlays;
    uint32_t numFourCC;
    HGSMIOFFSET FourCC;
    ULONG_PTR offVramBase;
    BOOLEAN bEnabled;
} VPOXDISPVHWAINFO;
#endif

typedef struct _VPOXVHWAREGION
{
    RECTL Rect;
    bool bValid;
}VPOXVHWAREGION, *PVPOXVHWAREGION;

typedef struct _VPOXVHWASURFDESC
{
    VPOXVHWA_SURFHANDLE hHostHandle;
    volatile uint32_t cPendingBltsSrc;
    volatile uint32_t cPendingBltsDst;
    volatile uint32_t cPendingFlipsCurr;
    volatile uint32_t cPendingFlipsTarg;
#ifdef DEBUG
    volatile uint32_t cFlipsCurr;
    volatile uint32_t cFlipsTarg;
#endif
    bool bVisible;
    VPOXVHWAREGION UpdatedMemRegion;
    VPOXVHWAREGION NonupdatedMemRegion;
}VPOXVHWASURFDESC, *PVPOXVHWASURFDESC;

typedef DECLCALLBACK(void) FNVPOXVHWACMDCOMPLETION(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd, void *pvContext);
typedef FNVPOXVHWACMDCOMPLETION *PFNVPOXVHWACMDCOMPLETION;

void VPoxDispVHWAInit(PVPOXDISPDEV pDev);
int  VPoxDispVHWAEnable(PVPOXDISPDEV pDev);
int  VPoxDispVHWADisable(PVPOXDISPDEV pDev);
int  VPoxDispVHWAInitHostInfo1(PVPOXDISPDEV pDev);
int  VPoxDispVHWAInitHostInfo2(PVPOXDISPDEV pDev, DWORD *pFourCC);

VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *VPoxDispVHWACommandCreate(PVPOXDISPDEV pDev, VPOXVHWACMD_TYPE enmCmd, VPOXVHWACMD_LENGTH cbCmd);
void VPoxDispVHWACommandRelease(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);
BOOL VPoxDispVHWACommandSubmit(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST*pCmd);
void VPoxDispVHWACommandSubmitAsynch(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd,
                                     PFNVPOXVHWACMDCOMPLETION pfnCompletion, void * pContext);
void VPoxDispVHWACommandSubmitAsynchAndComplete(PVPOXDISPDEV pDev, VPOXVHWACMD RT_UNTRUSTED_VOLATILE_HOST *pCmd);
void VPoxDispVHWACommandCheckHostCmds(PVPOXDISPDEV pDev);

PVPOXVHWASURFDESC VPoxDispVHWASurfDescAlloc();
void VPoxDispVHWASurfDescFree(PVPOXVHWASURFDESC pDesc);

uint64_t VPoxDispVHWAVramOffsetFromPDEV(PVPOXDISPDEV pDev, ULONG_PTR offPdev);

void VPoxDispVHWARectUnited(RECTL * pDst, RECTL * pRect1, RECTL * pRect2);
bool VPoxDispVHWARectIsEmpty(RECTL * pRect);
bool VPoxDispVHWARectIntersect(RECTL * pRect1, RECTL * pRect2);
bool VPoxDispVHWARectInclude(RECTL * pRect1, RECTL * pRect2);
bool VPoxDispVHWARegionIntersects(PVPOXVHWAREGION pReg, RECTL * pRect);
bool VPoxDispVHWARegionIncludes(PVPOXVHWAREGION pReg, RECTL * pRect);
bool VPoxDispVHWARegionIncluded(PVPOXVHWAREGION pReg, RECTL * pRect);
void VPoxDispVHWARegionSet(PVPOXVHWAREGION pReg, RECTL * pRect);
void VPoxDispVHWARegionAdd(PVPOXVHWAREGION pReg, RECTL * pRect);
void VPoxDispVHWARegionInit(PVPOXVHWAREGION pReg);
void VPoxDispVHWARegionClear(PVPOXVHWAREGION pReg);
bool VPoxDispVHWARegionValid(PVPOXVHWAREGION pReg);
void VPoxDispVHWARegionTrySubstitute(PVPOXVHWAREGION pReg, const RECTL *pRect);

uint32_t VPoxDispVHWAFromDDCAPS(uint32_t caps);
uint32_t VPoxDispVHWAToDDCAPS(uint32_t caps);
uint32_t VPoxDispVHWAFromDDSCAPS(uint32_t caps);
uint32_t VPoxDispVHWAToDDSCAPS(uint32_t caps);
uint32_t VPoxDispVHWAFromDDPFS(uint32_t caps);
uint32_t VPoxDispVHWAToDDPFS(uint32_t caps);
uint32_t VPoxDispVHWAFromDDCKEYCAPS(uint32_t caps);
uint32_t VPoxDispVHWAToDDCKEYCAPS(uint32_t caps);
uint32_t VPoxDispVHWAToDDBLTs(uint32_t caps);
uint32_t VPoxDispVHWAFromDDBLTs(uint32_t caps);
uint32_t VPoxDispVHWAFromDDCAPS2(uint32_t caps);
uint32_t VPoxDispVHWAToDDCAPS2(uint32_t caps);
uint32_t VPoxDispVHWAFromDDOVERs(uint32_t caps);
uint32_t VPoxDispVHWAToDDOVERs(uint32_t caps);
uint32_t VPoxDispVHWAFromDDCKEYs(uint32_t caps);
uint32_t VPoxDispVHWAToDDCKEYs(uint32_t caps);

int VPoxDispVHWAFromDDSURFACEDESC(VPOXVHWA_SURFACEDESC RT_UNTRUSTED_VOLATILE_HOST *pVHWADesc, DDSURFACEDESC *pDdDesc);
int VPoxDispVHWAFromDDPIXELFORMAT(VPOXVHWA_PIXELFORMAT RT_UNTRUSTED_VOLATILE_HOST *pVHWAFormat, DDPIXELFORMAT *pDdFormat);
void VPoxDispVHWAFromDDOVERLAYFX(VPOXVHWA_OVERLAYFX RT_UNTRUSTED_VOLATILE_HOST  *pVHWAOverlay, DDOVERLAYFX *pDdOverlay);
void VPoxDispVHWAFromDDCOLORKEY(VPOXVHWA_COLORKEY RT_UNTRUSTED_VOLATILE_HOST *pVHWACKey, DDCOLORKEY  *pDdCKey);
void VPoxDispVHWAFromDDBLTFX(VPOXVHWA_BLTFX RT_UNTRUSTED_VOLATILE_HOST *pVHWABlt, DDBLTFX *pDdBlt);
void VPoxDispVHWAFromRECTL(VPOXVHWA_RECTL *pDst, RECTL const *pSrc);
void VPoxDispVHWAFromRECTL(VPOXVHWA_RECTL RT_UNTRUSTED_VOLATILE_HOST *pDst, RECTL const *pSrc);

uint32_t VPoxDispVHWAUnsupportedDDCAPS(uint32_t caps);
uint32_t VPoxDispVHWAUnsupportedDDSCAPS(uint32_t caps);
uint32_t VPoxDispVHWAUnsupportedDDPFS(uint32_t caps);
uint32_t VPoxDispVHWAUnsupportedDDCEYCAPS(uint32_t caps);
uint32_t VPoxDispVHWASupportedDDCAPS(uint32_t caps);
uint32_t VPoxDispVHWASupportedDDSCAPS(uint32_t caps);
uint32_t VPoxDispVHWASupportedDDPFS(uint32_t caps);
uint32_t VPoxDispVHWASupportedDDCEYCAPS(uint32_t caps);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_xpdm_VPoxDispVHWA_h */

