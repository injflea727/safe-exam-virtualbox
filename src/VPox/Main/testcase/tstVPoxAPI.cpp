/* $Id: tstVPoxAPI.cpp $ */
/** @file
 * tstVPoxAPI - Checks VirtualPox API.
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
#include <VPox/com/com.h>
#include <VPox/com/string.h>
#include <VPox/com/array.h>
#include <VPox/com/Guid.h>
#include <VPox/com/ErrorInfo.h>
#include <VPox/com/errorprint.h>
#include <VPox/com/VirtualPox.h>
#include <VPox/sup.h>

#include <iprt/test.h>
#include <iprt/time.h>

using namespace com;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;
static Bstr   tstMachineName = "tstVPoxAPI test VM";


/** Worker for TST_COM_EXPR(). */
static HRESULT tstComExpr(HRESULT hrc, const char *pszOperation, int iLine)
{
    if (FAILED(hrc))
        RTTestFailed(g_hTest, "%s failed on line %u with hrc=%Rhrc", pszOperation, iLine, hrc);
    return hrc;
}

/** Macro that executes the given expression and report any failure.
 *  The expression must return a HRESULT. */
#define TST_COM_EXPR(expr) tstComExpr(expr, #expr, __LINE__)


static BOOL tstApiIVirtualPox(IVirtualPox *pVPox)
{
    HRESULT rc;
    Bstr bstrTmp;
    ULONG ulTmp;

    RTTestSub(g_hTest, "IVirtualPox::version");
    CHECK_ERROR(pVPox, COMGETTER(Version)(bstrTmp.asOutParam()));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::version");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::version failed", __LINE__);

    RTTestSub(g_hTest, "IVirtualPox::versionNormalized");
    CHECK_ERROR(pVPox, COMGETTER(VersionNormalized)(bstrTmp.asOutParam()));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::versionNormalized");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::versionNormalized failed", __LINE__);

    RTTestSub(g_hTest, "IVirtualPox::revision");
    CHECK_ERROR(pVPox, COMGETTER(Revision)(&ulTmp));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::revision");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::revision failed", __LINE__);

    RTTestSub(g_hTest, "IVirtualPox::packageType");
    CHECK_ERROR(pVPox, COMGETTER(PackageType)(bstrTmp.asOutParam()));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::packageType");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::packageType failed", __LINE__);

    RTTestSub(g_hTest, "IVirtualPox::APIVersion");
    CHECK_ERROR(pVPox, COMGETTER(APIVersion)(bstrTmp.asOutParam()));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::APIVersion");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::APIVersion failed", __LINE__);

    RTTestSub(g_hTest, "IVirtualPox::homeFolder");
    CHECK_ERROR(pVPox, COMGETTER(HomeFolder)(bstrTmp.asOutParam()));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::homeFolder");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::homeFolder failed", __LINE__);

    RTTestSub(g_hTest, "IVirtualPox::settingsFilePath");
    CHECK_ERROR(pVPox, COMGETTER(SettingsFilePath)(bstrTmp.asOutParam()));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::settingsFilePath");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::settingsFilePath failed", __LINE__);

    com::SafeIfaceArray<IGuestOSType> guestOSTypes;
    RTTestSub(g_hTest, "IVirtualPox::guestOSTypes");
    CHECK_ERROR(pVPox, COMGETTER(GuestOSTypes)(ComSafeArrayAsOutParam(guestOSTypes)));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::guestOSTypes");
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::guestOSTypes failed", __LINE__);

    /** Create VM */
    RTTestSub(g_hTest, "IVirtualPox::CreateMachine");
    ComPtr<IMachine> ptrMachine;
    com::SafeArray<BSTR> groups;
    /** Default VM settings */
    CHECK_ERROR(pVPox, CreateMachine(NULL,                          /** Settings */
                                     tstMachineName.raw(),          /** Name */
                                     ComSafeArrayAsInParam(groups), /** Groups */
                                     NULL,                          /** OS Type */
                                     NULL,                          /** Create flags */
                                     ptrMachine.asOutParam()));     /** Machine */
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::CreateMachine");
    else
    {
        RTTestFailed(g_hTest, "%d: IVirtualPox::CreateMachine failed", __LINE__);
        return FALSE;
    }

    RTTestSub(g_hTest, "IVirtualPox::RegisterMachine");
    CHECK_ERROR(pVPox, RegisterMachine(ptrMachine));
    if (SUCCEEDED(rc))
        RTTestPassed(g_hTest, "IVirtualPox::RegisterMachine");
    else
    {
        RTTestFailed(g_hTest, "%d: IVirtualPox::RegisterMachine failed", __LINE__);
        return FALSE;
    }

    ComPtr<IHost> host;
    RTTestSub(g_hTest, "IVirtualPox::host");
    CHECK_ERROR(pVPox, COMGETTER(Host)(host.asOutParam()));
    if (SUCCEEDED(rc))
    {
        /** @todo Add IHost testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::host");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::host failed", __LINE__);

    ComPtr<ISystemProperties> sysprop;
    RTTestSub(g_hTest, "IVirtualPox::systemProperties");
    CHECK_ERROR(pVPox, COMGETTER(SystemProperties)(sysprop.asOutParam()));
    if (SUCCEEDED(rc))
    {
        /** @todo Add ISystemProperties testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::systemProperties");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::systemProperties failed", __LINE__);

    com::SafeIfaceArray<IMachine> machines;
    RTTestSub(g_hTest, "IVirtualPox::machines");
    CHECK_ERROR(pVPox, COMGETTER(Machines)(ComSafeArrayAsOutParam(machines)));
    if (SUCCEEDED(rc))
    {
        bool bFound = FALSE;
        for (size_t i = 0; i < machines.size(); ++i)
        {
            if (machines[i])
            {
                Bstr tmpName;
                rc = machines[i]->COMGETTER(Name)(tmpName.asOutParam());
                if (SUCCEEDED(rc))
                {
                    if (tmpName == tstMachineName)
                    {
                        bFound = TRUE;
                        break;
                    }
                }
            }
        }

        if (bFound)
            RTTestPassed(g_hTest, "IVirtualPox::machines");
        else
            RTTestFailed(g_hTest, "%d: IVirtualPox::machines failed. No created machine found", __LINE__);
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::machines failed", __LINE__);

#if 0 /** Not yet implemented */
    com::SafeIfaceArray<ISharedFolder> sharedFolders;
    RTTestSub(g_hTest, "IVirtualPox::sharedFolders");
    CHECK_ERROR(pVPox, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(sharedFolders)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add ISharedFolders testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::sharedFolders");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::sharedFolders failed", __LINE__);
#endif

    com::SafeIfaceArray<IMedium> hardDisks;
    RTTestSub(g_hTest, "IVirtualPox::hardDisks");
    CHECK_ERROR(pVPox, COMGETTER(HardDisks)(ComSafeArrayAsOutParam(hardDisks)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add hardDisks testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::hardDisks");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::hardDisks failed", __LINE__);

    com::SafeIfaceArray<IMedium> DVDImages;
    RTTestSub(g_hTest, "IVirtualPox::DVDImages");
    CHECK_ERROR(pVPox, COMGETTER(DVDImages)(ComSafeArrayAsOutParam(DVDImages)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add DVDImages testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::DVDImages");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::DVDImages failed", __LINE__);

    com::SafeIfaceArray<IMedium> floppyImages;
    RTTestSub(g_hTest, "IVirtualPox::floppyImages");
    CHECK_ERROR(pVPox, COMGETTER(FloppyImages)(ComSafeArrayAsOutParam(floppyImages)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add floppyImages testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::floppyImages");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::floppyImages failed", __LINE__);

    com::SafeIfaceArray<IProgress> progressOperations;
    RTTestSub(g_hTest, "IVirtualPox::progressOperations");
    CHECK_ERROR(pVPox, COMGETTER(ProgressOperations)(ComSafeArrayAsOutParam(progressOperations)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add IProgress testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::progressOperations");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::progressOperations failed", __LINE__);

    ComPtr<IPerformanceCollector> performanceCollector;
    RTTestSub(g_hTest, "IVirtualPox::performanceCollector");
    CHECK_ERROR(pVPox, COMGETTER(PerformanceCollector)(performanceCollector.asOutParam()));
    if (SUCCEEDED(rc))
    {
        /** @todo Add IPerformanceCollector testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::performanceCollector");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::performanceCollector failed", __LINE__);

    com::SafeIfaceArray<IDHCPServer> DHCPServers;
    RTTestSub(g_hTest, "IVirtualPox::DHCPServers");
    CHECK_ERROR(pVPox, COMGETTER(DHCPServers)(ComSafeArrayAsOutParam(DHCPServers)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add IDHCPServers testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::DHCPServers");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::DHCPServers failed", __LINE__);

    com::SafeIfaceArray<INATNetwork> NATNetworks;
    RTTestSub(g_hTest, "IVirtualPox::NATNetworks");
    CHECK_ERROR(pVPox, COMGETTER(NATNetworks)(ComSafeArrayAsOutParam(NATNetworks)));
    if (SUCCEEDED(rc))
    {
        /** @todo Add INATNetworks testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::NATNetworks");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::NATNetworks failed", __LINE__);

    ComPtr<IEventSource> eventSource;
    RTTestSub(g_hTest, "IVirtualPox::eventSource");
    CHECK_ERROR(pVPox, COMGETTER(EventSource)(eventSource.asOutParam()));
    if (SUCCEEDED(rc))
    {
        /** @todo Add IEventSource testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::eventSource");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::eventSource failed", __LINE__);

    ComPtr<IExtPackManager> extensionPackManager;
    RTTestSub(g_hTest, "IVirtualPox::extensionPackManager");
    CHECK_ERROR(pVPox, COMGETTER(ExtensionPackManager)(extensionPackManager.asOutParam()));
    if (SUCCEEDED(rc))
    {
        /** @todo Add IExtPackManager testing here. */
        RTTestPassed(g_hTest, "IVirtualPox::extensionPackManager");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::extensionPackManager failed", __LINE__);

    com::SafeArray<BSTR> internalNetworks;
    RTTestSub(g_hTest, "IVirtualPox::internalNetworks");
    CHECK_ERROR(pVPox, COMGETTER(InternalNetworks)(ComSafeArrayAsOutParam(internalNetworks)));
    if (SUCCEEDED(rc))
    {
        RTTestPassed(g_hTest, "IVirtualPox::internalNetworks");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::internalNetworks failed", __LINE__);

    com::SafeArray<BSTR> genericNetworkDrivers;
    RTTestSub(g_hTest, "IVirtualPox::genericNetworkDrivers");
    CHECK_ERROR(pVPox, COMGETTER(GenericNetworkDrivers)(ComSafeArrayAsOutParam(genericNetworkDrivers)));
    if (SUCCEEDED(rc))
    {
        RTTestPassed(g_hTest, "IVirtualPox::genericNetworkDrivers");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualPox::genericNetworkDrivers failed", __LINE__);

    return TRUE;
}


static BOOL tstApiClean(IVirtualPox *pVPox)
{
    HRESULT rc;

    /** Delete created VM and its files */
    ComPtr<IMachine> machine;
    CHECK_ERROR_RET(pVPox, FindMachine(Bstr(tstMachineName).raw(), machine.asOutParam()), FALSE);
    SafeIfaceArray<IMedium> media;
    CHECK_ERROR_RET(machine, Unregister(CleanupMode_DetachAllReturnHardDisksOnly,
                                    ComSafeArrayAsOutParam(media)), FALSE);
    ComPtr<IProgress> progress;
    CHECK_ERROR_RET(machine, DeleteConfig(ComSafeArrayAsInParam(media), progress.asOutParam()), FALSE);
    CHECK_ERROR_RET(progress, WaitForCompletion(-1), FALSE);

    return TRUE;
}


int main()
{
    /*
     * Initialization.
     */
    RTEXITCODE rcExit = RTTestInitAndCreate("tstVPoxAPI", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    SUPR3Init(NULL); /* Better time support. */
    RTTestBanner(g_hTest);

    RTTestSub(g_hTest, "Initializing COM and singletons");
    HRESULT hrc = com::Initialize();
    if (SUCCEEDED(hrc))
    {
        ComPtr<IVirtualPoxClient> ptrVPoxClient;
        ComPtr<IVirtualPox> ptrVPox;
        hrc = TST_COM_EXPR(ptrVPoxClient.createInprocObject(CLSID_VirtualPoxClient));
        if (SUCCEEDED(hrc))
            hrc = TST_COM_EXPR(ptrVPoxClient->COMGETTER(VirtualPox)(ptrVPox.asOutParam()));
        if (SUCCEEDED(hrc))
        {
            ComPtr<ISession> ptrSession;
            hrc = TST_COM_EXPR(ptrSession.createInprocObject(CLSID_Session));
            if (SUCCEEDED(hrc))
            {
                RTTestSubDone(g_hTest);

                /*
                 * Call test functions.
                 */

                /** Test IVirtualPox interface */
                tstApiIVirtualPox(ptrVPox);


                /** Clean files/configs */
                tstApiClean(ptrVPox);
            }
        }

        ptrVPox.setNull();
        ptrVPoxClient.setNull();
        com::Shutdown();
    }
    else
        RTTestIFailed("com::Initialize failed with hrc=%Rhrc", hrc);
    return RTTestSummaryAndDestroy(g_hTest);
}
