/* $Id: webtest.cpp $ */
/** @file
 * webtest.cpp:
 *      demo webservice client in C++. This mimics some of the
 *      functionality of VPoxManage for testing purposes.
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

// gSOAP headers (must come after vpox includes because it checks for conflicting defs)
#include "soapStub.h"

// include generated namespaces table
#include "vpoxwebsrv.nsmap"

#include <iostream>
#include <sstream>
#include <string>

#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/errcore.h>


static void usage(int exitcode)
{
    std::cout <<
       "webtest: VirtualPox webservice testcase.\n"
       "\nUsage: webtest [options] [command]...\n"
       "\nSupported options:\n"
       " -h: print this help message and exit.\n"
       " -c URL: specify the webservice server URL (default http://localhost:18083/).\n"
       "\nSupported commands:\n"
       " - IWebsessionManager:\n"
       "   - webtest logon <user> <pass>: IWebsessionManager::logon().\n"
       "   - webtest getsession <vpoxref>: IWebsessionManager::getSessionObject().\n"
       "   - webtest logoff <vpoxref>: IWebsessionManager::logoff().\n"
       " - IVirtualPox:\n"
       "   - webtest version <vpoxref>: IVirtualPox::getVersion().\n"
       "   - webtest gethost <vpoxref>: IVirtualPox::getHost().\n"
       "   - webtest getpc <vpoxref>: IVirtualPox::getPerformanceCollector().\n"
       "   - webtest getmachines <vpoxref>: IVirtualPox::getMachines().\n"
       "   - webtest createmachine <vpoxref> <settingsPath> <name>: IVirtualPox::createMachine().\n"
       "   - webtest registermachine <vpoxref> <machineref>: IVirtualPox::registerMachine().\n"
       " - IHost:\n"
       "   - webtest getdvddrives <hostref>: IHost::getDVDDrives.\n"
       " - IHostDVDDrive:\n"
       "   - webtest getdvdname <dvdref>: IHostDVDDrive::getname.\n"
       " - IMachine:\n"
       "   - webtest getname <machineref>: IMachine::getName().\n"
       "   - webtest getid <machineref>: IMachine::getId().\n"
       "   - webtest getostype <machineref>: IMachine::getGuestOSType().\n"
       "   - webtest savesettings <machineref>: IMachine::saveSettings().\n"
       " - IPerformanceCollector:\n"
       "   - webtest setupmetrics <pcref>: IPerformanceCollector::setupMetrics()\n"
       "   - webtest querymetricsdata <pcref>: IPerformanceCollector::QueryMetricsData()\n"
       " - IVirtualPoxErrorInfo:\n"
       "   - webtest errorinfo <eiref>: various IVirtualPoxErrorInfo getters\n"
       " - All managed object references:\n"
       "   - webtest getif <ref>: report interface of object.\n"
       "   - webtest release <ref>: IUnknown::Release().\n";
    exit(exitcode);
}

/**
 *
 * @param argc
 * @param argv[]
 * @return
 */
