/* $Id: tstVPoxGINA.cpp $ */
/** @file
 * tstVPoxGINA.cpp - Simple testcase for invoking VPoxGINA.dll.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define UNICODE
#include <iprt/win/windows.h>
#include <stdio.h>

int main()
{
    DWORD dwErr;

    /**
     * Be sure that:
     * - the debug VPoxGINA gets loaded instead of a maybe installed
     *   release version in "C:\Windows\system32".
     */

    HMODULE hMod = LoadLibraryW(L"VPoxGINA.dll");
    if (!hMod)
    {
        dwErr = GetLastError();
        wprintf(L"VPoxGINA.dll not found, error=%ld\n", dwErr);
    }
    else
    {
        wprintf(L"VPoxGINA found\n");

        FARPROC pfnDebug = GetProcAddress(hMod, "VPoxGINADebug");
        if (!pfnDebug)
        {
            dwErr = GetLastError();
            wprintf(L"Could not load VPoxGINADebug, error=%ld\n", dwErr);
        }
        else
        {
            wprintf(L"Calling VPoxGINA ...\n");
            dwErr = pfnDebug();
        }

        FreeLibrary(hMod);
    }

    wprintf(L"Test returned: %ld\n", dwErr);

    return dwErr == ERROR_SUCCESS ? 0 : 1;
}

