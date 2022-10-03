/* $Id: VPoxVNCMain.cpp $ */
/** @file
 * VNC main module.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VPox/ExtPack/ExtPack.h>

#include <iprt/errcore.h>
#include <VPox/version.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include <iprt/path.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Pointer to the extension pack helpers. */
static PCVPOXEXTPACKHLP g_pHlp;


// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnInstalled}
//  */
// static DECLCALLBACK(void) vpoxVNCExtPack_Installed(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox, PRTERRINFO pErrInfo);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnUninstall}
//  */
// static DECLCALLBACK(int)  vpoxVNCExtPack_Uninstall(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnVirtualPoxReady}
//  */
// static DECLCALLBACK(void)  vpoxVNCExtPack_VirtualPoxReady(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnUnload}
//  */
// static DECLCALLBACK(void) vpoxVNCExtPack_Unload(PCVPOXEXTPACKREG pThis);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnVMCreated}
//  */
// static DECLCALLBACK(int)  vpoxVNCExtPack_VMCreated(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox, VPOXEXTPACK_IF_CS(IMachine) *pMachine);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnQueryObject}
//  */
// static DECLCALLBACK(void) vpoxVNCExtPack_QueryObject(PCVPOXEXTPACKREG pThis, PCRTUUID pObjectId);


static const VPOXEXTPACKREG g_vpoxVNCExtPackReg =
{
    VPOXEXTPACKREG_VERSION,
    /* .uVPoxFullVersion =  */  VPOX_FULL_VERSION,
    /* .pfnInstalled =      */  NULL,
    /* .pfnUninstall =      */  NULL,
    /* .pfnVirtualPoxReady =*/  NULL,
    /* .pfnUnload =         */  NULL,
    /* .pfnVMCreated =      */  NULL,
    /* .pfnQueryObject =    */  NULL,
    /* .pfnReserved1 =      */  NULL,
    /* .pfnReserved2 =      */  NULL,
    /* .pfnReserved3 =      */  NULL,
    /* .pfnReserved4 =      */  NULL,
    /* .pfnReserved5 =      */  NULL,
    /* .pfnReserved6 =      */  NULL,
    /* .uReserved7 =        */  0,
    VPOXEXTPACKREG_VERSION
};


/** @callback_method_impl{FNVPOXEXTPACKREGISTER}  */
extern "C" DECLEXPORT(int) VPoxExtPackRegister(PCVPOXEXTPACKHLP pHlp, PCVPOXEXTPACKREG *ppReg, PRTERRINFO pErrInfo)
{
    /*
     * Check the VirtualPox version.
     */
    if (!VPOXEXTPACK_IS_VER_COMPAT(pHlp->u32Version, VPOXEXTPACKHLP_VERSION))
        return RTErrInfoSetF(pErrInfo, VERR_VERSION_MISMATCH,
                             "Helper version mismatch - expected %#x got %#x",
                             VPOXEXTPACKHLP_VERSION, pHlp->u32Version);
    if (   VPOX_FULL_VERSION_GET_MAJOR(pHlp->uVPoxFullVersion) != VPOX_VERSION_MAJOR
        || VPOX_FULL_VERSION_GET_MINOR(pHlp->uVPoxFullVersion) != VPOX_VERSION_MINOR)
        return RTErrInfoSetF(pErrInfo, VERR_VERSION_MISMATCH,
                             "VirtualPox version mismatch - expected %u.%u got %u.%u",
                             VPOX_VERSION_MAJOR, VPOX_VERSION_MINOR,
                             VPOX_FULL_VERSION_GET_MAJOR(pHlp->uVPoxFullVersion),
                             VPOX_FULL_VERSION_GET_MINOR(pHlp->uVPoxFullVersion));

    /*
     * We're good, save input and return the registration structure.
     */
    g_pHlp = pHlp;
    *ppReg = &g_vpoxVNCExtPackReg;

    return VINF_SUCCESS;
}

