/* $Id: ComHostUtils.cpp $ */
/** @file
 * ComHostUtils.cpp
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
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
#if defined(RT_OS_WINDOWS) && !defined(VPOX_COM_OUTOFPROC_MODULE)
# define VPOX_COM_OUTOFPROC_MODULE
#endif
#include <VPox/com/com.h>
#include <VPox/com/listeners.h>
#include <VPox/com/string.h>
#include <VPox/com/Guid.h>
#include <VPox/com/array.h>
#include <VPox/com/ErrorInfo.h>
#include <VPox/com/errorprint.h>
#include <VPox/com/EventQueue.h>
#include <VPox/com/VirtualPox.h>

#include <iprt/alloca.h>
#include <iprt/buildconfig.h>
#include <iprt/errcore.h>
#include <iprt/net.h>                   /* must come before getopt */
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/string.h>


#include "../NetLib/VPoxNetLib.h"
#include "../NetLib/shared_ptr.h"

#include <vector>
#include <list>
#include <string>
#include <map>

#include "../NetLib/VPoxNetBaseService.h"

#ifdef RT_OS_WINDOWS /* WinMain */
# include <iprt/win/windows.h>
# include <stdlib.h>
# ifdef INET_ADDRSTRLEN
/* On Windows INET_ADDRSTRLEN defined as 22 Ws2ipdef.h, because it include port number */
#  undef INET_ADDRSTRLEN
# endif
# define INET_ADDRSTRLEN 16
#else
# include <netinet/in.h>
#endif

#include "utils.h"


VPOX_LISTENER_DECLARE(NATNetworkListenerImpl)


int localMappings(const ComNatPtr& nat, AddressToOffsetMapping& mapping)
{
    mapping.clear();

    ComBstrArray strs;
    size_t cStrs;
    HRESULT hrc = nat->COMGETTER(LocalMappings)(ComSafeArrayAsOutParam(strs));
    if (   SUCCEEDED(hrc)
        && (cStrs = strs.size()))
    {
        for (size_t i = 0; i < cStrs; ++i)
        {
            char szAddr[17];
            RTNETADDRIPV4 ip4addr;
            char *pszTerm;
            uint32_t u32Off;
            com::Utf8Str strLo2Off(strs[i]);
            const char *pszLo2Off = strLo2Off.c_str();

            RT_ZERO(szAddr);

            pszTerm = RTStrStr(pszLo2Off, "=");

            if (   pszTerm
                   && (pszTerm - pszLo2Off) <= INET_ADDRSTRLEN)
            {
                memcpy(szAddr, pszLo2Off, (pszTerm - pszLo2Off));
                int rc = RTNetStrToIPv4Addr(szAddr, &ip4addr);
                if (RT_SUCCESS(rc))
                {
                    u32Off = RTStrToUInt32(pszTerm + 1);
                    if (u32Off != 0)
                        mapping.insert(
                          AddressToOffsetMapping::value_type(ip4addr, u32Off));
                }
            }
        }
    }
    else
        return VERR_NOT_FOUND;

    return VINF_SUCCESS;
}


int hostDnsSearchList(const ComHostPtr& host, std::vector<std::string>& strings)
{
    strings.clear();

    ComBstrArray strs;
    if (SUCCEEDED(host->COMGETTER(SearchStrings)(ComSafeArrayAsOutParam(strs))))
    {
        for (unsigned int i = 0; i < strs.size(); ++i)
        {
            strings.push_back(com::Utf8Str(strs[i]).c_str());
        }
    }
    else
        return VERR_NOT_FOUND;

    return VINF_SUCCESS;
}


int hostDnsDomain(const ComHostPtr& host, std::string& domainStr)
{
    com::Bstr domain;
    if (SUCCEEDED(host->COMGETTER(DomainName)(domain.asOutParam())))
    {
        domainStr = com::Utf8Str(domain).c_str();
        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}


int createNatListener(ComNatListenerPtr& listener, const ComVirtualPoxPtr& vpoxptr,
                      NATNetworkEventAdapter *adapter, /* const */ ComEventTypeArray& events)
{
    ComObjPtr<NATNetworkListenerImpl> obj;
    HRESULT hrc = obj.createObject();
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = obj->init(new NATNetworkListener(), adapter);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    ComPtr<IEventSource> esVPox;
    hrc = vpoxptr->COMGETTER(EventSource)(esVPox.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    listener = obj;

    hrc = esVPox->RegisterListener(listener, ComSafeArrayAsInParam(events), true);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    return VINF_SUCCESS;
}

int destroyNatListener(ComNatListenerPtr& listener, const ComVirtualPoxPtr& vpoxptr)
{
    if (listener)
    {
        ComPtr<IEventSource> esVPox;
        HRESULT hrc = vpoxptr->COMGETTER(EventSource)(esVPox.asOutParam());
        AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        if (!esVPox.isNull())
        {
            hrc = esVPox->UnregisterListener(listener);
            AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        }
        listener.setNull();
    }
    return VINF_SUCCESS;
}

int createClientListener(ComNatListenerPtr& listener, const ComVirtualPoxClientPtr& vpoxclientptr,
                         NATNetworkEventAdapter *adapter, /* const */ ComEventTypeArray& events)
{
    ComObjPtr<NATNetworkListenerImpl> obj;
    HRESULT hrc = obj.createObject();
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = obj->init(new NATNetworkListener(), adapter);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    ComPtr<IEventSource> esVPox;
    hrc = vpoxclientptr->COMGETTER(EventSource)(esVPox.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    listener = obj;

    hrc = esVPox->RegisterListener(listener, ComSafeArrayAsInParam(events), true);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    return VINF_SUCCESS;
}

int destroyClientListener(ComNatListenerPtr& listener, const ComVirtualPoxClientPtr& vpoxclientptr)
{
    if (listener)
    {
        ComPtr<IEventSource> esVPox;
        HRESULT hrc = vpoxclientptr->COMGETTER(EventSource)(esVPox.asOutParam());
        AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        if (!esVPox.isNull())
        {
            hrc = esVPox->UnregisterListener(listener);
            AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        }
        listener.setNull();
    }
    return VINF_SUCCESS;
}
