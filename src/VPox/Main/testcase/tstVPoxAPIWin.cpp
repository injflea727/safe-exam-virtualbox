/* $Id: tstVPoxAPIWin.cpp $ */
/** @file
 *
 * tstVPoxAPIWin - sample program to illustrate the VirtualPox
 *                 COM API for machine management on Windows.
                   It only uses standard C/C++ and COM semantics,
 *                 no additional VPox classes/macros/helpers. To
 *                 make things even easier to follow, only the
 *                 standard Win32 API has been used. Typically,
 *                 C++ developers would make use of Microsoft's
 *                 ATL to ease development.
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

/*
 * PURPOSE OF THIS SAMPLE PROGRAM
 * ------------------------------
 *
 * This sample program is intended to demonstrate the minimal code necessary
 * to use VirtualPox COM API for learning puroses only. The program uses pure
 * Win32 API and doesn't have any extra dependencies to let you better
 * understand what is going on when a client talks to the VirtualPox core
 * using the COM framework.
 *
 * However, if you want to write a real application, it is highly recommended
 * to use our MS COM XPCOM Glue library and helper C++ classes. This way, you
 * will get at least the following benefits:
 *
 * a) better portability: both the MS COM (used on Windows) and XPCOM (used
 *    everywhere else) VirtualPox client application from the same source code
 *    (including common smart C++ templates for automatic interface pointer
 *    reference counter and string data management);
 * b) simpler XPCOM initialization and shutdown (only a single method call
 *    that does everything right).
 *
 * Currently, there is no separate sample program that uses the VirtualPox MS
 * COM XPCOM Glue library. Please refer to the sources of stock VirtualPox
 * applications such as the VirtualPox GUI frontend or the VPoxManage command
 * line frontend.
 */


#include <stdio.h>
#include <iprt/win/windows.h>  /* Avoid -Wall warnings. */
#include "VirtualPox.h"

#define SAFE_RELEASE(x) \
    if (x) { \
        x->Release(); \
        x = NULL; \
    }

int listVMs(IVirtualPox *virtualPox)
{
    HRESULT rc;

    /*
     * First we have to get a list of all registered VMs
     */
    SAFEARRAY *machinesArray = NULL;

    rc = virtualPox->get_Machines(&machinesArray);
    if (SUCCEEDED(rc))
    {
        IMachine **machines;
        rc = SafeArrayAccessData(machinesArray, (void **) &machines);
        if (SUCCEEDED(rc))
        {
            for (ULONG i = 0; i < machinesArray->rgsabound[0].cElements; ++i)
            {
                BSTR str;

                rc = machines[i]->get_Name(&str);
                if (SUCCEEDED(rc))
                {
                    printf("Name: %S\n", str);
                    SysFreeString(str);
                }
            }

            SafeArrayUnaccessData(machinesArray);
        }

        SafeArrayDestroy(machinesArray);
    }

    return 0;
}


int testErrorInfo(IVirtualPox *virtualPox)
{
    HRESULT rc;

    /* Try to find a machine that doesn't exist */
    IMachine *machine = NULL;
    BSTR machineName = SysAllocString(L"Foobar");

    rc = virtualPox->FindMachine(machineName, &machine);

    if (FAILED(rc))
    {
        IErrorInfo *errorInfo;

        rc = GetErrorInfo(0, &errorInfo);

        if (FAILED(rc))
            printf("Error getting error info! rc = 0x%x\n", rc);
        else
        {
            BSTR errorDescription = NULL;

            rc = errorInfo->GetDescription(&errorDescription);

            if (FAILED(rc) || !errorDescription)
                printf("Error getting error description! rc = 0x%x\n", rc);
            else
            {
                printf("Successfully retrieved error description: %S\n", errorDescription);

                SysFreeString(errorDescription);
            }

            errorInfo->Release();
        }
    }

    SAFE_RELEASE(machine);
    SysFreeString(machineName);

    return 0;
}


