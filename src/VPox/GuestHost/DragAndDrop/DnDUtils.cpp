/* $Id: DnDUtils.cpp $ */
/** @file
 * DnD - Common utility functions.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#include <VPox/GuestHost/DragAndDrop.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>


/**
 * Converts a VPOXDNDACTION to a string.
 *
 * @returns Stringified version of VPOXDNDACTION
 * @param   uAction             DnD action to convert.
 */
const char *DnDActionToStr(VPOXDNDACTION uAction)
{
    switch (uAction)
    {
        case VPOX_DND_ACTION_IGNORE: return "ignore";
        case VPOX_DND_ACTION_COPY:   return "copy";
        case VPOX_DND_ACTION_MOVE:   return "move";
        case VPOX_DND_ACTION_LINK:   return "link";
        default:
            break;
    }
    AssertMsgFailedReturn(("Unknown uAction=%d\n", uAction), "bad");
}