int main(int argc, char* argv[])
{
    bool fSSL = false;
    const char *pcszArgEndpoint = "http://localhost:18083/";

    /* SSL callbacks drag in IPRT sem/thread use, so make sure it is ready. */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    int ap;
    for (ap = 1; ap < argc; ap++)
    {
        if (argv[ap][0] == '-')
        {
            if (!strcmp(argv[ap], "-h"))
                usage(0);
            else if (!strcmp(argv[ap], "-c"))
            {
                ap++;
                if (ap >= argc)
                    usage(1);
                pcszArgEndpoint = argv[ap];
                fSSL = !strncmp(pcszArgEndpoint, "https://", 8);
            }
            else
                usage(1);
        }
        else
            break;
    }

    if (argc < 1 + ap)
        usage(1);

#ifdef WITH_OPENSSL
    if (fSSL)
        soap_ssl_init();
#endif /* WITH_OPENSSL */

    struct soap soap; // gSOAP runtime environment
    soap_init(&soap); // initialize runtime environment (only once)
#ifdef WITH_OPENSSL
    // Use SOAP_SSL_NO_AUTHENTICATION here to accept broken server configs.
    // In a real world setup please use at least SOAP_SSL_DEFAULT and provide
    // the necessary CA certificate for validating the server's certificate.
    if (fSSL && soap_ssl_client_context(&soap, SOAP_SSL_NO_AUTHENTICATION | SOAP_TLSv1,
                                        NULL /*clientkey*/, NULL /*password*/,
                                        NULL /*cacert*/, NULL /*capath*/,
                                        NULL /*randfile*/))
    {
        soap_print_fault(&soap, stderr);
        exit(1);
    }
#endif /* WITH_OPENSSL */

    const char *pcszMode = argv[ap];
    int soaprc = SOAP_SVR_FAULT;

    if (!strcmp(pcszMode, "logon"))
    {
        if (argc < 3 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IWebsessionManager_USCORElogon req;
            req.username = argv[ap + 1];
            req.password = argv[ap + 2];
            _vpox__IWebsessionManager_USCORElogonResponse resp;

            if (!(soaprc = soap_call___vpox__IWebsessionManager_USCORElogon(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
                std::cout << "VirtualPox objref: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "getsession"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IWebsessionManager_USCOREgetSessionObject req;
            req.refIVirtualPox = argv[ap + 1];
            _vpox__IWebsessionManager_USCOREgetSessionObjectResponse resp;

            if (!(soaprc = soap_call___vpox__IWebsessionManager_USCOREgetSessionObject(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
                std::cout << "session: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "logoff"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IWebsessionManager_USCORElogoff req;
            req.refIVirtualPox = argv[ap + 1];
            _vpox__IWebsessionManager_USCORElogoffResponse resp;

            if (!(soaprc = soap_call___vpox__IWebsessionManager_USCORElogoff(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
            {
                ;
            }
        }
    }
    else if (!strcmp(pcszMode, "version"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPox_USCOREgetVersion req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IVirtualPox_USCOREgetVersionResponse resp;

            if (!(soaprc = soap_call___vpox__IVirtualPox_USCOREgetVersion(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
                std::cout << "version: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "gethost"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPox_USCOREgetHost req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IVirtualPox_USCOREgetHostResponse resp;

            if (!(soaprc = soap_call___vpox__IVirtualPox_USCOREgetHost(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
            {
                std::cout << "Host objref " << resp.returnval << "\n";
            }
        }
    }
    else if (!strcmp(pcszMode, "getpc"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPox_USCOREgetPerformanceCollector req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IVirtualPox_USCOREgetPerformanceCollectorResponse resp;

            if (!(soaprc = soap_call___vpox__IVirtualPox_USCOREgetPerformanceCollector(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
            {
                std::cout << "Performance collector objref " << resp.returnval << "\n";
            }
        }
    }
    else if (!strcmp(pcszMode, "getmachines"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPox_USCOREgetMachines req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IVirtualPox_USCOREgetMachinesResponse resp;

            if (!(soaprc = soap_call___vpox__IVirtualPox_USCOREgetMachines(&soap,
                                                                pcszArgEndpoint,
                                                                NULL,
                                                                &req,
                                                                &resp)))
            {
                size_t c = resp.returnval.size();
                for (size_t i = 0;
                     i < c;
                     ++i)
                {
                    std::cout << "Machine " << i << ": objref " << resp.returnval[i] << "\n";
                }
            }
        }
    }
    else if (!strcmp(pcszMode, "createmachine"))
    {
        if (argc < 4 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPox_USCOREcreateMachine req;
            req._USCOREthis = argv[ap + 1];
            req.settingsFile = argv[ap + 2];
            req.name = argv[ap + 3];
            std::cout << "createmachine: settingsFile = \"" << req.settingsFile << "\", name = \"" << req.name << "\"\n";
            _vpox__IVirtualPox_USCOREcreateMachineResponse resp;

            if (!(soaprc = soap_call___vpox__IVirtualPox_USCOREcreateMachine(&soap,
                                                                  pcszArgEndpoint,
                                                                  NULL,
                                                                  &req,
                                                                  &resp)))
                std::cout << "Machine created: managed object reference ID is " << resp.returnval << "\n";
        }
    }
    else if (!strcmp(pcszMode, "registermachine"))
    {
        if (argc < 3 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPox_USCOREregisterMachine req;
            req._USCOREthis = argv[ap + 1];
            req.machine = argv[ap + 2];
            _vpox__IVirtualPox_USCOREregisterMachineResponse resp;
            if (!(soaprc = soap_call___vpox__IVirtualPox_USCOREregisterMachine(&soap,
                                                                    pcszArgEndpoint,
                                                                    NULL,
                                                                    &req,
                                                                    &resp)))
                std::cout << "Machine registered.\n";
        }
    }
    else if (!strcmp(pcszMode, "getdvddrives"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IHost_USCOREgetDVDDrives req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IHost_USCOREgetDVDDrivesResponse resp;
            if (!(soaprc = soap_call___vpox__IHost_USCOREgetDVDDrives(&soap,
                                                           pcszArgEndpoint,
                                                           NULL,
                                                           &req,
                                                           &resp)))
            {
                size_t c = resp.returnval.size();
                for (size_t i = 0;
                    i < c;
                    ++i)
                {
                    std::cout << "DVD drive " << i << ": objref " << resp.returnval[i] << "\n";
                }
            }
        }
    }
    else if (!strcmp(pcszMode, "getname"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IMachine_USCOREgetName req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IMachine_USCOREgetNameResponse resp;
            if (!(soaprc = soap_call___vpox__IMachine_USCOREgetName(&soap,
                                                         pcszArgEndpoint,
                                                         NULL,
                                                         &req,
                                                         &resp)))
                printf("Name is: %s\n", resp.returnval.c_str());
        }
    }
    else if (!strcmp(pcszMode, "getid"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IMachine_USCOREgetId req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IMachine_USCOREgetIdResponse resp;
            if (!(soaprc = soap_call___vpox__IMachine_USCOREgetId(&soap,
                                                       pcszArgEndpoint,
                                                       NULL,
                                                       &req,
                                                       &resp)))
                std::cout << "UUID is: " << resp.returnval << "\n";;
        }
    }
    else if (!strcmp(pcszMode, "getostypeid"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IMachine_USCOREgetOSTypeId req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IMachine_USCOREgetOSTypeIdResponse resp;
            if (!(soaprc = soap_call___vpox__IMachine_USCOREgetOSTypeId(&soap,
                                                             pcszArgEndpoint,
                                                             NULL,
                                                             &req,
                                                             &resp)))
                std::cout << "Guest OS type is: " << resp.returnval << "\n";
        }
    }
    else if (!strcmp(pcszMode, "savesettings"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IMachine_USCOREsaveSettings req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IMachine_USCOREsaveSettingsResponse resp;
            if (!(soaprc = soap_call___vpox__IMachine_USCOREsaveSettings(&soap,
                                                              pcszArgEndpoint,
                                                              NULL,
                                                              &req,
                                                              &resp)))
                std::cout << "Settings saved\n";
        }
    }
    else if (!strcmp(pcszMode, "setupmetrics"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IPerformanceCollector_USCOREsetupMetrics req;
            req._USCOREthis = argv[ap + 1];
//             req.metricNames[0] = "*";
//             req.objects
            req.period = 1;     // seconds
            req.count = 100;
            _vpox__IPerformanceCollector_USCOREsetupMetricsResponse resp;
            if (!(soaprc = soap_call___vpox__IPerformanceCollector_USCOREsetupMetrics(&soap,
                                                              pcszArgEndpoint,
                                                              NULL,
                                                              &req,
                                                              &resp)))
            {
                size_t c = resp.returnval.size();
                for (size_t i = 0;
                     i < c;
                     ++i)
                {
                    std::cout << "Metric " << i << ": objref " << resp.returnval[i] << "\n";
                }
            }
        }
    }
    else if (!strcmp(pcszMode, "querymetricsdata"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IPerformanceCollector_USCOREqueryMetricsData req;
            req._USCOREthis = argv[ap + 1];
//             req.metricNames[0] = "*";
//             req.objects
            _vpox__IPerformanceCollector_USCOREqueryMetricsDataResponse resp;
            if (!(soaprc = soap_call___vpox__IPerformanceCollector_USCOREqueryMetricsData(&soap,
                                                              pcszArgEndpoint,
                                                              NULL,
                                                              &req,
                                                              &resp)))
            {
                size_t c = resp.returnval.size();
                for (size_t i = 0;
                     i < c;
                     ++i)
                {
                    std::cout << "long " << i << ": " << resp.returnval[i] << "\n";
                }
            }
        }
    }
    else if (!strcmp(pcszMode, "errorinfo"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IVirtualPoxErrorInfo_USCOREgetResultCode req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IVirtualPoxErrorInfo_USCOREgetResultCodeResponse resp;
            if (!(soaprc = soap_call___vpox__IVirtualPoxErrorInfo_USCOREgetResultCode(&soap,
                                                                                      pcszArgEndpoint,
                                                                                      NULL,
                                                                                      &req,
                                                                                      &resp)))
            {
                std::cout << "ErrorInfo ResultCode: " << std::hex << resp.returnval << "\n";

                _vpox__IVirtualPoxErrorInfo_USCOREgetText req2;
                req2._USCOREthis = argv[ap + 1];
                _vpox__IVirtualPoxErrorInfo_USCOREgetTextResponse resp2;
                if (!(soaprc = soap_call___vpox__IVirtualPoxErrorInfo_USCOREgetText(&soap,
                                                                                    pcszArgEndpoint,
                                                                                    NULL,
                                                                                    &req2,
                                                                                    &resp2)))
                {
                    std::cout << "ErrorInfo Text:       " << resp2.returnval << "\n";

                    _vpox__IVirtualPoxErrorInfo_USCOREgetNext req3;
                    req3._USCOREthis = argv[ap + 1];
                    _vpox__IVirtualPoxErrorInfo_USCOREgetNextResponse resp3;
                    if (!(soaprc = soap_call___vpox__IVirtualPoxErrorInfo_USCOREgetNext(&soap,
                                                                                        pcszArgEndpoint,
                                                                                        NULL,
                                                                                        &req3,
                                                                                        &resp3)))
                        std::cout << "Next ErrorInfo:       " << resp3.returnval << "\n";
                }
            }
        }
    }
    else if (!strcmp(pcszMode, "release"))
    {
        if (argc < 2 + ap)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vpox__IManagedObjectRef_USCORErelease req;
            req._USCOREthis = argv[ap + 1];
            _vpox__IManagedObjectRef_USCOREreleaseResponse resp;
            if (!(soaprc = soap_call___vpox__IManagedObjectRef_USCORErelease(&soap,
                                                                  pcszArgEndpoint,
                                                                  NULL,
                                                                  &req,
                                                                  &resp)))
                std::cout << "Managed object reference " << req._USCOREthis << " released.\n";
        }
    }
    else
        std::cout << "Unknown mode parameter \"" << pcszMode << "\".\n";

    if (soaprc)
    {
        if (    (soap.fault)
             && (soap.fault->detail)
           )
        {
            // generic fault message whether the fault is known or not
            std::cerr << "Generic fault message:\n";
            soap_print_fault(&soap, stderr); // display the SOAP fault message on the stderr stream

            if (soap.fault->detail->vpox__InvalidObjectFault)
            {
                std::cerr << "Bad object ID: " << soap.fault->detail->vpox__InvalidObjectFault->badObjectID << "\n";
            }
            else if (soap.fault->detail->vpox__RuntimeFault)
            {
                std::cerr << "Result code:   0x" << std::hex << soap.fault->detail->vpox__RuntimeFault->resultCode << "\n";
                std::cerr << "ErrorInfo:     " << soap.fault->detail->vpox__RuntimeFault->returnval << "\n";
            }
        }
        else
        {
            std::cerr << "Invalid fault data, fault message:\n";
            soap_print_fault(&soap, stderr); // display the SOAP fault message on the stderr stream
        }
    }

    soap_destroy(&soap); // delete deserialized class instances (for C++ only)
    soap_end(&soap); // remove deserialized data and clean up
    soap_done(&soap); // detach the gSOAP environment

    return soaprc;
}

