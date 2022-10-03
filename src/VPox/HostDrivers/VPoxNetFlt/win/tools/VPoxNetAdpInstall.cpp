/* $Id: VPoxNetAdpInstall.cpp $ */
/** @file
 * NetAdpInstall - VPoxNetAdp installer command line tool.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VPox/VPoxNetCfg-win.h>
#include <VPox/VPoxDrvCfg-win.h>
#include <stdio.h>
#include <devguid.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VPOX_NETADP_APP_NAME L"NetAdpInstall"

#define VPOX_NETADP_HWID L"sun_VPoxNetAdp"
#ifdef NDIS60
# define VPOX_NETADP_INF L"VPoxNetAdp6.inf"
#else
# define VPOX_NETADP_INF L"VPoxNetAdp.inf"
#endif


static VOID winNetCfgLogger(LPCSTR szString)
{
    printf("%s\n", szString);
}


/** Wrapper around GetfullPathNameW that will try an alternative INF location.
 *
 * The default location is the current directory.  If not found there, the
 * alternative location is the executable directory.  If not found there either,
 * the first alternative is present to the caller.
 */
static DWORD MyGetfullPathNameW(LPCWSTR pwszName, size_t cchFull, LPWSTR pwszFull)
{
    LPWSTR pwszFilePart;
    DWORD dwSize = GetFullPathNameW(pwszName, (DWORD)cchFull, pwszFull, &pwszFilePart);
    if (dwSize <= 0)
        return dwSize;

    /* if it doesn't exist, see if the file exists in the same directory as the executable. */
    if (GetFileAttributesW(pwszFull) == INVALID_FILE_ATTRIBUTES)
    {
        WCHAR wsz[512];
        DWORD cch = GetModuleFileNameW(GetModuleHandle(NULL), &wsz[0], sizeof(wsz) / sizeof(wsz[0]));
        if (cch > 0)
        {
            while (cch > 0 && wsz[cch - 1] != '/' && wsz[cch - 1] != '\\' && wsz[cch - 1] != ':')
                cch--;
            unsigned i = 0;
            while (cch < sizeof(wsz) / sizeof(wsz[0]))
            {
                wsz[cch] = pwszFilePart[i++];
                if (!wsz[cch])
                {
                    dwSize = GetFullPathNameW(wsz, (DWORD)cchFull, pwszFull, NULL);
                    if (dwSize > 0 && GetFileAttributesW(pwszFull) != INVALID_FILE_ATTRIBUTES)
                        return dwSize;
                    break;
                }
                cch++;
            }
        }
    }

    /* fallback */
    return GetFullPathNameW(pwszName, (DWORD)cchFull, pwszFull, NULL);
}


