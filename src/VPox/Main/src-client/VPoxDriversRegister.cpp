/* $Id: VPoxDriversRegister.cpp $ */
/** @file
 *
 * Main driver registration.
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
#define LOG_GROUP LOG_GROUP_MAIN
#include "LoggingNew.h"

#include "MouseImpl.h"
#include "KeyboardImpl.h"
#include "DisplayImpl.h"
#include "VMMDev.h"
#ifdef VPOX_WITH_AUDIO_VRDE
# include "DrvAudioVRDE.h"
#endif
#ifdef VPOX_WITH_AUDIO_RECORDING
# include "DrvAudioRec.h"
#endif
#include "UsbWebcamInterface.h"
#ifdef VPOX_WITH_USB_CARDREADER
# include "UsbCardReader.h"
#endif
#include "ConsoleImpl.h"
#ifdef VPOX_WITH_PCI_PASSTHROUGH
# include "PCIRawDevImpl.h"
#endif

#include <VPox/vmm/pdmdrv.h>
#include <VPox/version.h>


/**
 * Register the main drivers.
 *
 * @returns VPox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VPox version number.
 */
extern "C" DECLEXPORT(int) VPoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VPoxDriversRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VPOX_VERSION, ("u32Version=%#x VPOX_VERSION=%#x\n", u32Version, VPOX_VERSION));

    int rc = pCallbacks->pfnRegister(pCallbacks, &Mouse::DrvReg);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &Keyboard::DrvReg);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &Display::DrvReg);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &VMMDev::DrvReg);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VPOX_WITH_AUDIO_VRDE
    rc = pCallbacks->pfnRegister(pCallbacks, &AudioVRDE::DrvReg);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VPOX_WITH_AUDIO_RECORDING
    rc = pCallbacks->pfnRegister(pCallbacks, &AudioVideoRec::DrvReg);
    if (RT_FAILURE(rc))
        return rc;
#endif

    rc = pCallbacks->pfnRegister(pCallbacks, &EmWebcam::DrvReg);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VPOX_WITH_USB_CARDREADER
    rc = pCallbacks->pfnRegister(pCallbacks, &UsbCardReader::DrvReg);
    if (RT_FAILURE(rc))
        return rc;
#endif

    rc = pCallbacks->pfnRegister(pCallbacks, &Console::DrvStatusReg);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VPOX_WITH_PCI_PASSTHROUGH
    rc = pCallbacks->pfnRegister(pCallbacks, &PCIRawDev::DrvReg);
    if (RT_FAILURE(rc))
        return rc;
#endif

    return VINF_SUCCESS;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
