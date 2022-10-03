/* $Id: VPoxDDUDeps.cpp $ */
/** @file
 * VPoxDDU - For dragging in library objects.
 */

/*
 * Copyright (C) 2007-2020 Oracle Corporation
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
#include <VPox/types.h>
#include <VPox/vd.h>
#ifdef VPOX_WITH_USB
# include <VPox/usblib.h>
# include <VPox/usbfilter.h>
# ifdef RT_OS_OS2
#  include <os2.h>
#  include <usbcalls.h>
# endif
#endif

/** Just a dummy global structure containing a bunch of
 * function pointers to code which is wanted in the link.
 */
PFNRT g_apfnVPoxDDUDeps[] =
{
    (PFNRT)VDInit,
    (PFNRT)VDIfCreateVfsStream,
    (PFNRT)VDIfCreateFromVfsStream,
    (PFNRT)VDCreateVfsFileFromDisk,
    (PFNRT)VDIfTcpNetInstDefaultCreate,
#ifdef VPOX_WITH_USB
    (PFNRT)USBFilterInit,
    (PFNRT)USBLibHashSerial,
# ifdef RT_OS_OS2
    (PFNRT)UsbOpen,
# endif
# if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS) /* PORTME */
    (PFNRT)USBLibInit,
# endif
#endif /* VPOX_WITH_USB */
    NULL
};