static int VPoxNetAdpInstall(void)
{
    VPoxNetCfgWinSetLogging(winNetCfgLogger);

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        wprintf(L"adding host-only interface..\n");

        WCHAR wszInfFile[MAX_PATH];
        DWORD cwcInfFile = MyGetfullPathNameW(VPOX_NETADP_INF, sizeof(wszInfFile) / sizeof(wszInfFile[0]), wszInfFile);
        if (cwcInfFile > 0)
        {
            INetCfg *pnc;
            LPWSTR lpszLockedBy = NULL;
            hr = VPoxNetCfgWinQueryINetCfg(&pnc, TRUE, VPOX_NETADP_APP_NAME, 10000, &lpszLockedBy);
            if (hr == S_OK)
            {

                hr = VPoxNetCfgWinNetAdpInstall(pnc, wszInfFile);

                if (hr == S_OK)
                {
                    wprintf(L"installed successfully\n");
                }
                else
                {
                    wprintf(L"error installing VPoxNetAdp (0x%x)\n", hr);
                }

                VPoxNetCfgWinReleaseINetCfg(pnc, TRUE);
            }
            else
                wprintf(L"VPoxNetCfgWinQueryINetCfg failed: hr = 0x%x\n", hr);
            /*
            hr = VPoxDrvCfgInfInstall(MpInf);
            if (FAILED(hr))
                printf("VPoxDrvCfgInfInstall failed %#x\n", hr);

            GUID guid;
            BSTR name, errMsg;

            hr = VPoxNetCfgWinCreateHostOnlyNetworkInterface (MpInf, true, &guid, &name, &errMsg);
            if (SUCCEEDED(hr))
            {
                ULONG ip, mask;
                hr = VPoxNetCfgWinGenHostOnlyNetworkNetworkIp(&ip, &mask);
                if (SUCCEEDED(hr))
                {
                    // ip returned by VPoxNetCfgWinGenHostOnlyNetworkNetworkIp is a network ip,
                    // i.e. 192.168.xxx.0, assign  192.168.xxx.1 for the hostonly adapter
                    ip = ip | (1 << 24);
                    hr = VPoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
                    if (SUCCEEDED(hr))
                    {
                        printf("installation successful\n");
                    }
                    else
                        printf("VPoxNetCfgWinEnableStaticIpConfig failed: hr = 0x%x\n", hr);
                }
                else
                    printf("VPoxNetCfgWinGenHostOnlyNetworkNetworkIp failed: hr = 0x%x\n", hr);
            }
            else
                printf("VPoxNetCfgWinCreateHostOnlyNetworkInterface failed: hr = 0x%x\n", hr);
            */
        }
        else
        {
            DWORD dwErr = GetLastError();
            wprintf(L"GetFullPathNameW failed: winEr = %d\n", dwErr);
            hr = HRESULT_FROM_WIN32(dwErr);
        }
        CoUninitialize();
    }
    else
        wprintf(L"Error initializing COM (0x%x)\n", hr);

    VPoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VPoxNetAdpUninstall(void)
{
    VPoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = VPoxNetCfgWinRemoveAllNetDevicesOfId(VPOX_NETADP_HWID);
        if (SUCCEEDED(hr))
        {
            hr = VPoxDrvCfgInfUninstallAllSetupDi(&GUID_DEVCLASS_NET, L"Net", VPOX_NETADP_HWID, 0/* could be SUOI_FORCEDELETE */);
            if (SUCCEEDED(hr))
            {
                printf("uninstallation successful\n");
            }
            else
                printf("uninstalled successfully, but failed to remove infs\n");
        }
        else
            printf("uninstall failed, hr = 0x%x\n", hr);
        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VPoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VPoxNetAdpUpdate(void)
{
    VPoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        BOOL fRebootRequired = FALSE;
        /*
         * Before we can update the driver for existing adapters we need to remove
         * all old driver packages from the driver cache. Otherwise we may end up
         * with both NDIS5 and NDIS6 versions of VPoxNetAdp in the cache which
         * will cause all sorts of trouble.
         */
        VPoxDrvCfgInfUninstallAllF(L"Net", VPOX_NETADP_HWID, SUOI_FORCEDELETE);
        hr = VPoxNetCfgWinUpdateHostOnlyNetworkInterface(VPOX_NETADP_INF, &fRebootRequired, VPOX_NETADP_HWID);
        if (SUCCEEDED(hr))
        {
            if (fRebootRequired)
                printf("!!REBOOT REQUIRED!!\n");
            printf("updated successfully\n");
        }
        else
            printf("update failed, hr = 0x%x\n", hr);

        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VPoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VPoxNetAdpDisable(void)
{
    VPoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("disabling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = VPoxNetCfgWinPropChangeAllNetDevicesOfId(VPOX_NETADP_HWID, VPOXNECTFGWINPROPCHANGE_TYPE_DISABLE);
        if (SUCCEEDED(hr))
        {
            printf("disabling successful\n");
        }
        else
            printf("disable failed, hr = 0x%x\n", hr);

        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VPoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VPoxNetAdpEnable(void)
{
    VPoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("enabling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = VPoxNetCfgWinPropChangeAllNetDevicesOfId(VPOX_NETADP_HWID, VPOXNECTFGWINPROPCHANGE_TYPE_ENABLE);
        if (SUCCEEDED(hr))
        {
            printf("enabling successful\n");
        }
        else
            printf("enabling failed, hr = 0x%x\n", hr);

        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VPoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static void printUsage(void)
{
    printf("host-only network adapter configuration tool\n"
            "  Usage: VPoxNetAdpInstall [cmd]\n"
            "    cmd can be one of the following values:\n"
            "       i  - install a new host-only interface (default command)\n"
            "       u  - uninstall all host-only interfaces\n"
            "       a  - update the host-only driver\n"
            "       d  - disable all host-only interfaces\n"
            "       e  - enable all host-only interfaces\n"
            "       h  - print this message\n");
}

int __cdecl main(int argc, char **argv)
{
    if (argc < 2)
        return VPoxNetAdpInstall();
    if (argc > 2)
    {
        printUsage();
        return 1;
    }

    if (!strcmp(argv[1], "i"))
        return VPoxNetAdpInstall();
    if (!strcmp(argv[1], "u"))
        return VPoxNetAdpUninstall();
    if (!strcmp(argv[1], "a"))
        return VPoxNetAdpUpdate();
    if (!strcmp(argv[1], "d"))
        return VPoxNetAdpDisable();
    if (!strcmp(argv[1], "e"))
        return VPoxNetAdpEnable();

    printUsage();
    return !strcmp(argv[1], "h");
}
