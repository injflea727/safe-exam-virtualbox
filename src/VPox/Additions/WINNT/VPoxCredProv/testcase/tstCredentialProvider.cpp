/* $Id: tstCredentialProvider.cpp $ */
/** @file
 * tstCredentialProvider - testcase for credential provider.
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

#include <iprt/win/windows.h>
#include <stdio.h>
#include <WinCred.h>

int main(int argc, TCHAR* argv[])
{
    BOOL save = false;
    DWORD authPackage = 0;
    LPVOID authBuffer;
    ULONG authBufferSize = 0;
    CREDUI_INFO credUiInfo;

    credUiInfo.pszCaptionText = TEXT("VPoxCaption");
    credUiInfo.pszMessageText = TEXT("VPoxMessage");
    credUiInfo.cbSize = sizeof(credUiInfo);
    credUiInfo.hbmBanner = NULL;
    credUiInfo.hwndParent = NULL;

    DWORD dwErr = CredUIPromptForWindowsCredentials(&(credUiInfo), 0, &(authPackage),
                                                    NULL, 0, &authBuffer, &authBufferSize, &(save), 0);
    printf("Test returned %ld\n", dwErr);

    return dwErr == ERROR_SUCCESS ? 0 : 1;
}
