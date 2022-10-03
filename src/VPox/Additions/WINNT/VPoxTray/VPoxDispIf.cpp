/* $Id: VPoxDispIf.cpp $ */
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VPoxTray.h"
#define _WIN32_WINNT 0x0601
#include <iprt/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/system.h>

#include <malloc.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifdef DEBUG
# define WARN(_m) do { \
            AssertFailed(); \
            LogRelFunc(_m); \
        } while (0)
#else
# define WARN(_m) do { \
            LogRelFunc(_m); \
        } while (0)
#endif

#ifdef VPOX_WITH_WDDM
#include <iprt/asm.h>
#endif

#include "VPoxDisplay.h"

#ifndef NT_SUCCESS
# define NT_SUCCESS(_Status) ((_Status) >= 0)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct VPOXDISPIF_OP
{
    PCVPOXDISPIF pIf;
    VPOXDISPKMT_ADAPTER Adapter;
    VPOXDISPKMT_DEVICE Device;
    VPOXDISPKMT_CONTEXT Context;
} VPOXDISPIF_OP;

/*
 * APIs specific to Win7 and above WDDM architecture. Not available for Vista WDDM.
 * This is the reason they have not been put in the VPOXDISPIF struct in VPoxDispIf.h.
 */
typedef struct _VPOXDISPLAYWDDMAPICONTEXT
{
    LONG (WINAPI * pfnSetDisplayConfig)(UINT numPathArrayElements,DISPLAYCONFIG_PATH_INFO *pathArray,UINT numModeInfoArrayElements,
                                    DISPLAYCONFIG_MODE_INFO *modeInfoArray, UINT Flags);
    LONG (WINAPI * pfnQueryDisplayConfig)(UINT Flags,UINT *pNumPathArrayElements, DISPLAYCONFIG_PATH_INFO *pPathInfoArray,
                                      UINT *pNumModeInfoArrayElements, DISPLAYCONFIG_MODE_INFO *pModeInfoArray,
                                      DISPLAYCONFIG_TOPOLOGY_ID *pCurrentTopologyId);
    LONG (WINAPI * pfnGetDisplayConfigBufferSizes)(UINT Flags, UINT *pNumPathArrayElements, UINT *pNumModeInfoArrayElements);
} _VPOXDISPLAYWDDMAPICONTEXT;

static _VPOXDISPLAYWDDMAPICONTEXT gCtx = {0};

typedef struct VPOXDISPIF_WDDM_DISPCFG
{
    UINT32 cPathInfoArray;
    DISPLAYCONFIG_PATH_INFO *pPathInfoArray;
    UINT32 cModeInfoArray;
    DISPLAYCONFIG_MODE_INFO *pModeInfoArray;
} VPOXDISPIF_WDDM_DISPCFG;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DWORD vpoxDispIfWddmResizeDisplay(PCVPOXDISPIF const pIf, UINT Id, BOOL fEnable, DISPLAY_DEVICE * paDisplayDevices,
                                         DEVMODE *paDeviceModes, UINT devModes);

static DWORD vpoxDispIfWddmResizeDisplay2(PCVPOXDISPIF const pIf, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT devModes);

static DWORD vpoxDispIfResizePerform(PCVPOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup,
                                     DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes);
static DWORD vpoxDispIfWddmEnableDisplaysTryingTopology(PCVPOXDISPIF const pIf, UINT cIds, UINT *pIds, BOOL fEnable);
static DWORD vpoxDispIfResizeStartedWDDMOp(VPOXDISPIF_OP *pOp);

static void vpoxDispIfWddmDcLogRel(VPOXDISPIF_WDDM_DISPCFG const *pCfg, UINT fFlags)
{
    LogRel(("Display config: Flags = 0x%08X\n", fFlags));

    LogRel(("PATH_INFO[%d]:\n", pCfg->cPathInfoArray));
    for (uint32_t i = 0; i < pCfg->cPathInfoArray; ++i)
    {
        DISPLAYCONFIG_PATH_INFO *p = &pCfg->pPathInfoArray[i];

        LogRel(("%d: flags 0x%08x\n", i, p->flags));

        LogRel(("  sourceInfo: adapterId 0x%08x:%08x, id %u, modeIdx %d, statusFlags 0x%08x\n",
                p->sourceInfo.adapterId.HighPart, p->sourceInfo.adapterId.LowPart,
                p->sourceInfo.id, p->sourceInfo.modeInfoIdx, p->sourceInfo.statusFlags));

        LogRel(("  targetInfo: adapterId 0x%08x:%08x, id %u, modeIdx %d,\n"
                "              ot %d, r %d, s %d, rr %d/%d, so %d, ta %d, statusFlags 0x%08x\n",
                p->targetInfo.adapterId.HighPart, p->targetInfo.adapterId.LowPart,
                p->targetInfo.id, p->targetInfo.modeInfoIdx,
                p->targetInfo.outputTechnology,
                p->targetInfo.rotation,
                p->targetInfo.scaling,
                p->targetInfo.refreshRate.Numerator, p->targetInfo.refreshRate.Denominator,
                p->targetInfo.scanLineOrdering,
                p->targetInfo.targetAvailable,
                p->targetInfo.statusFlags
              ));
    }

    LogRel(("MODE_INFO[%d]:\n", pCfg->cModeInfoArray));
    for (uint32_t i = 0; i < pCfg->cModeInfoArray; ++i)
    {
        DISPLAYCONFIG_MODE_INFO *p = &pCfg->pModeInfoArray[i];

        LogRel(("%d: adapterId 0x%08x:%08x, id %u\n",
                i, p->adapterId.HighPart, p->adapterId.LowPart, p->id));

        if (p->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE)
        {
            LogRel(("  src %ux%u, fmt %d, @%dx%d\n",
                    p->sourceMode.width, p->sourceMode.height, p->sourceMode.pixelFormat,
                    p->sourceMode.position.x, p->sourceMode.position.y));
        }
        else if (p->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
        {
            LogRel(("  tgt pr 0x%RX64, hSyncFreq %d/%d, vSyncFreq %d/%d, active %ux%u, total %ux%u, std %d, so %d\n",
                    p->targetMode.targetVideoSignalInfo.pixelRate,
                    p->targetMode.targetVideoSignalInfo.hSyncFreq.Numerator, p->targetMode.targetVideoSignalInfo.hSyncFreq.Denominator,
                    p->targetMode.targetVideoSignalInfo.vSyncFreq.Numerator, p->targetMode.targetVideoSignalInfo.vSyncFreq.Denominator,
                    p->targetMode.targetVideoSignalInfo.activeSize.cx, p->targetMode.targetVideoSignalInfo.activeSize.cy,
                    p->targetMode.targetVideoSignalInfo.totalSize.cx, p->targetMode.targetVideoSignalInfo.totalSize.cy,
                    p->targetMode.targetVideoSignalInfo.videoStandard,
                    p->targetMode.targetVideoSignalInfo.scanLineOrdering));
        }
        else
        {
            LogRel(("  Invalid infoType %u(0x%08x)\n", p->infoType, p->infoType));
        }
    }
}

static DWORD vpoxDispIfWddmDcCreate(VPOXDISPIF_WDDM_DISPCFG *pCfg, UINT32 fFlags)
{
    UINT32 cPathInfoArray = 0;
    UINT32 cModeInfoArray = 0;
    DISPLAYCONFIG_PATH_INFO *pPathInfoArray;
    DISPLAYCONFIG_MODE_INFO *pModeInfoArray;
    DWORD winEr = gCtx.pfnGetDisplayConfigBufferSizes(fFlags, &cPathInfoArray, &cModeInfoArray);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed GetDisplayConfigBufferSizes\n"));
        return winEr;
    }

    pPathInfoArray = (DISPLAYCONFIG_PATH_INFO *)malloc(cPathInfoArray * sizeof(DISPLAYCONFIG_PATH_INFO));
    if (!pPathInfoArray)
    {
        WARN(("VPoxTray: (WDDM) malloc failed!\n"));
        return ERROR_OUTOFMEMORY;
    }
    pModeInfoArray = (DISPLAYCONFIG_MODE_INFO *)malloc(cModeInfoArray * sizeof(DISPLAYCONFIG_MODE_INFO));
    if (!pModeInfoArray)
    {
        WARN(("VPoxTray: (WDDM) malloc failed!\n"));
        free(pPathInfoArray);
        return ERROR_OUTOFMEMORY;
    }

    winEr = gCtx.pfnQueryDisplayConfig(fFlags, &cPathInfoArray, pPathInfoArray, &cModeInfoArray, pModeInfoArray, NULL);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed QueryDisplayConfig\n"));
        free(pPathInfoArray);
        free(pModeInfoArray);
        return winEr;
    }

    pCfg->cPathInfoArray = cPathInfoArray;
    pCfg->pPathInfoArray = pPathInfoArray;
    pCfg->cModeInfoArray = cModeInfoArray;
    pCfg->pModeInfoArray = pModeInfoArray;
    return ERROR_SUCCESS;
}

static DWORD vpoxDispIfWddmDcClone(VPOXDISPIF_WDDM_DISPCFG *pCfg, VPOXDISPIF_WDDM_DISPCFG *pCfgDst)
{
    memset(pCfgDst, 0, sizeof (*pCfgDst));

    if (pCfg->cPathInfoArray)
    {
        pCfgDst->pPathInfoArray = (DISPLAYCONFIG_PATH_INFO *)malloc(pCfg->cPathInfoArray * sizeof (DISPLAYCONFIG_PATH_INFO));
        if (!pCfgDst->pPathInfoArray)
        {
            WARN(("VPoxTray: (WDDM) malloc failed!\n"));
            return ERROR_OUTOFMEMORY;
        }

        memcpy(pCfgDst->pPathInfoArray, pCfg->pPathInfoArray, pCfg->cPathInfoArray * sizeof (DISPLAYCONFIG_PATH_INFO));

        pCfgDst->cPathInfoArray = pCfg->cPathInfoArray;
    }

    if (pCfg->cModeInfoArray)
    {
        pCfgDst->pModeInfoArray = (DISPLAYCONFIG_MODE_INFO *)malloc(pCfg->cModeInfoArray * sizeof (DISPLAYCONFIG_MODE_INFO));
        if (!pCfgDst->pModeInfoArray)
        {
            WARN(("VPoxTray: (WDDM) malloc failed!\n"));
            if (pCfgDst->pPathInfoArray)
            {
                free(pCfgDst->pPathInfoArray);
                pCfgDst->pPathInfoArray = NULL;
            }
            return ERROR_OUTOFMEMORY;
        }

        memcpy(pCfgDst->pModeInfoArray, pCfg->pModeInfoArray, pCfg->cModeInfoArray * sizeof (DISPLAYCONFIG_MODE_INFO));

        pCfgDst->cModeInfoArray = pCfg->cModeInfoArray;
    }

    return ERROR_SUCCESS;
}


static VOID vpoxDispIfWddmDcTerm(VPOXDISPIF_WDDM_DISPCFG *pCfg)
{
    if (pCfg->pPathInfoArray)
        free(pCfg->pPathInfoArray);
    if (pCfg->pModeInfoArray)
        free(pCfg->pModeInfoArray);
    /* sanity */
    memset(pCfg, 0, sizeof (*pCfg));
}

static UINT32 g_cVPoxDispIfWddmDisplays = 0;
static DWORD vpoxDispIfWddmDcQueryNumDisplays(UINT32 *pcDisplays)
{
    if (!g_cVPoxDispIfWddmDisplays)
    {
        VPOXDISPIF_WDDM_DISPCFG DispCfg;
        *pcDisplays = 0;
        DWORD winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ALL_PATHS);
        if (winEr != ERROR_SUCCESS)
        {
            WARN(("VPoxTray:(WDDM) vpoxDispIfWddmDcCreate Failed winEr %d\n", winEr));
            return winEr;
        }

        int cDisplays = -1;

        for (UINT iter = 0; iter < DispCfg.cPathInfoArray; ++iter)
        {
            if (cDisplays < (int)(DispCfg.pPathInfoArray[iter].sourceInfo.id))
                cDisplays = (int)(DispCfg.pPathInfoArray[iter].sourceInfo.id);
        }

        cDisplays++;

        g_cVPoxDispIfWddmDisplays = cDisplays;
        Assert(g_cVPoxDispIfWddmDisplays);

        vpoxDispIfWddmDcTerm(&DispCfg);
    }

    *pcDisplays = g_cVPoxDispIfWddmDisplays;
    return ERROR_SUCCESS;
}

#define VPOX_WDDM_DC_SEARCH_PATH_ANY (~(UINT)0)
static int vpoxDispIfWddmDcSearchPath(VPOXDISPIF_WDDM_DISPCFG *pCfg, UINT srcId, UINT trgId)
{
    for (UINT iter = 0; iter < pCfg->cPathInfoArray; ++iter)
    {
        if (   (srcId == VPOX_WDDM_DC_SEARCH_PATH_ANY || pCfg->pPathInfoArray[iter].sourceInfo.id == srcId)
            && (trgId == VPOX_WDDM_DC_SEARCH_PATH_ANY || pCfg->pPathInfoArray[iter].targetInfo.id == trgId))
        {
            return (int)iter;
        }
    }
    return -1;
}

static int vpoxDispIfWddmDcSearchActiveSourcePath(VPOXDISPIF_WDDM_DISPCFG *pCfg, UINT srcId)
{
    for (UINT i = 0; i < pCfg->cPathInfoArray; ++i)
    {
        if (   pCfg->pPathInfoArray[i].sourceInfo.id == srcId
            && RT_BOOL(pCfg->pPathInfoArray[i].flags & DISPLAYCONFIG_PATH_ACTIVE))
        {
            return (int)i;
        }
    }
    return -1;
}

