/* $Id: vbva.c $ */
/** @file
 * VirtualPox X11 Additions graphics driver 2D acceleration functions
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(IN_XF86_MODULE) && !defined(NO_ANSIC)
# include "xf86_ansic.h"
#endif
#include "compiler.h"

#include "vpoxvideo.h"

#ifdef XORG_7X
# include <stdlib.h>
# include <string.h>
#endif

/**************************************************************************
* Main functions                                                          *
**************************************************************************/

/**
 * Callback function called by the X server to tell us about dirty
 * rectangles in the video buffer.
 *
 * @param pScrn   pointer to the information structure for the current
 *                screen
 * @param iRects  Number of dirty rectangles to update
 * @param aRects  Array of structures containing the coordinates of the
 *                rectangles
 */
void vbvxHandleDirtyRect(ScrnInfoPtr pScrn, int iRects, BoxPtr aRects)
{
    VBVACMDHDR cmdHdr;
    VPOXPtr pVPox;
    int i;
    unsigned j;

    pVPox = pScrn->driverPrivate;
    if (!pScrn->vtSema)
        return;

    for (j = 0; j < pVPox->cScreens; ++j)
    {
        /* Just continue quietly if VBVA is not currently active. */
        struct VBVABUFFER *pVBVA = pVPox->pScreens[j].aVbvaCtx.pVBVA;
        if (   !pVBVA
            || !(pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_ENABLED))
            continue;
        for (i = 0; i < iRects; ++i)
        {
            if (   aRects[i].x1 >   pVPox->pScreens[j].aScreenLocation.x
                                  + pVPox->pScreens[j].aScreenLocation.cx
                || aRects[i].y1 >   pVPox->pScreens[j].aScreenLocation.y
                                  + pVPox->pScreens[j].aScreenLocation.cy
                || aRects[i].x2 <   pVPox->pScreens[j].aScreenLocation.x
                || aRects[i].y2 <   pVPox->pScreens[j].aScreenLocation.y)
                continue;
            cmdHdr.x = (int16_t)aRects[i].x1 - pVPox->pScreens[0].aScreenLocation.x;
            cmdHdr.y = (int16_t)aRects[i].y1 - pVPox->pScreens[0].aScreenLocation.y;
            cmdHdr.w = (uint16_t)(aRects[i].x2 - aRects[i].x1);
            cmdHdr.h = (uint16_t)(aRects[i].y2 - aRects[i].y1);

#if 0
            TRACE_LOG("display=%u, x=%d, y=%d, w=%d, h=%d\n",
                      j, cmdHdr.x, cmdHdr.y, cmdHdr.w, cmdHdr.h);
#endif

            if (VPoxVBVABufferBeginUpdate(&pVPox->pScreens[j].aVbvaCtx,
                                          &pVPox->guestCtx))
            {
                VPoxVBVAWrite(&pVPox->pScreens[j].aVbvaCtx, &pVPox->guestCtx, &cmdHdr,
                              sizeof(cmdHdr));
                VPoxVBVABufferEndUpdate(&pVPox->pScreens[j].aVbvaCtx);
            }
        }
    }
}

static DECLCALLBACK(void *) hgsmiEnvAlloc(void *pvEnv, HGSMISIZE cb)
{
    RT_NOREF(pvEnv);
    return calloc(1, cb);
}

static DECLCALLBACK(void) hgsmiEnvFree(void *pvEnv, void *pv)
{
    RT_NOREF(pvEnv);
    free(pv);
}

static HGSMIENV g_hgsmiEnv =
{
    NULL,
    hgsmiEnvAlloc,
    hgsmiEnvFree
};

/**
 * Calculate the location in video RAM of and initialise the heap for guest to
 * host messages.
 */
void vbvxSetUpHGSMIHeapInGuest(VPOXPtr pVPox, uint32_t cbVRAM)
{
    int rc;
    uint32_t offVRAMBaseMapping, offGuestHeapMemory, cbGuestHeapMemory;
    void *pvGuestHeapMemory;

    VPoxHGSMIGetBaseMappingInfo(cbVRAM, &offVRAMBaseMapping, NULL, &offGuestHeapMemory, &cbGuestHeapMemory, NULL);
    pvGuestHeapMemory = ((uint8_t *)pVPox->base) + offVRAMBaseMapping + offGuestHeapMemory;
    rc = VPoxHGSMISetupGuestContext(&pVPox->guestCtx, pvGuestHeapMemory, cbGuestHeapMemory,
                                    offVRAMBaseMapping + offGuestHeapMemory, &g_hgsmiEnv);
    AssertMsg(RT_SUCCESS(rc), ("Failed to set up the guest-to-host message buffer heap, rc=%d\n", rc));
    pVPox->cbView = offVRAMBaseMapping;
}

