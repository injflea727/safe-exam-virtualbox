/* $Id: VPoxSkeletonMainVM.cpp $ */
/** @file
 * Skeleton main VM module.
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
#include <VPox/vmm/cfgm.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include <iprt/path.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Pointer to the extension pack helpers. */
static PCVPOXEXTPACKHLP g_pHlp;


// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnConsoleReady}
//  */
// static DECLCALLBACK(void)  vpoxSkeletonExtPackVM_ConsoleReady(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnUnload}
//  */
// static DECLCALLBACK(void) vpoxSkeletonExtPackVM_Unload(PCVPOXEXTPACKVMREG pThis);
//
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnVMConfigureVMM}
//  */
// static DECLCALLBACK(int)  vpoxSkeletonExtPackVM_VMConfigureVMM(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnVMPowerOn}
//  */
// static DECLCALLBACK(int)  vpoxSkeletonExtPackVM_VMPowerOn(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnVMPowerOff}
//  */
// static DECLCALLBACK(void) vpoxSkeletonExtPackVM_VMPowerOff(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnQueryObject}
//  */
// static DECLCALLBACK(void) vpoxSkeletonExtPackVM_QueryObject(PCVPOXEXTPACKVMREG pThis, PCRTUUID pObjectId);


static const VPOXEXTPACKVMREG g_vpoxSkeletonExtPackVMReg =
{
    VPOXEXTPACKVMREG_VERSION,
    /* .uVPoxFullVersion =  */  VPOX_FULL_VERSION,
    /* .pfnConsoleReady =   */  NULL,
    /* .pfnUnload =         */  NULL,
    /* .pfnVMConfigureVMM = */  NULL,
    /* .pfnVMPowerOn =      */  NULL,
    /* .pfnVMPowerOff =     */  NULL,
    /* .pfnQueryObject =    */  NULL,
    /* .pfnReserved1 =      */  NULL,
    /* .pfnReserved2 =      */  NULL,
    /* .pfnReserved3 =      */  NULL,
    /* .pfnReserved4 =      */  NULL,
    /* .pfnReserved5 =      */  NULL,
    /* .pfnReserved6 =      */  NULL,
    /* .uReserved7 =        */  0,
    VPOXEXTPACKVMREG_VERSION
};


/** @callback_method_impl{FNVPOXEXTPACKVMREGISTER}  */
extern "C" DECLEXPORT(int) VPoxExtPackVMRegister(PCVPOXEXTPACKHLP pHlp, PCVPOXEXTPACKVMREG *ppReg, PRTERRINFO pErrInfo)
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
    *ppReg = &g_vpoxSkeletonExtPackVMReg;

    return VINF_SUCCESS;
}