static int vpoxDispIfWddmDcSearchActivePath(VPOXDISPIF_WDDM_DISPCFG *pCfg, UINT srcId, UINT trgId)
{
    int idx = vpoxDispIfWddmDcSearchPath(pCfg, srcId, trgId);
    if (idx < 0)
        return idx;

    if (!(pCfg->pPathInfoArray[idx].flags & DISPLAYCONFIG_PATH_ACTIVE))
        return -1;

    return idx;
}

static VOID vpoxDispIfWddmDcSettingsInvalidateModeIndex(VPOXDISPIF_WDDM_DISPCFG *pCfg, int idx)
{
    pCfg->pPathInfoArray[idx].sourceInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
    pCfg->pPathInfoArray[idx].targetInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
}

static VOID vpoxDispIfWddmDcSettingsInvalidateModeIndeces(VPOXDISPIF_WDDM_DISPCFG *pCfg)
{
    for (UINT iter = 0; iter < pCfg->cPathInfoArray; ++iter)
    {
        vpoxDispIfWddmDcSettingsInvalidateModeIndex(pCfg, (int)iter);
    }

    if (pCfg->pModeInfoArray)
    {
        free(pCfg->pModeInfoArray);
        pCfg->pModeInfoArray = NULL;
    }
    pCfg->cModeInfoArray = 0;
}

static DWORD vpoxDispIfWddmDcSettingsModeAdd(VPOXDISPIF_WDDM_DISPCFG *pCfg, UINT *pIdx)
{
    UINT32 cModeInfoArray = pCfg->cModeInfoArray + 1;
    DISPLAYCONFIG_MODE_INFO *pModeInfoArray = (DISPLAYCONFIG_MODE_INFO *)malloc(cModeInfoArray * sizeof (DISPLAYCONFIG_MODE_INFO));
    if (!pModeInfoArray)
    {
        WARN(("VPoxTray: (WDDM) malloc failed!\n"));
        return ERROR_OUTOFMEMORY;
    }

    memcpy (pModeInfoArray, pCfg->pModeInfoArray, pCfg->cModeInfoArray * sizeof(DISPLAYCONFIG_MODE_INFO));
    memset(&pModeInfoArray[cModeInfoArray-1], 0, sizeof (pModeInfoArray[0]));
    free(pCfg->pModeInfoArray);
    *pIdx = cModeInfoArray-1;
    pCfg->pModeInfoArray = pModeInfoArray;
    pCfg->cModeInfoArray = cModeInfoArray;
    return ERROR_SUCCESS;
}

static DWORD vpoxDispIfWddmDcSettingsUpdate(VPOXDISPIF_WDDM_DISPCFG *pCfg, int idx, DEVMODE *pDeviceMode, BOOL fInvalidateSrcMode, BOOL fEnable)
{
    if (fInvalidateSrcMode)
        pCfg->pPathInfoArray[idx].sourceInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
    else if (pDeviceMode)
    {
        UINT iSrcMode = pCfg->pPathInfoArray[idx].sourceInfo.modeInfoIdx;
        if (iSrcMode == DISPLAYCONFIG_PATH_MODE_IDX_INVALID)
        {

            WARN(("VPoxTray: (WDDM) no source mode index specified"));
            DWORD winEr = vpoxDispIfWddmDcSettingsModeAdd(pCfg, &iSrcMode);
            if (winEr != ERROR_SUCCESS)
            {
                WARN(("VPoxTray:(WDDM) vpoxDispIfWddmDcSettingsModeAdd Failed winEr %d\n", winEr));
                return winEr;
            }
            pCfg->pPathInfoArray[idx].sourceInfo.modeInfoIdx = iSrcMode;
        }

        for (int i = 0; i < (int)pCfg->cPathInfoArray; ++i)
        {
            if (i == idx)
                continue;

            if (pCfg->pPathInfoArray[i].sourceInfo.modeInfoIdx == iSrcMode)
            {
                /* this is something we're not expecting/supporting */
                WARN(("VPoxTray: (WDDM) multiple paths have the same mode index"));
                return ERROR_NOT_SUPPORTED;
            }
        }

        if (pDeviceMode->dmFields & DM_PELSWIDTH)
            pCfg->pModeInfoArray[iSrcMode].sourceMode.width = pDeviceMode->dmPelsWidth;
        if (pDeviceMode->dmFields & DM_PELSHEIGHT)
            pCfg->pModeInfoArray[iSrcMode].sourceMode.height = pDeviceMode->dmPelsHeight;
        if (pDeviceMode->dmFields & DM_POSITION)
        {
            LogFlowFunc(("DM_POSITION %d,%d -> %d,%d\n",
                         pCfg->pModeInfoArray[iSrcMode].sourceMode.position.x,
                         pCfg->pModeInfoArray[iSrcMode].sourceMode.position.y,
                         pDeviceMode->dmPosition.x, pDeviceMode->dmPosition.y));
            pCfg->pModeInfoArray[iSrcMode].sourceMode.position.x = pDeviceMode->dmPosition.x;
            pCfg->pModeInfoArray[iSrcMode].sourceMode.position.y = pDeviceMode->dmPosition.y;
        }
        if (pDeviceMode->dmFields & DM_BITSPERPEL)
        {
            switch (pDeviceMode->dmBitsPerPel)
            {
                case 32:
                    pCfg->pModeInfoArray[iSrcMode].sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                    break;
                case 24:
                    pCfg->pModeInfoArray[iSrcMode].sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_24BPP;
                    break;
                case 16:
                    pCfg->pModeInfoArray[iSrcMode].sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_16BPP;
                    break;
                case 8:
                    pCfg->pModeInfoArray[iSrcMode].sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_8BPP;
                    break;
                default:
                    LogRel(("VPoxTray: (WDDM) invalid bpp %d, using 32\n", pDeviceMode->dmBitsPerPel));
                    pCfg->pModeInfoArray[iSrcMode].sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                    break;
            }
        }
    }

    pCfg->pPathInfoArray[idx].targetInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;

    /* "A refresh rate with both the numerator and denominator set to zero indicates that
     * the caller does not specify a refresh rate and the operating system should use
     * the most optimal refresh rate available. For this case, in a call to the SetDisplayConfig
     * function, the caller must set the scanLineOrdering member to the
     * DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED value; otherwise, SetDisplayConfig fails."
     *
     * If a refresh rate is set to a value, then the resize will fail if miniport driver
     * does not support VSync, i.e. with display-only driver on Win8+ (@bugref{8440}).
     */
    pCfg->pPathInfoArray[idx].targetInfo.refreshRate.Numerator = 0;
    pCfg->pPathInfoArray[idx].targetInfo.refreshRate.Denominator = 0;
    pCfg->pPathInfoArray[idx].targetInfo.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED;

    /* Make sure that "The output can be forced on this target even if a monitor is not detected." */
    pCfg->pPathInfoArray[idx].targetInfo.targetAvailable = TRUE;
    pCfg->pPathInfoArray[idx].targetInfo.statusFlags |= DISPLAYCONFIG_TARGET_FORCIBLE;

    if (fEnable)
        pCfg->pPathInfoArray[idx].flags |= DISPLAYCONFIG_PATH_ACTIVE;
    else
        pCfg->pPathInfoArray[idx].flags &= ~DISPLAYCONFIG_PATH_ACTIVE;

    return ERROR_SUCCESS;
}

static DWORD vpoxDispIfWddmDcSet(VPOXDISPIF_WDDM_DISPCFG *pCfg, UINT fFlags)
{
    DWORD winEr = gCtx.pfnSetDisplayConfig(pCfg->cPathInfoArray, pCfg->pPathInfoArray, pCfg->cModeInfoArray, pCfg->pModeInfoArray, fFlags);
    if (winEr != ERROR_SUCCESS)
        Log(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed for Flags 0x%x\n", fFlags));
    return winEr;
}

static BOOL vpoxDispIfWddmDcSettingsAdjustSupportedPaths(VPOXDISPIF_WDDM_DISPCFG *pCfg)
{
    BOOL fAdjusted = FALSE;
    for (UINT iter = 0; iter < pCfg->cPathInfoArray; ++iter)
    {
        if (pCfg->pPathInfoArray[iter].sourceInfo.id == pCfg->pPathInfoArray[iter].targetInfo.id)
            continue;

        if (!(pCfg->pPathInfoArray[iter].flags & DISPLAYCONFIG_PATH_ACTIVE))
            continue;

        pCfg->pPathInfoArray[iter].flags &= ~DISPLAYCONFIG_PATH_ACTIVE;
        fAdjusted = TRUE;
    }

    return fAdjusted;
}

static void vpoxDispIfWddmDcSettingsAttachDisbledToPrimary(VPOXDISPIF_WDDM_DISPCFG *pCfg)
{
    for (UINT iter = 0; iter < pCfg->cPathInfoArray; ++iter)
    {
        if ((pCfg->pPathInfoArray[iter].flags & DISPLAYCONFIG_PATH_ACTIVE))
            continue;

        pCfg->pPathInfoArray[iter].sourceInfo.id = 0;
        pCfg->pPathInfoArray[iter].sourceInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
        pCfg->pPathInfoArray[iter].targetInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
    }
}

static DWORD vpoxDispIfWddmDcSettingsIncludeAllTargets(VPOXDISPIF_WDDM_DISPCFG *pCfg)
{
    UINT32 cDisplays = 0;
    VPOXDISPIF_WDDM_DISPCFG AllCfg;
    BOOL fAllCfgInited = FALSE;

    DWORD winEr = vpoxDispIfWddmDcQueryNumDisplays(&cDisplays);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) vpoxDispIfWddmDcQueryNumDisplays Failed winEr %d\n", winEr));
        return winEr;
    }

    DISPLAYCONFIG_PATH_INFO *pPathInfoArray = (DISPLAYCONFIG_PATH_INFO *)malloc(cDisplays * sizeof(DISPLAYCONFIG_PATH_INFO));
    if (!pPathInfoArray)
    {
        WARN(("malloc failed\n"));
        return ERROR_OUTOFMEMORY;
    }

    for (UINT i = 0; i < cDisplays; ++i)
    {
        int idx = vpoxDispIfWddmDcSearchPath(pCfg, i, i);
        if (idx < 0)
        {
            idx = vpoxDispIfWddmDcSearchPath(pCfg, VPOX_WDDM_DC_SEARCH_PATH_ANY, i);
            if (idx >= 0)
            {
                WARN(("VPoxTray:(WDDM) different source and target paare enabled, this is something we would not expect\n"));
            }
        }

        if (idx >= 0)
            pPathInfoArray[i] = pCfg->pPathInfoArray[idx];
        else
        {
            if (!fAllCfgInited)
            {
                winEr = vpoxDispIfWddmDcCreate(&AllCfg, QDC_ALL_PATHS);
                if (winEr != ERROR_SUCCESS)
                {
                    WARN(("VPoxTray:(WDDM) vpoxDispIfWddmDcCreate Failed winEr %d\n", winEr));
                    free(pPathInfoArray);
                    return winEr;
                }
                fAllCfgInited = TRUE;
            }

            idx = vpoxDispIfWddmDcSearchPath(&AllCfg, i, i);
            if (idx < 0)
            {
                WARN(("VPoxTray:(WDDM) %d %d path not supported\n", i, i));
                idx = vpoxDispIfWddmDcSearchPath(pCfg, VPOX_WDDM_DC_SEARCH_PATH_ANY, i);
                if (idx < 0)
                {
                    WARN(("VPoxTray:(WDDM) %d %d path not supported\n", -1, i));
                }
            }

            if (idx >= 0)
            {
                pPathInfoArray[i] = AllCfg.pPathInfoArray[idx];

                if (pPathInfoArray[i].flags & DISPLAYCONFIG_PATH_ACTIVE)
                {
                    WARN(("VPoxTray:(WDDM) disabled path %d %d is marked active\n",
                            pPathInfoArray[i].sourceInfo.id, pPathInfoArray[i].targetInfo.id));
                    pPathInfoArray[i].flags &= ~DISPLAYCONFIG_PATH_ACTIVE;
                }

                Assert(pPathInfoArray[i].sourceInfo.modeInfoIdx == DISPLAYCONFIG_PATH_MODE_IDX_INVALID);
                Assert(pPathInfoArray[i].sourceInfo.statusFlags == 0);

                Assert(pPathInfoArray[i].targetInfo.modeInfoIdx == DISPLAYCONFIG_PATH_MODE_IDX_INVALID);
                Assert(pPathInfoArray[i].targetInfo.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15);
                Assert(pPathInfoArray[i].targetInfo.rotation == DISPLAYCONFIG_ROTATION_IDENTITY);
                Assert(pPathInfoArray[i].targetInfo.scaling == DISPLAYCONFIG_SCALING_PREFERRED);
                Assert(pPathInfoArray[i].targetInfo.refreshRate.Numerator == 0);
                Assert(pPathInfoArray[i].targetInfo.refreshRate.Denominator == 0);
                Assert(pPathInfoArray[i].targetInfo.scanLineOrdering == DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED);
                Assert(pPathInfoArray[i].targetInfo.targetAvailable == TRUE);
                Assert(pPathInfoArray[i].targetInfo.statusFlags == DISPLAYCONFIG_TARGET_FORCIBLE);

                Assert(pPathInfoArray[i].flags == 0);
            }
            else
            {
                pPathInfoArray[i].sourceInfo.adapterId = pCfg->pPathInfoArray[0].sourceInfo.adapterId;
                pPathInfoArray[i].sourceInfo.id = i;
                pPathInfoArray[i].sourceInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
                pPathInfoArray[i].sourceInfo.statusFlags = 0;

                pPathInfoArray[i].targetInfo.adapterId = pPathInfoArray[i].sourceInfo.adapterId;
                pPathInfoArray[i].targetInfo.id = i;
                pPathInfoArray[i].targetInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
                pPathInfoArray[i].targetInfo.outputTechnology = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15;
                pPathInfoArray[i].targetInfo.rotation = DISPLAYCONFIG_ROTATION_IDENTITY;
                pPathInfoArray[i].targetInfo.scaling = DISPLAYCONFIG_SCALING_PREFERRED;
                pPathInfoArray[i].targetInfo.refreshRate.Numerator = 0;
                pPathInfoArray[i].targetInfo.refreshRate.Denominator = 0;
                pPathInfoArray[i].targetInfo.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED;
                pPathInfoArray[i].targetInfo.targetAvailable = TRUE;
                pPathInfoArray[i].targetInfo.statusFlags = DISPLAYCONFIG_TARGET_FORCIBLE;

                pPathInfoArray[i].flags = 0;
            }
        }
    }

    free(pCfg->pPathInfoArray);
    pCfg->pPathInfoArray = pPathInfoArray;
    pCfg->cPathInfoArray = cDisplays;
    if (fAllCfgInited)
        vpoxDispIfWddmDcTerm(&AllCfg);

    return ERROR_SUCCESS;
}

