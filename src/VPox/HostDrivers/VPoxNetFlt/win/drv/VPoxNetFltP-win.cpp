/* $Id: VPoxNetFltP-win.cpp $ */
/** @file
 * VPoxNetFltP-win.cpp - Bridged Networking Driver, Windows Specific Code.
 * Protocol edge
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
#include "VPoxNetFltCmn-win.h"

#ifdef VPOXNETADP
# error "No protocol edge"
#endif

#define VPOXNETFLT_PT_STATUS_IS_FILTERED(_s) (\
       (_s) == NDIS_STATUS_MEDIA_CONNECT \
    || (_s) == NDIS_STATUS_MEDIA_DISCONNECT \
    )

/**
 * performs binding to the given adapter
 */
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtDoBinding(PVPOXNETFLTINS pThis, PNDIS_STRING pOurDeviceName, PNDIS_STRING pBindToDeviceName)
{
    Assert(pThis->u.s.WinIf.PtState.PowerState == NdisDeviceStateD3);
    Assert(pThis->u.s.WinIf.PtState.OpState == kVPoxNetDevOpState_Deinitialized);
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    vpoxNetFltWinSetOpState(&pThis->u.s.WinIf.PtState, kVPoxNetDevOpState_Initializing);

    NDIS_STATUS Status = vpoxNetFltWinCopyString(&pThis->u.s.WinIf.MpDeviceName, pOurDeviceName);
    Assert (Status == NDIS_STATUS_SUCCESS);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        vpoxNetFltWinSetPowerState(&pThis->u.s.WinIf.PtState, NdisDeviceStateD0);
        pThis->u.s.WinIf.OpenCloseStatus = NDIS_STATUS_SUCCESS;

        UINT iMedium;
        NDIS_STATUS TmpStatus;
        NDIS_MEDIUM aenmNdisMedium[] =
        {
                /* Ethernet */
                NdisMedium802_3,
                /* Wan */
                NdisMediumWan
        };

        NdisResetEvent(&pThis->u.s.WinIf.OpenCloseEvent);

        NdisOpenAdapter(&Status, &TmpStatus, &pThis->u.s.WinIf.hBinding, &iMedium,
                aenmNdisMedium, RT_ELEMENTS(aenmNdisMedium),
                g_VPoxNetFltGlobalsWin.Pt.hProtocol,
                pThis,
                pBindToDeviceName,
                0, /* IN UINT OpenOptions, (reserved, should be NULL) */
                NULL /* IN PSTRING AddressingInformation  OPTIONAL */
                );
        Assert(Status == NDIS_STATUS_PENDING || Status == STATUS_SUCCESS);
        if (Status == NDIS_STATUS_PENDING)
        {
            NdisWaitEvent(&pThis->u.s.WinIf.OpenCloseEvent, 0);
            Status = pThis->u.s.WinIf.OpenCloseStatus;
        }

        Assert(Status == NDIS_STATUS_SUCCESS);
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Assert(pThis->u.s.WinIf.hBinding);
            pThis->u.s.WinIf.enmMedium = aenmNdisMedium[iMedium];
            vpoxNetFltWinSetOpState(&pThis->u.s.WinIf.PtState, kVPoxNetDevOpState_Initialized);

            Status = vpoxNetFltWinMpInitializeDevideInstance(pThis);
            Assert(Status == NDIS_STATUS_SUCCESS);
            if (Status == NDIS_STATUS_SUCCESS)
            {
                return NDIS_STATUS_SUCCESS;
            }
            else
            {
                LogRelFunc(("vpoxNetFltWinMpInitializeDevideInstance failed, Status 0x%x\n", Status));
            }

            vpoxNetFltWinSetOpState(&pThis->u.s.WinIf.PtState, kVPoxNetDevOpState_Deinitializing);
            vpoxNetFltWinPtCloseInterface(pThis, &TmpStatus);
            Assert(TmpStatus == NDIS_STATUS_SUCCESS);
            vpoxNetFltWinSetOpState(&pThis->u.s.WinIf.PtState, kVPoxNetDevOpState_Deinitialized);
        }
        else
        {
            LogRelFunc(("NdisOpenAdapter failed, Status (0x%x)", Status));
        }

        vpoxNetFltWinSetOpState(&pThis->u.s.WinIf.PtState, kVPoxNetDevOpState_Deinitialized);
        pThis->u.s.WinIf.hBinding = NULL;
    }

    return Status;
}

static VOID vpoxNetFltWinPtBindAdapter(OUT PNDIS_STATUS pStatus,
        IN NDIS_HANDLE hBindContext,
        IN PNDIS_STRING pDeviceNameStr,
        IN PVOID pvSystemSpecific1,
        IN PVOID pvSystemSpecific2)
{
    LogFlowFuncEnter();
    RT_NOREF2(hBindContext, pvSystemSpecific2);

    NDIS_STATUS Status;
    NDIS_HANDLE hConfig = NULL;

    NdisOpenProtocolConfiguration(&Status, &hConfig, (PNDIS_STRING)pvSystemSpecific1);
    Assert(Status == NDIS_STATUS_SUCCESS);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        PNDIS_CONFIGURATION_PARAMETER pParam;
        NDIS_STRING UppedBindStr = NDIS_STRING_CONST("UpperBindings");
        NdisReadConfiguration(&Status, &pParam, hConfig, &UppedBindStr, NdisParameterString);
        Assert(Status == NDIS_STATUS_SUCCESS);
        if (Status == NDIS_STATUS_SUCCESS)
        {
            PVPOXNETFLTINS pNetFlt;
            Status = vpoxNetFltWinPtInitBind(&pNetFlt, &pParam->ParameterData.StringData, pDeviceNameStr);
            Assert(Status == NDIS_STATUS_SUCCESS);
        }

        NdisCloseConfiguration(hConfig);
    }

    *pStatus = Status;

    LogFlowFunc(("LEAVE: Status 0x%x\n", Status));
}

static VOID vpoxNetFltWinPtOpenAdapterComplete(IN NDIS_HANDLE hProtocolBindingContext, IN NDIS_STATUS Status, IN NDIS_STATUS OpenErrorStatus)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;
    RT_NOREF1(OpenErrorStatus);

    LogFlowFunc(("ENTER: pNetFlt (0x%p), Status (0x%x), OpenErrorStatus(0x%x)\n", pNetFlt, Status, OpenErrorStatus));
    Assert(pNetFlt->u.s.WinIf.OpenCloseStatus == NDIS_STATUS_SUCCESS);
    Assert(Status == NDIS_STATUS_SUCCESS);
    if (pNetFlt->u.s.WinIf.OpenCloseStatus == NDIS_STATUS_SUCCESS)
    {
        pNetFlt->u.s.WinIf.OpenCloseStatus = Status;
        Assert(Status == NDIS_STATUS_SUCCESS);
        if (Status != NDIS_STATUS_SUCCESS)
            LogRelFunc(("Open Complete status is 0x%x", Status));
    }
    else
        LogRelFunc(("Adapter maintained status is 0x%x", pNetFlt->u.s.WinIf.OpenCloseStatus));
    NdisSetEvent(&pNetFlt->u.s.WinIf.OpenCloseEvent);
    LogFlowFunc(("LEAVE: pNetFlt (0x%p), Status (0x%x), OpenErrorStatus(0x%x)\n", pNetFlt, Status, OpenErrorStatus));
}

static void vpoxNetFltWinPtRequestsWaitComplete(PVPOXNETFLTINS pNetFlt)
{
    /* wait for request to complete */
    while (vpoxNetFltWinAtomicUoReadWinState(pNetFlt->u.s.WinIf.StateFlags).fRequestInfo == VPOXNDISREQUEST_INPROGRESS)
    {
        vpoxNetFltWinSleep(2);
    }

    /*
     * If the below miniport is going to low power state, complete the queued request
     */
    RTSpinlockAcquire(pNetFlt->hSpinlock);
    if (pNetFlt->u.s.WinIf.StateFlags.fRequestInfo & VPOXNDISREQUEST_QUEUED)
    {
        /* mark the request as InProgress before posting it to RequestComplete */
        pNetFlt->u.s.WinIf.StateFlags.fRequestInfo = VPOXNDISREQUEST_INPROGRESS;
        RTSpinlockRelease(pNetFlt->hSpinlock);
        vpoxNetFltWinPtRequestComplete(pNetFlt, &pNetFlt->u.s.WinIf.PassDownRequest, NDIS_STATUS_FAILURE);
    }
    else
    {
        RTSpinlockRelease(pNetFlt->hSpinlock);
    }
}

DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtDoUnbinding(PVPOXNETFLTINS pNetFlt, bool bOnUnbind)
{
    NDIS_STATUS Status;
    uint64_t NanoTS = RTTimeSystemNanoTS();
    int cPPUsage;

    LogFlowFunc(("ENTER: pNetFlt 0x%p\n", pNetFlt));

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    Assert(vpoxNetFltWinGetOpState(&pNetFlt->u.s.WinIf.PtState) == kVPoxNetDevOpState_Initialized);

    RTSpinlockAcquire(pNetFlt->hSpinlock);

    ASMAtomicUoWriteBool(&pNetFlt->fDisconnectedFromHost, true);
    ASMAtomicUoWriteBool(&pNetFlt->fRediscoveryPending, false);
    ASMAtomicUoWriteU64(&pNetFlt->NanoTSLastRediscovery, NanoTS);

    vpoxNetFltWinSetOpState(&pNetFlt->u.s.WinIf.PtState, kVPoxNetDevOpState_Deinitializing);
    if (!bOnUnbind)
    {
        vpoxNetFltWinSetOpState(&pNetFlt->u.s.WinIf.MpState, kVPoxNetDevOpState_Deinitializing);
    }

    RTSpinlockRelease(pNetFlt->hSpinlock);

    vpoxNetFltWinPtRequestsWaitComplete(pNetFlt);

    vpoxNetFltWinWaitDereference(&pNetFlt->u.s.WinIf.MpState);
    vpoxNetFltWinWaitDereference(&pNetFlt->u.s.WinIf.PtState);

    /* check packet pool is empty */
    cPPUsage = NdisPacketPoolUsage(pNetFlt->u.s.WinIf.hSendPacketPool);
    Assert(cPPUsage == 0);
    cPPUsage = NdisPacketPoolUsage(pNetFlt->u.s.WinIf.hRecvPacketPool);
    Assert(cPPUsage == 0);
    /* for debugging only, ignore the err in release */
    NOREF(cPPUsage);

    if (!bOnUnbind || !vpoxNetFltWinMpDeInitializeDeviceInstance(pNetFlt, &Status))
    {
        vpoxNetFltWinPtCloseInterface(pNetFlt, &Status);
        vpoxNetFltWinSetOpState(&pNetFlt->u.s.WinIf.PtState, kVPoxNetDevOpState_Deinitialized);

        if (!bOnUnbind)
        {
            Assert(vpoxNetFltWinGetOpState(&pNetFlt->u.s.WinIf.MpState) == kVPoxNetDevOpState_Deinitializing);
            vpoxNetFltWinSetOpState(&pNetFlt->u.s.WinIf.MpState, kVPoxNetDevOpState_Deinitialized);
        }
        else
        {
            Assert(vpoxNetFltWinGetOpState(&pNetFlt->u.s.WinIf.MpState) == kVPoxNetDevOpState_Deinitialized);
        }
    }
    else
    {
        Assert(vpoxNetFltWinGetOpState(&pNetFlt->u.s.WinIf.MpState) == kVPoxNetDevOpState_Deinitialized);
    }

    LogFlowFunc(("LEAVE: pNetFlt 0x%p\n", pNetFlt));

    return Status;
}

static VOID vpoxNetFltWinPtUnbindAdapter(OUT PNDIS_STATUS pStatus,
        IN NDIS_HANDLE hContext,
        IN NDIS_HANDLE hUnbindContext)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hContext;
    RT_NOREF1(hUnbindContext);

    LogFlowFunc(("ENTER: pNetFlt (0x%p)\n", pNetFlt));

    *pStatus = vpoxNetFltWinDetachFromInterface(pNetFlt, true);
    Assert(*pStatus == NDIS_STATUS_SUCCESS);

    LogFlowFunc(("LEAVE: pNetFlt (0x%p)\n", pNetFlt));
}

static VOID vpoxNetFltWinPtUnloadProtocol()
{
    LogFlowFuncEnter();
    NDIS_STATUS Status = vpoxNetFltWinPtDeregister(&g_VPoxNetFltGlobalsWin.Pt);
    Assert(Status == NDIS_STATUS_SUCCESS); NOREF(Status);
    LogFlowFunc(("LEAVE: PtDeregister Status (0x%x)\n", Status));
}


static VOID vpoxNetFltWinPtCloseAdapterComplete(IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)ProtocolBindingContext;

    LogFlowFunc(("ENTER: pNetFlt (0x%p), Status (0x%x)\n", pNetFlt, Status));
    Assert(pNetFlt->u.s.WinIf.OpenCloseStatus == NDIS_STATUS_SUCCESS);
    Assert(Status == NDIS_STATUS_SUCCESS);
    Assert(pNetFlt->u.s.WinIf.OpenCloseStatus == NDIS_STATUS_SUCCESS);
    if (pNetFlt->u.s.WinIf.OpenCloseStatus == NDIS_STATUS_SUCCESS)
    {
        pNetFlt->u.s.WinIf.OpenCloseStatus = Status;
    }
    NdisSetEvent(&pNetFlt->u.s.WinIf.OpenCloseEvent);
    LogFlowFunc(("LEAVE: pNetFlt (0x%p), Status (0x%x)\n", pNetFlt, Status));
}

static VOID vpoxNetFltWinPtResetComplete(IN NDIS_HANDLE hProtocolBindingContext, IN NDIS_STATUS Status)
{
    RT_NOREF2(hProtocolBindingContext, Status);
    LogFlowFunc(("ENTER: pNetFlt 0x%p, Status 0x%x\n", hProtocolBindingContext, Status));
    /*
     * should never be here
     */
    AssertFailed();
    LogFlowFunc(("LEAVE: pNetFlt 0x%p, Status 0x%x\n", hProtocolBindingContext, Status));
}

static NDIS_STATUS vpoxNetFltWinPtHandleQueryInfoComplete(PVPOXNETFLTINS pNetFlt, NDIS_STATUS Status)
{
    PNDIS_REQUEST pRequest = &pNetFlt->u.s.WinIf.PassDownRequest;

    switch (pRequest->DATA.QUERY_INFORMATION.Oid)
    {
        case OID_PNP_CAPABILITIES:
        {
            if (Status == NDIS_STATUS_SUCCESS)
            {
                if (pRequest->DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof (NDIS_PNP_CAPABILITIES))
                {
                    PNDIS_PNP_CAPABILITIES pPnPCaps = (PNDIS_PNP_CAPABILITIES)(pRequest->DATA.QUERY_INFORMATION.InformationBuffer);
                    PNDIS_PM_WAKE_UP_CAPABILITIES pPmWuCaps = &pPnPCaps->WakeUpCapabilities;
                    pPmWuCaps->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
                    pPmWuCaps->MinPatternWakeUp = NdisDeviceStateUnspecified;
                    pPmWuCaps->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
                    *pNetFlt->u.s.WinIf.pcPDRBytesRW = sizeof (NDIS_PNP_CAPABILITIES);
                    *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = 0;
                    Status = NDIS_STATUS_SUCCESS;
                }
                else
                {
                    AssertFailed();
                    *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = sizeof(NDIS_PNP_CAPABILITIES);
                    Status = NDIS_STATUS_RESOURCES;
                }
            }
            break;
        }

        case OID_GEN_MAC_OPTIONS:
        {
            if (Status == NDIS_STATUS_SUCCESS)
            {
                if (pRequest->DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof (ULONG))
                {
                    pNetFlt->u.s.WinIf.fMacOptions = *(PULONG)pRequest->DATA.QUERY_INFORMATION.InformationBuffer;
#ifndef VPOX_LOOPBACK_USEFLAGS
                    /* clearing this flag tells ndis we'll handle loopback ourselves
                     * the ndis layer or nic driver below us would loopback packets as necessary */
                    *(PULONG)pRequest->DATA.QUERY_INFORMATION.InformationBuffer &= ~NDIS_MAC_OPTION_NO_LOOPBACK;
#else
                    /* we have to catch loopbacks from the underlying driver, so no duplications will occur,
                     * just indicate NDIS to handle loopbacks for the packets coming from the protocol */
                    *(PULONG)pRequest->DATA.QUERY_INFORMATION.InformationBuffer |= NDIS_MAC_OPTION_NO_LOOPBACK;
#endif
                }
                else
                {
                    AssertFailed();
                    *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = sizeof (ULONG);
                    Status = NDIS_STATUS_RESOURCES;
                }
            }
            break;
        }

        case OID_GEN_CURRENT_PACKET_FILTER:
        {
            if (VPOXNETFLT_PROMISCUOUS_SUPPORTED(pNetFlt))
            {
                /* we're here _ONLY_ in the passthru mode */
                Assert(pNetFlt->u.s.WinIf.StateFlags.fProcessingPacketFilter && !pNetFlt->u.s.WinIf.StateFlags.fPPFNetFlt);
                if (pNetFlt->u.s.WinIf.StateFlags.fProcessingPacketFilter && !pNetFlt->u.s.WinIf.StateFlags.fPPFNetFlt)
                {
                    Assert(pNetFlt->enmTrunkState != INTNETTRUNKIFSTATE_ACTIVE);
                    vpoxNetFltWinDereferenceModePassThru(pNetFlt);
                    vpoxNetFltWinDereferenceWinIf(pNetFlt);
                }

                if (Status == NDIS_STATUS_SUCCESS)
                {
                    if (pRequest->DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof (ULONG))
                    {
                        /* the filter request is issued below only in case netflt is not active,
                         * simply update the cache here */
                        /* cache the filter used by upper protocols */
                        pNetFlt->u.s.WinIf.fUpperProtocolSetFilter = *(PULONG)pRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                        pNetFlt->u.s.WinIf.StateFlags.fUpperProtSetFilterInitialized = TRUE;
                    }
                    else
                    {
                        AssertFailed();
                        *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = sizeof (ULONG);
                        Status = NDIS_STATUS_RESOURCES;
                    }
                }
            }
            break;
        }

        default:
            Assert(pRequest->DATA.QUERY_INFORMATION.Oid != OID_PNP_QUERY_POWER);
            break;
    }

    *pNetFlt->u.s.WinIf.pcPDRBytesRW = pRequest->DATA.QUERY_INFORMATION.BytesWritten;
    *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = pRequest->DATA.QUERY_INFORMATION.BytesNeeded;

    return Status;
}

