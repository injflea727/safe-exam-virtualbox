/* $Id: VPoxSkeletonMain.cpp $ */
/** @file
 * Skeleton main module.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
// static DECLCALLBACK(void) vpoxSkeletonExtPack_Installed(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox, PRTERRINFO pErrInfo);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnUninstall}
//  */
// static DECLCALLBACK(int)  vpoxSkeletonExtPack_Uninstall(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnVirtualPoxReady}
//  */
// static DECLCALLBACK(void)  vpoxSkeletonExtPack_VirtualPoxReady(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnUnload}
//  */
// static DECLCALLBACK(void) vpoxSkeletonExtPack_Unload(PCVPOXEXTPACKREG pThis);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnVMCreated}
//  */
// static DECLCALLBACK(int)  vpoxSkeletonExtPack_VMCreated(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox, VPOXEXTPACK_IF_CS(IMachine) *pMachine);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKREG,pfnQueryObject}
//  */
// static DECLCALLBACK(int)  vpoxSkeletonExtPack_QueryObject(PCVPOXEXTPACKREG pThis, PCRTUUID pObjectId);


static const VPOXEXTPACKREG g_vpoxSkeletonExtPackReg =
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
    *ppReg = &g_vpoxSkeletonExtPackReg;

    return VINF_SUCCESS;
}