/** Callback to fill in the view structures */
static DECLCALLBACK(int) vpoxFillViewInfo(void *pvVPox, struct VBVAINFOVIEW *pViews, uint32_t cViews)
{
    VPOXPtr pVPox = (VPOXPtr)pvVPox;
    unsigned i;
    for (i = 0; i < cViews; ++i)
    {
        pViews[i].u32ViewIndex = i;
        pViews[i].u32ViewOffset = 0;
        pViews[i].u32ViewSize = pVPox->cbView;
        pViews[i].u32MaxScreenSize = pVPox->cbFBMax;
    }
    return VINF_SUCCESS;
}

/**
 * Initialise VirtualPox's accelerated video extensions.
 *
 * @returns TRUE on success, FALSE on failure
 */
static Bool vpoxSetupVRAMVbva(VPOXPtr pVPox)
{
    int rc = VINF_SUCCESS;
    unsigned i;

    pVPox->cbFBMax = pVPox->cbView;
    for (i = 0; i < pVPox->cScreens; ++i)
    {
        pVPox->cbFBMax -= VBVA_MIN_BUFFER_SIZE;
        pVPox->pScreens[i].aoffVBVABuffer = pVPox->cbFBMax;
        TRACE_LOG("VBVA buffer offset for screen %u: 0x%lx\n", i,
                  (unsigned long) pVPox->cbFBMax);
        VPoxVBVASetupBufferContext(&pVPox->pScreens[i].aVbvaCtx,
                                   pVPox->pScreens[i].aoffVBVABuffer,
                                   VBVA_MIN_BUFFER_SIZE);
    }
    TRACE_LOG("Maximum framebuffer size: %lu (0x%lx)\n",
              (unsigned long) pVPox->cbFBMax,
              (unsigned long) pVPox->cbFBMax);
    rc = VPoxHGSMISendViewInfo(&pVPox->guestCtx, pVPox->cScreens,
                               vpoxFillViewInfo, (void *)pVPox);
    AssertMsg(RT_SUCCESS(rc), ("Failed to send the view information to the host, rc=%d\n", rc));
    return TRUE;
}

static Bool haveHGSMIModeHintAndCursorReportingInterface(VPOXPtr pVPox)
{
    uint32_t fModeHintReporting, fCursorReporting;

    return    RT_SUCCESS(VPoxQueryConfHGSMI(&pVPox->guestCtx, VPOX_VBVA_CONF32_MODE_HINT_REPORTING, &fModeHintReporting))
           && RT_SUCCESS(VPoxQueryConfHGSMI(&pVPox->guestCtx, VPOX_VBVA_CONF32_GUEST_CURSOR_REPORTING, &fCursorReporting))
           && fModeHintReporting == VINF_SUCCESS
           && fCursorReporting == VINF_SUCCESS;
}

static Bool hostHasScreenBlankingFlag(VPOXPtr pVPox)
{
    uint32_t fScreenFlags;

    return    RT_SUCCESS(VPoxQueryConfHGSMI(&pVPox->guestCtx, VPOX_VBVA_CONF32_SCREEN_FLAGS, &fScreenFlags))
           && fScreenFlags & VBVA_SCREEN_F_BLANK;
}

/**
 * Inform VPox that we will supply it with dirty rectangle information
 * and install the dirty rectangle handler.
 *
 * @returns TRUE for success, FALSE for failure
 * @param   pScrn   Pointer to a structure describing the X screen in use
 */
Bool
vpoxEnableVbva(ScrnInfoPtr pScrn)
{
    Bool rc = TRUE;
    unsigned i;
    VPOXPtr pVPox = pScrn->driverPrivate;

    TRACE_ENTRY();
    if (!vpoxSetupVRAMVbva(pVPox))
        return FALSE;
    for (i = 0; i < pVPox->cScreens; ++i)
    {
        struct VBVABUFFER *pVBVA;

        pVBVA = (struct VBVABUFFER *) (  ((uint8_t *)pVPox->base)
                                       + pVPox->pScreens[i].aoffVBVABuffer);
        if (!VPoxVBVAEnable(&pVPox->pScreens[i].aVbvaCtx, &pVPox->guestCtx,
                            pVBVA, i))
            rc = FALSE;
    }
    AssertMsg(rc, ("Failed to enable screen update reporting for at least one virtual monitor.\n"));
    pVPox->fHaveHGSMIModeHints = haveHGSMIModeHintAndCursorReportingInterface(pVPox);
    pVPox->fHostHasScreenBlankingFlag = hostHasScreenBlankingFlag(pVPox);
    return rc;
}

/**
 * Inform VPox that we will stop supplying it with dirty rectangle
 * information. This function is intended to be called when an X
 * virtual terminal is disabled, or the X server is terminated.
 *
 * @returns TRUE for success, FALSE for failure
 * @param   pScrn   Pointer to a structure describing the X screen in use
 */
void
vpoxDisableVbva(ScrnInfoPtr pScrn)
{
    unsigned i;
    VPOXPtr pVPox = pScrn->driverPrivate;

    TRACE_ENTRY();
    for (i = 0; i < pVPox->cScreens; ++i)
        VPoxVBVADisable(&pVPox->pScreens[i].aVbvaCtx, &pVPox->guestCtx, i);
}