static NDIS_STATUS vpoxNetFltWinPtHandleSetInfoComplete(PVPOXNETFLTINS pNetFlt, NDIS_STATUS Status)
{
    PNDIS_REQUEST pRequest = &pNetFlt->u.s.WinIf.PassDownRequest;

    switch (pRequest->DATA.SET_INFORMATION.Oid)
    {
        case OID_GEN_CURRENT_PACKET_FILTER:
        {
            if (VPOXNETFLT_PROMISCUOUS_SUPPORTED(pNetFlt))
            {
                Assert(Status == NDIS_STATUS_SUCCESS);
                if (pNetFlt->u.s.WinIf.StateFlags.fProcessingPacketFilter)
                {
                    if (pNetFlt->u.s.WinIf.StateFlags.fPPFNetFlt)
                    {
                        Assert(pNetFlt->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE);
                        pNetFlt->u.s.WinIf.StateFlags.fPPFNetFlt = 0;
                        if (Status == NDIS_STATUS_SUCCESS)
                        {
                            if (pRequest->DATA.SET_INFORMATION.InformationBufferLength >= sizeof (ULONG))
                            {
                                pNetFlt->u.s.WinIf.fOurSetFilter = *((PULONG)pRequest->DATA.SET_INFORMATION.InformationBuffer);
                                Assert(pNetFlt->u.s.WinIf.fOurSetFilter == NDIS_PACKET_TYPE_PROMISCUOUS);
                            }
                            else
                            {
                                AssertFailed();
                                *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = sizeof (ULONG);
                                Status = NDIS_STATUS_RESOURCES;
                            }
                        }
                        vpoxNetFltWinDereferenceNetFlt(pNetFlt);
                    }
                    else
                    {
                        Assert(pNetFlt->enmTrunkState != INTNETTRUNKIFSTATE_ACTIVE);

                        if (Status == NDIS_STATUS_SUCCESS)
                        {
                            if (pRequest->DATA.SET_INFORMATION.InformationBufferLength >= sizeof (ULONG))
                            {
                                /* the request was issued when the netflt was not active, simply update the cache here */
                                pNetFlt->u.s.WinIf.fUpperProtocolSetFilter = *((PULONG)pRequest->DATA.SET_INFORMATION.InformationBuffer);
                                pNetFlt->u.s.WinIf.StateFlags.fUpperProtSetFilterInitialized = TRUE;
                            }
                            else
                            {
                                AssertFailed();
                                *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = sizeof (ULONG);
                                Status = NDIS_STATUS_RESOURCES;
                            }
                        }
                        vpoxNetFltWinDereferenceModePassThru(pNetFlt);
                    }

                    pNetFlt->u.s.WinIf.StateFlags.fProcessingPacketFilter = 0;
                    vpoxNetFltWinDereferenceWinIf(pNetFlt);
                }
#ifdef DEBUG_misha
                else
                {
                    AssertFailed();
                }
#endif
            }
            break;
        }

        default:
            Assert(pRequest->DATA.SET_INFORMATION.Oid != OID_PNP_SET_POWER);
            break;
    }

    *pNetFlt->u.s.WinIf.pcPDRBytesRW = pRequest->DATA.SET_INFORMATION.BytesRead;
    *pNetFlt->u.s.WinIf.pcPDRBytesNeeded = pRequest->DATA.SET_INFORMATION.BytesNeeded;

    return Status;
}

DECLHIDDEN(VOID) vpoxNetFltWinPtRequestComplete(NDIS_HANDLE hContext, PNDIS_REQUEST pNdisRequest, NDIS_STATUS Status)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hContext;
    PNDIS_REQUEST pSynchRequest = pNetFlt->u.s.WinIf.pSynchRequest;

    LogFlowFunc(("ENTER: pNetFlt (0x%p), pNdisRequest (0x%p), Status (0x%x)\n", pNetFlt, pNdisRequest, Status));

    if (pSynchRequest == pNdisRequest)
    {
        /* asynchronous completion of our sync request */
        /*1.set the status */
        pNetFlt->u.s.WinIf.SynchCompletionStatus = Status;
        /* 2. set event */
        KeSetEvent(&pNetFlt->u.s.WinIf.hSynchCompletionEvent, 0, FALSE);
        /* 3. return; */

        LogFlowFunc(("LEAVE: pNetFlt (0x%p), pNdisRequest (0x%p), Status (0x%x)\n", pNetFlt, pNdisRequest, Status));
        return;
    }

    Assert(&pNetFlt->u.s.WinIf.PassDownRequest == pNdisRequest);
    Assert(pNetFlt->u.s.WinIf.StateFlags.fRequestInfo == VPOXNDISREQUEST_INPROGRESS);
    vpoxNetFltWinMpRequestStateComplete(pNetFlt);

    switch (pNdisRequest->RequestType)
    {
      case NdisRequestQueryInformation:
          Status = vpoxNetFltWinPtHandleQueryInfoComplete(pNetFlt, Status);
          NdisMQueryInformationComplete(pNetFlt->u.s.WinIf.hMiniport, Status);
          break;

      case NdisRequestSetInformation:
          Status = vpoxNetFltWinPtHandleSetInfoComplete(pNetFlt, Status);
          NdisMSetInformationComplete(pNetFlt->u.s.WinIf.hMiniport, Status);
          break;

      default:
          AssertFailed();
          break;
    }

    LogFlowFunc(("LEAVE: pNetFlt (0x%p), pNdisRequest (0x%p), Status (0x%x)\n", pNetFlt, pNdisRequest, Status));
}