static DWORD vpoxDispIfOpBegin(PCVPOXDISPIF pIf, VPOXDISPIF_OP *pOp)
{
    pOp->pIf = pIf;

    HRESULT hr = vpoxDispKmtOpenAdapter(&pIf->modeData.wddm.KmtCallbacks, &pOp->Adapter);
    if (SUCCEEDED(hr))
    {
        hr = vpoxDispKmtCreateDevice(&pOp->Adapter, &pOp->Device);
        if (SUCCEEDED(hr))
        {
            hr = vpoxDispKmtCreateContext(&pOp->Device, &pOp->Context, VPOXWDDM_CONTEXT_TYPE_CUSTOM_DISPIF_RESIZE,
                    NULL, 0ULL);
            if (SUCCEEDED(hr))
                return ERROR_SUCCESS;
            else
                WARN(("VPoxTray: vpoxDispKmtCreateContext failed hr 0x%x", hr));

            vpoxDispKmtDestroyDevice(&pOp->Device);
        }
        else
            WARN(("VPoxTray: vpoxDispKmtCreateDevice failed hr 0x%x", hr));

        vpoxDispKmtCloseAdapter(&pOp->Adapter);
    }

    return ERROR_NOT_SUPPORTED;
}

static VOID vpoxDispIfOpEnd(VPOXDISPIF_OP *pOp)
{
    vpoxDispKmtDestroyContext(&pOp->Context);
    vpoxDispKmtDestroyDevice(&pOp->Device);
    vpoxDispKmtCloseAdapter(&pOp->Adapter);
}

/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us */
DWORD VPoxDispIfInit(PVPOXDISPIF pDispIf)
{
    /* Note: NT4 is handled implicitly by VPoxDispIfSwitchMode(). */
    VPoxDispIfSwitchMode(pDispIf, VPOXDISPIF_MODE_XPDM, NULL);

    return NO_ERROR;
}

#ifdef VPOX_WITH_WDDM
static void vpoxDispIfWddmTerm(PCVPOXDISPIF pIf);
static DWORD vpoxDispIfWddmInit(PCVPOXDISPIF pIf);
#endif

DWORD VPoxDispIfTerm(PVPOXDISPIF pIf)
{
#ifdef VPOX_WITH_WDDM
    if (pIf->enmMode >= VPOXDISPIF_MODE_WDDM)
    {
        vpoxDispIfWddmTerm(pIf);

        vpoxDispKmtCallbacksTerm(&pIf->modeData.wddm.KmtCallbacks);
    }
#endif

    pIf->enmMode = VPOXDISPIF_MODE_UNKNOWN;
    return NO_ERROR;
}

static DWORD vpoxDispIfEscapeXPDM(PCVPOXDISPIF pIf, PVPOXDISPIFESCAPE pEscape, int cbData, int iDirection)
{
    RT_NOREF(pIf);
    HDC  hdc = GetDC(HWND_DESKTOP);
    VOID *pvData = cbData ? VPOXDISPIFESCAPE_DATA(pEscape, VOID) : NULL;
    int iRet = ExtEscape(hdc, pEscape->escapeCode,
            iDirection >= 0 ? cbData : 0,
            iDirection >= 0 ? (LPSTR)pvData : NULL,
            iDirection <= 0 ? cbData : 0,
            iDirection <= 0 ? (LPSTR)pvData : NULL);
    ReleaseDC(HWND_DESKTOP, hdc);
    if (iRet > 0)
        return VINF_SUCCESS;
    if (iRet == 0)
        return ERROR_NOT_SUPPORTED;
    /* else */
    return ERROR_GEN_FAILURE;
}

#ifdef VPOX_WITH_WDDM
static DWORD vpoxDispIfSwitchToWDDM(PVPOXDISPIF pIf)
{
    DWORD err = NO_ERROR;

    bool fSupported = true;

    uint64_t const uNtVersion = RTSystemGetNtVersion();
    if (uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(6, 0, 0))
    {
        LogFunc(("this is vista and up\n"));
        HMODULE hUser = GetModuleHandle("user32.dll");
        if (hUser)
        {
            *(uintptr_t *)&pIf->modeData.wddm.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
            LogFunc(("VPoxDisplayInit: pfnChangeDisplaySettingsEx = %p\n", pIf->modeData.wddm.pfnChangeDisplaySettingsEx));
            fSupported &= !!(pIf->modeData.wddm.pfnChangeDisplaySettingsEx);

            *(uintptr_t *)&pIf->modeData.wddm.pfnEnumDisplayDevices = (uintptr_t)GetProcAddress(hUser, "EnumDisplayDevicesA");
            LogFunc(("VPoxDisplayInit: pfnEnumDisplayDevices = %p\n", pIf->modeData.wddm.pfnEnumDisplayDevices));
            fSupported &= !!(pIf->modeData.wddm.pfnEnumDisplayDevices);

            /* for win 7 and above */
            if (uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(6, 1, 0))
            {
                *(uintptr_t *)&gCtx.pfnSetDisplayConfig = (uintptr_t)GetProcAddress(hUser, "SetDisplayConfig");
                LogFunc(("VPoxDisplayInit: pfnSetDisplayConfig = %p\n", gCtx.pfnSetDisplayConfig));
                fSupported &= !!(gCtx.pfnSetDisplayConfig);

                *(uintptr_t *)&gCtx.pfnQueryDisplayConfig = (uintptr_t)GetProcAddress(hUser, "QueryDisplayConfig");
                LogFunc(("VPoxDisplayInit: pfnQueryDisplayConfig = %p\n", gCtx.pfnQueryDisplayConfig));
                fSupported &= !!(gCtx.pfnQueryDisplayConfig);

                *(uintptr_t *)&gCtx.pfnGetDisplayConfigBufferSizes = (uintptr_t)GetProcAddress(hUser, "GetDisplayConfigBufferSizes");
                LogFunc(("VPoxDisplayInit: pfnGetDisplayConfigBufferSizes = %p\n", gCtx.pfnGetDisplayConfigBufferSizes));
                fSupported &= !!(gCtx.pfnGetDisplayConfigBufferSizes);
            }

            /* this is vista and up */
            HRESULT hr = vpoxDispKmtCallbacksInit(&pIf->modeData.wddm.KmtCallbacks);
            if (FAILED(hr))
            {
                WARN(("VPoxTray: vpoxDispKmtCallbacksInit failed hr 0x%x\n", hr));
                err = hr;
            }
        }
        else
        {
            WARN(("GetModuleHandle(USER32) failed, err(%d)\n", GetLastError()));
            err = ERROR_NOT_SUPPORTED;
        }
    }
    else
    {
        WARN(("can not switch to VPOXDISPIF_MODE_WDDM, because os is not Vista or upper\n"));
        err = ERROR_NOT_SUPPORTED;
    }

    if (err == ERROR_SUCCESS)
    {
        err = vpoxDispIfWddmInit(pIf);
    }

    return err;
}

static DWORD vpoxDispIfSwitchToWDDM_W7(PVPOXDISPIF pIf)
{
    return vpoxDispIfSwitchToWDDM(pIf);
}

static DWORD vpoxDispIfWDDMAdpHdcCreate(int iDisplay, HDC *phDc, DISPLAY_DEVICE *pDev)
{
    DWORD winEr = ERROR_INVALID_STATE;
    memset(pDev, 0, sizeof (*pDev));
    pDev->cb = sizeof (*pDev);

    for (int i = 0; ; ++i)
    {
        if (EnumDisplayDevices(NULL, /* LPCTSTR lpDevice */ i, /* DWORD iDevNum */
                pDev, 0 /* DWORD dwFlags*/))
        {
            if (i == iDisplay || (iDisplay < 0 && pDev->StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE))
            {
                HDC hDc = CreateDC(NULL, pDev->DeviceName, NULL, NULL);
                if (hDc)
                {
                    *phDc = hDc;
                    return NO_ERROR;
                }
                else
                {
                    winEr = GetLastError();
                    WARN(("CreateDC failed %d", winEr));
                    break;
                }
            }
            Log(("display data no match display(%d): i(%d), flags(%d)", iDisplay, i, pDev->StateFlags));
        }
        else
        {
            winEr = GetLastError();
            WARN(("EnumDisplayDevices failed %d", winEr));
            break;
        }
    }

    WARN(("vpoxDispIfWDDMAdpHdcCreate failure branch %d", winEr));
    return winEr;
}

static DWORD vpoxDispIfEscapeWDDM(PCVPOXDISPIF pIf, PVPOXDISPIFESCAPE pEscape, int cbData, BOOL fHwAccess)
{
    DWORD winEr = ERROR_SUCCESS;
    VPOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vpoxDispKmtOpenAdapter(&pIf->modeData.wddm.KmtCallbacks, &Adapter);
    if (!SUCCEEDED(hr))
    {
        WARN(("VPoxTray: vpoxDispKmtOpenAdapter failed hr 0x%x\n", hr));
        return hr;
    }

    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = Adapter.hAdapter;
    //EscapeData.hDevice = NULL;
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    if (fHwAccess)
        EscapeData.Flags.HardwareAccess = 1;
    EscapeData.pPrivateDriverData = pEscape;
    EscapeData.PrivateDriverDataSize = VPOXDISPIFESCAPE_SIZE(cbData);
    //EscapeData.hContext = NULL;

    NTSTATUS Status = pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
    if (NT_SUCCESS(Status))
        winEr = ERROR_SUCCESS;
    else
    {
        WARN(("VPoxTray: pfnD3DKMTEscape(0x%08X) failed Status 0x%x\n", pEscape->escapeCode, Status));
        winEr = ERROR_GEN_FAILURE;
    }

    vpoxDispKmtCloseAdapter(&Adapter);

    return winEr;
}
#endif

