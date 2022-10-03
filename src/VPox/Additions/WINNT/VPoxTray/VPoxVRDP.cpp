/* $Id: VPoxVRDP.cpp $ */
/** @file
 * VPoxVRDP - VPox VRDP connection notification
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

/* 0x0501 for SPI_SETDROPSHADOW */
#define _WIN32_WINNT 0x0501
#include <iprt/win/windows.h>
#include "VPoxTray.h"
#include "VPoxHelpers.h"
#include "VPoxVRDP.h"

#include <VPox/VMMDev.h> /* for VMMDEV_EVENT_VRDP and VRDP_EXPERIENCE_LEVEL_XXX */
#ifdef DEBUG
# define LOG_ENABLED
# define LOG_GROUP LOG_GROUP_DEFAULT
#endif
#include <VPox/log.h>

#include <iprt/assert.h>
#include <iprt/ldr.h>



/* The guest receives VRDP_ACTIVE/VRDP_INACTIVE notifications.
 *
 * When VRDP_ACTIVE is received, the guest asks host about the experience level.
 * The experience level is an integer value, different values disable some GUI effects.
 *
 * On VRDP_INACTIVE the original values are restored.
 *
 * Note: that this is not controlled from the client, that is a per VM settings.
 *
 * Note: theming is disabled separately by EnableTheming.
 */

#define VPOX_SPI_STRING   0
#define VPOX_SPI_BOOL_PTR 1
#define VPOX_SPI_BOOL     2
#define VPOX_SPI_PTR      3

static ANIMATIONINFO animationInfoDisable =
{
    sizeof (ANIMATIONINFO),
    FALSE
};

typedef struct _VPOXVRDPEXPPARAM
{
    const char *name;
    UINT  uActionSet;
    UINT  uActionGet;
    uint32_t level;                    /* The parameter remain enabled at this or higher level. */
    int   type;
    void  *pvDisable;
    UINT  cbSavedValue;
    char  achSavedValue[2 * MAX_PATH]; /* Large enough to save the bitmap path. */
} VPOXVRDPEXPPARAM, *PVPOXVRDPEXPPARAM;

typedef struct _VPOXVRDPCONTEXT
{
    const VPOXSERVICEENV *pEnv;

    uint32_t level;
    BOOL fSavedThemeEnabled;

    RTLDRMOD hModUxTheme;

    HRESULT (* pfnEnableTheming)(BOOL fEnable);
    BOOL (* pfnIsThemeActive)(VOID);
} VPOXVRDPCONTEXT, *PVPOXVRDPCONTEXT;

static VPOXVRDPCONTEXT g_Ctx = { 0 };

#define SPI_(l, a) #a, SPI_SET##a, SPI_GET##a, VRDP_EXPERIENCE_LEVEL_##l

static VPOXVRDPEXPPARAM s_aSPIParams[] =
{
    { SPI_(MEDIUM, DESKWALLPAPER),           VPOX_SPI_STRING,   "" },
    { SPI_(FULL,   DROPSHADOW),              VPOX_SPI_BOOL_PTR,       },
    { SPI_(HIGH,   FONTSMOOTHING),           VPOX_SPI_BOOL,           },
    { SPI_(FULL,   MENUFADE),                VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   COMBOBOXANIMATION),       VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   CURSORSHADOW),            VPOX_SPI_BOOL_PTR,       },
    { SPI_(HIGH,   GRADIENTCAPTIONS),        VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   LISTBOXSMOOTHSCROLLING),  VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   MENUANIMATION),           VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   SELECTIONFADE),           VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   TOOLTIPANIMATION),        VPOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   ANIMATION),               VPOX_SPI_PTR,      &animationInfoDisable, sizeof (ANIMATIONINFO) },
    { SPI_(MEDIUM, DRAGFULLWINDOWS),         VPOX_SPI_BOOL,           }
};

#undef SPI_