static VOID vpoxNetFltWinPtStatus(IN NDIS_HANDLE hProtocolBindingContext, IN NDIS_STATUS GeneralStatus, IN PVOID pvStatusBuffer, IN UINT cbStatusBuffer)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;

    LogFlowFunc(("ENTER: pNetFlt (0x%p), GeneralStatus (0x%x)\n", pNetFlt, GeneralStatus));

    if (vpoxNetFltWinReferenceWinIf(pNetFlt))
    {
        Assert(pNetFlt->u.s.WinIf.hMiniport);

        if (VPOXNETFLT_PT_STATUS_IS_FILTERED(GeneralStatus))
        {
            pNetFlt->u.s.WinIf.MpIndicatedMediaStatus = GeneralStatus;
        }
        NdisMIndicateStatus(pNetFlt->u.s.WinIf.hMiniport,
                            GeneralStatus,
                            pvStatusBuffer,
                            cbStatusBuffer);

        vpoxNetFltWinDereferenceWinIf(pNetFlt);
    }
    else
    {
        if (pNetFlt->u.s.WinIf.hMiniport != NULL
                && VPOXNETFLT_PT_STATUS_IS_FILTERED(GeneralStatus)
           )
        {
            pNetFlt->u.s.WinIf.MpUnindicatedMediaStatus = GeneralStatus;
        }
    }

    LogFlowFunc(("LEAVE: pNetFlt (0x%p), GeneralStatus (0x%x)\n", pNetFlt, GeneralStatus));
}


static VOID vpoxNetFltWinPtStatusComplete(IN NDIS_HANDLE hProtocolBindingContext)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;

    LogFlowFunc(("ENTER: pNetFlt (0x%p)\n", pNetFlt));

    if (vpoxNetFltWinReferenceWinIf(pNetFlt))
    {
        NdisMIndicateStatusComplete(pNetFlt->u.s.WinIf.hMiniport);

        vpoxNetFltWinDereferenceWinIf(pNetFlt);
    }

    LogFlowFunc(("LEAVE: pNetFlt (0x%p)\n", pNetFlt));
}

static VOID vpoxNetFltWinPtSendComplete(IN NDIS_HANDLE hProtocolBindingContext, IN PNDIS_PACKET pPacket, IN NDIS_STATUS Status)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;
    PVPOXNETFLT_PKTRSVD_PT pSendInfo = (PVPOXNETFLT_PKTRSVD_PT)pPacket->ProtocolReserved;
    PNDIS_PACKET pOrigPacket = pSendInfo->pOrigPacket;
    PVOID pBufToFree = pSendInfo->pBufToFree;
    LogFlowFunc(("ENTER: pNetFlt (0x%p), pPacket (0x%p), Status (0x%x)\n", pNetFlt, pPacket, Status));

#if defined(DEBUG_NETFLT_PACKETS) || !defined(VPOX_LOOPBACK_USEFLAGS)
    /** @todo for optimization we could check only for netflt-mode packets
     * do it for all for now */
     vpoxNetFltWinLbRemoveSendPacket(pNetFlt, pPacket);
#endif

     if (pOrigPacket)
     {
         NdisIMCopySendCompletePerPacketInfo(pOrigPacket, pPacket);
         NdisFreePacket(pPacket);
         /* the ptk was posted from the upperlying protocol */
         NdisMSendComplete(pNetFlt->u.s.WinIf.hMiniport, pOrigPacket, Status);
     }
     else
     {
         /* if the pOrigPacket is zero - the ptk was originated by netFlt send/receive
          * need to free packet buffers */
         vpoxNetFltWinFreeSGNdisPacket(pPacket, !pBufToFree);
     }

     if (pBufToFree)
     {
         vpoxNetFltWinMemFree(pBufToFree);
     }

    vpoxNetFltWinDereferenceWinIf(pNetFlt);

    LogFlowFunc(("LEAVE: pNetFlt (0x%p), pPacket (0x%p), Status (0x%x)\n", pNetFlt, pPacket, Status));
}

/**
 * removes searches for the packet in the list and removes it if found
 * @return true if the packet was found and removed, false - otherwise
 */
static bool vpoxNetFltWinRemovePacketFromList(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PNDIS_PACKET pPacket)
{
    PVPOXNETFLT_PKTRSVD_TRANSFERDATA_PT pTDR = (PVPOXNETFLT_PKTRSVD_TRANSFERDATA_PT)pPacket->ProtocolReserved;
    return vpoxNetFltWinInterlockedSearchListEntry(pList, &pTDR->ListEntry, true /* remove*/);
}

/**
 * puts the packet to the tail of the list
 */
static void vpoxNetFltWinPutPacketToList(PVPOXNETFLT_INTERLOCKED_SINGLE_LIST pList, PNDIS_PACKET pPacket, PNDIS_BUFFER pOrigBuffer)
{
    PVPOXNETFLT_PKTRSVD_TRANSFERDATA_PT pTDR = (PVPOXNETFLT_PKTRSVD_TRANSFERDATA_PT)pPacket->ProtocolReserved;
    pTDR->pOrigBuffer = pOrigBuffer;
    vpoxNetFltWinInterlockedPutTail(pList, &pTDR->ListEntry);
}

static bool vpoxNetFltWinPtTransferDataCompleteActive(PVPOXNETFLTINS pNetFltIf, PNDIS_PACKET pPacket, NDIS_STATUS Status)
{
    PNDIS_BUFFER pBuffer;
    PVPOXNETFLT_PKTRSVD_TRANSFERDATA_PT pTDR;

    if (!vpoxNetFltWinRemovePacketFromList(&pNetFltIf->u.s.WinIf.TransferDataList, pPacket))
        return false;

    pTDR = (PVPOXNETFLT_PKTRSVD_TRANSFERDATA_PT)pPacket->ProtocolReserved;
    Assert(pTDR);
    Assert(pTDR->pOrigBuffer);

    do
    {
        NdisUnchainBufferAtFront(pPacket, &pBuffer);

        Assert(pBuffer);

        NdisFreeBuffer(pBuffer);

        pBuffer = pTDR->pOrigBuffer;

        NdisChainBufferAtBack(pPacket, pBuffer);

        /* data transfer was initiated when the netFlt was active
         * the netFlt is still retained by us
         * 1. check if loopback
         * 2. enqueue packet
         * 3. release netFlt */

        if (Status == NDIS_STATUS_SUCCESS)
        {

#ifdef VPOX_LOOPBACK_USEFLAGS
            if (vpoxNetFltWinIsLoopedBackPacket(pPacket))
            {
                /* should not be here */
                AssertFailed();
            }
#else
            PNDIS_PACKET pLb = vpoxNetFltWinLbSearchLoopBack(pNetFltIf, pPacket, false);
            if (pLb)
            {
#ifndef DEBUG_NETFLT_RECV_TRANSFERDATA
                /* should not be here */
                AssertFailed();
#endif
                if (!vpoxNetFltWinLbIsFromIntNet(pLb))
                {
                    /* the packet is not from int net, need to pass it up to the host */
                    NdisMIndicateReceivePacket(pNetFltIf->u.s.WinIf.hMiniport, &pPacket, 1);
                    /* dereference NetFlt, WinIf will be dereferenced on Packet return */
                    vpoxNetFltWinDereferenceNetFlt(pNetFltIf);
                    break;
                }
            }
#endif
            else
            {
                /* 2. enqueue */
                /* use the same packet info to put the packet in the processing packet queue */
                PVPOXNETFLT_PKTRSVD_MP pRecvInfo = (PVPOXNETFLT_PKTRSVD_MP)pPacket->MiniportReserved;

                VPOXNETFLT_LBVERIFY(pNetFltIf, pPacket);

                pRecvInfo->pOrigPacket = NULL;
                pRecvInfo->pBufToFree = NULL;

                NdisGetPacketFlags(pPacket) = 0;
# ifdef VPOXNETFLT_NO_PACKET_QUEUE
                if (vpoxNetFltWinPostIntnet(pNetFltIf, pPacket, 0))
                {
                    /* drop it */
                    vpoxNetFltWinFreeSGNdisPacket(pPacket, true);
                    vpoxNetFltWinDereferenceWinIf(pNetFltIf);
                }
                else
                {
                    NdisMIndicateReceivePacket(pNetFltIf->u.s.WinIf.hMiniport, &pPacket, 1);
                }
                vpoxNetFltWinDereferenceNetFlt(pNetFltIf);
                break;
# else
                Status = vpoxNetFltWinQuEnqueuePacket(pNetFltIf, pPacket, PACKET_MINE);
                if (Status == NDIS_STATUS_SUCCESS)
                {
                    break;
                }
                AssertFailed();
# endif
            }
        }
        else
        {
            AssertFailed();
        }
        /* we are here because of error either in data transfer or in enqueueing the packet */
        vpoxNetFltWinFreeSGNdisPacket(pPacket, true);
        vpoxNetFltWinDereferenceNetFlt(pNetFltIf);
        vpoxNetFltWinDereferenceWinIf(pNetFltIf);
    } while (0);

    return true;
}