DWORD VPoxDispIfEscape(PCVPOXDISPIF pIf, PVPOXDISPIFESCAPE pEscape, int cbData)
{
    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
        case VPOXDISPIF_MODE_XPDM:
            return vpoxDispIfEscapeXPDM(pIf, pEscape, cbData, 1);
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfEscapeWDDM(pIf, pEscape, cbData, TRUE /* BOOL fHwAccess */);
#endif
        default:
            LogFunc(("unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

DWORD VPoxDispIfEscapeInOut(PCVPOXDISPIF const pIf, PVPOXDISPIFESCAPE pEscape, int cbData)
{
    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
        case VPOXDISPIF_MODE_XPDM:
            return vpoxDispIfEscapeXPDM(pIf, pEscape, cbData, 0);
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfEscapeWDDM(pIf, pEscape, cbData, TRUE /* BOOL fHwAccess */);
#endif
        default:
            LogFunc(("unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

#ifdef VPOX_WITH_WDDM

#define VPOXRR_TIMER_ID 1234

typedef struct VPOXRR
{
    HANDLE hThread;
    DWORD idThread;
    HANDLE hEvent;
    HWND hWnd;
    CRITICAL_SECTION CritSect;
    UINT_PTR idTimer;
    PCVPOXDISPIF pIf;
    UINT iChangedMode;
    BOOL fEnable;
    BOOL fExtDispSup;
    DISPLAY_DEVICE *paDisplayDevices;
    DEVMODE *paDeviceModes;
    UINT cDevModes;
} VPOXRR, *PVPOXRR;

static VPOXRR g_VPoxRr = {0};

#define VPOX_E_INSUFFICIENT_BUFFER HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
#define VPOX_E_NOT_SUPPORTED HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)

static void vpoxRrRetryStopLocked()
{
    PVPOXRR pMon = &g_VPoxRr;
    if (pMon->pIf)
    {
        if (pMon->paDisplayDevices)
        {
            free(pMon->paDisplayDevices);
            pMon->paDisplayDevices = NULL;
        }

        if (pMon->paDeviceModes)
        {
            free(pMon->paDeviceModes);
            pMon->paDeviceModes = NULL;
        }

        if (pMon->idTimer)
        {
            KillTimer(pMon->hWnd, pMon->idTimer);
            pMon->idTimer = 0;
        }

        pMon->cDevModes = 0;
        pMon->pIf = NULL;
    }
}

static void VPoxRrRetryStop()
{
    PVPOXRR pMon = &g_VPoxRr;
    EnterCriticalSection(&pMon->CritSect);
    vpoxRrRetryStopLocked();
    LeaveCriticalSection(&pMon->CritSect);
}

//static DWORD vpoxDispIfWddmValidateFixResize(PCVPOXDISPIF const pIf, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes);

static void vpoxRrRetryReschedule()
{
}

static void VPoxRrRetrySchedule(PCVPOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    PVPOXRR pMon = &g_VPoxRr;
    EnterCriticalSection(&pMon->CritSect);
    vpoxRrRetryStopLocked();

    pMon->pIf = pIf;
    pMon->iChangedMode = iChangedMode;
    pMon->fEnable = fEnable;
    pMon->fExtDispSup = fExtDispSup;

    if (cDevModes)
    {
        pMon->paDisplayDevices = (DISPLAY_DEVICE*)malloc(sizeof (*paDisplayDevices) * cDevModes);
        Assert(pMon->paDisplayDevices);
        if (!pMon->paDisplayDevices)
        {
            Log(("malloc failed!"));
            vpoxRrRetryStopLocked();
            LeaveCriticalSection(&pMon->CritSect);
            return;
        }
        memcpy(pMon->paDisplayDevices, paDisplayDevices, sizeof (*paDisplayDevices) * cDevModes);

        pMon->paDeviceModes = (DEVMODE*)malloc(sizeof (*paDeviceModes) * cDevModes);
        Assert(pMon->paDeviceModes);
        if (!pMon->paDeviceModes)
        {
            Log(("malloc failed!"));
            vpoxRrRetryStopLocked();
            LeaveCriticalSection(&pMon->CritSect);
            return;
        }
        memcpy(pMon->paDeviceModes, paDeviceModes, sizeof (*paDeviceModes) * cDevModes);
    }
    pMon->cDevModes = cDevModes;

    pMon->idTimer = SetTimer(pMon->hWnd, VPOXRR_TIMER_ID, 1000, (TIMERPROC)NULL);
    Assert(pMon->idTimer);
    if (!pMon->idTimer)
    {
        WARN(("VPoxTray: SetTimer failed!, err %d\n", GetLastError()));
        vpoxRrRetryStopLocked();
    }

    LeaveCriticalSection(&pMon->CritSect);
}

static void vpoxRrRetryPerform()
{
    PVPOXRR pMon = &g_VPoxRr;
    EnterCriticalSection(&pMon->CritSect);
    if (pMon->pIf)
    {
        DWORD dwErr = vpoxDispIfResizePerform(pMon->pIf, pMon->iChangedMode, pMon->fEnable, pMon->fExtDispSup, pMon->paDisplayDevices, pMon->paDeviceModes, pMon->cDevModes);
        if (ERROR_RETRY != dwErr)
            VPoxRrRetryStop();
        else
            vpoxRrRetryReschedule();
    }
    LeaveCriticalSection(&pMon->CritSect);
}

static LRESULT CALLBACK vpoxRrWndProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch(uMsg)
    {
        case WM_DISPLAYCHANGE:
        {
            Log(("VPoxTray: WM_DISPLAYCHANGE\n"));
            VPoxRrRetryStop();
            return 0;
        }
        case WM_TIMER:
        {
            if (wParam == VPOXRR_TIMER_ID)
            {
                Log(("VPoxTray: VPOXRR_TIMER_ID\n"));
                vpoxRrRetryPerform();
                return 0;
            }
            break;
        }
        case WM_NCHITTEST:
            LogFunc(("got WM_NCHITTEST for hwnd(0x%x)\n", hwnd));
            return HTNOWHERE;
        default:
            break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#define VPOXRRWND_NAME "VPoxRrWnd"

static HRESULT vpoxRrWndCreate(HWND *phWnd)
{
    HRESULT hr = S_OK;

    /** @todo r=andy Use VPOXSERVICEENV::hInstance. */
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);

    /* Register the Window Class. */
    WNDCLASSEX wc = { 0 };
    wc.cbSize     = sizeof(WNDCLASSEX);

    if (!GetClassInfoEx(hInstance, VPOXRRWND_NAME, &wc))
    {
        wc.lpfnWndProc   = vpoxRrWndProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = VPOXRRWND_NAME;

        if (!RegisterClassEx(&wc))
        {
            WARN(("RegisterClass failed, winErr(%d)\n", GetLastError()));
            hr = E_FAIL;
        }
    }

    if (hr == S_OK)
    {
        HWND hWnd = CreateWindowEx (WS_EX_TOOLWINDOW,
                                        VPOXRRWND_NAME, VPOXRRWND_NAME,
                                        WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED,
                                        -100, -100,
                                        10, 10,
                                        NULL, //GetDesktopWindow() /* hWndParent */,
                                        NULL /* hMenu */,
                                        hInstance,
                                        NULL /* lpParam */);
        Assert(hWnd);
        if (hWnd)
        {
            *phWnd = hWnd;
        }
        else
        {
            WARN(("CreateWindowEx failed, winErr(%d)\n", GetLastError()));
            hr = E_FAIL;
        }
    }

    return hr;
}

static HRESULT vpoxRrWndDestroy(HWND hWnd)
{
    BOOL bResult = DestroyWindow(hWnd);
    if (bResult)
        return S_OK;

    DWORD winErr = GetLastError();
    WARN(("DestroyWindow failed, winErr(%d) for hWnd(0x%x)\n", winErr, hWnd));

    return HRESULT_FROM_WIN32(winErr);
}

static HRESULT vpoxRrWndInit()
{
    PVPOXRR pMon = &g_VPoxRr;
    return vpoxRrWndCreate(&pMon->hWnd);
}

HRESULT vpoxRrWndTerm()
{
    PVPOXRR pMon = &g_VPoxRr;
    HRESULT hrTmp = vpoxRrWndDestroy(pMon->hWnd);
    Assert(hrTmp == S_OK); NOREF(hrTmp);

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    UnregisterClass(VPOXRRWND_NAME, hInstance);

    return S_OK;
}

#define WM_VPOXRR_INIT_QUIT (WM_APP+2)

HRESULT vpoxRrRun()
{
    PVPOXRR pMon = &g_VPoxRr;
    MSG Msg;

    HRESULT hr = S_FALSE;

    /* Create the thread message queue*/
    PeekMessage(&Msg,
            NULL /* HWND hWnd */,
            WM_USER /* UINT wMsgFilterMin */,
            WM_USER /* UINT wMsgFilterMax */,
            PM_NOREMOVE);

    /*
    * Send signal that message queue is ready.
    * From this moment only the thread is ready to receive messages.
    */
    BOOL bRc = SetEvent(pMon->hEvent);
    if (!bRc)
    {
        DWORD winErr = GetLastError();
        WARN(("SetEvent failed, winErr = (%d)", winErr));
        HRESULT hrTmp = HRESULT_FROM_WIN32(winErr);
        Assert(hrTmp != S_OK); NOREF(hrTmp);
    }

    do
    {
        BOOL bResult = GetMessage(&Msg,
            0 /*HWND hWnd*/,
            0 /*UINT wMsgFilterMin*/,
            0 /*UINT wMsgFilterMax*/
            );

        if (bResult == -1) /* error occurred */
        {
            DWORD winEr = GetLastError();
            hr = HRESULT_FROM_WIN32(winEr);
            /* just ensure we never return success in this case */
            Assert(hr != S_OK);
            Assert(hr != S_FALSE);
            if (hr == S_OK || hr == S_FALSE)
                hr = E_FAIL;
            WARN(("VPoxTray: GetMessage returned -1, err %d\n", winEr));
            VPoxRrRetryStop();
            break;
        }

        if(!bResult) /* WM_QUIT was posted */
        {
            hr = S_FALSE;
            Log(("VPoxTray: GetMessage returned FALSE\n"));
            VPoxRrRetryStop();
            break;
        }

        switch (Msg.message)
        {
            case WM_VPOXRR_INIT_QUIT:
            case WM_CLOSE:
            {
                Log(("VPoxTray: closing Rr %d\n", Msg.message));
                VPoxRrRetryStop();
                PostQuitMessage(0);
                break;
            }
            default:
                TranslateMessage(&Msg);
                DispatchMessage(&Msg);
                break;
        }
    } while (1);
    return 0;
}

static DWORD WINAPI vpoxRrRunnerThread(void *pvUser)
{
    RT_NOREF(pvUser);
    HRESULT hr = vpoxRrWndInit();
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        hr = vpoxRrRun();
        Assert(hr == S_OK);

        vpoxRrWndTerm();
    }

    return 0;
}

HRESULT VPoxRrInit()
{
    HRESULT hr = E_FAIL;
    PVPOXRR pMon = &g_VPoxRr;
    memset(pMon, 0, sizeof (*pMon));

    InitializeCriticalSection(&pMon->CritSect);

    pMon->hEvent = CreateEvent(NULL, /* LPSECURITY_ATTRIBUTES lpEventAttributes*/
            TRUE, /* BOOL bManualReset*/
            FALSE, /* BOOL bInitialState */
            NULL /* LPCTSTR lpName */
          );
    if (pMon->hEvent)
    {
        pMon->hThread = CreateThread(NULL /* LPSECURITY_ATTRIBUTES lpThreadAttributes */,
                                              0 /* SIZE_T dwStackSize */,
                                              vpoxRrRunnerThread,
                                              pMon,
                                              0 /* DWORD dwCreationFlags */,
                                              &pMon->idThread);
        if (pMon->hThread)
        {
            DWORD dwResult = WaitForSingleObject(pMon->hEvent, INFINITE);
            if (dwResult == WAIT_OBJECT_0)
                return S_OK;
            else
            {
                Log(("WaitForSingleObject failed!"));
                hr = E_FAIL;
            }
        }
        else
        {
            DWORD winErr = GetLastError();
            WARN(("CreateThread failed, winErr = (%d)", winErr));
            hr = HRESULT_FROM_WIN32(winErr);
            Assert(hr != S_OK);
        }
        CloseHandle(pMon->hEvent);
    }
    else
    {
        DWORD winErr = GetLastError();
        WARN(("CreateEvent failed, winErr = (%d)", winErr));
        hr = HRESULT_FROM_WIN32(winErr);
        Assert(hr != S_OK);
    }

    DeleteCriticalSection(&pMon->CritSect);

    return hr;
}

VOID VPoxRrTerm()
{
    HRESULT hr;
    PVPOXRR pMon = &g_VPoxRr;
    if (!pMon->hThread)
        return;

    BOOL bResult = PostThreadMessage(pMon->idThread, WM_VPOXRR_INIT_QUIT, 0, 0);
    DWORD winErr;
    if (bResult
            || (winErr = GetLastError()) == ERROR_INVALID_THREAD_ID) /* <- could be that the thread is terminated */
    {
        DWORD dwErr = WaitForSingleObject(pMon->hThread, INFINITE);
        if (dwErr == WAIT_OBJECT_0)
        {
            hr = S_OK;
        }
        else
        {
            winErr = GetLastError();
            hr = HRESULT_FROM_WIN32(winErr);
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(winErr);
    }

    DeleteCriticalSection(&pMon->CritSect);

    CloseHandle(pMon->hThread);
    pMon->hThread = 0;
    CloseHandle(pMon->hEvent);
    pMon->hThread = 0;
}

static DWORD vpoxDispIfWddmInit(PCVPOXDISPIF pIf)
{
    RT_NOREF(pIf);
    HRESULT hr = VPoxRrInit();
    if (SUCCEEDED(hr))
        return ERROR_SUCCESS;
    WARN(("VPoxTray: VPoxRrInit failed hr 0x%x\n", hr));
    return hr;
}

static void vpoxDispIfWddmTerm(PCVPOXDISPIF pIf)
{
    RT_NOREF(pIf);
    VPoxRrTerm();
}

static DWORD vpoxDispIfQueryDisplayConnection(VPOXDISPIF_OP *pOp, UINT32 iDisplay, BOOL *pfConnected)
{
    if (pOp->pIf->enmMode == VPOXDISPIF_MODE_WDDM)
    {
        /** @todo do we need ti impl it? */
        *pfConnected = TRUE;
        return ERROR_SUCCESS;
    }

    *pfConnected = FALSE;

    VPOXDISPIF_WDDM_DISPCFG DispCfg;
    DWORD winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ALL_PATHS);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcCreate winEr %d\n", winEr));
        return winEr;
    }

    int idx = vpoxDispIfWddmDcSearchPath(&DispCfg, iDisplay, iDisplay);
    *pfConnected = (idx >= 0);

    vpoxDispIfWddmDcTerm(&DispCfg);

    return ERROR_SUCCESS;
}

static DWORD vpoxDispIfWaitDisplayDataInited(VPOXDISPIF_OP *pOp)
{
    DWORD winEr = ERROR_SUCCESS;
    do
    {
        Sleep(100);

        D3DKMT_POLLDISPLAYCHILDREN PollData = {0};
        PollData.hAdapter = pOp->Adapter.hAdapter;
        PollData.NonDestructiveOnly = 1;
        NTSTATUS Status = pOp->pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTPollDisplayChildren(&PollData);
        if (Status != 0)
        {
            Log(("VPoxTray: (WDDM) pfnD3DKMTPollDisplayChildren failed, Status (0x%x)\n", Status));
            continue;
        }

        BOOL fFound = FALSE;
#if 0
        for (UINT i = 0; i < VPOXWDDM_SCREENMASK_SIZE; ++i)
        {
            if (pu8DisplayMask && !ASMBitTest(pu8DisplayMask, i))
                continue;

            BOOL fConnected = FALSE;
            winEr = vpoxDispIfQueryDisplayConnection(pOp, i, &fConnected);
            if (winEr != ERROR_SUCCESS)
            {
                WARN(("VPoxTray: (WDDM) Failed vpoxDispIfQueryDisplayConnection winEr %d\n", winEr));
                return winEr;
            }

            if (!fConnected)
            {
                WARN(("VPoxTray: (WDDM) Display %d not connected, not expected\n", i));
                fFound = TRUE;
                break;
            }
        }
#endif
        if (!fFound)
            break;
    } while (1);

    return winEr;
}

static DWORD vpoxDispIfUpdateModesWDDM(VPOXDISPIF_OP *pOp, uint32_t u32TargetId, const RTRECTSIZE *pSize)
{
    DWORD winEr = ERROR_SUCCESS;
    VPOXDISPIFESCAPE_UPDATEMODES EscData = {0};
    EscData.EscapeHdr.escapeCode = VPOXESC_UPDATEMODES;
    EscData.u32TargetId = u32TargetId;
    EscData.Size = *pSize;

    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = pOp->Adapter.hAdapter;
#ifdef VPOX_DISPIF_WITH_OPCONTEXT
    /* win8.1 does not allow context-based escapes for display-only mode */
    EscapeData.hDevice = pOp->Device.hDevice;
    EscapeData.hContext = pOp->Context.hContext;
#endif
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags.HardwareAccess = 1;
    EscapeData.pPrivateDriverData = &EscData;
    EscapeData.PrivateDriverDataSize = sizeof (EscData);

    NTSTATUS Status = pOp->pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
    if (NT_SUCCESS(Status))
        winEr = ERROR_SUCCESS;
    else
    {
        WARN(("VPoxTray: pfnD3DKMTEscape VPOXESC_UPDATEMODES failed Status 0x%x\n", Status));
        winEr = ERROR_GEN_FAILURE;
    }

#ifdef VPOX_WDDM_REPLUG_ON_MODE_CHANGE
    /* The code was disabled because VPOXESC_UPDATEMODES should not cause (un)plugging virtual displays. */
    winEr =  vpoxDispIfWaitDisplayDataInited(pOp);
    if (winEr != NO_ERROR)
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWaitDisplayDataInited winEr %d\n", winEr));
#endif

    return winEr;
}

static DWORD vpoxDispIfTargetConnectivityWDDM(VPOXDISPIF_OP *pOp, uint32_t u32TargetId, uint32_t fu32Connect)
{
    VPOXDISPIFESCAPE_TARGETCONNECTIVITY PrivateData;
    RT_ZERO(PrivateData);
    PrivateData.EscapeHdr.escapeCode = VPOXESC_TARGET_CONNECTIVITY;
    PrivateData.u32TargetId = u32TargetId;
    PrivateData.fu32Connect = fu32Connect;

    D3DKMT_ESCAPE EscapeData;
    RT_ZERO(EscapeData);
    EscapeData.hAdapter = pOp->Adapter.hAdapter;
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags.HardwareAccess = 1;
    EscapeData.pPrivateDriverData = &PrivateData;
    EscapeData.PrivateDriverDataSize = sizeof(PrivateData);

    NTSTATUS Status = pOp->pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
    if (NT_SUCCESS(Status))
        return ERROR_SUCCESS;

    WARN(("VPoxTray: pfnD3DKMTEscape VPOXESC_TARGETCONNECTIVITY failed Status 0x%x\n", Status));
    return ERROR_GEN_FAILURE;
}

DWORD vpoxDispIfCancelPendingResizeWDDM(PCVPOXDISPIF const pIf)
{
    RT_NOREF(pIf);
    Log(("VPoxTray: cancelling pending resize\n"));
    VPoxRrRetryStop();
    return NO_ERROR;
}

static DWORD vpoxDispIfWddmResizeDisplayVista(DEVMODE *paDeviceModes, DISPLAY_DEVICE *paDisplayDevices, DWORD cDevModes, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup)
{
    /* Without this, Windows will not ask the miniport for its
     * mode table but uses an internal cache instead.
     */
    for (DWORD i = 0; i < cDevModes; i++)
    {
        DEVMODE tempDevMode;
        ZeroMemory (&tempDevMode, sizeof (tempDevMode));
        tempDevMode.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings((LPSTR)paDisplayDevices[i].DeviceName, 0xffffff, &tempDevMode);
        Log(("VPoxTray: ResizeDisplayDevice: EnumDisplaySettings last error %d\n", GetLastError ()));
    }

    DWORD winEr = EnableAndResizeDispDev(paDeviceModes, paDisplayDevices, cDevModes, iChangedMode, paDeviceModes[iChangedMode].dmPelsWidth, paDeviceModes[iChangedMode].dmPelsHeight,
            paDeviceModes[iChangedMode].dmBitsPerPel, paDeviceModes[iChangedMode].dmPosition.x, paDeviceModes[iChangedMode].dmPosition.y, fEnable, fExtDispSup);
    if (winEr != NO_ERROR)
        WARN(("VPoxTray: (WDDM) Failed EnableAndResizeDispDev winEr %d\n", winEr));

    return winEr;
}

static DWORD vpoxDispIfResizePerform(PCVPOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    LogFunc((" ENTER"));
    DWORD winEr;

    if (pIf->enmMode > VPOXDISPIF_MODE_WDDM)
    {
        if (fEnable)
            paDisplayDevices[iChangedMode].StateFlags |= DISPLAY_DEVICE_ACTIVE;
        else
            paDisplayDevices[iChangedMode].StateFlags &= ~DISPLAY_DEVICE_ACTIVE;

        winEr = vpoxDispIfWddmResizeDisplay2(pIf, paDisplayDevices, paDeviceModes, cDevModes);

        if (winEr != NO_ERROR)
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmResizeDisplay winEr %d\n", winEr));
    }
    else
    {
        winEr = vpoxDispIfWddmResizeDisplayVista(paDeviceModes, paDisplayDevices, cDevModes, iChangedMode, fEnable, fExtDispSup);
        if (winEr != NO_ERROR)
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmResizeDisplayVista winEr %d\n", winEr));
    }

    LogFunc((" LEAVE"));
    return winEr;
}

DWORD vpoxDispIfResizeModesWDDM(PCVPOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    DWORD winEr = NO_ERROR;

    Log(("VPoxTray: vpoxDispIfResizeModesWDDM iChanged %d cDevModes %d fEnable %d fExtDispSup %d\n", iChangedMode, cDevModes, fEnable, fExtDispSup));
    VPoxRrRetryStop();

    VPOXDISPIF_OP Op;

    winEr = vpoxDispIfOpBegin(pIf, &Op);
    if (winEr != NO_ERROR)
    {
        WARN(("VPoxTray: vpoxDispIfOpBegin failed winEr 0x%x", winEr));
        return winEr;
    }

/*  The pfnD3DKMTInvalidateActiveVidPn was deprecated since Win7 and causes deadlocks since Win10 TH2.
    Instead, the VidPn Manager can replace an old VidPn as soon as SetDisplayConfig or ChangeDisplaySettingsEx will try to set a new display mode.
    On Vista D3DKMTInvalidateActiveVidPn is still required. TBD: Get rid of it. */
    if (Op.pIf->enmMode < VPOXDISPIF_MODE_WDDM_W7)
    {
        D3DKMT_INVALIDATEACTIVEVIDPN ddiArgInvalidateVidPN;
        VPOXWDDM_RECOMMENDVIDPN vpoxRecommendVidPN;

        memset(&ddiArgInvalidateVidPN, 0, sizeof(ddiArgInvalidateVidPN));
        memset(&vpoxRecommendVidPN, 0, sizeof(vpoxRecommendVidPN));

        uint32_t cElements = 0;

        for (uint32_t i = 0; i < cDevModes; ++i)
        {
            if ((i == iChangedMode) ? fEnable : (paDisplayDevices[i].StateFlags & DISPLAY_DEVICE_ACTIVE))
            {
                vpoxRecommendVidPN.aSources[cElements].Size.cx = paDeviceModes[i].dmPelsWidth;
                vpoxRecommendVidPN.aSources[cElements].Size.cy = paDeviceModes[i].dmPelsHeight;
                vpoxRecommendVidPN.aTargets[cElements].iSource = cElements;
                ++cElements;
            }
            else
                vpoxRecommendVidPN.aTargets[cElements].iSource = -1;
        }

        ddiArgInvalidateVidPN.hAdapter = Op.Adapter.hAdapter;
        ddiArgInvalidateVidPN.pPrivateDriverData = &vpoxRecommendVidPN;
        ddiArgInvalidateVidPN.PrivateDriverDataSize = sizeof (vpoxRecommendVidPN);

        NTSTATUS Status;
        Status = Op.pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTInvalidateActiveVidPn(&ddiArgInvalidateVidPN);
        LogFunc(("D3DKMTInvalidateActiveVidPn returned %d)\n", Status));
    }

    vpoxDispIfTargetConnectivityWDDM(&Op, iChangedMode, fEnable? 1: 0);

    /* Whether the current display is already or should be enabled. */
    BOOL fChangedEnable = fEnable || RT_BOOL(paDisplayDevices[iChangedMode].StateFlags & DISPLAY_DEVICE_ACTIVE);

    if (fChangedEnable)
    {
        RTRECTSIZE Size;

        Size.cx = paDeviceModes[iChangedMode].dmPelsWidth;
        Size.cy = paDeviceModes[iChangedMode].dmPelsHeight;

        LogFunc(("Calling vpoxDispIfUpdateModesWDDM to change target %d mode to (%d x %d)\n", iChangedMode, Size.cx, Size.cy));
        winEr = vpoxDispIfUpdateModesWDDM(&Op, iChangedMode, &Size);
    }

    winEr = vpoxDispIfResizePerform(pIf, iChangedMode, fEnable, fExtDispSup, paDisplayDevices, paDeviceModes, cDevModes);

    if (winEr == ERROR_RETRY)
    {
        VPoxRrRetrySchedule(pIf, iChangedMode, fEnable, fExtDispSup, paDisplayDevices, paDeviceModes, cDevModes);

        winEr = NO_ERROR;
    }

    vpoxDispIfOpEnd(&Op);

    return winEr;
}

static DWORD vpoxDispIfWddmEnableDisplays(PCVPOXDISPIF const pIf, UINT cIds, UINT *pIds, BOOL fEnabled, BOOL fSetTopology, DEVMODE *pDeviceMode)
{
    RT_NOREF(pIf);
    VPOXDISPIF_WDDM_DISPCFG DispCfg;

    DWORD winEr;
    int iPath;

    winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ONLY_ACTIVE_PATHS);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcCreate winEr %d\n", winEr));
        return winEr;
    }

    UINT cChangeIds = 0;
    UINT *pChangeIds = (UINT*)alloca(cIds * sizeof (*pChangeIds));
    if (!pChangeIds)
    {
        WARN(("VPoxTray: (WDDM) Failed to alloc change ids\n"));
        winEr = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (UINT i = 0; i < cIds; ++i)
    {
        UINT Id = pIds[i];
        bool fIsDup = false;
        for (UINT j = 0; j < cChangeIds; ++j)
        {
            if (pChangeIds[j] == Id)
            {
                fIsDup = true;
                break;
            }
        }

        if (fIsDup)
            continue;

        iPath = vpoxDispIfWddmDcSearchPath(&DispCfg, Id, Id);

        if (!((iPath >= 0) && (DispCfg.pPathInfoArray[iPath].flags & DISPLAYCONFIG_PATH_ACTIVE)) != !fEnabled)
        {
            pChangeIds[cChangeIds] = Id;
            ++cChangeIds;
        }
    }

    if (cChangeIds == 0)
    {
        Log(("VPoxTray: (WDDM) vpoxDispIfWddmEnableDisplay: settings are up to date\n"));
        winEr = ERROR_SUCCESS;
        goto done;
    }

    /* we want to set primary for every disabled for non-topoly mode only */
    winEr = vpoxDispIfWddmDcSettingsIncludeAllTargets(&DispCfg);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcSettingsIncludeAllTargets winEr %d\n", winEr));
        return winEr;
    }

    if (fSetTopology)
        vpoxDispIfWddmDcSettingsInvalidateModeIndeces(&DispCfg);

    for (UINT i = 0; i < cChangeIds; ++i)
    {
        UINT Id = pChangeIds[i];
        /* re-query paths */
        iPath = vpoxDispIfWddmDcSearchPath(&DispCfg, VPOX_WDDM_DC_SEARCH_PATH_ANY, Id);
        if (iPath < 0)
        {
            WARN(("VPoxTray: (WDDM) path index not found while it should"));
            winEr = ERROR_GEN_FAILURE;
            goto done;
        }

        winEr = vpoxDispIfWddmDcSettingsUpdate(&DispCfg, iPath, pDeviceMode, !fEnabled || fSetTopology, fEnabled);
        if (winEr != ERROR_SUCCESS)
        {
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcSettingsUpdate winEr %d\n", winEr));
            goto done;
        }
    }

    if (!fSetTopology)
        vpoxDispIfWddmDcSettingsAttachDisbledToPrimary(&DispCfg);

#if 0
    /* ensure the zero-index (primary) screen is enabled */
    iPath = vpoxDispIfWddmDcSearchPath(&DispCfg, 0, 0);
    if (iPath < 0)
    {
        WARN(("VPoxTray: (WDDM) path index not found while it should"));
        winEr = ERROR_GEN_FAILURE;
        goto done;
    }

    winEr = vpoxDispIfWddmDcSettingsUpdate(&DispCfg, iPath, /* just re-use device node here*/ pDeviceMode, fSetTopology, TRUE);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcSettingsUpdate winEr %d\n", winEr));
        goto done;
    }
#endif

    UINT fSetFlags = !fSetTopology ? (SDC_USE_SUPPLIED_DISPLAY_CONFIG) : (SDC_ALLOW_PATH_ORDER_CHANGES | SDC_TOPOLOGY_SUPPLIED);
    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_VALIDATE);
    if (winEr != ERROR_SUCCESS)
    {
        if (!fSetTopology)
        {
            WARN(("VPoxTray: (WDDM) vpoxDispIfWddmDcSet validation failed winEr, trying with changes %d\n", winEr));
            fSetFlags |= SDC_ALLOW_CHANGES;
        }
        else
        {
            Log(("VPoxTray: (WDDM) vpoxDispIfWddmDcSet topology validation failed winEr %d\n", winEr));
            goto done;
        }
    }

    if (!fSetTopology)
        fSetFlags |= SDC_SAVE_TO_DATABASE;

    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_APPLY);
    if (winEr != ERROR_SUCCESS)
        WARN(("VPoxTray: (WDDM) vpoxDispIfWddmDcSet apply failed winEr %d\n", winEr));

