/* $Id: tstHook.cpp $ */
/** @file
 * VPoxHook testcase.
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
#include <iprt/win/windows.h>
#include <VPoxHook.h>
#include <stdio.h>


int main()
{
    printf("Enabling global hook\n");

    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, VPOXHOOK_GLOBAL_WT_EVENT_NAME);

    VPoxHookInstallWindowTracker(GetModuleHandle("VPoxHook.dll"));
    getchar();

    printf("Disabling global hook\n");
    VPoxHookRemoveWindowTracker();
    CloseHandle(hEvent);

    return 0;
}