static VOID vpoxNetFltWinPtTransferDataComplete(IN NDIS_HANDLE hProtocolBindingContext,
                    IN PNDIS_PACKET pPacket,
                    IN NDIS_STATUS Status,
                    IN UINT cbTransferred)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;
    LogFlowFunc(("ENTER: pNetFlt (0x%p), pPacket (0x%p), Status (0x%x), cbTransfered (%d)\n", pNetFlt, pPacket, Status, cbTransferred));
    if (!vpoxNetFltWinPtTransferDataCompleteActive(pNetFlt, pPacket, Status))
    {
        if (pNetFlt->u.s.WinIf.hMiniport)
        {
            NdisMTransferDataComplete(pNetFlt->u.s.WinIf.hMiniport,
                                      pPacket,
                                      Status,
                                      cbTransferred);
        }

        vpoxNetFltWinDereferenceWinIf(pNetFlt);
    }
    /* else - all processing is done with vpoxNetFltWinPtTransferDataCompleteActive already */

    LogFlowFunc(("LEAVE: pNetFlt (0x%p), pPacket (0x%p), Status (0x%x), cbTransfered (%d)\n", pNetFlt, pPacket, Status, cbTransferred));
}

static INT vpoxNetFltWinRecvPacketPassThru(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket)
{
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    PNDIS_PACKET pMyPacket;
    NDIS_STATUS Status = vpoxNetFltWinPrepareRecvPacket(pNetFlt, pPacket, &pMyPacket, true);
    /* the Status holds the current packet status it will be checked for NDIS_STATUS_RESOURCES later
     * (see below) */
    Assert(pMyPacket);
    if (pMyPacket)
    {
        NdisMIndicateReceivePacket(pNetFlt->u.s.WinIf.hMiniport, &pMyPacket, 1);
        if (Status == NDIS_STATUS_RESOURCES)
        {
            NdisDprFreePacket(pMyPacket);
            return 0;
        }

        return 1;
    }

    return 0;
}

/**
 * process the packet receive in a "passthru" mode
 */
static NDIS_STATUS vpoxNetFltWinRecvPassThru(PVPOXNETFLTINS pNetFlt, PNDIS_PACKET pPacket)
{
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    NDIS_STATUS Status;
    PNDIS_PACKET pMyPacket;

    NdisDprAllocatePacket(&Status, &pMyPacket, pNetFlt->u.s.WinIf.hRecvPacketPool);
    Assert(Status == NDIS_STATUS_SUCCESS);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        vpoxNetFltWinCopyPacketInfoOnRecv(pMyPacket, pPacket, true /* force NDIS_STATUS_RESOURCES */);
        Assert(NDIS_GET_PACKET_STATUS(pMyPacket) == NDIS_STATUS_RESOURCES);

        NdisMIndicateReceivePacket(pNetFlt->u.s.WinIf.hMiniport, &pMyPacket, 1);

        NdisDprFreePacket(pMyPacket);
    }
    return Status;
}

static VOID vpoxNetFltWinRecvIndicatePassThru(PVPOXNETFLTINS pNetFlt, NDIS_HANDLE MacReceiveContext,
                PVOID pHeaderBuffer, UINT cbHeaderBuffer, PVOID pLookAheadBuffer, UINT cbLookAheadBuffer, UINT cbPacket)
{
    /* Note: we're using KeGetCurrentProcessorNumber, which is not entirely correct in case
    * we're running on 64bit win7+, which can handle > 64 CPUs, however since KeGetCurrentProcessorNumber
    * always returns the number < than the number of CPUs in the first group, we're guaranteed to have CPU index < 64
    * @todo: use KeGetCurrentProcessorNumberEx for Win7+ 64 and dynamically extended array */
    ULONG Proc = KeGetCurrentProcessorNumber();
    Assert(Proc < RT_ELEMENTS(pNetFlt->u.s.WinIf.abIndicateRxComplete));
    pNetFlt->u.s.WinIf.abIndicateRxComplete[Proc] = TRUE;
    switch (pNetFlt->u.s.WinIf.enmMedium)
    {
        case NdisMedium802_3:
        case NdisMediumWan:
            NdisMEthIndicateReceive(pNetFlt->u.s.WinIf.hMiniport,
                                         MacReceiveContext,
                                         (PCHAR)pHeaderBuffer,
                                         cbHeaderBuffer,
                                         pLookAheadBuffer,
                                         cbLookAheadBuffer,
                                         cbPacket);
            break;
        default:
            AssertFailed();
            break;
    }
}

/**
 * process the ProtocolReceive in an "active" mode
 *
 * @return NDIS_STATUS_SUCCESS - the packet is processed
 * NDIS_STATUS_PENDING - the packet is being processed, we are waiting for the ProtocolTransferDataComplete to be called
 * NDIS_STATUS_NOT_ACCEPTED - the packet is not needed - typically this is because this is a loopback packet
 * NDIS_STATUS_FAILURE - packet processing failed
 */
static NDIS_STATUS vpoxNetFltWinPtReceiveActive(PVPOXNETFLTINS pNetFlt, NDIS_HANDLE MacReceiveContext, PVOID pHeaderBuffer, UINT cbHeaderBuffer,
                        PVOID pLookaheadBuffer, UINT cbLookaheadBuffer, UINT cbPacket)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    do
    {
        if (cbHeaderBuffer != VPOXNETFLT_PACKET_ETHEADER_SIZE)
        {
            Status = NDIS_STATUS_NOT_ACCEPTED;
            break;
        }

#ifndef DEBUG_NETFLT_RECV_TRANSFERDATA
        if (cbPacket == cbLookaheadBuffer)
        {
            PINTNETSG pSG;
            PUCHAR pRcvData;
#ifndef VPOX_LOOPBACK_USEFLAGS
            PNDIS_PACKET pLb;
#endif

            /* allocate SG buffer */
            Status = vpoxNetFltWinAllocSG(cbPacket + cbHeaderBuffer, &pSG);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                AssertFailed();
                break;
            }

            pRcvData = (PUCHAR)pSG->aSegs[0].pv;

            NdisMoveMappedMemory(pRcvData, pHeaderBuffer, cbHeaderBuffer);

            NdisCopyLookaheadData(pRcvData+cbHeaderBuffer,
                                                  pLookaheadBuffer,
                                                  cbLookaheadBuffer,
                                                  pNetFlt->u.s.WinIf.fMacOptions);
#ifndef VPOX_LOOPBACK_USEFLAGS
            pLb = vpoxNetFltWinLbSearchLoopBackBySG(pNetFlt, pSG, false);
            if (pLb)
            {
#ifndef DEBUG_NETFLT_RECV_NOPACKET
                /* should not be here */
                AssertFailed();
#endif
                if (!vpoxNetFltWinLbIsFromIntNet(pLb))
                {
                    PNDIS_PACKET pMyPacket;
                    pMyPacket = vpoxNetFltWinNdisPacketFromSG(pNetFlt, /* PVPOXNETFLTINS */
                        pSG, /* PINTNETSG */
                        pSG, /* PVOID pBufToFree */
                        false, /* bool bToWire */
                        false); /* bool bCopyMemory */
                    if (pMyPacket)
                    {
                        NdisMIndicateReceivePacket(pNetFlt->u.s.WinIf.hMiniport, &pMyPacket, 1);
                        /* dereference the NetFlt here & indicate SUCCESS, which would mean the caller would not do a dereference
                         * the WinIf dereference will be done on packet return */
                        vpoxNetFltWinDereferenceNetFlt(pNetFlt);
                        Status = NDIS_STATUS_SUCCESS;
                    }
                    else
                    {
                        vpoxNetFltWinMemFree(pSG);
                        Status = NDIS_STATUS_FAILURE;
                    }
                }
                else
                {
                    vpoxNetFltWinMemFree(pSG);
                    Status = NDIS_STATUS_NOT_ACCEPTED;
                }
                break;
            }
#endif
            VPOXNETFLT_LBVERIFYSG(pNetFlt, pSG);

                /* enqueue SG */
# ifdef VPOXNETFLT_NO_PACKET_QUEUE
            if (vpoxNetFltWinPostIntnet(pNetFlt, pSG, VPOXNETFLT_PACKET_SG))
            {
                /* drop it */
                vpoxNetFltWinMemFree(pSG);
                vpoxNetFltWinDereferenceWinIf(pNetFlt);
            }
            else
            {
                PNDIS_PACKET pMyPacket = vpoxNetFltWinNdisPacketFromSG(pNetFlt, /* PVPOXNETFLTINS */
                        pSG, /* PINTNETSG */
                        pSG, /* PVOID pBufToFree */
                        false, /* bool bToWire */
                        false); /* bool bCopyMemory */
                Assert(pMyPacket);
                if (pMyPacket)
                {
                    NDIS_SET_PACKET_STATUS(pMyPacket, NDIS_STATUS_SUCCESS);

                    DBG_CHECK_PACKET_AND_SG(pMyPacket, pSG);

                    LogFlow(("non-ndis packet info, packet created (%p)\n", pMyPacket));
                    NdisMIndicateReceivePacket(pNetFlt->u.s.WinIf.hMiniport, &pMyPacket, 1);
                }
                else
                {
                    vpoxNetFltWinDereferenceWinIf(pNetFlt);
                    Status = NDIS_STATUS_RESOURCES;
                }
            }
            vpoxNetFltWinDereferenceNetFlt(pNetFlt);
# else
            Status = vpoxNetFltWinQuEnqueuePacket(pNetFlt, pSG, PACKET_SG | PACKET_MINE);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                AssertFailed();
                vpoxNetFltWinMemFree(pSG);
                break;
            }