done:
    vpoxDispIfWddmDcTerm(&DispCfg);

    return winEr;
}

static DWORD vpoxDispIfWddmEnableDisplaysTryingTopology(PCVPOXDISPIF const pIf, UINT cIds, UINT *pIds, BOOL fEnable)
{
    DWORD winEr = vpoxDispIfWddmEnableDisplays(pIf, cIds, pIds, fEnable, FALSE, NULL);
    if (winEr != ERROR_SUCCESS)
    {
        if (fEnable)
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmEnableDisplay mode winEr %d\n", winEr));
        else
            Log(("VPoxTray: (WDDM) Failed vpoxDispIfWddmEnableDisplay mode winEr %d\n", winEr));
        winEr = vpoxDispIfWddmEnableDisplays(pIf, cIds, pIds, fEnable, TRUE, NULL);
        if (winEr != ERROR_SUCCESS)
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmEnableDisplay mode winEr %d\n", winEr));
    }

    return winEr;
}

BOOL VPoxDispIfResizeDisplayWin7(PCVPOXDISPIF const pIf, uint32_t cDispDef, const VMMDevDisplayDef *paDispDef)
{
    const VMMDevDisplayDef *pDispDef;
    uint32_t i;

    /* SetDisplayConfig assumes the top-left corner of a primary display at (0, 0) position */
    const VMMDevDisplayDef* pDispDefPrimary = NULL;

    for (i = 0; i < cDispDef; ++i)
    {
        pDispDef = &paDispDef[i];

        if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_PRIMARY)
        {
            pDispDefPrimary = pDispDef;
            break;
        }
    }

    VPOXDISPIF_OP Op;
    DWORD winEr = vpoxDispIfOpBegin(pIf, &Op);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: vpoxDispIfOpBegin failed winEr 0x%x", winEr));
        return (winEr == ERROR_SUCCESS);
    }

    for (i = 0; i < cDispDef; ++i)
    {
        pDispDef = &paDispDef[i];

        if (RT_BOOL(pDispDef->fDisplayFlags & VMMDEV_DISPLAY_DISABLED))
            continue;

        if (   RT_BOOL(pDispDef->fDisplayFlags & VMMDEV_DISPLAY_CX)
            && RT_BOOL(pDispDef->fDisplayFlags & VMMDEV_DISPLAY_CY))
        {
            RTRECTSIZE Size;
            Size.cx = pDispDef->cx;
            Size.cy = pDispDef->cy;

            winEr = vpoxDispIfUpdateModesWDDM(&Op, pDispDef->idDisplay, &Size);
            if (winEr != ERROR_SUCCESS)
                break;
        }
    }

    vpoxDispIfOpEnd(&Op);

    if (winEr != ERROR_SUCCESS)
        return (winEr == ERROR_SUCCESS);

    VPOXDISPIF_WDDM_DISPCFG DispCfg;
    winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ALL_PATHS);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: vpoxDispIfWddmDcCreate failed winEr 0x%x", winEr));
        return (winEr == ERROR_SUCCESS);
    }

    for (i = 0; i < cDispDef; ++i)
    {
        pDispDef = &paDispDef[i];

        /* Modify the path which the same source and target ids. */
        int const iPath = vpoxDispIfWddmDcSearchPath(&DispCfg, pDispDef->idDisplay, pDispDef->idDisplay);
        if (iPath < 0)
        {
            WARN(("VPoxTray:(WDDM) Unexpected iPath(%d) between src(%d) and tgt(%d)\n", iPath, pDispDef->idDisplay, pDispDef->idDisplay));
            continue;
        }

        /* If the source is used by another active path, then deactivate the path. */
        int const iActiveSrcPath = vpoxDispIfWddmDcSearchActiveSourcePath(&DispCfg, pDispDef->idDisplay);
        if (iActiveSrcPath >= 0 && iActiveSrcPath != iPath)
            DispCfg.pPathInfoArray[iActiveSrcPath].flags &= ~DISPLAYCONFIG_PATH_ACTIVE;

        DISPLAYCONFIG_PATH_INFO *pPathInfo = &DispCfg.pPathInfoArray[iPath];

        if (!(pDispDef->fDisplayFlags & VMMDEV_DISPLAY_DISABLED))
        {
            DISPLAYCONFIG_SOURCE_MODE *pSrcMode;
            DISPLAYCONFIG_TARGET_MODE *pTgtMode;

            if (pPathInfo->flags & DISPLAYCONFIG_PATH_ACTIVE)
            {
                UINT iSrcMode = pPathInfo->sourceInfo.modeInfoIdx;
                UINT iTgtMode = pPathInfo->targetInfo.modeInfoIdx;

                if (iSrcMode >= DispCfg.cModeInfoArray || iTgtMode >= DispCfg.cModeInfoArray)
                {
                    WARN(("VPoxTray:(WDDM) Unexpected iSrcMode(%d) and/or iTgtMode(%d)\n", iSrcMode, iTgtMode));
                    continue;
                }

                pSrcMode = &DispCfg.pModeInfoArray[iSrcMode].sourceMode;
                pTgtMode = &DispCfg.pModeInfoArray[iTgtMode].targetMode;

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_CX)
                {
                    pSrcMode->width =
                    pTgtMode->targetVideoSignalInfo.activeSize.cx =
                    pTgtMode->targetVideoSignalInfo.totalSize.cx = pDispDef->cx;
                }

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_CY)
                {
                    pSrcMode->height =
                    pTgtMode->targetVideoSignalInfo.activeSize.cy =
                    pTgtMode->targetVideoSignalInfo.totalSize.cy = pDispDef->cy;
                }

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_ORIGIN)
                {
                    pSrcMode->position.x = pDispDef->xOrigin - (pDispDefPrimary ? pDispDefPrimary->xOrigin : 0);
                    pSrcMode->position.y = pDispDef->yOrigin - (pDispDefPrimary ? pDispDefPrimary->yOrigin : 0);
                }

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_BPP)
                {
                    switch (pDispDef->cBitsPerPixel)
                    {
                    case 32:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                        break;
                    case 24:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_24BPP;
                        break;
                    case 16:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_16BPP;
                        break;
                    case 8:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_8BPP;
                        break;
                    default:
                        WARN(("VPoxTray: (WDDM) invalid bpp %d, using 32bpp instead\n", pDispDef->cBitsPerPixel));
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                        break;
                    }
                }
            }
            else
            {
                /* "The source and target modes for each source and target identifiers can only appear
                 * in the modeInfoArray array once."
                 * Try to find the source mode.
                 */
                DISPLAYCONFIG_MODE_INFO *pSrcModeInfo = NULL;
                int iSrcModeInfo = -1;
                for (UINT j = 0; j < DispCfg.cModeInfoArray; ++j)
                {
                    if (   DispCfg.pModeInfoArray[j].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE
                        && DispCfg.pModeInfoArray[j].id == pDispDef->idDisplay)
                    {
                        pSrcModeInfo = &DispCfg.pModeInfoArray[j];
                        iSrcModeInfo = (int)j;
                        break;
                    }
                }

                if (pSrcModeInfo == NULL)
                {
                    /* No mode yet. Add the new mode to the ModeInfo array. */
                    DISPLAYCONFIG_MODE_INFO *paModeInfo = (DISPLAYCONFIG_MODE_INFO *)realloc(DispCfg.pModeInfoArray, (DispCfg.cModeInfoArray + 1) * sizeof(DISPLAYCONFIG_MODE_INFO));
                    if (!paModeInfo)
                    {
                        WARN(("VPoxTray:(WDDM) Unable to re-allocate DispCfg.pModeInfoArray\n"));
                        continue;
                    }

                    DispCfg.pModeInfoArray = paModeInfo;
                    DispCfg.cModeInfoArray += 1;

                    iSrcModeInfo = DispCfg.cModeInfoArray - 1;
                    pSrcModeInfo = &DispCfg.pModeInfoArray[iSrcModeInfo];
                    RT_ZERO(*pSrcModeInfo);

                    pSrcModeInfo->infoType  = DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE;
                    pSrcModeInfo->id        = pDispDef->idDisplay;
                    pSrcModeInfo->adapterId = DispCfg.pModeInfoArray[0].adapterId;
                }

                /* Update the source mode information. */
                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_CX)
                {
                    pSrcModeInfo->sourceMode.width = pDispDef->cx;
                }

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_CY)
                {
                    pSrcModeInfo->sourceMode.height = pDispDef->cy;
                }

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_BPP)
                {
                    switch (pDispDef->cBitsPerPixel)
                    {
                        case 32:
                            pSrcModeInfo->sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                            break;
                        case 24:
                            pSrcModeInfo->sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_24BPP;
                            break;
                        case 16:
                            pSrcModeInfo->sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_16BPP;
                            break;
                        case 8:
                            pSrcModeInfo->sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_8BPP;
                            break;
                        default:
                            WARN(("VPoxTray: (WDDM) invalid bpp %d, using 32bpp instead\n", pDispDef->cBitsPerPixel));
                            pSrcModeInfo->sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                            break;
                    }
                }

                if (pDispDef->fDisplayFlags & VMMDEV_DISPLAY_ORIGIN)
                {
                    pSrcModeInfo->sourceMode.position.x = pDispDef->xOrigin - (pDispDefPrimary ? pDispDefPrimary->xOrigin : 0);
                    pSrcModeInfo->sourceMode.position.y = pDispDef->yOrigin - (pDispDefPrimary ? pDispDefPrimary->yOrigin : 0);
                }

                /* Configure the path information. */
                Assert(pPathInfo->sourceInfo.id == pDispDef->idDisplay);
                pPathInfo->sourceInfo.modeInfoIdx = iSrcModeInfo;

                Assert(pPathInfo->targetInfo.id == pDispDef->idDisplay);
                /* "If the index value is DISPLAYCONFIG_PATH_MODE_IDX_INVALID ..., this indicates
                 * the mode information is not being specified. It is valid for the path plus source mode ...
                 * information to be specified for a given path."
                 */
                pPathInfo->targetInfo.modeInfoIdx      = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
                pPathInfo->targetInfo.outputTechnology = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15;
                pPathInfo->targetInfo.rotation         = DISPLAYCONFIG_ROTATION_IDENTITY;
                pPathInfo->targetInfo.scaling          = DISPLAYCONFIG_SCALING_PREFERRED;
                /* "A refresh rate with both the numerator and denominator set to zero indicates that
                 * the caller does not specify a refresh rate and the operating system should use
                 * the most optimal refresh rate available. For this case, in a call to the SetDisplayConfig
                 * function, the caller must set the scanLineOrdering member to the
                 * DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED value; otherwise, SetDisplayConfig fails."
                 *
                 * If a refresh rate is set to a value, then the resize will fail if miniport driver
                 * does not support VSync, i.e. with display-only driver on Win8+ (@bugref{8440}).
                 */
                pPathInfo->targetInfo.refreshRate.Numerator   = 0;
                pPathInfo->targetInfo.refreshRate.Denominator = 0;
                pPathInfo->targetInfo.scanLineOrdering        = DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED;
                /* Make sure that "The output can be forced on this target even if a monitor is not detected." */
                pPathInfo->targetInfo.targetAvailable         = TRUE;
                pPathInfo->targetInfo.statusFlags             = DISPLAYCONFIG_TARGET_FORCIBLE;
            }

            pPathInfo->flags |= DISPLAYCONFIG_PATH_ACTIVE;
        }
        else
        {
            pPathInfo->flags &= ~DISPLAYCONFIG_PATH_ACTIVE;
        }
    }

    UINT fSetFlags = SDC_USE_SUPPLIED_DISPLAY_CONFIG;
    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_VALIDATE);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to VALIDATE winEr %d.\n", winEr));
        vpoxDispIfWddmDcLogRel(&DispCfg, fSetFlags);
        fSetFlags |= SDC_ALLOW_CHANGES;
    }

    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_SAVE_TO_DATABASE | SDC_APPLY);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to SET, winEr %d.\n", winEr));

        vpoxDispIfWddmDcSettingsInvalidateModeIndeces(&DispCfg);
        winEr = vpoxDispIfWddmDcSet(&DispCfg, SDC_TOPOLOGY_SUPPLIED | SDC_ALLOW_PATH_ORDER_CHANGES | SDC_APPLY);
        if (winEr != ERROR_SUCCESS)
        {
            WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to APPLY TOPOLOGY ONLY, winEr %d.\n", winEr));
            winEr = vpoxDispIfWddmDcSet(&DispCfg, SDC_USE_SUPPLIED_DISPLAY_CONFIG | SDC_APPLY);
            if (winEr != ERROR_SUCCESS)
            {
                WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to APPLY ANY TOPOLOGY, winEr %d.\n", winEr));
            }
        }
    }

    vpoxDispIfWddmDcTerm(&DispCfg);

    return (winEr == ERROR_SUCCESS);
}