static void vpoxExperienceSet(uint32_t uLevel)
{
    for (size_t i = 0; i < RT_ELEMENTS(s_aSPIParams); i++)
    {
        PVPOXVRDPEXPPARAM pParam = &s_aSPIParams[i];
        if (pParam->level > uLevel)
        {
            /*
             * The parameter has to be disabled.
             */
            LogFlowFunc(("Saving %s\n", pParam->name));

            /* Save the current value. */
            switch (pParam->type)
            {
                case VPOX_SPI_STRING:
                {
                    /* The 2nd parameter is size in characters of the buffer.
                     * The 3rd parameter points to the buffer.
                     */
                    SystemParametersInfo (pParam->uActionGet,
                                          MAX_PATH,
                                          pParam->achSavedValue,
                                          0);
                } break;

                case VPOX_SPI_BOOL:
                case VPOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter points to BOOL. */
                    SystemParametersInfo (pParam->uActionGet,
                                          0,
                                          pParam->achSavedValue,
                                          0);
                } break;

                case VPOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure.
                     * The cbSize member of this structure must be set.
                     * The uiParam parameter must alos be set.
                     */
                    if (pParam->cbSavedValue > sizeof (pParam->achSavedValue))
                    {
                        LogFlowFunc(("Not enough space %d > %d\n", pParam->cbSavedValue, sizeof (pParam->achSavedValue)));
                        break;
                    }

                    *(UINT *)&pParam->achSavedValue[0] = pParam->cbSavedValue;

                    SystemParametersInfo (s_aSPIParams[i].uActionGet,
                                          s_aSPIParams[i].cbSavedValue,
                                          s_aSPIParams[i].achSavedValue,
                                          0);
                } break;

                default:
                    break;
            }

            LogFlowFunc(("Disabling %s\n", pParam->name));

            /* Disable the feature. */
            switch (pParam->type)
            {
                case VPOX_SPI_STRING:
                {
                    /* The 3rd parameter points to the string. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->pvDisable,
                                          SPIF_SENDCHANGE);
                } break;

                case VPOX_SPI_BOOL:
                {
                    /* The 2nd parameter is BOOL. */
                    SystemParametersInfo (pParam->uActionSet,
                                          FALSE,
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VPOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter is NULL to disable. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VPOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->pvDisable,
                                          SPIF_SENDCHANGE);
                } break;

                default:
                    break;
            }
        }
    }
}

static void vpoxExperienceRestore(uint32_t uLevel)
{
    int i;
    for (i = 0; i < RT_ELEMENTS(s_aSPIParams); i++)
    {
        PVPOXVRDPEXPPARAM pParam = &s_aSPIParams[i];
        if (pParam->level > uLevel)
        {
            LogFlowFunc(("Restoring %s\n", pParam->name));

            /* Restore the feature. */
            switch (pParam->type)
            {
                case VPOX_SPI_STRING:
                {
                    /* The 3rd parameter points to the string. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->achSavedValue,
                                          SPIF_SENDCHANGE);
                } break;

                case VPOX_SPI_BOOL:
                {
                    /* The 2nd parameter is BOOL. */
                    SystemParametersInfo (pParam->uActionSet,
                                          *(BOOL *)&pParam->achSavedValue[0],
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VPOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter is NULL to disable. */
                    BOOL fSaved = *(BOOL *)&pParam->achSavedValue[0];

                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          fSaved? &fSaved: NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VPOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->achSavedValue,
                                          SPIF_SENDCHANGE);
                } break;

                default:
                    break;
            }
        }
    }
}

static DECLCALLBACK(int) VPoxVRDPInit(const PVPOXSERVICEENV pEnv, void **ppInstance)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(ppInstance, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PVPOXVRDPCONTEXT pCtx = &g_Ctx; /* Only one instance at the moment. */
    AssertPtr(pCtx);

    pCtx->pEnv               = pEnv;
    pCtx->level              = VRDP_EXPERIENCE_LEVEL_FULL;
    pCtx->fSavedThemeEnabled = FALSE;

    int rc = RTLdrLoadSystem("UxTheme.dll", false /*fNoUnload*/, &g_Ctx.hModUxTheme);
    if (RT_SUCCESS(rc))
    {
        *(PFNRT *)&pCtx->pfnEnableTheming = RTLdrGetFunction(g_Ctx.hModUxTheme, "EnableTheming");
        *(PFNRT *)&pCtx->pfnIsThemeActive = RTLdrGetFunction(g_Ctx.hModUxTheme, "IsThemeActive");

        *ppInstance = &g_Ctx;
    }
    else
    {
        g_Ctx.hModUxTheme = NIL_RTLDRMOD;
        g_Ctx.pfnEnableTheming = NULL;
        g_Ctx.pfnIsThemeActive = NULL;
    }

    /* Tell the caller that the service does not work but it is OK to continue. */
    if (RT_FAILURE(rc))
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) VPoxVRDPDestroy(void *pInstance)
{
    AssertPtrReturnVoid(pInstance);

    LogFlowFuncEnter();

    PVPOXVRDPCONTEXT pCtx = (PVPOXVRDPCONTEXT)pInstance;

    vpoxExperienceRestore (pCtx->level);
    if (pCtx->hModUxTheme != NIL_RTLDRMOD)
    {
        RTLdrClose(g_Ctx.hModUxTheme);
        pCtx->hModUxTheme = NIL_RTLDRMOD;
    }

    return;
}