# endif
#endif
        }
        else
        {
            PNDIS_PACKET pPacket;
            PNDIS_BUFFER pTransferBuffer;
            PNDIS_BUFFER pOrigBuffer;
            PUCHAR pMemBuf;
            UINT cbBuf = cbPacket + cbHeaderBuffer;
            UINT cbTransferred;

            /* allocate NDIS Packet buffer */
            NdisAllocatePacket(&Status, &pPacket, pNetFlt->u.s.WinIf.hRecvPacketPool);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                AssertFailed();
                break;
            }

            VPOXNETFLT_OOB_INIT(pPacket);

#ifdef VPOX_LOOPBACK_USEFLAGS
            /* set "don't loopback" flags */
            NdisGetPacketFlags(pPacket) = g_VPoxNetFltGlobalsWin.fPacketDontLoopBack;
#else
            NdisGetPacketFlags(pPacket) =  0;
#endif

            Status = vpoxNetFltWinMemAlloc((PVOID*)(&pMemBuf), cbBuf);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                AssertFailed();
                NdisFreePacket(pPacket);
                break;
            }
            NdisAllocateBuffer(&Status, &pTransferBuffer, pNetFlt->u.s.WinIf.hRecvBufferPool, pMemBuf + cbHeaderBuffer, cbPacket);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                AssertFailed();
                Status = NDIS_STATUS_FAILURE;
                NdisFreePacket(pPacket);
                vpoxNetFltWinMemFree(pMemBuf);
                break;
            }

            NdisAllocateBuffer(&Status, &pOrigBuffer, pNetFlt->u.s.WinIf.hRecvBufferPool, pMemBuf, cbBuf);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                AssertFailed();
                Status = NDIS_STATUS_FAILURE;
                NdisFreeBuffer(pTransferBuffer);
                NdisFreePacket(pPacket);
                vpoxNetFltWinMemFree(pMemBuf);
                break;
            }

            NdisChainBufferAtBack(pPacket, pTransferBuffer);

            NdisMoveMappedMemory(pMemBuf, pHeaderBuffer, cbHeaderBuffer);

            vpoxNetFltWinPutPacketToList(&pNetFlt->u.s.WinIf.TransferDataList, pPacket, pOrigBuffer);

#ifdef DEBUG_NETFLT_RECV_TRANSFERDATA
            if (cbPacket == cbLookaheadBuffer)
            {
                NdisCopyLookaheadData(pMemBuf+cbHeaderBuffer,
                                                  pLookaheadBuffer,
                                                  cbLookaheadBuffer,
                                                  pNetFlt->u.s.WinIf.fMacOptions);
            }
            else
#endif
            {
                Assert(cbPacket > cbLookaheadBuffer);

                NdisTransferData(&Status, pNetFlt->u.s.WinIf.hBinding, MacReceiveContext,
                        0,  /* ByteOffset */
                        cbPacket, pPacket, &cbTransferred);
            }

            if (Status != NDIS_STATUS_PENDING)
            {
                vpoxNetFltWinPtTransferDataComplete(pNetFlt, pPacket, Status, cbTransferred);
            }
        }
    } while (0);

    return Status;
}

static NDIS_STATUS vpoxNetFltWinPtReceive(IN NDIS_HANDLE hProtocolBindingContext,
                        IN NDIS_HANDLE MacReceiveContext,
                        IN PVOID pHeaderBuffer,
                        IN UINT cbHeaderBuffer,
                        IN PVOID pLookAheadBuffer,
                        IN UINT cbLookAheadBuffer,
                        IN UINT cbPacket)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;
    PNDIS_PACKET pPacket = NULL;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    bool bNetFltActive;
    bool fWinIfActive = vpoxNetFltWinReferenceWinIfNetFlt(pNetFlt, &bNetFltActive);
    const bool bPassThruActive = !bNetFltActive;

    LogFlowFunc(("ENTER: pNetFlt (0x%p)\n", pNetFlt));

    if (fWinIfActive)
    {
        do
        {
#ifndef DEBUG_NETFLT_RECV_NOPACKET
            pPacket = NdisGetReceivedPacket(pNetFlt->u.s.WinIf.hBinding, MacReceiveContext);
            if (pPacket)
            {
# ifndef VPOX_LOOPBACK_USEFLAGS
                PNDIS_PACKET pLb = NULL;
# else
                if (vpoxNetFltWinIsLoopedBackPacket(pPacket))
                {
                    AssertFailed();
                    /* nothing else to do here, just return the packet */
                    //NdisReturnPackets(&pPacket, 1);
                    Status = NDIS_STATUS_NOT_ACCEPTED;
                    break;
                }

                VPOXNETFLT_LBVERIFY(pNetFlt, pPacket);
# endif

                if (bNetFltActive)
                {
# ifndef VPOX_LOOPBACK_USEFLAGS
                    pLb = vpoxNetFltWinLbSearchLoopBack(pNetFlt, pPacket, false);
                    if (!pLb)
# endif
                    {
                        VPOXNETFLT_LBVERIFY(pNetFlt, pPacket);

# ifdef VPOXNETFLT_NO_PACKET_QUEUE
                        if (vpoxNetFltWinPostIntnet(pNetFlt, pPacket, 0))
                        {
                            /* drop it */
                            break;
                        }
# else
                        Status = vpoxNetFltWinQuEnqueuePacket(pNetFlt, pPacket, PACKET_COPY);
                        Assert(Status == NDIS_STATUS_SUCCESS);
                        if (Status == NDIS_STATUS_SUCCESS)
                        {
                            //NdisReturnPackets(&pPacket, 1);
                            fWinIfActive = false;
                            bNetFltActive = false;
                            break;
                        }
# endif
                    }
# ifndef VPOX_LOOPBACK_USEFLAGS
                    else if (vpoxNetFltWinLbIsFromIntNet(pLb))
                    {
                        /* nothing else to do here, just return the packet */
                        //NdisReturnPackets(&pPacket, 1);
                        Status = NDIS_STATUS_NOT_ACCEPTED;
                        break;
                    }
                    /* we are here because this is a looped back packet set not from intnet
                     * we will post it to the upper protocol */
# endif
                }

                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
# ifndef VPOX_LOOPBACK_USEFLAGS
                    Assert(!pLb || !vpoxNetFltWinLbIsFromIntNet(pLb));
# endif
                    Status = vpoxNetFltWinRecvPassThru(pNetFlt, pPacket);
                    Assert(Status == STATUS_SUCCESS);
                    /* we are done with packet processing, and we will
                     * not receive packet return event for this packet,
                     * fWinIfActive should be true to ensure we release WinIf*/
                    Assert(fWinIfActive);
                    if (Status == STATUS_SUCCESS)
                        break;
                }
                else
                {
                    /* intnet processing failed - fall back to no-packet mode */
                    Assert(bNetFltActive);
                    Assert(fWinIfActive);
                }

            }
#endif /* #ifndef DEBUG_NETFLT_RECV_NOPACKET */

            if (bNetFltActive)
            {
                Status = vpoxNetFltWinPtReceiveActive(pNetFlt, MacReceiveContext, pHeaderBuffer, cbHeaderBuffer,
                        pLookAheadBuffer, cbLookAheadBuffer, cbPacket);
                if (NT_SUCCESS(Status))
                {
                    if (Status != NDIS_STATUS_NOT_ACCEPTED)
                    {
                        fWinIfActive = false;
                        bNetFltActive = false;
                    }
                    else
                    {
#ifndef VPOX_LOOPBACK_USEFLAGS
                        /* this is a loopback packet, nothing to do here */
#else
                        AssertFailed();
                        /* should not be here */
#endif
                    }
                    break;
                }
            }

            /* we are done with packet processing, and we will
             * not receive packet return event for this packet,
             * fWinIfActive should be true to ensure we release WinIf*/
            Assert(fWinIfActive);

            vpoxNetFltWinRecvIndicatePassThru(pNetFlt, MacReceiveContext, pHeaderBuffer, cbHeaderBuffer, pLookAheadBuffer, cbLookAheadBuffer, cbPacket);
            /* the status could contain an error value here in case the IntNet recv failed,
             * ensure we return back success status */
            Status = NDIS_STATUS_SUCCESS;

        } while (0);

        if (bNetFltActive)
        {
            vpoxNetFltWinDereferenceNetFlt(pNetFlt);
        }
        else if (bPassThruActive)
        {
            vpoxNetFltWinDereferenceModePassThru(pNetFlt);
        }
        if (fWinIfActive)
        {
            vpoxNetFltWinDereferenceWinIf(pNetFlt);
        }
    }
    else
    {
        Status = NDIS_STATUS_FAILURE;
    }

    LogFlowFunc(("LEAVE: pNetFlt (0x%p)\n", pNetFlt));

    return Status;

}

