/* $Id: module.cpp $ */
/** @file
 * XPCOM module implementation functions
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

#define LOG_GROUP LOG_GROUP_MAIN

/* Make sure all the stdint.h macros are included - must come first! */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#include <nsIGenericFactory.h>

// generated file
#include <VPox/com/VirtualPox.h>

#include "SessionImpl.h"
#include "VirtualPoxClientImpl.h"
#include "RemoteUSBDeviceImpl.h"
#include "USBDeviceImpl.h"

// XPCOM glue code unfolding

/*
 * Declare extern variables here to tell the compiler that
 * NS_DECL_CLASSINFO(SessionWrap)
 * already exists in the VPoxAPIWrap library.
 */
NS_DECL_CI_INTERFACE_GETTER(SessionWrap)
extern nsIClassInfo *NS_CLASSINFO_NAME(SessionWrap);

/*
 * Declare extern variables here to tell the compiler that
 * NS_DECL_CLASSINFO(VirtualPoxClientWrap)
 * already exists in the VPoxAPIWrap library.
 */
NS_DECL_CI_INTERFACE_GETTER(VirtualPoxClientWrap)
extern nsIClassInfo *NS_CLASSINFO_NAME(VirtualPoxClientWrap);

/**
 *  Singleton class factory that holds a reference to the created instance
 *  (preventing it from being destroyed) until the module is explicitly
 *  unloaded by the XPCOM shutdown code.
 *
 *  Suitable for IN-PROC components.
 */
class VirtualPoxClientClassFactory : public VirtualPoxClient
{
public:
    virtual ~VirtualPoxClientClassFactory()
    {
        FinalRelease();
        instance = 0;
    }

    static nsresult GetInstance(VirtualPoxClient **inst)
    {
        int rv = NS_OK;
        if (instance == 0)
        {
            instance = new VirtualPoxClientClassFactory();
            if (instance)
            {
                instance->AddRef(); // protect FinalConstruct()
                rv = instance->FinalConstruct();
                if (NS_FAILED(rv))
                    instance->Release();
                else
                    instance->AddRef(); // self-reference
            }
            else
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else
        {
            instance->AddRef();
        }
        *inst = instance;
        return rv;
    }

    static nsresult FactoryDestructor()
    {
        if (instance)
            instance->Release();
        return NS_OK;
    }

private:
    static VirtualPoxClient *instance;
};

VirtualPoxClient *VirtualPoxClientClassFactory::instance = nsnull;


NS_GENERIC_FACTORY_CONSTRUCTOR_WITH_RC(Session)

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR_WITH_RC(VirtualPoxClient, VirtualPoxClientClassFactory::GetInstance)

/**
 *  Component definition table.
 *  Lists all components defined in this module.
 */
static const nsModuleComponentInfo components[] =
{
    {
        "Session component", // description
        NS_SESSION_CID, NS_SESSION_CONTRACTID, // CID/ContractID
        SessionConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
        NULL, // destructor function
        NS_CI_INTERFACE_GETTER_NAME(SessionWrap), // interfaces function
        NULL, // language helper
        &NS_CLASSINFO_NAME(SessionWrap) // global class info & flags
    },
    {
        "VirtualPoxClient component", // description
        NS_VIRTUALPOXCLIENT_CID, NS_VIRTUALPOXCLIENT_CONTRACTID, // CID/ContractID
        VirtualPoxClientConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
        VirtualPoxClientClassFactory::FactoryDestructor, // destructor function
        NS_CI_INTERFACE_GETTER_NAME(VirtualPoxClientWrap), // interfaces function
        NULL, // language helper
        &NS_CLASSINFO_NAME(VirtualPoxClientWrap) // global class info & flags
    },
};

NS_IMPL_NSGETMODULE (VirtualPox_Client_Module, components)
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