static DWORD vpoxDispIfWddmResizeDisplay2(PCVPOXDISPIF const pIf, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT devModes)
{
    RT_NOREF(pIf, paDeviceModes);
    VPOXDISPIF_WDDM_DISPCFG DispCfg;
    DWORD winEr = ERROR_SUCCESS;
    UINT idx;
    int iPath;

    winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ALL_PATHS);

    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcCreate\n"));
        return winEr;
    }

    for (idx = 0; idx < devModes; idx++)
    {
        DEVMODE *pDeviceMode = &paDeviceModes[idx];

        if (paDisplayDevices[idx].StateFlags & DISPLAY_DEVICE_ACTIVE)
        {
            DISPLAYCONFIG_PATH_INFO *pPathInfo;

            iPath = vpoxDispIfWddmDcSearchPath(&DispCfg, idx, idx);

            if (iPath < 0)
            {
                WARN(("VPoxTray:(WDDM) Unexpected iPath(%d) between src(%d) and tgt(%d)\n", iPath, idx, idx));
                continue;
            }

            pPathInfo = &DispCfg.pPathInfoArray[iPath];

            if (pPathInfo->flags & DISPLAYCONFIG_PATH_ACTIVE)
            {
                UINT iSrcMode, iTgtMode;
                DISPLAYCONFIG_SOURCE_MODE *pSrcMode;
                DISPLAYCONFIG_TARGET_MODE *pTgtMode;

                iSrcMode = pPathInfo->sourceInfo.modeInfoIdx;
                iTgtMode = pPathInfo->targetInfo.modeInfoIdx;

                if (iSrcMode >= DispCfg.cModeInfoArray || iTgtMode >= DispCfg.cModeInfoArray)
                {
                    WARN(("VPoxTray:(WDDM) Unexpected iSrcMode(%d) and/or iTgtMode(%d)\n", iSrcMode, iTgtMode));
                    continue;
                }

                pSrcMode = &DispCfg.pModeInfoArray[iSrcMode].sourceMode;
                pTgtMode = &DispCfg.pModeInfoArray[iTgtMode].targetMode;

                if (pDeviceMode->dmFields & DM_PELSWIDTH)
                {
                    pSrcMode->width = pDeviceMode->dmPelsWidth;
                    pTgtMode->targetVideoSignalInfo.activeSize.cx = pDeviceMode->dmPelsWidth;
                    pTgtMode->targetVideoSignalInfo.totalSize.cx  = pDeviceMode->dmPelsWidth;
                }

                if (pDeviceMode->dmFields & DM_PELSHEIGHT)
                {
                    pSrcMode->height = pDeviceMode->dmPelsHeight;
                    pTgtMode->targetVideoSignalInfo.activeSize.cy = pDeviceMode->dmPelsHeight;
                    pTgtMode->targetVideoSignalInfo.totalSize.cy  = pDeviceMode->dmPelsHeight;
                }

                if (pDeviceMode->dmFields & DM_POSITION)
                {
                    pSrcMode->position.x = pDeviceMode->dmPosition.x;
                    pSrcMode->position.y = pDeviceMode->dmPosition.y;
                }

                if (pDeviceMode->dmFields & DM_BITSPERPEL)
                {
                    switch (pDeviceMode->dmBitsPerPel)
                    {
                    case 32:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                        break;
                    case 24:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_24BPP;
                        break;
                    case 16:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_16BPP;
                        break;
                    case 8:
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_8BPP;
                        break;
                    default:
                        LogRel(("VPoxTray: (WDDM) invalid bpp %d, using 32bpp instead\n", pDeviceMode->dmBitsPerPel));
                        pSrcMode->pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                        break;
                    }
                }
            }
            else
            {
                DISPLAYCONFIG_MODE_INFO *pModeInfo, *pModeInfoNew;

                pModeInfo = (DISPLAYCONFIG_MODE_INFO *)realloc(DispCfg.pModeInfoArray, (DispCfg.cModeInfoArray + 2) * sizeof(DISPLAYCONFIG_MODE_INFO));

                if (!pModeInfo)
                {
                    WARN(("VPoxTray:(WDDM) Unable to re-allocate DispCfg.pModeInfoArray\n"));
                    continue;
                }

                DispCfg.pModeInfoArray = pModeInfo;

                *pPathInfo = DispCfg.pPathInfoArray[0];
                pPathInfo->sourceInfo.id = idx;
                pPathInfo->targetInfo.id = idx;

                pModeInfoNew = &pModeInfo[DispCfg.cModeInfoArray];

                pModeInfoNew->infoType = DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE;
                pModeInfoNew->id = idx;
                pModeInfoNew->adapterId = pModeInfo[0].adapterId;
                pModeInfoNew->sourceMode.width  = pDeviceMode->dmPelsWidth;
                pModeInfoNew->sourceMode.height = pDeviceMode->dmPelsHeight;
                pModeInfoNew->sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
                pModeInfoNew->sourceMode.position.x = pDeviceMode->dmPosition.x;
                pModeInfoNew->sourceMode.position.y = pDeviceMode->dmPosition.y;
                pPathInfo->sourceInfo.modeInfoIdx = DispCfg.cModeInfoArray;

                pModeInfoNew++;
                pModeInfoNew->infoType = DISPLAYCONFIG_MODE_INFO_TYPE_TARGET;
                pModeInfoNew->id = idx;
                pModeInfoNew->adapterId = pModeInfo[0].adapterId;
                pModeInfoNew->targetMode = pModeInfo[0].targetMode;
                pModeInfoNew->targetMode.targetVideoSignalInfo.activeSize.cx = pDeviceMode->dmPelsWidth;
                pModeInfoNew->targetMode.targetVideoSignalInfo.totalSize.cx  = pDeviceMode->dmPelsWidth;
                pModeInfoNew->targetMode.targetVideoSignalInfo.activeSize.cy = pDeviceMode->dmPelsHeight;
                pModeInfoNew->targetMode.targetVideoSignalInfo.totalSize.cy  = pDeviceMode->dmPelsHeight;
                pPathInfo->targetInfo.modeInfoIdx = DispCfg.cModeInfoArray + 1;

                DispCfg.cModeInfoArray += 2;
            }
        }
        else
        {
            iPath = vpoxDispIfWddmDcSearchActivePath(&DispCfg, idx, idx);

            if (iPath >= 0)
            {
                DispCfg.pPathInfoArray[idx].flags &= ~DISPLAYCONFIG_PATH_ACTIVE;
            }
        }
    }

    UINT fSetFlags = SDC_USE_SUPPLIED_DISPLAY_CONFIG;
    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_VALIDATE);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to validate winEr %d.\n", winEr));
        fSetFlags |= SDC_ALLOW_CHANGES;
    }

    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_SAVE_TO_DATABASE | SDC_APPLY);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to validate winEr %d.\n", winEr));
    }

    vpoxDispIfWddmDcTerm(&DispCfg);

    return winEr;
}