static VOID vpoxNetFltWinPtReceiveComplete(NDIS_HANDLE hProtocolBindingContext)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;
    bool fNetFltActive;
    bool fWinIfActive = vpoxNetFltWinReferenceWinIfNetFlt(pNetFlt, &fNetFltActive);
    NDIS_HANDLE hMiniport = pNetFlt->u.s.WinIf.hMiniport;
    /* Note: we're using KeGetCurrentProcessorNumber, which is not entirely correct in case
    * we're running on 64bit win7+, which can handle > 64 CPUs, however since KeGetCurrentProcessorNumber
    * always returns the number < than the number of CPUs in the first group, we're guaranteed to have CPU index < 64
    * @todo: use KeGetCurrentProcessorNumberEx for Win7+ 64 and dynamically extended array */
    ULONG iProc = KeGetCurrentProcessorNumber();
    Assert(iProc < RT_ELEMENTS(pNetFlt->u.s.WinIf.abIndicateRxComplete));

    LogFlowFunc(("ENTER: pNetFlt (0x%p)\n", pNetFlt));

    if (hMiniport != NULL && pNetFlt->u.s.WinIf.abIndicateRxComplete[iProc])
    {
        switch (pNetFlt->u.s.WinIf.enmMedium)
        {
            case NdisMedium802_3:
            case NdisMediumWan:
                NdisMEthIndicateReceiveComplete(hMiniport);
                break;
            default:
                AssertFailed();
                break;
        }
    }

    pNetFlt->u.s.WinIf.abIndicateRxComplete[iProc] = FALSE;

    if (fWinIfActive)
    {
        if (fNetFltActive)
            vpoxNetFltWinDereferenceNetFlt(pNetFlt);
        else
            vpoxNetFltWinDereferenceModePassThru(pNetFlt);
        vpoxNetFltWinDereferenceWinIf(pNetFlt);
    }

    LogFlowFunc(("LEAVE: pNetFlt (0x%p)\n", pNetFlt));
}

static INT vpoxNetFltWinPtReceivePacket(NDIS_HANDLE hProtocolBindingContext, PNDIS_PACKET pPacket)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;
    INT cRefCount = 0;
    bool bNetFltActive;
    bool fWinIfActive = vpoxNetFltWinReferenceWinIfNetFlt(pNetFlt, &bNetFltActive);
    const bool bPassThruActive = !bNetFltActive;

    LogFlowFunc(("ENTER: pNetFlt (0x%p)\n", pNetFlt));

    if (fWinIfActive)
    {
        do
        {
#ifdef VPOX_LOOPBACK_USEFLAGS
            if (vpoxNetFltWinIsLoopedBackPacket(pPacket))
            {
                AssertFailed();
                Log(("lb_rp"));

                /* nothing else to do here, just return the packet */
                cRefCount = 0;
                //NdisReturnPackets(&pPacket, 1);
                break;
            }

            VPOXNETFLT_LBVERIFY(pNetFlt, pPacket);
#endif

            if (bNetFltActive)
            {
#ifndef VPOX_LOOPBACK_USEFLAGS
                PNDIS_PACKET pLb = vpoxNetFltWinLbSearchLoopBack(pNetFlt, pPacket, false);
                if (!pLb)
#endif
                {
#ifndef VPOXNETFLT_NO_PACKET_QUEUE
                    NDIS_STATUS fStatus;
#endif
                    bool fResources = NDIS_GET_PACKET_STATUS(pPacket) == NDIS_STATUS_RESOURCES; NOREF(fResources);

                    VPOXNETFLT_LBVERIFY(pNetFlt, pPacket);
#ifdef DEBUG_misha
                    /** @todo remove this assert.
                     * this is a temporary assert for debugging purposes:
                     * we're probably doing something wrong with the packets if the miniport reports NDIS_STATUS_RESOURCES */
                    Assert(!fResources);
#endif

#ifdef VPOXNETFLT_NO_PACKET_QUEUE
                    if (vpoxNetFltWinPostIntnet(pNetFlt, pPacket, 0))
                    {
                        /* drop it */
                        cRefCount = 0;
                        break;
                    }

#else
                    fStatus = vpoxNetFltWinQuEnqueuePacket(pNetFlt, pPacket, fResources ? PACKET_COPY : 0);
                    if (fStatus == NDIS_STATUS_SUCCESS)
                    {
                        bNetFltActive = false;
                        fWinIfActive = false;
                        if (fResources)
                        {
                            cRefCount = 0;
                            //NdisReturnPackets(&pPacket, 1);
                        }
                        else
                            cRefCount = 1;
                        break;
                    }
                    else
                    {
                        AssertFailed();
                    }
#endif
                }
#ifndef VPOX_LOOPBACK_USEFLAGS
                else if (vpoxNetFltWinLbIsFromIntNet(pLb))
                {
                    /* the packet is from intnet, it has already been set to the host,
                     * no need for loopng it back to the host again */
                    /* nothing else to do here, just return the packet */
                    cRefCount = 0;
                    //NdisReturnPackets(&pPacket, 1);
                    break;
                }
#endif
            }

            cRefCount = vpoxNetFltWinRecvPacketPassThru(pNetFlt, pPacket);
            if (cRefCount)
            {
                Assert(cRefCount == 1);
                fWinIfActive = false;
            }

        } while (FALSE);

        if (bNetFltActive)
        {
            vpoxNetFltWinDereferenceNetFlt(pNetFlt);
        }
        else if (bPassThruActive)
        {
            vpoxNetFltWinDereferenceModePassThru(pNetFlt);
        }
        if (fWinIfActive)
        {
            vpoxNetFltWinDereferenceWinIf(pNetFlt);
        }
    }
    else
    {
        cRefCount = 0;
        //NdisReturnPackets(&pPacket, 1);
    }

    LogFlowFunc(("LEAVE: pNetFlt (0x%p), cRefCount (%d)\n", pNetFlt, cRefCount));

    return cRefCount;
}

DECLHIDDEN(bool) vpoxNetFltWinPtCloseInterface(PVPOXNETFLTINS pNetFlt, PNDIS_STATUS pStatus)
{
    RTSpinlockAcquire(pNetFlt->hSpinlock);

    if (pNetFlt->u.s.WinIf.StateFlags.fInterfaceClosing)
    {
        RTSpinlockRelease(pNetFlt->hSpinlock);
        AssertFailed();
        return false;
    }
    if (pNetFlt->u.s.WinIf.hBinding == NULL)
    {
        RTSpinlockRelease(pNetFlt->hSpinlock);
        AssertFailed();
        return false;
    }

    pNetFlt->u.s.WinIf.StateFlags.fInterfaceClosing = TRUE;
    RTSpinlockRelease(pNetFlt->hSpinlock);

    NdisResetEvent(&pNetFlt->u.s.WinIf.OpenCloseEvent);
    NdisCloseAdapter(pStatus, pNetFlt->u.s.WinIf.hBinding);
    if (*pStatus == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pNetFlt->u.s.WinIf.OpenCloseEvent, 0);
        *pStatus = pNetFlt->u.s.WinIf.OpenCloseStatus;
    }

    Assert (*pStatus == NDIS_STATUS_SUCCESS);

    pNetFlt->u.s.WinIf.hBinding = NULL;

    return true;
}

