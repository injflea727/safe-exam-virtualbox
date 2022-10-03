/* $Id: VPoxNetFltNobj.h $ */
/** @file
 * VPoxNetFltNobj.h - Notify Object for Bridged Networking Driver.
 * Used to filter Bridged Networking Driver bindings
 */
/*
 * Copyright (C) 2011-2020 Oracle Corporation
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetFlt_win_nobj_VPoxNetFltNobj_h
#define VPOX_INCLUDED_SRC_VPoxNetFlt_win_nobj_VPoxNetFltNobj_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>

#include "VPox/com/defs.h"
#include "VPoxNetFltNobjT.h"
#include "VPoxNetFltNobjRc.h"

#define VPOXNETFLTNOTIFY_ONFAIL_BINDDEFAULT false

/*
 * VirtualPox Bridging driver notify object.
 * Needed to make our driver bind to "real" host adapters only
 */
class ATL_NO_VTABLE VPoxNetFltNobj :
    public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
    public ATL::CComCoClass<VPoxNetFltNobj, &CLSID_VPoxNetFltNobj>,
    public INetCfgComponentControl,
    public INetCfgComponentNotifyBinding
{
public:
    VPoxNetFltNobj();
    virtual ~VPoxNetFltNobj();

    BEGIN_COM_MAP(VPoxNetFltNobj)
        COM_INTERFACE_ENTRY(INetCfgComponentControl)
        COM_INTERFACE_ENTRY(INetCfgComponentNotifyBinding)
    END_COM_MAP()

    // this is a "just in case" conditional, which is not defined
#ifdef VPOX_FORCE_REGISTER_SERVER
    DECLARE_REGISTRY_RESOURCEID(IDR_VPOXNETFLT_NOBJ)
#endif

    /* INetCfgComponentControl methods */
    STDMETHOD(Initialize)(IN INetCfgComponent *pNetCfgComponent, IN INetCfg *pNetCfg, IN BOOL bInstalling);
    STDMETHOD(ApplyRegistryChanges)();
    STDMETHOD(ApplyPnpChanges)(IN INetCfgPnpReconfigCallback *pCallback);
    STDMETHOD(CancelChanges)();

    /* INetCfgComponentNotifyBinding methods */
    STDMETHOD(NotifyBindingPath)(IN DWORD dwChangeFlag, IN INetCfgBindingPath *pNetCfgBP);
    STDMETHOD(QueryBindingPath)(IN DWORD dwChangeFlag, IN INetCfgBindingPath *pNetCfgBP);
private:

    void init(IN INetCfgComponent *pNetCfgComponent, IN INetCfg *pNetCfg, IN BOOL bInstalling);
    void cleanup();

    /* these two used to maintain the component info passed to
     * INetCfgComponentControl::Initialize */
    INetCfg *mpNetCfg;
    INetCfgComponent *mpNetCfgComponent;
    BOOL mbInstalling;
};

#endif /* !VPOX_INCLUDED_SRC_VPoxNetFlt_win_nobj_VPoxNetFltNobj_h */