static DWORD vpoxDispIfWddmResizeDisplay(PCVPOXDISPIF const pIf, UINT Id, BOOL fEnable, DISPLAY_DEVICE *paDisplayDevices,
                                         DEVMODE *paDeviceModes, UINT devModes)
{
    RT_NOREF(paDisplayDevices, devModes);
    VPOXDISPIF_WDDM_DISPCFG DispCfg;
    DWORD winEr;
    int iPath;

    winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ONLY_ACTIVE_PATHS);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcCreate\n"));
        return winEr;
    }

    iPath = vpoxDispIfWddmDcSearchActivePath(&DispCfg, Id, Id);

    if (iPath < 0)
    {
        vpoxDispIfWddmDcTerm(&DispCfg);

        if (!fEnable)
        {
            /* nothing to be done here, just leave */
            return ERROR_SUCCESS;
        }

        winEr = vpoxDispIfWddmEnableDisplaysTryingTopology(pIf, 1, &Id, fEnable);
        if (winEr != ERROR_SUCCESS)
        {
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmEnableDisplaysTryingTopology winEr %d\n", winEr));
            return winEr;
        }

        winEr = vpoxDispIfWddmDcCreate(&DispCfg, QDC_ONLY_ACTIVE_PATHS);
        if (winEr != ERROR_SUCCESS)
        {
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcCreate winEr %d\n", winEr));
            return winEr;
        }

        iPath = vpoxDispIfWddmDcSearchPath(&DispCfg, Id, Id);
        if (iPath < 0)
        {
            WARN(("VPoxTray: (WDDM) path (%d) is still disabled, going to retry winEr %d\n", winEr));
            vpoxDispIfWddmDcTerm(&DispCfg);
            return ERROR_RETRY;
        }
    }

    Assert(iPath >= 0);

    if (!fEnable)
    {
        /* need to disable it, and we are done */
        vpoxDispIfWddmDcTerm(&DispCfg);

        winEr = vpoxDispIfWddmEnableDisplaysTryingTopology(pIf, 1, &Id, fEnable);
        if (winEr != ERROR_SUCCESS)
        {
            WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmEnableDisplaysTryingTopology winEr %d\n", winEr));
            return winEr;
        }

        return winEr;
    }

    Assert(fEnable);

    winEr = vpoxDispIfWddmDcSettingsUpdate(&DispCfg, iPath, &paDeviceModes[Id], FALSE, fEnable);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray: (WDDM) Failed vpoxDispIfWddmDcSettingsUpdate\n"));
        vpoxDispIfWddmDcTerm(&DispCfg);
        return winEr;
    }

    UINT fSetFlags = SDC_USE_SUPPLIED_DISPLAY_CONFIG;
    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_VALIDATE);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to validate winEr %d.\n", winEr));
        fSetFlags |= SDC_ALLOW_CHANGES;
    }

    winEr = vpoxDispIfWddmDcSet(&DispCfg, fSetFlags | SDC_SAVE_TO_DATABASE | SDC_APPLY);
    if (winEr != ERROR_SUCCESS)
    {
        WARN(("VPoxTray:(WDDM) pfnSetDisplayConfig Failed to validate winEr %d.\n", winEr));
    }

    vpoxDispIfWddmDcTerm(&DispCfg);

    return winEr;
}

#endif /* VPOX_WITH_WDDM */