/**
 * Thread function to wait for and process mode change requests
 */
static DECLCALLBACK(int) VPoxVRDPWorker(void *pvInstance, bool volatile *pfShutdown)
{
    AssertPtrReturn(pvInstance, VERR_INVALID_POINTER);
    PVPOXVRDPCONTEXT pCtx = (PVPOXVRDPCONTEXT)pvInstance;

    LogFlowFuncEnter();

    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    int rc = VbglR3CtlFilterMask(VMMDEV_EVENT_VRDP, 0 /*fNot*/);
    if (RT_FAILURE(rc))
    {
        LogRel(("VbglR3CtlFilterMask(VMMDEV_EVENT_VRDP, 0) failed with %Rrc, exiting...\n"));
        return rc;
    }

    for (;;)
    {
        /*
         * Wait for the event, checking the shutdown flag both before and after the call.
         */
        if (*pfShutdown)
        {
            rc = VINF_SUCCESS;
            break;
        }

        uint32_t fEvent = 0;
        rc = VbglR3WaitEvent(VMMDEV_EVENT_VRDP, 5000 /*ms*/, &fEvent);

        if (*pfShutdown)
        {
            rc = VINF_SUCCESS;
            break;
        }

        if (RT_SUCCESS(rc))
        {
            /* did we get the right event? */
            if (fEvent & VMMDEV_EVENT_VRDP)
            {
                bool     fActive = false;
                uint32_t uExperienceLevel = 0;
                rc = VbglR3VrdpGetChangeRequest(&fActive, &uExperienceLevel);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("u8VRDPActive = %d, level %d\n", fActive, uExperienceLevel));

                    if (fActive)
                    {
                        pCtx->level = uExperienceLevel;
                        vpoxExperienceSet (pCtx->level);

                        if (pCtx->level == VRDP_EXPERIENCE_LEVEL_ZERO
                            && pCtx->pfnEnableTheming
                            && pCtx->pfnIsThemeActive)
                        {
                            pCtx->fSavedThemeEnabled = pCtx->pfnIsThemeActive ();

                            LogFlowFunc(("pCtx->fSavedThemeEnabled = %d\n", pCtx->fSavedThemeEnabled));

                            if (pCtx->fSavedThemeEnabled)
                            {
                                pCtx->pfnEnableTheming (FALSE);
                            }
                        }
                    }
                    else
                    {
                        if (pCtx->level == VRDP_EXPERIENCE_LEVEL_ZERO
                            && pCtx->pfnEnableTheming
                            && pCtx->pfnIsThemeActive)
                        {
                            if (pCtx->fSavedThemeEnabled)
                            {
                                /** @todo the call returns S_OK but theming remains disabled. */
                                HRESULT hrc = pCtx->pfnEnableTheming (TRUE);
                                LogFlowFunc(("enabling theme rc = 0x%08X\n", hrc)); NOREF(hrc);
                                pCtx->fSavedThemeEnabled = FALSE;
                            }
                        }

                        vpoxExperienceRestore (pCtx->level);

                        pCtx->level = VRDP_EXPERIENCE_LEVEL_FULL;
                    }
                }
                else
                {
                    /* sleep a bit to not eat too much CPU in case the above call always fails */
                    RTThreadSleep(10);
                }
            }
        }
        /* sleep a bit to not eat too much CPU in case the above call always fails */
        else
            RTThreadSleep(50);
    }

    int rc2 = VbglR3CtlFilterMask(0 /*fOr*/, VMMDEV_EVENT_VRDP);
    if (RT_FAILURE(rc2))
        LogRel(("VbglR3CtlFilterMask(0 /*fOr*/, VMMDEV_EVENT_VRDP) failed with %Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * The service description.
 */
VPOXSERVICEDESC g_SvcDescVRDP =
{
    /* pszName. */
    "VRDP",
    /* pszDescription. */
    "VRDP Connection Notification",
    /* methods */
    VPoxVRDPInit,
    VPoxVRDPWorker,
    NULL /* pfnStop */,
    VPoxVRDPDestroy
};

