/* $Id: VPoxBusMouseMainVM.cpp $ */
/** @file
 * Bus Mouse main VM module.
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

#include <VPox/err.h>
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
// static DECLCALLBACK(void)  vpoxBusMouseExtPackVM_ConsoleReady(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnUnload}
//  */
// static DECLCALLBACK(void) vpoxBusMouseExtPackVM_Unload(PCVPOXEXTPACKVMREG pThis);

/**
 * @interface_method_impl{VPOXEXTPACKVMREG,pfnVMConfigureVMM
 */
static DECLCALLBACK(int)  vpoxBusMouseExtPackVM_VMConfigureVMM(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM)
{
    RT_NOREF(pThis, pConsole);

    /*
     * Find the bus mouse module and tell PDM to load it.
     * ASSUME /PDM/Devices exists.
     */
    char szPath[RTPATH_MAX];
    int rc = g_pHlp->pfnFindModule(g_pHlp, "VPoxBusMouseR3", NULL, VPOXEXTPACKMODKIND_R3, szPath, sizeof(szPath), NULL);
    if (RT_FAILURE(rc))
        return rc;

    PCFGMNODE pCfgRoot = CFGMR3GetRoot(pVM);
    AssertReturn(pCfgRoot, VERR_INTERNAL_ERROR_3);

    PCFGMNODE pCfgDevices = CFGMR3GetChild(pCfgRoot, "PDM/Devices");
    AssertReturn(pCfgDevices, VERR_INTERNAL_ERROR_3);

    PCFGMNODE pCfgMine;
    rc = CFGMR3InsertNode(pCfgDevices, "VPoxBusMouse", &pCfgMine);
    AssertRCReturn(rc, rc);
    rc = CFGMR3InsertString(pCfgMine, "Path", szPath);
    AssertRCReturn(rc, rc);

    /*
     * Tell PDM where to find the R0 and RC modules for the bus mouse device.
     */
#ifdef VPOX_WITH_RAW_MODE
    rc = g_pHlp->pfnFindModule(g_pHlp, "VPoxBusMouseRC", NULL, VPOXEXTPACKMODKIND_RC, szPath, sizeof(szPath), NULL);
    AssertRCReturn(rc, rc);
    RTPathStripFilename(szPath);
    rc = CFGMR3InsertString(pCfgMine, "RCSearchPath", szPath);
    AssertRCReturn(rc, rc);
#endif

    rc = g_pHlp->pfnFindModule(g_pHlp, "VPoxBusMouseR0", NULL, VPOXEXTPACKMODKIND_R0, szPath, sizeof(szPath), NULL);
    AssertRCReturn(rc, rc);
    RTPathStripFilename(szPath);
    rc = CFGMR3InsertString(pCfgMine, "R0SearchPath", szPath);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnVMPowerOn}
//  */
// static DECLCALLBACK(int)  vpoxBusMouseExtPackVM_VMPowerOn(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnVMPowerOff}
//  */
// static DECLCALLBACK(void) vpoxBusMouseExtPackVM_VMPowerOff(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VPOXEXTPACKVMREG,pfnQueryObject}
//  */
// static DECLCALLBACK(void) vpoxBusMouseExtPackVM_QueryObject(PCVPOXEXTPACKVMREG pThis, PCRTUUID pObjectId);


static const VPOXEXTPACKVMREG g_vpoxBusMouseExtPackVMReg =
{
    VPOXEXTPACKVMREG_VERSION,
    /* .uVPoxFullVersion =  */  VPOX_FULL_VERSION,
    /* .pfnConsoleReady =   */  NULL,
    /* .pfnUnload =         */  NULL,
    /* .pfnVMConfigureVMM = */  vpoxBusMouseExtPackVM_VMConfigureVMM,
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
    *ppReg = &g_vpoxBusMouseExtPackVMReg;

    return VINF_SUCCESS;
}