int testStartVM(IVirtualPox *virtualPox)
{
    HRESULT rc;

    /* Try to start a VM called "WinXP SP2". */
    IMachine *machine = NULL;
    BSTR machineName = SysAllocString(L"WinXP SP2");

    rc = virtualPox->FindMachine(machineName, &machine);

    if (FAILED(rc))
    {
        IErrorInfo *errorInfo;

        rc = GetErrorInfo(0, &errorInfo);

        if (FAILED(rc))
            printf("Error getting error info! rc = 0x%x\n", rc);
        else
        {
            BSTR errorDescription = NULL;

            rc = errorInfo->GetDescription(&errorDescription);

            if (FAILED(rc) || !errorDescription)
                printf("Error getting error description! rc = 0x%x\n", rc);
            else
            {
                printf("Successfully retrieved error description: %S\n", errorDescription);

                SysFreeString(errorDescription);
            }

            SAFE_RELEASE(errorInfo);
        }
    }
    else
    {
        ISession *session = NULL;
        IConsole *console = NULL;
        IProgress *progress = NULL;
        BSTR sessiontype = SysAllocString(L"gui");
        BSTR guid;

        do
        {
            rc = machine->get_Id(&guid); /* Get the GUID of the machine. */
            if (!SUCCEEDED(rc))
            {
                printf("Error retrieving machine ID! rc = 0x%x\n", rc);
                break;
            }

            /* Create the session object. */
            rc = CoCreateInstance(CLSID_Session,        /* the VirtualPox base object */
                                  NULL,                 /* no aggregation */
                                  CLSCTX_INPROC_SERVER, /* the object lives in the current process */
                                  IID_ISession,         /* IID of the interface */
                                  (void**)&session);
            if (!SUCCEEDED(rc))
            {
                printf("Error creating Session instance! rc = 0x%x\n", rc);
                break;
            }

            /* Start a VM session using the delivered VPox GUI. */
            rc = machine->LaunchVMProcess(session, sessiontype,
                                          NULL, &progress);
            if (!SUCCEEDED(rc))
            {
                printf("Could not open remote session! rc = 0x%x\n", rc);
                break;
            }

            /* Wait until VM is running. */
            printf("Starting VM, please wait ...\n");
            rc = progress->WaitForCompletion(-1);

            /* Get console object. */
            session->get_Console(&console);

            /* Bring console window to front. */
            machine->ShowConsoleWindow(0);

            printf("Press enter to power off VM and close the session...\n");
            getchar();

            /* Power down the machine. */
            rc = console->PowerDown(&progress);

            /* Wait until VM is powered down. */
            printf("Powering off VM, please wait ...\n");
            rc = progress->WaitForCompletion(-1);

            /* Close the session. */
            rc = session->UnlockMachine();

        } while (0);

        SAFE_RELEASE(console);
        SAFE_RELEASE(progress);
        SAFE_RELEASE(session);
        SysFreeString(guid);
        SysFreeString(sessiontype);
        SAFE_RELEASE(machine);
    }

    SysFreeString(machineName);

    return 0;
}


int main()
{
    /* Initialize the COM subsystem. */
    CoInitialize(NULL);

    /* Instantiate the VirtualPox root object. */
    IVirtualPoxClient *virtualPoxClient;
    HRESULT rc = CoCreateInstance(CLSID_VirtualPoxClient, /* the VirtualPoxClient object */
                                  NULL,                   /* no aggregation */
                                  CLSCTX_INPROC_SERVER,   /* the object lives in the current process */
                                  IID_IVirtualPoxClient,  /* IID of the interface */
                                  (void**)&virtualPoxClient);
    if (SUCCEEDED(rc))
    {
        IVirtualPox *virtualPox;
        rc = virtualPoxClient->get_VirtualPox(&virtualPox);
        if (SUCCEEDED(rc))
        {
            listVMs(virtualPox);

            testErrorInfo(virtualPox);

            /* Enable the following line to get a VM started. */
            //testStartVM(virtualPox);

            /* Release the VirtualPox object. */
            virtualPox->Release();
            virtualPoxClient->Release();
        }
        else
            printf("Error creating VirtualPox instance! rc = 0x%x\n", rc);
    }

    CoUninitialize();
    return 0;
}