DWORD VPoxDispIfResizeModes(PCVPOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
            return ERROR_NOT_SUPPORTED;
        case VPOXDISPIF_MODE_XPDM:
            return ERROR_NOT_SUPPORTED;
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfResizeModesWDDM(pIf, iChangedMode, fEnable, fExtDispSup, paDisplayDevices, paDeviceModes, cDevModes);
#endif
        default:
            WARN(("unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

DWORD VPoxDispIfCancelPendingResize(PCVPOXDISPIF const pIf)
{
    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
            return NO_ERROR;
        case VPOXDISPIF_MODE_XPDM:
            return NO_ERROR;
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfCancelPendingResizeWDDM(pIf);
#endif
        default:
            WARN(("unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

static DWORD vpoxDispIfConfigureTargetsWDDM(VPOXDISPIF_OP *pOp, uint32_t *pcConnected)
{
    VPOXDISPIFESCAPE EscapeHdr = {0};
    EscapeHdr.escapeCode = VPOXESC_CONFIGURETARGETS;
    EscapeHdr.u32CmdSpecific = 0;

    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = pOp->Adapter.hAdapter;
#ifdef VPOX_DISPIF_WITH_OPCONTEXT
    /* win8.1 does not allow context-based escapes for display-only mode */
    EscapeData.hDevice = pOp->Device.hDevice;
    EscapeData.hContext = pOp->Context.hContext;
#endif
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags.HardwareAccess = 1;
    EscapeData.pPrivateDriverData = &EscapeHdr;
    EscapeData.PrivateDriverDataSize = sizeof (EscapeHdr);

    NTSTATUS Status = pOp->pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
    if (NT_SUCCESS(Status))
    {
        if (pcConnected)
            *pcConnected = EscapeHdr.u32CmdSpecific;
        return NO_ERROR;
    }
    WARN(("VPoxTray: pfnD3DKMTEscape VPOXESC_CONFIGURETARGETS failed Status 0x%x\n", Status));
    return Status;
}

static DWORD vpoxDispIfResizeStartedWDDMOp(VPOXDISPIF_OP *pOp)
{
    DWORD NumDevices = VPoxDisplayGetCount();
    if (NumDevices == 0)
    {
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: Zero devices found\n"));
        return ERROR_GEN_FAILURE;
    }

    DISPLAY_DEVICE *paDisplayDevices = (DISPLAY_DEVICE *)alloca (sizeof (DISPLAY_DEVICE) * NumDevices);
    DEVMODE *paDeviceModes = (DEVMODE *)alloca (sizeof (DEVMODE) * NumDevices);
    DWORD DevNum = 0;
    DWORD DevPrimaryNum = 0;

    DWORD winEr = VPoxDisplayGetConfig(NumDevices, &DevPrimaryNum, &DevNum, paDisplayDevices, paDeviceModes);
    if (winEr != NO_ERROR)
    {
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: VPoxGetDisplayConfig failed, %d\n", winEr));
        return winEr;
    }

    if (NumDevices != DevNum)
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: NumDevices(%d) != DevNum(%d)\n", NumDevices, DevNum));


    uint32_t cConnected = 0;
    winEr = vpoxDispIfConfigureTargetsWDDM(pOp, &cConnected);
    if (winEr != NO_ERROR)
    {
        WARN(("VPoxTray: vpoxDispIfConfigureTargetsWDDM failed winEr 0x%x\n", winEr));
        return winEr;
    }

    if (!cConnected)
    {
        Log(("VPoxTray: all targets already connected, nothing to do\n"));
        return NO_ERROR;
    }

    winEr = vpoxDispIfWaitDisplayDataInited(pOp);
    if (winEr != NO_ERROR)
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: vpoxDispIfWaitDisplayDataInited failed winEr 0x%x\n", winEr));

    DWORD NewNumDevices = VPoxDisplayGetCount();
    if (NewNumDevices == 0)
    {
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: Zero devices found\n"));
        return ERROR_GEN_FAILURE;
    }

    if (NewNumDevices != NumDevices)
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: NumDevices(%d) != NewNumDevices(%d)\n", NumDevices, NewNumDevices));

    DISPLAY_DEVICE *paNewDisplayDevices = (DISPLAY_DEVICE *)alloca (sizeof (DISPLAY_DEVICE) * NewNumDevices);
    DEVMODE *paNewDeviceModes = (DEVMODE *)alloca (sizeof (DEVMODE) * NewNumDevices);
    DWORD NewDevNum = 0;
    DWORD NewDevPrimaryNum = 0;

    winEr = VPoxDisplayGetConfig(NewNumDevices, &NewDevPrimaryNum, &NewDevNum, paNewDisplayDevices, paNewDeviceModes);
    if (winEr != NO_ERROR)
    {
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: VPoxGetDisplayConfig failed for new devices, %d\n", winEr));
        return winEr;
    }

    if (NewNumDevices != NewDevNum)
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: NewNumDevices(%d) != NewDevNum(%d)\n", NewNumDevices, NewDevNum));

    DWORD minDevNum = RT_MIN(DevNum, NewDevNum);
    UINT *pIds = (UINT*)alloca (sizeof (UINT) * minDevNum);
    UINT cIds = 0;
    for (DWORD i = 0; i < minDevNum; ++i)
    {
        if ((paNewDisplayDevices[i].StateFlags & DISPLAY_DEVICE_ACTIVE)
                && !(paDisplayDevices[i].StateFlags & DISPLAY_DEVICE_ACTIVE))
        {
            pIds[cIds] = i;
            ++cIds;
        }
    }

    if (!cIds)
    {
        /* this is something we would not regularly expect */
        WARN(("VPoxTray: all targets already have proper config, nothing to do\n"));
        return NO_ERROR;
    }

    if (pOp->pIf->enmMode > VPOXDISPIF_MODE_WDDM)
    {
        winEr = vpoxDispIfWddmEnableDisplaysTryingTopology(pOp->pIf, cIds, pIds, FALSE);
        if (winEr != NO_ERROR)
            WARN(("VPoxTray: vpoxDispIfWddmEnableDisplaysTryingTopology failed to record current settings, %d, ignoring\n", winEr));
    }
    else
    {
        for (DWORD i = 0; i < cIds; ++i)
        {
            winEr = vpoxDispIfWddmResizeDisplayVista(paNewDeviceModes, paNewDisplayDevices, NewDevNum, i, FALSE, TRUE);
            if (winEr != NO_ERROR)
                WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp: vpoxDispIfWddmResizeDisplayVista failed winEr 0x%x\n", winEr));
        }
    }

    return winEr;
}


static DWORD vpoxDispIfResizeStartedWDDM(PCVPOXDISPIF const pIf)
{
    VPOXDISPIF_OP Op;

    DWORD winEr = vpoxDispIfOpBegin(pIf, &Op);
    if (winEr != NO_ERROR)
    {
        WARN(("VPoxTray: vpoxDispIfOpBegin failed winEr 0x%x\n", winEr));
        return winEr;
    }

    winEr = vpoxDispIfResizeStartedWDDMOp(&Op);
    if (winEr != NO_ERROR)
    {
        WARN(("VPoxTray: vpoxDispIfResizeStartedWDDMOp failed winEr 0x%x\n", winEr));
    }

    vpoxDispIfOpEnd(&Op);

    return winEr;
}

DWORD VPoxDispIfResizeStarted(PCVPOXDISPIF const pIf)
{
    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
            return NO_ERROR;
        case VPOXDISPIF_MODE_XPDM:
            return NO_ERROR;
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfResizeStartedWDDM(pIf);
#endif
        default:
            WARN(("unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

static DWORD vpoxDispIfSwitchToXPDM_NT4(PVPOXDISPIF pIf)
{
    RT_NOREF(pIf);
    return NO_ERROR;
}

static DWORD vpoxDispIfSwitchToXPDM(PVPOXDISPIF pIf)
{
    DWORD err = NO_ERROR;

    uint64_t const uNtVersion = RTSystemGetNtVersion();
    if (uNtVersion >= RTSYSTEM_MAKE_NT_VERSION(5, 0, 0))
    {
        HMODULE hUser = GetModuleHandle("user32.dll");
        if (NULL != hUser)
        {
            *(uintptr_t *)&pIf->modeData.xpdm.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
            LogFunc(("pfnChangeDisplaySettingsEx = %p\n", pIf->modeData.xpdm.pfnChangeDisplaySettingsEx));
            bool const fSupported = RT_BOOL(pIf->modeData.xpdm.pfnChangeDisplaySettingsEx);
            if (!fSupported)
            {
                WARN(("pfnChangeDisplaySettingsEx function pointer failed to initialize\n"));
                err = ERROR_NOT_SUPPORTED;
            }
        }
        else
        {
            WARN(("failed to get USER32 handle, err (%d)\n", GetLastError()));
            err = ERROR_NOT_SUPPORTED;
        }
    }
    else
    {
        WARN(("can not switch to VPOXDISPIF_MODE_XPDM, because os is not >= w2k\n"));
        err = ERROR_NOT_SUPPORTED;
    }

    return err;
}

DWORD VPoxDispIfSwitchMode(PVPOXDISPIF pIf, VPOXDISPIF_MODE enmMode, VPOXDISPIF_MODE *penmOldMode)
{
    /** @todo may need to addd synchronization in case we want to change modes dynamically
     * i.e. currently the mode is supposed to be initialized once on service initialization */
    if (penmOldMode)
        *penmOldMode = pIf->enmMode;

    if (enmMode == pIf->enmMode)
        return NO_ERROR;

    /* Make sure that we never try to run anything else but VPOXDISPIF_MODE_XPDM_NT4 on NT4 guests.
     * Anything else will get us into serious trouble. */
    if (RTSystemGetNtVersion() < RTSYSTEM_MAKE_NT_VERSION(5, 0, 0))
        enmMode = VPOXDISPIF_MODE_XPDM_NT4;

#ifdef VPOX_WITH_WDDM
    if (pIf->enmMode >= VPOXDISPIF_MODE_WDDM)
    {
        vpoxDispIfWddmTerm(pIf);

        vpoxDispKmtCallbacksTerm(&pIf->modeData.wddm.KmtCallbacks);
    }
#endif

    DWORD err = NO_ERROR;
    switch (enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
            LogFunc(("request to switch to VPOXDISPIF_MODE_XPDM_NT4\n"));
            err = vpoxDispIfSwitchToXPDM_NT4(pIf);
            if (err == NO_ERROR)
            {
                LogFunc(("successfully switched to XPDM_NT4 mode\n"));
                pIf->enmMode = VPOXDISPIF_MODE_XPDM_NT4;
            }
            else
                WARN(("failed to switch to XPDM_NT4 mode, err (%d)\n", err));
            break;
        case VPOXDISPIF_MODE_XPDM:
            LogFunc(("request to switch to VPOXDISPIF_MODE_XPDM\n"));
            err = vpoxDispIfSwitchToXPDM(pIf);
            if (err == NO_ERROR)
            {
                LogFunc(("successfully switched to XPDM mode\n"));
                pIf->enmMode = VPOXDISPIF_MODE_XPDM;
            }
            else
                WARN(("failed to switch to XPDM mode, err (%d)\n", err));
            break;
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        {
            LogFunc(("request to switch to VPOXDISPIF_MODE_WDDM\n"));
            err = vpoxDispIfSwitchToWDDM(pIf);
            if (err == NO_ERROR)
            {
                LogFunc(("successfully switched to WDDM mode\n"));
                pIf->enmMode = VPOXDISPIF_MODE_WDDM;
            }
            else
                WARN(("failed to switch to WDDM mode, err (%d)\n", err));
            break;
        }
        case VPOXDISPIF_MODE_WDDM_W7:
        {
            LogFunc(("request to switch to VPOXDISPIF_MODE_WDDM_W7\n"));
            err = vpoxDispIfSwitchToWDDM_W7(pIf);
            if (err == NO_ERROR)
            {
                LogFunc(("successfully switched to WDDM mode\n"));
                pIf->enmMode = VPOXDISPIF_MODE_WDDM_W7;
            }
            else
                WARN(("failed to switch to WDDM mode, err (%d)\n", err));
            break;
        }
#endif
        default:
            err = ERROR_INVALID_PARAMETER;
            break;
    }
    return err;
}

static DWORD vpoxDispIfSeamlessCreateWDDM(PCVPOXDISPIF const pIf, VPOXDISPIF_SEAMLESS *pSeamless, HANDLE hEvent)
{
    RT_NOREF(hEvent);
    HRESULT hr = vpoxDispKmtOpenAdapter(&pIf->modeData.wddm.KmtCallbacks, &pSeamless->modeData.wddm.Adapter);
    if (SUCCEEDED(hr))
    {
#ifndef VPOX_DISPIF_WITH_OPCONTEXT
        return ERROR_SUCCESS;
#else
        hr = vpoxDispKmtCreateDevice(&pSeamless->modeData.wddm.Adapter, &pSeamless->modeData.wddm.Device);
        if (SUCCEEDED(hr))
        {
            hr = vpoxDispKmtCreateContext(&pSeamless->modeData.wddm.Device, &pSeamless->modeData.wddm.Context, VPOXWDDM_CONTEXT_TYPE_CUSTOM_DISPIF_SEAMLESS,
                    hEvent, 0ULL);
            if (SUCCEEDED(hr))
                return ERROR_SUCCESS;
            WARN(("VPoxTray: vpoxDispKmtCreateContext failed hr 0x%x", hr));

            vpoxDispKmtDestroyDevice(&pSeamless->modeData.wddm.Device);
        }
        else
            WARN(("VPoxTray: vpoxDispKmtCreateDevice failed hr 0x%x", hr));

        vpoxDispKmtCloseAdapter(&pSeamless->modeData.wddm.Adapter);
#endif /* VPOX_DISPIF_WITH_OPCONTEXT */
    }

    return hr;
}

static DWORD vpoxDispIfSeamlessTermWDDM(VPOXDISPIF_SEAMLESS *pSeamless)
{
#ifdef VPOX_DISPIF_WITH_OPCONTEXT
    vpoxDispKmtDestroyContext(&pSeamless->modeData.wddm.Context);
    vpoxDispKmtDestroyDevice(&pSeamless->modeData.wddm.Device);
#endif
    vpoxDispKmtCloseAdapter(&pSeamless->modeData.wddm.Adapter);

    return NO_ERROR;
}

static DWORD vpoxDispIfSeamlessSubmitWDDM(VPOXDISPIF_SEAMLESS *pSeamless, VPOXDISPIFESCAPE *pData, int cbData)
{
    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = pSeamless->modeData.wddm.Adapter.hAdapter;
#ifdef VPOX_DISPIF_WITH_OPCONTEXT
    EscapeData.hDevice = pSeamless->modeData.wddm.Device.hDevice;
    EscapeData.hContext = pSeamless->modeData.wddm.Context.hContext;
#endif
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    /*EscapeData.Flags.HardwareAccess = 1;*/
    EscapeData.pPrivateDriverData = pData;
    EscapeData.PrivateDriverDataSize = VPOXDISPIFESCAPE_SIZE(cbData);

    NTSTATUS Status = pSeamless->pIf->modeData.wddm.KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
    if (NT_SUCCESS(Status))
        return ERROR_SUCCESS;

    WARN(("VPoxTray: pfnD3DKMTEscape Seamless failed Status 0x%x\n", Status));
    return Status;
}

DWORD VPoxDispIfSeamlessCreate(PCVPOXDISPIF const pIf, VPOXDISPIF_SEAMLESS *pSeamless, HANDLE hEvent)
{
    memset(pSeamless, 0, sizeof (*pSeamless));
    pSeamless->pIf = pIf;

    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
        case VPOXDISPIF_MODE_XPDM:
            return NO_ERROR;
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfSeamlessCreateWDDM(pIf, pSeamless, hEvent);
#endif
        default:
            break;
    }

    WARN(("VPoxTray: VPoxDispIfSeamlessCreate: invalid mode %d\n", pIf->enmMode));
    return ERROR_INVALID_PARAMETER;
}

DWORD VPoxDispIfSeamlessTerm(VPOXDISPIF_SEAMLESS *pSeamless)
{
    PCVPOXDISPIF const pIf = pSeamless->pIf;
    DWORD winEr;
    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
        case VPOXDISPIF_MODE_XPDM:
            winEr = NO_ERROR;
            break;
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            winEr = vpoxDispIfSeamlessTermWDDM(pSeamless);
            break;
#endif
        default:
            WARN(("VPoxTray: VPoxDispIfSeamlessTerm: invalid mode %d\n", pIf->enmMode));
            winEr = ERROR_INVALID_PARAMETER;
            break;
    }

    if (winEr == NO_ERROR)
        memset(pSeamless, 0, sizeof (*pSeamless));

    return winEr;
}

DWORD VPoxDispIfSeamlessSubmit(VPOXDISPIF_SEAMLESS *pSeamless, VPOXDISPIFESCAPE *pData, int cbData)
{
    PCVPOXDISPIF const pIf = pSeamless->pIf;

    if (pData->escapeCode != VPOXESC_SETVISIBLEREGION)
    {
        WARN(("VPoxTray: invalid escape code for Seamless submit %d\n", pData->escapeCode));
        return ERROR_INVALID_PARAMETER;
    }

    switch (pIf->enmMode)
    {
        case VPOXDISPIF_MODE_XPDM_NT4:
        case VPOXDISPIF_MODE_XPDM:
            return VPoxDispIfEscape(pIf, pData, cbData);
#ifdef VPOX_WITH_WDDM
        case VPOXDISPIF_MODE_WDDM:
        case VPOXDISPIF_MODE_WDDM_W7:
            return vpoxDispIfSeamlessSubmitWDDM(pSeamless, pData, cbData);
#endif
        default:
            WARN(("VPoxTray: VPoxDispIfSeamlessSubmit: invalid mode %d\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