static NDIS_STATUS vpoxNetFltWinPtPnPSetPower(PVPOXNETFLTINS pNetFlt, NDIS_DEVICE_POWER_STATE enmPowerState)
{
    NDIS_DEVICE_POWER_STATE enmPrevPowerState = vpoxNetFltWinGetPowerState(&pNetFlt->u.s.WinIf.PtState);

    RTSpinlockAcquire(pNetFlt->hSpinlock);

    vpoxNetFltWinSetPowerState(&pNetFlt->u.s.WinIf.PtState, enmPowerState);

    if (vpoxNetFltWinGetPowerState(&pNetFlt->u.s.WinIf.PtState) > NdisDeviceStateD0)
    {
        if (enmPrevPowerState == NdisDeviceStateD0)
        {
            pNetFlt->u.s.WinIf.StateFlags.fStandBy = TRUE;
        }
        RTSpinlockRelease(pNetFlt->hSpinlock);
        vpoxNetFltWinPtRequestsWaitComplete(pNetFlt);
        vpoxNetFltWinWaitDereference(&pNetFlt->u.s.WinIf.MpState);
        vpoxNetFltWinWaitDereference(&pNetFlt->u.s.WinIf.PtState);

        /* check packet pool is empty */
        UINT cPPUsage = NdisPacketPoolUsage(pNetFlt->u.s.WinIf.hSendPacketPool);
        Assert(cPPUsage == 0);
        cPPUsage = NdisPacketPoolUsage(pNetFlt->u.s.WinIf.hRecvPacketPool);
        Assert(cPPUsage == 0);
        /* for debugging only, ignore the err in release */
        NOREF(cPPUsage);

        Assert(!pNetFlt->u.s.WinIf.StateFlags.fRequestInfo);
    }
    else
    {
        if (enmPrevPowerState > NdisDeviceStateD0)
        {
            pNetFlt->u.s.WinIf.StateFlags.fStandBy = FALSE;
        }

        if (pNetFlt->u.s.WinIf.StateFlags.fRequestInfo & VPOXNDISREQUEST_QUEUED)
        {
            pNetFlt->u.s.WinIf.StateFlags.fRequestInfo = VPOXNDISREQUEST_INPROGRESS;
            RTSpinlockRelease(pNetFlt->hSpinlock);

            vpoxNetFltWinMpRequestPost(pNetFlt);
        }
        else
        {
            RTSpinlockRelease(pNetFlt->hSpinlock);
        }
    }

    return NDIS_STATUS_SUCCESS;
}


static NDIS_STATUS vpoxNetFltWinPtPnPEvent(IN NDIS_HANDLE hProtocolBindingContext, IN PNET_PNP_EVENT pNetPnPEvent)
{
    PVPOXNETFLTINS pNetFlt = (PVPOXNETFLTINS)hProtocolBindingContext;

    LogFlowFunc(("ENTER: pNetFlt (0x%p), NetEvent (%d)\n", pNetFlt, pNetPnPEvent->NetEvent));

    switch (pNetPnPEvent->NetEvent)
    {
        case NetEventSetPower:
        {
            NDIS_DEVICE_POWER_STATE enmPowerState = *((PNDIS_DEVICE_POWER_STATE)pNetPnPEvent->Buffer);
            NDIS_STATUS rcNdis = vpoxNetFltWinPtPnPSetPower(pNetFlt, enmPowerState);
            LogFlowFunc(("LEAVE: pNetFlt (0x%p), NetEvent (%d), rcNdis=%#x\n", pNetFlt, pNetPnPEvent->NetEvent, rcNdis));
            return rcNdis;
        }

        case NetEventReconfigure:
        {
            if (!pNetFlt)
            {
                NdisReEnumerateProtocolBindings(g_VPoxNetFltGlobalsWin.Pt.hProtocol);
            }
        }
        /** @todo r=bird: Is the fall thru intentional?? */
        default:
            LogFlowFunc(("LEAVE: pNetFlt (0x%p), NetEvent (%d)\n", pNetFlt, pNetPnPEvent->NetEvent));
            return NDIS_STATUS_SUCCESS;
    }

}

#ifdef __cplusplus
# define PTCHARS_40(_p) ((_p).Ndis40Chars)
#else
# define PTCHARS_40(_p) (_p)
#endif

/**
 * register the protocol edge
 */
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtRegister(PVPOXNETFLTGLOBALS_PT pGlobalsPt, PDRIVER_OBJECT pDriverObject,
                                                PUNICODE_STRING pRegistryPathStr)
{
    RT_NOREF2(pDriverObject, pRegistryPathStr);
    NDIS_PROTOCOL_CHARACTERISTICS PtChars;
    NDIS_STRING NameStr;

    NdisInitUnicodeString(&NameStr, VPOXNETFLT_NAME_PROTOCOL);

    NdisZeroMemory(&PtChars, sizeof (PtChars));
    PTCHARS_40(PtChars).MajorNdisVersion = VPOXNETFLT_VERSION_PT_NDIS_MAJOR;
    PTCHARS_40(PtChars).MinorNdisVersion = VPOXNETFLT_VERSION_PT_NDIS_MINOR;

    PTCHARS_40(PtChars).Name = NameStr;
    PTCHARS_40(PtChars).OpenAdapterCompleteHandler = vpoxNetFltWinPtOpenAdapterComplete;
    PTCHARS_40(PtChars).CloseAdapterCompleteHandler = vpoxNetFltWinPtCloseAdapterComplete;
    PTCHARS_40(PtChars).SendCompleteHandler = vpoxNetFltWinPtSendComplete;
    PTCHARS_40(PtChars).TransferDataCompleteHandler = vpoxNetFltWinPtTransferDataComplete;
    PTCHARS_40(PtChars).ResetCompleteHandler = vpoxNetFltWinPtResetComplete;
    PTCHARS_40(PtChars).RequestCompleteHandler = vpoxNetFltWinPtRequestComplete;
    PTCHARS_40(PtChars).ReceiveHandler = vpoxNetFltWinPtReceive;
    PTCHARS_40(PtChars).ReceiveCompleteHandler = vpoxNetFltWinPtReceiveComplete;
    PTCHARS_40(PtChars).StatusHandler = vpoxNetFltWinPtStatus;
    PTCHARS_40(PtChars).StatusCompleteHandler = vpoxNetFltWinPtStatusComplete;
    PTCHARS_40(PtChars).BindAdapterHandler = vpoxNetFltWinPtBindAdapter;
    PTCHARS_40(PtChars).UnbindAdapterHandler = vpoxNetFltWinPtUnbindAdapter;
    PTCHARS_40(PtChars).UnloadHandler = vpoxNetFltWinPtUnloadProtocol;
#if !defined(DEBUG_NETFLT_RECV)
    PTCHARS_40(PtChars).ReceivePacketHandler = vpoxNetFltWinPtReceivePacket;
#endif
    PTCHARS_40(PtChars).PnPEventHandler = vpoxNetFltWinPtPnPEvent;

    NDIS_STATUS Status;
    NdisRegisterProtocol(&Status, &pGlobalsPt->hProtocol, &PtChars, sizeof (PtChars));
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

/**
 * deregister the protocol edge
 */
DECLHIDDEN(NDIS_STATUS) vpoxNetFltWinPtDeregister(PVPOXNETFLTGLOBALS_PT pGlobalsPt)
{
    if (!pGlobalsPt->hProtocol)
        return NDIS_STATUS_SUCCESS;

    NDIS_STATUS Status;

    NdisDeregisterProtocol(&Status, pGlobalsPt->hProtocol);
    Assert (Status == NDIS_STATUS_SUCCESS);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        NdisZeroMemory(pGlobalsPt, sizeof (*pGlobalsPt));
    }
    return Status;
}
