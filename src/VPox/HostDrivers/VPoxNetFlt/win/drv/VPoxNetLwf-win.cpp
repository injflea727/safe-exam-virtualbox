/* $Id: VPoxNetLwf-win.cpp $ */
/** @file
 * VPoxNetLwf-win.cpp - NDIS6 Bridged Networking Driver, Windows-specific code.
 */
/*
 * Copyright (C) 2014-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NET_FLT_DRV

/*
 * If VPOXNETLWF_SYNC_SEND is defined we won't allocate data buffers, but use
 * the original buffers coming from IntNet to build MDLs around them. This
 * also means that we need to wait for send operation to complete before
 * returning the buffers, which hinders performance way too much.
 */
//#define VPOXNETLWF_SYNC_SEND

/*
 * If VPOXNETLWF_FIXED_SIZE_POOLS is defined we pre-allocate data buffers of
 * fixed size in five pools. Each pool uses different size to accomodate packets
 * of various sizes. We allocate these buffers once and re-use them when send
 * operation is complete.
 * If VPOXNETLWF_FIXED_SIZE_POOLS is not defined we allocate data buffers before
 * each send operation and free then upon completion.
 */
#define VPOXNETLWF_FIXED_SIZE_POOLS

/*
 * Don't ask me why it is 42. Empirically this is what goes down the stack.
 * OTOH, as we know from trustworthy sources, 42 is the answer, so be it.
 */
#define VPOXNETLWF_MAX_FRAME_SIZE(mtu) (mtu + 42)

#include <VPox/version.h>
#include <VPox/err.h>
#include <iprt/initterm.h>
#include <iprt/net.h>
#include <iprt/list.h>
#include <VPox/intnetinline.h>

#include <iprt/nt/ntddk.h>
#include <iprt/nt/ndis.h>
#include <iprt/win/netioapi.h>
#include <mstcpip.h>

#define LogError(x) DbgPrint x

#if 0
#undef Log
#define Log(x) DbgPrint x
#undef LogFlow
#define LogFlow(x) DbgPrint x
#endif

/** We have an entirely different structure than the one defined in VPoxNetFltCmn-win.h */
typedef struct VPOXNETFLTWIN
{
    /** filter module context handle */
    NDIS_HANDLE hModuleCtx;
    /** IP address change notifier handle */
    HANDLE hNotifier; /* Must be here as hModuleCtx may already be NULL when vpoxNetFltOsDeleteInstance is called */
} VPOXNETFLTWIN, *PVPOXNETFLTWIN;
#define VPOXNETFLT_NO_PACKET_QUEUE
#define VPOXNETFLT_OS_SPECFIC 1
#include "VPoxNetFltInternal.h"

#include "VPoxNetLwf-win.h"
#include "VPox/VPoxNetCmn-win.h"

typedef enum {
    LwfState_Detached = 0,
    LwfState_Attaching,
    LwfState_Paused,
    LwfState_Restarting,
    LwfState_Running,
    LwfState_Pausing,
    LwfState_32BitHack = 0x7fffffff
} VPOXNETLWFSTATE;

/*
 * Valid state transitions are:
 * 1) Disconnected -> Connecting   : start the worker thread, attempting to init IDC;
 * 2) Connecting   -> Disconnected : failed to start IDC init worker thread;
 * 3) Connecting   -> Connected    : IDC init successful, terminate the worker;
 * 4) Connecting   -> Stopping     : IDC init incomplete, but the driver is being unloaded, terminate the worker;
 * 5) Connected    -> Stopping     : IDC init was successful, no worker, the driver is being unloaded;
 *
 * Driver terminates in Stopping state.
 */
typedef enum {
    LwfIdcState_Disconnected = 0, /* Initial state */
    LwfIdcState_Connecting,       /* Attemping to init IDC, worker thread running */
    LwfIdcState_Connected,        /* Successfully connected to IDC, worker thread terminated */
    LwfIdcState_Stopping          /* Terminating the worker thread and disconnecting IDC */
} VPOXNETLWFIDCSTATE;

struct _VPOXNETLWF_MODULE;

typedef struct VPOXNETLWFGLOBALS
{
    /** synch event used for device creation synchronization */
    //KEVENT SynchEvent;
    /** Device reference count */
    //int cDeviceRefs;
    /** ndis device */
    NDIS_HANDLE hDevice;
    /** device object */
    PDEVICE_OBJECT pDevObj;
    /** our filter driver handle */
    NDIS_HANDLE hFilterDriver;
    /** lock protecting the module list */
    NDIS_SPIN_LOCK Lock;
    /** the head of module list */
    RTLISTANCHOR listModules;
    /** IDC initialization state */
    volatile uint32_t enmIdcState;
    /** IDC init thread handle */
    HANDLE hInitIdcThread;
} VPOXNETLWFGLOBALS, *PVPOXNETLWFGLOBALS;

/**
 * The (common) global data.
 */
static VPOXNETFLTGLOBALS g_VPoxNetFltGlobals;
/* win-specific global data */
VPOXNETLWFGLOBALS g_VPoxNetLwfGlobals;

#ifdef VPOXNETLWF_FIXED_SIZE_POOLS
static ULONG g_cbPool[] = { 576+56, 1556, 4096+56, 6192+56, 9056 };
#endif /* VPOXNETLWF_FIXED_SIZE_POOLS */

typedef struct _VPOXNETLWF_MODULE {
    RTLISTNODE node;

    NDIS_HANDLE hFilter;
#ifndef VPOXNETLWF_FIXED_SIZE_POOLS
    NDIS_HANDLE hPool;
#else /* VPOXNETLWF_FIXED_SIZE_POOLS */
    NDIS_HANDLE hPool[RT_ELEMENTS(g_cbPool)];
#endif /* VPOXNETLWF_FIXED_SIZE_POOLS */
    PVPOXNETLWFGLOBALS pGlobals;
    /** Associated instance of NetFlt, one-to-one relationship */
    PVPOXNETFLTINS pNetFlt; /// @todo Consider automic access!
    /** Module state as described in http://msdn.microsoft.com/en-us/library/windows/hardware/ff550017(v=vs.85).aspx */
    volatile uint32_t enmState; /* No lock needed yet, atomic should suffice. */
    /** Mutex to prevent pausing while transmitting on behalf of NetFlt */
    NDIS_MUTEX InTransmit;
#ifdef VPOXNETLWF_SYNC_SEND
    /** Event signalled when sending to the wire is complete */
    KEVENT EventWire;
    /** Event signalled when NDIS returns our receive notification */
    KEVENT EventHost;
#else /* !VPOXNETLWF_SYNC_SEND */
    /** Event signalled when all pending sends (both to wire and host) have completed */
    NDIS_EVENT EventSendComplete;
    /** Counter for pending sends (both to wire and host) */
    int32_t cPendingBuffers;
    /** Work Item to deliver offloading indications at passive IRQL */
    NDIS_HANDLE hWorkItem;
#endif /* !VPOXNETLWF_SYNC_SEND */
    /** MAC address of underlying adapter */
    RTMAC MacAddr;
    /** Size of offload config structure */
    USHORT cbOffloadConfig;
    /** Saved offload configuration */
    PNDIS_OFFLOAD pSavedOffloadConfig;
    /** Temporary buffer for disabling offload configuration */
    PNDIS_OFFLOAD pDisabledOffloadConfig;
    /** the cloned request we have passed down */
    PNDIS_OID_REQUEST pPendingRequest;
    /** true if the underlying miniport supplied offloading config */
    bool fOffloadConfigValid;
    /** true if the trunk expects data from us */
    bool fActive;
    /** true if the host wants the adapter to be in promisc mode */
    bool fHostPromisc;
    /** true if the user wants packets being sent or received by VMs to be visible to the host in promisc mode */
    bool fPassVmTrafficToHost;
    /** Name of underlying adapter */
    char szMiniportName[1];
} VPOXNETLWF_MODULE;
typedef VPOXNETLWF_MODULE *PVPOXNETLWF_MODULE;

/*
 * A structure to wrap OID requests in.
 */
typedef struct _VPOXNETLWF_OIDREQ {
    NDIS_OID_REQUEST Request;
    NDIS_STATUS Status;
    NDIS_EVENT Event;
} VPOXNETLWF_OIDREQ;
typedef VPOXNETLWF_OIDREQ *PVPOXNETLWF_OIDREQ;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static FILTER_ATTACH                            vpoxNetLwfWinAttach;
static FILTER_DETACH                            vpoxNetLwfWinDetach;
static FILTER_RESTART                           vpoxNetLwfWinRestart;
static FILTER_PAUSE                             vpoxNetLwfWinPause;
static FILTER_OID_REQUEST                       vpoxNetLwfWinOidRequest;
static FILTER_OID_REQUEST_COMPLETE              vpoxNetLwfWinOidRequestComplete;
//static FILTER_CANCEL_OID_REQUEST                vpoxNetLwfWinCancelOidRequest;
static FILTER_STATUS                            vpoxNetLwfWinStatus;
//static FILTER_NET_PNP_EVENT                     vpoxNetLwfWinPnPEvent;
static FILTER_SEND_NET_BUFFER_LISTS             vpoxNetLwfWinSendNetBufferLists;
static FILTER_SEND_NET_BUFFER_LISTS_COMPLETE    vpoxNetLwfWinSendNetBufferListsComplete;
static FILTER_RECEIVE_NET_BUFFER_LISTS          vpoxNetLwfWinReceiveNetBufferLists;
static FILTER_RETURN_NET_BUFFER_LISTS           vpoxNetLwfWinReturnNetBufferLists;
static KSTART_ROUTINE vpoxNetLwfWinInitIdcWorker;

static VOID vpoxNetLwfWinUnloadDriver(IN PDRIVER_OBJECT pDriver);
static int  vpoxNetLwfWinInitBase(void);
static int  vpoxNetLwfWinFini(void);



/**
 * Logs an error to the system event log.
 *
 * @param   ErrCode        Error to report to event log.
 * @param   ReturnedStatus Error that was reported by the driver to the caller.
 * @param   uErrId         Unique error id representing the location in the driver.
 * @param   cbDumpData     Number of bytes at pDumpData.
 * @param   pDumpData      Pointer to data that will be added to the message (see 'details' tab).
 */
static void vpoxNetLwfLogErrorEvent(NTSTATUS uErrCode, NTSTATUS uReturnedStatus, ULONG uErrId)
{
    /* Figure out how many modules are attached and if they are going to fit into the dump data. */
    unsigned cMaxModules = (ERROR_LOG_MAXIMUM_SIZE - FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData)) / sizeof(RTMAC);
    unsigned cModules = 0;
    PVPOXNETLWF_MODULE pModuleCtx;
    NdisAcquireSpinLock(&g_VPoxNetLwfGlobals.Lock);
    RTListForEach(&g_VPoxNetLwfGlobals.listModules, pModuleCtx, VPOXNETLWF_MODULE, node)
        ++cModules;
    NdisReleaseSpinLock(&g_VPoxNetLwfGlobals.Lock);
    /* Prevent overflow */
    if (cModules > cMaxModules)
        cModules = cMaxModules;

    /* DumpDataSize must be a multiple of sizeof(ULONG). */
    unsigned cbDumpData = (cModules * sizeof(RTMAC) + 3) & ~3;
    /* Prevent underflow */
    unsigned cbTotal = RT_MAX(FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + cbDumpData,
                              sizeof(IO_ERROR_LOG_PACKET));

    PIO_ERROR_LOG_PACKET pErrEntry;
    pErrEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(g_VPoxNetLwfGlobals.pDevObj,
                                                              (UCHAR)cbTotal);
    if (pErrEntry)
    {
        PRTMAC pDump = (PRTMAC)pErrEntry->DumpData;
        /*
         * Initialize the whole structure with zeros in case we are suddenly short
         * of data because the list is empty or has become smaller.
         */
        memset(pErrEntry, 0, cbTotal);

        NdisAcquireSpinLock(&g_VPoxNetLwfGlobals.Lock);
        RTListForEach(&g_VPoxNetLwfGlobals.listModules, pModuleCtx, VPOXNETLWF_MODULE, node)
        {
            /* The list could have been modified while we were allocating the entry, rely on cModules instead! */
            if (cModules-- == 0)
                break;
            *pDump++ = pModuleCtx->MacAddr;
        }
        NdisReleaseSpinLock(&g_VPoxNetLwfGlobals.Lock);

        pErrEntry->DumpDataSize     = cbDumpData;
        pErrEntry->ErrorCode        = uErrCode;
        pErrEntry->UniqueErrorValue = uErrId;
        pErrEntry->FinalStatus      = uReturnedStatus;
        IoWriteErrorLogEntry(pErrEntry);
    }
    else
    {
        DbgPrint("Failed to allocate error log entry (cb=%u)\n", cbTotal);
    }
}

#ifdef DEBUG

static const char *vpoxNetLwfWinStatusToText(NDIS_STATUS code)
{
    switch (code)
    {
        case NDIS_STATUS_MEDIA_CONNECT: return "NDIS_STATUS_MEDIA_CONNECT";
        case NDIS_STATUS_MEDIA_DISCONNECT: return "NDIS_STATUS_MEDIA_DISCONNECT";
        case NDIS_STATUS_RESET_START: return "NDIS_STATUS_RESET_START";
        case NDIS_STATUS_RESET_END: return "NDIS_STATUS_RESET_END";
        case NDIS_STATUS_MEDIA_BUSY: return "NDIS_STATUS_MEDIA_BUSY";
        case NDIS_STATUS_MEDIA_SPECIFIC_INDICATION: return "NDIS_STATUS_MEDIA_SPECIFIC_INDICATION";
        case NDIS_STATUS_LINK_SPEED_CHANGE: return "NDIS_STATUS_LINK_SPEED_CHANGE";
        case NDIS_STATUS_LINK_STATE: return "NDIS_STATUS_LINK_STATE";
        case NDIS_STATUS_PORT_STATE: return "NDIS_STATUS_PORT_STATE";
        case NDIS_STATUS_OPER_STATUS: return "NDIS_STATUS_OPER_STATUS";
        case NDIS_STATUS_NETWORK_CHANGE: return "NDIS_STATUS_NETWORK_CHANGE";
        case NDIS_STATUS_PACKET_FILTER: return "NDIS_STATUS_PACKET_FILTER";
        case NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG: return "NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG";
        case NDIS_STATUS_TASK_OFFLOAD_HARDWARE_CAPABILITIES: return "NDIS_STATUS_TASK_OFFLOAD_HARDWARE_CAPABILITIES";
        case NDIS_STATUS_OFFLOAD_ENCASPULATION_CHANGE: return "NDIS_STATUS_OFFLOAD_ENCASPULATION_CHANGE";
        case NDIS_STATUS_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES: return "NDIS_STATUS_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES";
    }
    return "unknown";
}

static void vpoxNetLwfWinDumpFilterTypes(ULONG uFlags)
{
    if (uFlags & NDIS_PACKET_TYPE_DIRECTED) Log5(("   NDIS_PACKET_TYPE_DIRECTED\n"));
    if (uFlags & NDIS_PACKET_TYPE_MULTICAST) Log5(("   NDIS_PACKET_TYPE_MULTICAST\n"));
    if (uFlags & NDIS_PACKET_TYPE_ALL_MULTICAST) Log5(("   NDIS_PACKET_TYPE_ALL_MULTICAST\n"));
    if (uFlags & NDIS_PACKET_TYPE_BROADCAST) Log5(("   NDIS_PACKET_TYPE_BROADCAST\n"));
    if (uFlags & NDIS_PACKET_TYPE_PROMISCUOUS) Log5(("   NDIS_PACKET_TYPE_PROMISCUOUS\n"));
    if (uFlags & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) Log5(("   NDIS_PACKET_TYPE_ALL_FUNCTIONAL\n"));
    if (uFlags & NDIS_PACKET_TYPE_ALL_LOCAL) Log5(("   NDIS_PACKET_TYPE_ALL_LOCAL\n"));
    if (uFlags & NDIS_PACKET_TYPE_FUNCTIONAL) Log5(("   NDIS_PACKET_TYPE_FUNCTIONAL\n"));
    if (uFlags & NDIS_PACKET_TYPE_GROUP) Log5(("   NDIS_PACKET_TYPE_GROUP\n"));
    if (uFlags & NDIS_PACKET_TYPE_MAC_FRAME) Log5(("   NDIS_PACKET_TYPE_MAC_FRAME\n"));
    if (uFlags & NDIS_PACKET_TYPE_SMT) Log5(("   NDIS_PACKET_TYPE_SMT\n"));
    if (uFlags & NDIS_PACKET_TYPE_SOURCE_ROUTING) Log5(("   NDIS_PACKET_TYPE_SOURCE_ROUTING\n"));
    if (uFlags == 0) Log5(("   NONE\n"));
}

DECLINLINE(void) vpoxNetLwfWinDumpEncapsulation(const char *pcszText, ULONG uEncapsulation)
{
    if (uEncapsulation == NDIS_ENCAPSULATION_NOT_SUPPORTED)
        Log5(("%s not supported\n", pcszText));
    else
    {
        Log5(("%s", pcszText));
        if (uEncapsulation & NDIS_ENCAPSULATION_NULL)
            Log5((" null"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_802_3)
            Log5((" 802.3"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q)
            Log5((" 802.3pq"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q_IN_OOB)
            Log5((" 802.3pq(oob)"));
        if (uEncapsulation & NDIS_ENCAPSULATION_IEEE_LLC_SNAP_ROUTED)
            Log5((" LLC"));
        Log5(("\n"));
    }
}

DECLINLINE(const char *) vpoxNetLwfWinSetOnOffText(ULONG uOnOff)
{
    switch (uOnOff)
    {
        case NDIS_OFFLOAD_SET_NO_CHANGE: return "no change";
        case NDIS_OFFLOAD_SET_ON: return "on";
        case NDIS_OFFLOAD_SET_OFF: return "off";
    }
    return "unknown";
}

DECLINLINE(const char *) vpoxNetLwfWinOnOffText(ULONG uOnOff)
{
    switch (uOnOff)
    {
        case NDIS_OFFLOAD_NOT_SUPPORTED: return "off";
        case NDIS_OFFLOAD_SUPPORTED: return "on";
    }
    return "unknown";
}

DECLINLINE(const char *) vpoxNetLwfWinSupportedText(ULONG uSupported)
{
    switch (uSupported)
    {
        case NDIS_OFFLOAD_NOT_SUPPORTED: return "not supported";
        case NDIS_OFFLOAD_SUPPORTED: return "supported";
    }
    return "unknown";
}

static void vpoxNetLwfWinDumpSetOffloadSettings(PNDIS_OFFLOAD pOffloadConfig)
{
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv4Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv4Transmit.Encapsulation);
    Log5(("   Checksum.IPv4Transmit.IpOptionsSupported          = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpOptionsSupported         = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpChecksum                 = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv4Transmit.UdpChecksum                 = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.UdpChecksum)));
    Log5(("   Checksum.IPv4Transmit.IpChecksum                  = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv4Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv4Receive.Encapsulation);
    Log5(("   Checksum.IPv4Receive.IpOptionsSupported           = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpOptionsSupported          = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpChecksum                  = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpChecksum)));
    Log5(("   Checksum.IPv4Receive.UdpChecksum                  = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.UdpChecksum)));
    Log5(("   Checksum.IPv4Receive.IpChecksum                   = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv6Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv6Transmit.Encapsulation);
    Log5(("   Checksum.IPv6Transmit.IpExtensionHeadersSupported = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpOptionsSupported         = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpChecksum                 = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv6Transmit.UdpChecksum                 = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Transmit.UdpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv6Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv6Receive.Encapsulation);
    Log5(("   Checksum.IPv6Receive.IpExtensionHeadersSupported  = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Receive.TcpOptionsSupported          = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Receive.TcpChecksum                  = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpChecksum)));
    Log5(("   Checksum.IPv6Receive.UdpChecksum                  = %s\n", vpoxNetLwfWinSetOnOffText(pOffloadConfig->Checksum.IPv6Receive.UdpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   LsoV1.IPv4.Encapsulation                          =", pOffloadConfig->LsoV1.IPv4.Encapsulation);
    Log5(("   LsoV1.IPv4.TcpOptions                             = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.TcpOptions)));
    Log5(("   LsoV1.IPv4.IpOptions                              = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.IpOptions)));
    vpoxNetLwfWinDumpEncapsulation("   LsoV2.IPv4.Encapsulation                          =", pOffloadConfig->LsoV2.IPv4.Encapsulation);
    vpoxNetLwfWinDumpEncapsulation("   LsoV2.IPv6.Encapsulation                          =", pOffloadConfig->LsoV2.IPv6.Encapsulation);
    Log5(("   LsoV2.IPv6.IpExtensionHeadersSupported            = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.IpExtensionHeadersSupported)));
    Log5(("   LsoV2.IPv6.TcpOptionsSupported                    = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.TcpOptionsSupported)));
}

static void vpoxNetLwfWinDumpOffloadSettings(PNDIS_OFFLOAD pOffloadConfig)
{
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv4Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv4Transmit.Encapsulation);
    Log5(("   Checksum.IPv4Transmit.IpOptionsSupported          = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpOptionsSupported         = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Transmit.TcpChecksum                 = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv4Transmit.UdpChecksum                 = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.UdpChecksum)));
    Log5(("   Checksum.IPv4Transmit.IpChecksum                  = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Transmit.IpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv4Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv4Receive.Encapsulation);
    Log5(("   Checksum.IPv4Receive.IpOptionsSupported           = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpOptionsSupported          = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv4Receive.TcpChecksum                  = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.TcpChecksum)));
    Log5(("   Checksum.IPv4Receive.UdpChecksum                  = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.UdpChecksum)));
    Log5(("   Checksum.IPv4Receive.IpChecksum                   = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv4Receive.IpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv6Transmit.Encapsulation               =", pOffloadConfig->Checksum.IPv6Transmit.Encapsulation);
    Log5(("   Checksum.IPv6Transmit.IpExtensionHeadersSupported = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpOptionsSupported         = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Transmit.TcpChecksum                 = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.TcpChecksum)));
    Log5(("   Checksum.IPv6Transmit.UdpChecksum                 = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Transmit.UdpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   Checksum.IPv6Receive.Encapsulation                =", pOffloadConfig->Checksum.IPv6Receive.Encapsulation);
    Log5(("   Checksum.IPv6Receive.IpExtensionHeadersSupported  = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.IpExtensionHeadersSupported)));
    Log5(("   Checksum.IPv6Receive.TcpOptionsSupported          = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpOptionsSupported)));
    Log5(("   Checksum.IPv6Receive.TcpChecksum                  = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.TcpChecksum)));
    Log5(("   Checksum.IPv6Receive.UdpChecksum                  = %s\n", vpoxNetLwfWinOnOffText(pOffloadConfig->Checksum.IPv6Receive.UdpChecksum)));
    vpoxNetLwfWinDumpEncapsulation("   LsoV1.IPv4.Encapsulation                          =", pOffloadConfig->LsoV1.IPv4.Encapsulation);
    Log5(("   LsoV1.IPv4.TcpOptions                             = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.TcpOptions)));
    Log5(("   LsoV1.IPv4.IpOptions                              = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV1.IPv4.IpOptions)));
    vpoxNetLwfWinDumpEncapsulation("   LsoV2.IPv4.Encapsulation                          =", pOffloadConfig->LsoV2.IPv4.Encapsulation);
    vpoxNetLwfWinDumpEncapsulation("   LsoV2.IPv6.Encapsulation                          =", pOffloadConfig->LsoV2.IPv6.Encapsulation);
    Log5(("   LsoV2.IPv6.IpExtensionHeadersSupported            = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.IpExtensionHeadersSupported)));
    Log5(("   LsoV2.IPv6.TcpOptionsSupported                    = %s\n", vpoxNetLwfWinSupportedText(pOffloadConfig->LsoV2.IPv6.TcpOptionsSupported)));
}

static const char *vpoxNetLwfWinStateToText(uint32_t enmState)
{
    switch (enmState)
    {
        case LwfState_Detached: return "Detached";
        case LwfState_Attaching: return "Attaching";
        case LwfState_Paused: return "Paused";
        case LwfState_Restarting: return "Restarting";
        case LwfState_Running: return "Running";
        case LwfState_Pausing: return "Pausing";
    }
    return "invalid";
}

static void vpoxNetLwfWinDumpPackets(const char *pszMsg, PNET_BUFFER_LIST pBufLists)
{
    for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
    {
        for (PNET_BUFFER pBuf = NET_BUFFER_LIST_FIRST_NB(pList); pBuf; pBuf = NET_BUFFER_NEXT_NB(pBuf))
        {
            Log6(("%s packet: src=%p cb=%d offset=%d", pszMsg, pList->SourceHandle, NET_BUFFER_DATA_LENGTH(pBuf), NET_BUFFER_DATA_OFFSET(pBuf)));
            for (PMDL pMdl = NET_BUFFER_FIRST_MDL(pBuf);
                 pMdl != NULL;
                 pMdl = NDIS_MDL_LINKAGE(pMdl))
            {
                Log6((" MDL: cb=%d", MmGetMdlByteCount(pMdl)));
            }
            Log6(("\n"));
        }
    }
}

DECLINLINE(const char *) vpoxNetLwfWinEthTypeStr(uint16_t uType)
{
    switch (uType)
    {
        case RTNET_ETHERTYPE_IPV4: return "IP";
        case RTNET_ETHERTYPE_IPV6: return "IPv6";
        case RTNET_ETHERTYPE_ARP:  return "ARP";
    }
    return "unknown";
}

#define VPOXNETLWF_PKTDMPSIZE 0x50

/**
 * Dump a packet to debug log.
 *
 * @param   cpPacket    The packet.
 * @param   cb          The size of the packet.
 * @param   cszText     A string denoting direction of packet transfer.
 */
DECLINLINE(void) vpoxNetLwfWinDumpPacket(PCINTNETSG pSG, const char *cszText)
{
    uint8_t bPacket[VPOXNETLWF_PKTDMPSIZE];

    uint32_t cb = pSG->cbTotal < VPOXNETLWF_PKTDMPSIZE ? pSG->cbTotal : VPOXNETLWF_PKTDMPSIZE;
    IntNetSgReadEx(pSG, 0, cb, bPacket);

    AssertReturnVoid(cb >= 14);

    uint8_t *pHdr = bPacket;
    uint8_t *pEnd = bPacket + cb;
    AssertReturnVoid(pEnd - pHdr >= 14);
    uint16_t uEthType = RT_N2H_U16(*(uint16_t*)(pHdr+12));
    Log2(("NetLWF: %s (%d bytes), %RTmac => %RTmac, EthType=%s(0x%x)\n",
          cszText, pSG->cbTotal, pHdr+6, pHdr, vpoxNetLwfWinEthTypeStr(uEthType), uEthType));
    pHdr += sizeof(RTNETETHERHDR);
    if (uEthType == RTNET_ETHERTYPE_VLAN)
    {
        AssertReturnVoid(pEnd - pHdr >= 4);
        uEthType = RT_N2H_U16(*(uint16_t*)(pHdr+2));
        Log2((" + VLAN: id=%d EthType=%s(0x%x)\n", RT_N2H_U16(*(uint16_t*)(pHdr)) & 0xFFF,
              vpoxNetLwfWinEthTypeStr(uEthType), uEthType));
        pHdr += 2 * sizeof(uint16_t);
    }
    uint8_t uProto = 0xFF;
    switch (uEthType)
    {
        case RTNET_ETHERTYPE_IPV6:
            AssertReturnVoid(pEnd - pHdr >= 40);
            uProto = pHdr[6];
            Log2((" + IPv6: %RTnaipv6 => %RTnaipv6\n", pHdr+8, pHdr+24));
            pHdr += 40;
            break;
        case RTNET_ETHERTYPE_IPV4:
            AssertReturnVoid(pEnd - pHdr >= 20);
            uProto = pHdr[9];
            Log2((" + IP: %RTnaipv4 => %RTnaipv4\n", *(uint32_t*)(pHdr+12), *(uint32_t*)(pHdr+16)));
            pHdr += (pHdr[0] & 0xF) * 4;
            break;
        case RTNET_ETHERTYPE_ARP:
            AssertReturnVoid(pEnd - pHdr >= 28);
            AssertReturnVoid(RT_N2H_U16(*(uint16_t*)(pHdr+2)) == RTNET_ETHERTYPE_IPV4);
            switch (RT_N2H_U16(*(uint16_t*)(pHdr+6)))
            {
                case 1: /* ARP request */
                    Log2((" + ARP-REQ: who-has %RTnaipv4 tell %RTnaipv4\n",
                          *(uint32_t*)(pHdr+24), *(uint32_t*)(pHdr+14)));
                    break;
                case 2: /* ARP reply */
                    Log2((" + ARP-RPL: %RTnaipv4 is-at %RTmac\n",
                          *(uint32_t*)(pHdr+14), pHdr+8));
                    break;
                default:
                    Log2((" + ARP: unknown op %d\n", RT_N2H_U16(*(uint16_t*)(pHdr+6))));
                    break;
            }
            break;
        /* There is no default case as uProto is initialized with 0xFF */
    }
    while (uProto != 0xFF)
    {
        switch (uProto)
        {
            case 0:  /* IPv6 Hop-by-Hop option*/
            case 60: /* IPv6 Destination option*/
            case 43: /* IPv6 Routing option */
            case 44: /* IPv6 Fragment option */
                Log2((" + IPv6 option (%d): <not implemented>\n", uProto));
                uProto = pHdr[0];
                pHdr += pHdr[1] * 8 + 8; /* Skip to the next extension/protocol */
                break;
            case 51: /* IPv6 IPsec AH */
                Log2((" + IPv6 IPsec AH: <not implemented>\n"));
                uProto = pHdr[0];
                pHdr += (pHdr[1] + 2) * 4; /* Skip to the next extension/protocol */
                break;
            case 50: /* IPv6 IPsec ESP */
                /* Cannot decode IPsec, fall through */
                Log2((" + IPv6 IPsec ESP: <not implemented>\n"));
                uProto = 0xFF;
                break;
            case 59: /* No Next Header */
                Log2((" + IPv6 No Next Header\n"));
                uProto = 0xFF;
                break;
            case 58: /* IPv6-ICMP */
                switch (pHdr[0])
                {
                    case 1:   Log2((" + IPv6-ICMP: destination unreachable, code %d\n", pHdr[1])); break;
                    case 128: Log2((" + IPv6-ICMP: echo request\n")); break;
                    case 129: Log2((" + IPv6-ICMP: echo reply\n")); break;
                    default:  Log2((" + IPv6-ICMP: unknown type %d, code %d\n", pHdr[0], pHdr[1])); break;
                }
                uProto = 0xFF;
                break;
            case 1: /* ICMP */
                switch (pHdr[0])
                {
                    case 0:  Log2((" + ICMP: echo reply\n")); break;
                    case 8:  Log2((" + ICMP: echo request\n")); break;
                    case 3:  Log2((" + ICMP: destination unreachable, code %d\n", pHdr[1])); break;
                    default: Log2((" + ICMP: unknown type %d, code %d\n", pHdr[0], pHdr[1])); break;
                }
                uProto = 0xFF;
                break;
            case 6: /* TCP */
                Log2((" + TCP: src=%d dst=%d seq=%x ack=%x\n",
                      RT_N2H_U16(*(uint16_t*)(pHdr)), RT_N2H_U16(*(uint16_t*)(pHdr+2)),
                      RT_N2H_U32(*(uint32_t*)(pHdr+4)), RT_N2H_U32(*(uint32_t*)(pHdr+8))));
                uProto = 0xFF;
                break;
            case 17: /* UDP */
                Log2((" + UDP: src=%d dst=%d\n",
                      RT_N2H_U16(*(uint16_t*)(pHdr)), RT_N2H_U16(*(uint16_t*)(pHdr+2))));
                uProto = 0xFF;
                break;
            default:
                Log2((" + Unknown: proto=0x%x\n", uProto));
                uProto = 0xFF;
                break;
        }
    }
    Log3(("%.*Rhxd\n", cb, bPacket));
}

#else /* !DEBUG */
# define vpoxNetLwfWinDumpFilterTypes(uFlags)    do { } while (0)
# define vpoxNetLwfWinDumpOffloadSettings(p)     do { } while (0)
# define vpoxNetLwfWinDumpSetOffloadSettings(p)  do { } while (0)
# define vpoxNetLwfWinDumpPackets(m,l)           do { } while (0)
# define vpoxNetLwfWinDumpPacket(p,t)            do { } while (0)
#endif /* !DEBUG */

DECLINLINE(bool) vpoxNetLwfWinChangeState(PVPOXNETLWF_MODULE pModuleCtx, uint32_t enmNew, uint32_t enmOld = LwfState_32BitHack)
{
    AssertReturn(pModuleCtx, false);

    bool fSuccess = true;
    if (enmOld != LwfState_32BitHack)
    {
        fSuccess = ASMAtomicCmpXchgU32(&pModuleCtx->enmState, enmNew, enmOld);
        if (fSuccess)
            Log(("vpoxNetLwfWinChangeState: state change %s -> %s\n",
                 vpoxNetLwfWinStateToText(enmOld),
                 vpoxNetLwfWinStateToText(enmNew)));
        else
            Log(("ERROR! vpoxNetLwfWinChangeState: failed state change %s (actual=%s) -> %s\n",
                 vpoxNetLwfWinStateToText(enmOld),
                 vpoxNetLwfWinStateToText(ASMAtomicReadU32(&pModuleCtx->enmState)),
                 vpoxNetLwfWinStateToText(enmNew)));
        Assert(fSuccess);
    }
    else
    {
        uint32_t enmPrevState = ASMAtomicXchgU32(&pModuleCtx->enmState, enmNew);
        Log(("vpoxNetLwfWinChangeState: state change %s -> %s\n",
             vpoxNetLwfWinStateToText(enmPrevState),
             vpoxNetLwfWinStateToText(enmNew)));
        NOREF(enmPrevState);
    }
    return fSuccess;
}

DECLINLINE(void) vpoxNetLwfWinInitOidRequest(PVPOXNETLWF_OIDREQ pRequest)
{
    NdisZeroMemory(pRequest, sizeof(VPOXNETLWF_OIDREQ));

    NdisInitializeEvent(&pRequest->Event);

    pRequest->Request.Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
    pRequest->Request.Header.Revision = NDIS_OID_REQUEST_REVISION_1;
    pRequest->Request.Header.Size = NDIS_SIZEOF_OID_REQUEST_REVISION_1;

    pRequest->Request.RequestId = (PVOID)VPOXNETLWF_REQ_ID;
}

static NDIS_STATUS vpoxNetLwfWinSyncOidRequest(PVPOXNETLWF_MODULE pModuleCtx, PVPOXNETLWF_OIDREQ pRequest)
{
    NDIS_STATUS Status = NdisFOidRequest(pModuleCtx->hFilter, &pRequest->Request);
    if (Status == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pRequest->Event, 0);
        Status = pRequest->Status;
    }
    return Status;
}

DECLINLINE(void) vpoxNetLwfWinCopyOidRequestResults(PNDIS_OID_REQUEST pFrom, PNDIS_OID_REQUEST pTo)
{
    switch (pFrom->RequestType)
    {
        case NdisRequestSetInformation:
            pTo->DATA.SET_INFORMATION.BytesRead   = pFrom->DATA.SET_INFORMATION.BytesRead;
            pTo->DATA.SET_INFORMATION.BytesNeeded = pFrom->DATA.SET_INFORMATION.BytesNeeded;
            break;
        case NdisRequestMethod:
            pTo->DATA.METHOD_INFORMATION.OutputBufferLength = pFrom->DATA.METHOD_INFORMATION.OutputBufferLength;
            pTo->DATA.METHOD_INFORMATION.BytesWritten       = pFrom->DATA.METHOD_INFORMATION.BytesWritten;
            pTo->DATA.METHOD_INFORMATION.BytesRead          = pFrom->DATA.METHOD_INFORMATION.BytesRead;
            pTo->DATA.METHOD_INFORMATION.BytesNeeded        = pFrom->DATA.METHOD_INFORMATION.BytesNeeded;
            break;
        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
        default:
            pTo->DATA.QUERY_INFORMATION.BytesWritten = pFrom->DATA.QUERY_INFORMATION.BytesWritten;
            pTo->DATA.QUERY_INFORMATION.BytesNeeded  = pFrom->DATA.QUERY_INFORMATION.BytesNeeded;
    }
}

void inline vpoxNetLwfWinOverridePacketFiltersUp(PVPOXNETLWF_MODULE pModuleCtx, ULONG *pFilters)
{
    if (ASMAtomicReadBool(&pModuleCtx->fActive) && !ASMAtomicReadBool(&pModuleCtx->fHostPromisc))
        *pFilters &= ~NDIS_PACKET_TYPE_PROMISCUOUS;
}

NDIS_STATUS vpoxNetLwfWinOidRequest(IN NDIS_HANDLE hModuleCtx,
                                    IN PNDIS_OID_REQUEST pOidRequest)
{
    LogFlow(("==>vpoxNetLwfWinOidRequest: module=%p\n", hModuleCtx));
    vpoxNetCmnWinDumpOidRequest(__FUNCTION__, pOidRequest);
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)hModuleCtx;
    PNDIS_OID_REQUEST pClone = NULL;
    NDIS_STATUS Status = NdisAllocateCloneOidRequest(pModuleCtx->hFilter,
                                                     pOidRequest,
                                                     VPOXNETLWF_MEM_TAG,
                                                     &pClone);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        /* Save the pointer to the original */
        *((PNDIS_OID_REQUEST*)(pClone->SourceReserved)) = pOidRequest;

        pClone->RequestId = pOidRequest->RequestId;
        /* We are not supposed to get another request until we are through with the one we "postponed" */
        PNDIS_OID_REQUEST pPrev = ASMAtomicXchgPtrT(&pModuleCtx->pPendingRequest, pClone, PNDIS_OID_REQUEST);
        Assert(pPrev == NULL);
        pModuleCtx->pPendingRequest = pClone;
        if (pOidRequest->RequestType == NdisRequestSetInformation
            && pOidRequest->DATA.SET_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER)
        {
            ASMAtomicWriteBool(&pModuleCtx->fHostPromisc, !!(*(ULONG*)pOidRequest->DATA.SET_INFORMATION.InformationBuffer & NDIS_PACKET_TYPE_PROMISCUOUS));
            Log(("vpoxNetLwfWinOidRequest: host wanted to set packet filter value to:\n"));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pOidRequest->DATA.SET_INFORMATION.InformationBuffer);
            /* Keep adapter in promisc mode as long as we are active. */
            if (ASMAtomicReadBool(&pModuleCtx->fActive))
                *(ULONG*)pClone->DATA.SET_INFORMATION.InformationBuffer |= NDIS_PACKET_TYPE_PROMISCUOUS;
            Log5(("vpoxNetLwfWinOidRequest: pass the following packet filters to miniport:\n"));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pOidRequest->DATA.SET_INFORMATION.InformationBuffer);
        }
        if (pOidRequest->RequestType == NdisRequestSetInformation
            && pOidRequest->DATA.SET_INFORMATION.Oid == OID_TCP_OFFLOAD_CURRENT_CONFIG)
        {
            Log5(("vpoxNetLwfWinOidRequest: offloading set to:\n"));
            vpoxNetLwfWinDumpSetOffloadSettings((PNDIS_OFFLOAD)pOidRequest->DATA.SET_INFORMATION.InformationBuffer);
        }

        /* Forward the clone to underlying filters/miniport */
        Status = NdisFOidRequest(pModuleCtx->hFilter, pClone);
        if (Status != NDIS_STATUS_PENDING)
        {
            /* Synchronous completion */
            pPrev = ASMAtomicXchgPtrT(&pModuleCtx->pPendingRequest, NULL, PNDIS_OID_REQUEST);
            Assert(pPrev == pClone);
            Log5(("vpoxNetLwfWinOidRequest: got the following packet filters from miniport:\n"));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pOidRequest->DATA.QUERY_INFORMATION.InformationBuffer);
            /*
             * The host does not expect the adapter to be in promisc mode,
             * unless it enabled the mode. Let's not disillusion it.
             */
            if (   pOidRequest->RequestType == NdisRequestQueryInformation
                && pOidRequest->DATA.QUERY_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER)
                vpoxNetLwfWinOverridePacketFiltersUp(pModuleCtx, (ULONG*)pOidRequest->DATA.QUERY_INFORMATION.InformationBuffer);
            Log5(("vpoxNetLwfWinOidRequest: reporting to the host the following packet filters:\n"));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pOidRequest->DATA.QUERY_INFORMATION.InformationBuffer);
            vpoxNetLwfWinCopyOidRequestResults(pClone, pOidRequest);
            NdisFreeCloneOidRequest(pModuleCtx->hFilter, pClone);
        }
        /* In case of async completion we do the rest in vpoxNetLwfWinOidRequestComplete() */
    }
    else
    {
        LogError(("vpoxNetLwfWinOidRequest: NdisAllocateCloneOidRequest failed with 0x%x\n", Status));
    }
    LogFlow(("<==vpoxNetLwfWinOidRequest: Status=0x%x\n", Status));
    return Status;
}

VOID vpoxNetLwfWinOidRequestComplete(IN NDIS_HANDLE hModuleCtx,
                                     IN PNDIS_OID_REQUEST pRequest,
                                     IN NDIS_STATUS Status)
{
    LogFlow(("==>vpoxNetLwfWinOidRequestComplete: module=%p req=%p status=0x%x\n", hModuleCtx, pRequest, Status));
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)hModuleCtx;
    PNDIS_OID_REQUEST pOriginal = *((PNDIS_OID_REQUEST*)(pRequest->SourceReserved));
    if (pOriginal)
    {
        /* NDIS is supposed to serialize requests */
        PNDIS_OID_REQUEST pPrev = ASMAtomicXchgPtrT(&pModuleCtx->pPendingRequest, NULL, PNDIS_OID_REQUEST);
        Assert(pPrev == pRequest); NOREF(pPrev);

        Log5(("vpoxNetLwfWinOidRequestComplete: completed rq type=%d oid=%x\n", pRequest->RequestType, pRequest->DATA.QUERY_INFORMATION.Oid));
        vpoxNetLwfWinCopyOidRequestResults(pRequest, pOriginal);
        if (   pRequest->RequestType == NdisRequestQueryInformation
            && pRequest->DATA.QUERY_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER)
        {
            Log5(("vpoxNetLwfWinOidRequestComplete: underlying miniport reports its packet filters:\n"));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pRequest->DATA.QUERY_INFORMATION.InformationBuffer);
            vpoxNetLwfWinOverridePacketFiltersUp(pModuleCtx, (ULONG*)pRequest->DATA.QUERY_INFORMATION.InformationBuffer);
            Log5(("vpoxNetLwfWinOidRequestComplete: reporting the following packet filters to upper protocol:\n"));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pRequest->DATA.QUERY_INFORMATION.InformationBuffer);
        }
        NdisFreeCloneOidRequest(pModuleCtx->hFilter, pRequest);
        NdisFOidRequestComplete(pModuleCtx->hFilter, pOriginal, Status);
    }
    else
    {
        /* This is not a clone, we originated it */
        Log(("vpoxNetLwfWinOidRequestComplete: locally originated request (%p) completed, status=0x%x\n", pRequest, Status));
        PVPOXNETLWF_OIDREQ pRqWrapper = RT_FROM_MEMBER(pRequest, VPOXNETLWF_OIDREQ, Request);
        pRqWrapper->Status = Status;
        NdisSetEvent(&pRqWrapper->Event);
    }
    LogFlow(("<==vpoxNetLwfWinOidRequestComplete\n"));
}


static bool vpoxNetLwfWinIsPromiscuous(PVPOXNETLWF_MODULE pModuleCtx)
{
    return ASMAtomicReadBool(&pModuleCtx->fHostPromisc);
}

#if 0
static NDIS_STATUS vpoxNetLwfWinGetPacketFilter(PVPOXNETLWF_MODULE pModuleCtx)
{
    LogFlow(("==>vpoxNetLwfWinGetPacketFilter: module=%p\n", pModuleCtx));
    VPOXNETLWF_OIDREQ Rq;
    vpoxNetLwfWinInitOidRequest(&Rq);
    Rq.Request.RequestType = NdisRequestQueryInformation;
    Rq.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    Rq.Request.DATA.QUERY_INFORMATION.InformationBuffer = &pModuleCtx->uPacketFilter;
    Rq.Request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(pModuleCtx->uPacketFilter);
    NDIS_STATUS Status = vpoxNetLwfWinSyncOidRequest(pModuleCtx, &Rq);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        LogError(("vpoxNetLwfWinGetPacketFilter: vpoxNetLwfWinSyncOidRequest(query, OID_GEN_CURRENT_PACKET_FILTER) failed with 0x%x\n", Status));
        return FALSE;
    }
    if (Rq.Request.DATA.QUERY_INFORMATION.BytesWritten != sizeof(pModuleCtx->uPacketFilter))
    {
        LogError(("vpoxNetLwfWinGetPacketFilter: vpoxNetLwfWinSyncOidRequest(query, OID_GEN_CURRENT_PACKET_FILTER) failed to write neccessary amount (%d bytes), actually written %d bytes\n", sizeof(pModuleCtx->uPacketFilter), Rq.Request.DATA.QUERY_INFORMATION.BytesWritten));
    }

    Log5(("vpoxNetLwfWinGetPacketFilter: OID_GEN_CURRENT_PACKET_FILTER query returned the following filters:\n"));
    vpoxNetLwfWinDumpFilterTypes(pModuleCtx->uPacketFilter);

    LogFlow(("<==vpoxNetLwfWinGetPacketFilter: status=0x%x\n", Status));
    return Status;
}
#endif

static NDIS_STATUS vpoxNetLwfWinSetPacketFilter(PVPOXNETLWF_MODULE pModuleCtx, bool fPromisc)
{
    LogFlow(("==>vpoxNetLwfWinSetPacketFilter: module=%p %s\n", pModuleCtx, fPromisc ? "promiscuous" : "normal"));
    ULONG uFilter = 0;
    VPOXNETLWF_OIDREQ Rq;
    vpoxNetLwfWinInitOidRequest(&Rq);
    Rq.Request.RequestType = NdisRequestQueryInformation;
    Rq.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    Rq.Request.DATA.QUERY_INFORMATION.InformationBuffer = &uFilter;
    Rq.Request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(uFilter);
    NDIS_STATUS Status = vpoxNetLwfWinSyncOidRequest(pModuleCtx, &Rq);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        LogError(("vpoxNetLwfWinSetPacketFilter: vpoxNetLwfWinSyncOidRequest(query, OID_GEN_CURRENT_PACKET_FILTER) failed with 0x%x\n", Status));
        return Status;
    }
    if (Rq.Request.DATA.QUERY_INFORMATION.BytesWritten != sizeof(uFilter))
    {
        LogError(("vpoxNetLwfWinSetPacketFilter: vpoxNetLwfWinSyncOidRequest(query, OID_GEN_CURRENT_PACKET_FILTER) failed to write neccessary amount (%d bytes), actually written %d bytes\n", sizeof(uFilter), Rq.Request.DATA.QUERY_INFORMATION.BytesWritten));
        return NDIS_STATUS_FAILURE;
    }

    Log5(("vpoxNetLwfWinSetPacketFilter: OID_GEN_CURRENT_PACKET_FILTER query returned the following filters:\n"));
    vpoxNetLwfWinDumpFilterTypes(uFilter);

    if (fPromisc)
    {
        /* If we about to go promiscuous, save the state before we change it. */
        ASMAtomicWriteBool(&pModuleCtx->fHostPromisc, !!(uFilter & NDIS_PACKET_TYPE_PROMISCUOUS));
        uFilter |= NDIS_PACKET_TYPE_PROMISCUOUS;
    }
    else
    {
        /* Reset promisc only if it was not enabled before we had changed it. */
        if (!ASMAtomicReadBool(&pModuleCtx->fHostPromisc))
            uFilter &= ~NDIS_PACKET_TYPE_PROMISCUOUS;
    }

    Log5(("vpoxNetLwfWinSetPacketFilter: OID_GEN_CURRENT_PACKET_FILTER about to set the following filters:\n"));
    vpoxNetLwfWinDumpFilterTypes(uFilter);

    NdisResetEvent(&Rq.Event); /* need to reset as it has been set by query op */
    Rq.Request.RequestType = NdisRequestSetInformation;
    Rq.Request.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    Rq.Request.DATA.SET_INFORMATION.InformationBuffer = &uFilter;
    Rq.Request.DATA.SET_INFORMATION.InformationBufferLength = sizeof(uFilter);
    Status = vpoxNetLwfWinSyncOidRequest(pModuleCtx, &Rq);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        LogError(("vpoxNetLwfWinSetPacketFilter: vpoxNetLwfWinSyncOidRequest(set, OID_GEN_CURRENT_PACKET_FILTER, vvv below vvv) failed with 0x%x\n", Status));
        vpoxNetLwfWinDumpFilterTypes(uFilter);
    }
    LogFlow(("<==vpoxNetLwfWinSetPacketFilter: status=0x%x\n", Status));
    return Status;
}


static NTSTATUS vpoxNetLwfWinDevDispatch(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    RT_NOREF1(pDevObj);
    PIO_STACK_LOCATION pIrpSl = IoGetCurrentIrpStackLocation(pIrp);;
    NTSTATUS Status = STATUS_SUCCESS;

    switch (pIrpSl->MajorFunction)
    {
        case IRP_MJ_DEVICE_CONTROL:
            Status = STATUS_NOT_SUPPORTED;
            break;
        case IRP_MJ_CREATE:
        case IRP_MJ_CLEANUP:
        case IRP_MJ_CLOSE:
            break;
        default:
            AssertFailed();
            break;
    }

    pIrp->IoStatus.Status = Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return Status;
}

/** @todo So far we had no use for device, should we even bother to create it? */
static NDIS_STATUS vpoxNetLwfWinDevCreate(PVPOXNETLWFGLOBALS pGlobals)
{
    NDIS_STRING DevName, LinkName;
    PDRIVER_DISPATCH aMajorFunctions[IRP_MJ_MAXIMUM_FUNCTION+1];
    NdisInitUnicodeString(&DevName, VPOXNETLWF_NAME_DEVICE);
    NdisInitUnicodeString(&LinkName, VPOXNETLWF_NAME_LINK);

    Assert(!pGlobals->hDevice);
    Assert(!pGlobals->pDevObj);
    NdisZeroMemory(aMajorFunctions, sizeof (aMajorFunctions));
    aMajorFunctions[IRP_MJ_CREATE] = vpoxNetLwfWinDevDispatch;
    aMajorFunctions[IRP_MJ_CLEANUP] = vpoxNetLwfWinDevDispatch;
    aMajorFunctions[IRP_MJ_CLOSE] = vpoxNetLwfWinDevDispatch;
    aMajorFunctions[IRP_MJ_DEVICE_CONTROL] = vpoxNetLwfWinDevDispatch;

    NDIS_DEVICE_OBJECT_ATTRIBUTES DeviceAttributes;
    NdisZeroMemory(&DeviceAttributes, sizeof(DeviceAttributes));
    DeviceAttributes.Header.Type = NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
    DeviceAttributes.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceAttributes.Header.Size = sizeof(DeviceAttributes);
    DeviceAttributes.DeviceName = &DevName;
    DeviceAttributes.SymbolicName = &LinkName;
    DeviceAttributes.MajorFunctions = aMajorFunctions;
    //DeviceAttributes.ExtensionSize = sizeof(FILTER_DEVICE_EXTENSION);

    NDIS_STATUS Status = NdisRegisterDeviceEx(pGlobals->hFilterDriver,
                                              &DeviceAttributes,
                                              &pGlobals->pDevObj,
                                              &pGlobals->hDevice);
    Log(("vpoxNetLwfWinDevCreate: NdisRegisterDeviceEx returned 0x%x\n", Status));
    Assert(Status == NDIS_STATUS_SUCCESS);
#if 0
    if (Status == NDIS_STATUS_SUCCESS)
    {
        PFILTER_DEVICE_EXTENSION pExtension;
        pExtension = NdisGetDeviceReservedExtension(pGlobals->pDevObj);
        pExtension->Signature = VPOXNETLWF_MEM_TAG;
        pExtension->Handle = pGlobals->hFilterDriver;
    }
#endif
    return Status;
}

static void vpoxNetLwfWinDevDestroy(PVPOXNETLWFGLOBALS pGlobals)
{
    Assert(pGlobals->hDevice);
    Assert(pGlobals->pDevObj);
    NdisDeregisterDeviceEx(pGlobals->hDevice);
    pGlobals->hDevice = NULL;
    pGlobals->pDevObj = NULL;
}

static void vpoxNetLwfWinDisableOffloading(PNDIS_OFFLOAD pOffloadConfig)
{
    pOffloadConfig->Checksum.IPv4Transmit.Encapsulation               = NDIS_ENCAPSULATION_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv4Transmit.IpOptionsSupported          = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv4Transmit.TcpOptionsSupported         = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv4Transmit.TcpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv4Transmit.UdpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv4Transmit.IpChecksum                  = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv6Transmit.Encapsulation               = NDIS_ENCAPSULATION_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv6Transmit.TcpOptionsSupported         = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv6Transmit.TcpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->Checksum.IPv6Transmit.UdpChecksum                 = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->LsoV1.IPv4.Encapsulation                          = NDIS_ENCAPSULATION_NOT_SUPPORTED;
    pOffloadConfig->LsoV1.IPv4.TcpOptions                             = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->LsoV1.IPv4.IpOptions                              = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->LsoV2.IPv4.Encapsulation                          = NDIS_ENCAPSULATION_NOT_SUPPORTED;
    pOffloadConfig->LsoV2.IPv6.Encapsulation                          = NDIS_ENCAPSULATION_NOT_SUPPORTED;
    pOffloadConfig->LsoV2.IPv6.IpExtensionHeadersSupported            = NDIS_OFFLOAD_NOT_SUPPORTED;
    pOffloadConfig->LsoV2.IPv6.TcpOptionsSupported                    = NDIS_OFFLOAD_NOT_SUPPORTED;
}

static void vpoxNetLwfWinUpdateSavedOffloadConfig(PVPOXNETLWF_MODULE pModuleCtx, PNDIS_OFFLOAD pOffload)
{
    if (pModuleCtx->cbOffloadConfig < pOffload->Header.Size)
    {
        vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, STATUS_SUCCESS, 10);
        return;
    }

    NdisMoveMemory(pModuleCtx->pSavedOffloadConfig, pOffload, pOffload->Header.Size);
    NdisMoveMemory(pModuleCtx->pDisabledOffloadConfig, pOffload, pOffload->Header.Size);
    vpoxNetLwfWinDisableOffloading(pModuleCtx->pDisabledOffloadConfig);
    pModuleCtx->fOffloadConfigValid = true;
}

#ifdef VPOXNETLWF_FIXED_SIZE_POOLS
static void vpoxNetLwfWinFreePools(PVPOXNETLWF_MODULE pModuleCtx, int cPools)
{
    for (int i = 0; i < cPools; ++i)
    {
        if (pModuleCtx->hPool[i])
        {
            NdisFreeNetBufferListPool(pModuleCtx->hPool[i]);
            Log4(("vpoxNetLwfWinFreePools: freed NBL+NB pool 0x%p\n", pModuleCtx->hPool[i]));
        }
    }
}
#endif /* VPOXNETLWF_FIXED_SIZE_POOLS */


static void vpoxNetLwfWinFreeModuleResources(PVPOXNETLWF_MODULE pModuleCtx)
{
#ifdef VPOXNETLWF_FIXED_SIZE_POOLS
    vpoxNetLwfWinFreePools(pModuleCtx, RT_ELEMENTS(g_cbPool));
#else /* !VPOXNETLWF_FIXED_SIZE_POOLS */
    if (pModuleCtx->hPool)
    {
        NdisFreeNetBufferListPool(pModuleCtx->hPool);
        Log4(("vpoxNetLwfWinFreeModuleResources: freed NBL+NB pool 0x%p\n", pModuleCtx->hPool));
    }
#endif /* !VPOXNETLWF_FIXED_SIZE_POOLS */
    if (pModuleCtx->pDisabledOffloadConfig)
        NdisFreeMemory(pModuleCtx->pDisabledOffloadConfig, 0, 0);
    if (pModuleCtx->pSavedOffloadConfig)
        NdisFreeMemory(pModuleCtx->pSavedOffloadConfig, 0, 0);
    if (pModuleCtx->hWorkItem)
        NdisFreeIoWorkItem(pModuleCtx->hWorkItem);
    NdisFreeMemory(pModuleCtx, 0, 0);
}


DECLARE_GLOBAL_CONST_UNICODE_STRING(g_strHostOnlyMiniportName, L"VirtualPox Host-Only");

static NDIS_STATUS vpoxNetLwfWinAttach(IN NDIS_HANDLE hFilter, IN NDIS_HANDLE hDriverCtx,
                                       IN PNDIS_FILTER_ATTACH_PARAMETERS pParameters)
{
    LogFlow(("==>vpoxNetLwfWinAttach: filter=%p\n", hFilter));

    PVPOXNETLWFGLOBALS pGlobals = (PVPOXNETLWFGLOBALS)hDriverCtx;
    if (!pGlobals)
    {
        vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, NDIS_STATUS_FAILURE, 1);
        return NDIS_STATUS_FAILURE;
    }

    /*
     * We need a copy of NDIS_STRING structure as we are going to modify length
     * of the base miniport instance name since RTL does not support comparing
     * first n characters of two strings. We check if miniport names start with
     * "Virtual Host-Only" to detect host-only adapters. It is a waste of resources
     * to bind our filter to host-only adapters since they now operate independently.
     */
    NDIS_STRING strTruncatedInstanceName = *pParameters->BaseMiniportInstanceName; /* Do not copy data, only the structure itself */
    strTruncatedInstanceName.Length = g_strHostOnlyMiniportName.Length; /* Truncate instance name */
    if (RtlEqualUnicodeString(&strTruncatedInstanceName, &g_strHostOnlyMiniportName, TRUE /* Case insensitive */))
    {
        DbgPrint("vpoxNetLwfWinAttach: won't attach to %wZ\n", pParameters->BaseMiniportInstanceName);
        return NDIS_STATUS_FAILURE;
    }

    ANSI_STRING strMiniportName;
    /* We use the miniport name to associate this filter module with the netflt instance */
    NTSTATUS rc = RtlUnicodeStringToAnsiString(&strMiniportName,
                                               pParameters->BaseMiniportName,
                                               TRUE);
    if (rc != STATUS_SUCCESS)
    {
        LogError(("vpoxNetLwfWinAttach: RtlUnicodeStringToAnsiString(%ls) failed with 0x%x\n",
             pParameters->BaseMiniportName, rc));
        vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, NDIS_STATUS_FAILURE, 2);
        return NDIS_STATUS_FAILURE;
    }
    DbgPrint("vpoxNetLwfWinAttach: friendly name=%wZ\n", pParameters->BaseMiniportInstanceName);
    DbgPrint("vpoxNetLwfWinAttach: name=%Z\n", &strMiniportName);

    UINT cbModuleWithNameExtra = sizeof(VPOXNETLWF_MODULE) + strMiniportName.Length;
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)NdisAllocateMemoryWithTagPriority(hFilter,
                                                                      cbModuleWithNameExtra,
                                                                      VPOXNETLWF_MEM_TAG,
                                                                      LowPoolPriority);
    if (!pModuleCtx)
    {
        LogError(("vpoxNetLwfWinAttach: Failed to allocate module context for %ls\n", pParameters->BaseMiniportName));
        RtlFreeAnsiString(&strMiniportName);
        vpoxNetLwfLogErrorEvent(IO_ERR_INSUFFICIENT_RESOURCES, NDIS_STATUS_RESOURCES, 3);
        return NDIS_STATUS_RESOURCES;
    }
    Log4(("vpoxNetLwfWinAttach: allocated module context 0x%p\n", pModuleCtx));

    NdisZeroMemory(pModuleCtx, cbModuleWithNameExtra);
    NdisMoveMemory(pModuleCtx->szMiniportName, strMiniportName.Buffer, strMiniportName.Length);
    RtlFreeAnsiString(&strMiniportName);

    pModuleCtx->hWorkItem = NdisAllocateIoWorkItem(g_VPoxNetLwfGlobals.hFilterDriver);
    if (!pModuleCtx->hWorkItem)
    {
        LogError(("vpoxNetLwfWinAttach: Failed to allocate work item for %ls\n",
                pParameters->BaseMiniportName));
        NdisFreeMemory(pModuleCtx, 0, 0);
        vpoxNetLwfLogErrorEvent(IO_ERR_INSUFFICIENT_RESOURCES, NDIS_STATUS_RESOURCES, 4);
        return NDIS_STATUS_RESOURCES;
    }

    Assert(pParameters->MacAddressLength == sizeof(RTMAC));
    NdisMoveMemory(&pModuleCtx->MacAddr, pParameters->CurrentMacAddress, RT_MIN(sizeof(RTMAC), pParameters->MacAddressLength));

    pModuleCtx->cbOffloadConfig = sizeof(NDIS_OFFLOAD) * 2; /* Best guess to accomodate future expansion. */
    /* Get the exact size, if possible. */
    if (pParameters->DefaultOffloadConfiguration)
        pModuleCtx->cbOffloadConfig = pParameters->DefaultOffloadConfiguration->Header.Size;
    else
        vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, STATUS_SUCCESS, 8);

    pModuleCtx->pSavedOffloadConfig =
        (PNDIS_OFFLOAD)NdisAllocateMemoryWithTagPriority(hFilter, pModuleCtx->cbOffloadConfig,
                                                         VPOXNETLWF_MEM_TAG, LowPoolPriority);
    pModuleCtx->pDisabledOffloadConfig =
        (PNDIS_OFFLOAD)NdisAllocateMemoryWithTagPriority(hFilter, pModuleCtx->cbOffloadConfig,
                                                         VPOXNETLWF_MEM_TAG, LowPoolPriority);
    if (!pModuleCtx->pSavedOffloadConfig || !pModuleCtx->pDisabledOffloadConfig)
    {
        LogError(("vpoxNetLwfWinAttach: Failed to allocate offload config buffers for %ls\n",
                pParameters->BaseMiniportName));
        vpoxNetLwfWinFreeModuleResources(pModuleCtx);
        vpoxNetLwfLogErrorEvent(IO_ERR_INSUFFICIENT_RESOURCES, NDIS_STATUS_RESOURCES, 9);
        return NDIS_STATUS_RESOURCES;
    }

    if (pParameters->DefaultOffloadConfiguration)
        vpoxNetLwfWinUpdateSavedOffloadConfig(pModuleCtx, pParameters->DefaultOffloadConfiguration);
    else
    {
        NdisZeroMemory(pModuleCtx->pDisabledOffloadConfig, pModuleCtx->cbOffloadConfig);
        pModuleCtx->pDisabledOffloadConfig->Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
        pModuleCtx->pDisabledOffloadConfig->Header.Revision = NDIS_OFFLOAD_REVISION_1;
        pModuleCtx->pDisabledOffloadConfig->Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;
    }

    pModuleCtx->pGlobals  = pGlobals;
    pModuleCtx->hFilter   = hFilter;
    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Attaching);
    /* Initialize transmission mutex and events */
    NDIS_INIT_MUTEX(&pModuleCtx->InTransmit);
#ifdef VPOXNETLWF_SYNC_SEND
    KeInitializeEvent(&pModuleCtx->EventWire, SynchronizationEvent, FALSE);
    KeInitializeEvent(&pModuleCtx->EventHost, SynchronizationEvent, FALSE);
#else /* !VPOXNETLWF_SYNC_SEND */
    NdisInitializeEvent(&pModuleCtx->EventSendComplete);
    pModuleCtx->cPendingBuffers = 0;
#endif /* !VPOXNETLWF_SYNC_SEND */

#ifdef VPOXNETLWF_FIXED_SIZE_POOLS
    for (int i = 0; i < RT_ELEMENTS(g_cbPool); ++i)
    {
        /* Allocate buffer pools */
        NET_BUFFER_LIST_POOL_PARAMETERS PoolParams;
        NdisZeroMemory(&PoolParams, sizeof(PoolParams));
        PoolParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
        PoolParams.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
        PoolParams.Header.Size = sizeof(PoolParams);
        PoolParams.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
        PoolParams.fAllocateNetBuffer = TRUE;
        PoolParams.ContextSize = 0; /** @todo Do we need to consider underlying drivers? I think not. */
        PoolParams.PoolTag = VPOXNETLWF_MEM_TAG;
        PoolParams.DataSize = g_cbPool[i];
        pModuleCtx->hPool[i] = NdisAllocateNetBufferListPool(hFilter, &PoolParams);
        if (!pModuleCtx->hPool[i])
        {
            LogError(("vpoxNetLwfWinAttach: NdisAllocateNetBufferListPool failed\n"));
            vpoxNetLwfWinFreeModuleResources(pModuleCtx);
            vpoxNetLwfLogErrorEvent(IO_ERR_INSUFFICIENT_RESOURCES, NDIS_STATUS_RESOURCES, 7);
            return NDIS_STATUS_RESOURCES;
        }
        Log4(("vpoxNetLwfWinAttach: allocated NBL+NB pool (data size=%u) 0x%p\n",
              PoolParams.DataSize, pModuleCtx->hPool[i]));
    }
#else /* !VPOXNETLWF_FIXED_SIZE_POOLS */
    /* Allocate buffer pools */
    NET_BUFFER_LIST_POOL_PARAMETERS PoolParams;
    NdisZeroMemory(&PoolParams, sizeof(PoolParams));
    PoolParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    PoolParams.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    PoolParams.Header.Size = sizeof(PoolParams);
    PoolParams.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
    PoolParams.fAllocateNetBuffer = TRUE;
    PoolParams.ContextSize = 0; /** @todo Do we need to consider underlying drivers? I think not. */
    PoolParams.PoolTag = VPOXNETLWF_MEM_TAG;
    pModuleCtx->hPool = NdisAllocateNetBufferListPool(hFilter, &PoolParams);
    if (!pModuleCtx->hPool)
    {
        LogError(("vpoxNetLwfWinAttach: NdisAllocateNetBufferListPool failed\n"));
        vpoxNetLwfWinFreeModuleResources(pModuleCtx);
        vpoxNetLwfLogErrorEvent(IO_ERR_INSUFFICIENT_RESOURCES, NDIS_STATUS_RESOURCES, 7);
        return NDIS_STATUS_RESOURCES;
    }
    Log4(("vpoxNetLwfWinAttach: allocated NBL+NB pool 0x%p\n", pModuleCtx->hPool));
#endif /* !VPOXNETLWF_FIXED_SIZE_POOLS */

    NDIS_FILTER_ATTRIBUTES Attributes;
    NdisZeroMemory(&Attributes, sizeof(Attributes));
    Attributes.Header.Revision = NDIS_FILTER_ATTRIBUTES_REVISION_1;
    Attributes.Header.Size = sizeof(Attributes);
    Attributes.Header.Type = NDIS_OBJECT_TYPE_FILTER_ATTRIBUTES;
    Attributes.Flags = 0;
    NDIS_STATUS Status = NdisFSetAttributes(hFilter, pModuleCtx, &Attributes);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        LogError(("vpoxNetLwfWinAttach: NdisFSetAttributes failed with 0x%x\n", Status));
        vpoxNetLwfWinFreeModuleResources(pModuleCtx);
        vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, NDIS_STATUS_RESOURCES, 5);
        return NDIS_STATUS_RESOURCES;
    }
    /* Insert into module chain */
    NdisAcquireSpinLock(&pGlobals->Lock);
    RTListPrepend(&pGlobals->listModules, &pModuleCtx->node);
    NdisReleaseSpinLock(&pGlobals->Lock);

    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Paused);

    /// @todo Somehow the packet filter is 0 at this point: Status = vpoxNetLwfWinGetPacketFilter(pModuleCtx);
    /// @todo We actually update it later in status handler, perhaps we should not do anything here.

    LogFlow(("<==vpoxNetLwfWinAttach: Status = 0x%x\n", Status));
    return Status;
}

static VOID vpoxNetLwfWinDetach(IN NDIS_HANDLE hModuleCtx)
{
    LogFlow(("==>vpoxNetLwfWinDetach: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)hModuleCtx;
    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Detached, LwfState_Paused);

    /* Remove from module chain */
    NdisAcquireSpinLock(&pModuleCtx->pGlobals->Lock);
    RTListNodeRemove(&pModuleCtx->node);
    NdisReleaseSpinLock(&pModuleCtx->pGlobals->Lock);

    PVPOXNETFLTINS pNetFltIns = pModuleCtx->pNetFlt; /// @todo Atomic?
    if (pNetFltIns && vpoxNetFltTryRetainBusyNotDisconnected(pNetFltIns))
    {
        /*
         * Set hModuleCtx to null now in order to prevent filter restart,
         * OID requests and other stuff associated with NetFlt deactivation.
         */
        pNetFltIns->u.s.WinIf.hModuleCtx = NULL;
        /* Notify NetFlt that we are going down */
        pNetFltIns->pSwitchPort->pfnDisconnect(pNetFltIns->pSwitchPort, &pNetFltIns->MyPort, vpoxNetFltPortReleaseBusy);
        /* We do not 'release' netflt instance since it has been done by pfnDisconnect */
    }
    pModuleCtx->pNetFlt = NULL;

    /*
     * We have to make sure that all NET_BUFFER_LIST structures have been freed by now, but
     * it does not require us to do anything here since it has already been taken care of
     * by vpoxNetLwfWinPause().
     */
    vpoxNetLwfWinFreeModuleResources(pModuleCtx);
    Log4(("vpoxNetLwfWinDetach: freed module context 0x%p\n", pModuleCtx));
    LogFlow(("<==vpoxNetLwfWinDetach\n"));
}


static NDIS_STATUS vpoxNetLwfWinPause(IN NDIS_HANDLE hModuleCtx, IN PNDIS_FILTER_PAUSE_PARAMETERS pParameters)
{
    RT_NOREF1(pParameters);
    LogFlow(("==>vpoxNetLwfWinPause: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)hModuleCtx;
    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Pausing, LwfState_Running);
    /* Wait for pending send/indication operations to complete. */
    NDIS_WAIT_FOR_MUTEX(&pModuleCtx->InTransmit);
#ifndef VPOXNETLWF_SYNC_SEND
    NdisWaitEvent(&pModuleCtx->EventSendComplete, 1000 /* ms */);
#endif /* !VPOXNETLWF_SYNC_SEND */
    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Paused, LwfState_Pausing);
    NDIS_RELEASE_MUTEX(&pModuleCtx->InTransmit);
    LogFlow(("<==vpoxNetLwfWinPause\n"));
    return NDIS_STATUS_SUCCESS; /* Failure is not an option */
}


static void vpoxNetLwfWinIndicateOffload(PVPOXNETLWF_MODULE pModuleCtx, PNDIS_OFFLOAD pOffload)
{
    Log5(("vpoxNetLwfWinIndicateOffload: offload config changed to:\n"));
    vpoxNetLwfWinDumpOffloadSettings(pOffload);
    NDIS_STATUS_INDICATION OffloadingIndication;
    NdisZeroMemory(&OffloadingIndication, sizeof(OffloadingIndication));
    OffloadingIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    OffloadingIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
    OffloadingIndication.Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
    OffloadingIndication.SourceHandle = pModuleCtx->hFilter;
    OffloadingIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
    OffloadingIndication.StatusBuffer = pOffload;
    OffloadingIndication.StatusBufferSize = pOffload->Header.Size;
    NdisFIndicateStatus(pModuleCtx->hFilter, &OffloadingIndication);
}


static NDIS_STATUS vpoxNetLwfWinRestart(IN NDIS_HANDLE hModuleCtx, IN PNDIS_FILTER_RESTART_PARAMETERS pParameters)
{
    RT_NOREF1(pParameters);
    LogFlow(("==>vpoxNetLwfWinRestart: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)hModuleCtx;
    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Restarting, LwfState_Paused);

    /* By default the packets that go between VMs and wire are invisible to the host. */
    pModuleCtx->fPassVmTrafficToHost = false;

    NDIS_HANDLE hConfig;
    NDIS_CONFIGURATION_OBJECT cfgObj;
    cfgObj.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    cfgObj.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    cfgObj.Header.Size = sizeof(NDIS_CONFIGURATION_OBJECT);
    cfgObj.NdisHandle = g_VPoxNetLwfGlobals.hFilterDriver;

    NDIS_STATUS Status = NdisOpenConfigurationEx(&cfgObj, &hConfig);
    if (Status == NDIS_STATUS_SUCCESS)
    {
        NDIS_STRING strCfgParam = NDIS_STRING_CONST("PassVmTrafficToHost");
        PNDIS_CONFIGURATION_PARAMETER pParam = NULL;
        NdisReadConfiguration(&Status, &pParam, hConfig, &strCfgParam, NdisParameterInteger);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vpoxNetLwfWinRestart: Failed to read 'PassVmTrafficToHost' from the registry.\n"));
        }
        else if (pParam->ParameterData.IntegerData != 0)
        {
            Log(("vpoxNetLwfWinRestart: Allowing the host to see VM traffic in promisc mode by user request.\n"));
            pModuleCtx->fPassVmTrafficToHost = true;
        }
        NdisCloseConfiguration(hConfig);
    }
    vpoxNetLwfWinChangeState(pModuleCtx, LwfState_Running, LwfState_Restarting);
    LogFlow(("<==vpoxNetLwfWinRestart: Status = 0x%x, returning NDIS_STATUS_SUCCESS nontheless.\n", Status));
    return NDIS_STATUS_SUCCESS;
}


static void vpoxNetLwfWinDestroySG(PINTNETSG pSG)
{
    NdisFreeMemory(pSG, 0, 0);
    Log4(("vpoxNetLwfWinDestroySG: freed SG 0x%p\n", pSG));
}

/**
 * Worker for vpoxNetLwfWinNBtoSG() that gets the max segment count needed.
 * @note vpoxNetLwfWinNBtoSG may use fewer depending on cbPacket and offset!
 * @note vpoxNetAdpWinCalcSegments() is a copy of this code.
 */
DECLINLINE(ULONG) vpoxNetLwfWinCalcSegments(PNET_BUFFER pNetBuf)
{
    ULONG cSegs = 0;
    for (PMDL pMdl = NET_BUFFER_CURRENT_MDL(pNetBuf); pMdl; pMdl = NDIS_MDL_LINKAGE(pMdl))
    {
        /* Skip empty MDLs (see @bugref{9233}) */
        if (MmGetMdlByteCount(pMdl))
            cSegs++;
    }
    return cSegs;
}

DECLINLINE(void) vpoxNetLwfWinFreeMdlChain(PMDL pMdl)
{
#ifndef VPOXNETLWF_FIXED_SIZE_POOLS
    PMDL pMdlNext;
    while (pMdl)
    {
        pMdlNext = pMdl->Next;
# ifndef VPOXNETLWF_SYNC_SEND
        PUCHAR pDataBuf;
        ULONG cb = 0;
        NdisQueryMdl(pMdl, &pDataBuf, &cb, NormalPagePriority);
# endif /* !VPOXNETLWF_SYNC_SEND */
        NdisFreeMdl(pMdl);
        Log4(("vpoxNetLwfWinFreeMdlChain: freed MDL 0x%p\n", pMdl));
# ifndef VPOXNETLWF_SYNC_SEND
        NdisFreeMemory(pDataBuf, 0, 0);
        Log4(("vpoxNetLwfWinFreeMdlChain: freed data buffer 0x%p\n", pDataBuf));
# endif /* !VPOXNETLWF_SYNC_SEND */
        pMdl = pMdlNext;
    }
#else  /* VPOXNETLWF_FIXED_SIZE_POOLS */
    RT_NOREF1(pMdl);
#endif /* VPOXNETLWF_FIXED_SIZE_POOLS */
}

/** @todo
 * 1) Copy data from SG to MDL (if we decide to complete asynchronously).
 * 2) Provide context/backfill space. Nobody does it, should we?
 * 3) We always get a single segment from intnet. Simplify?
 */
static PNET_BUFFER_LIST vpoxNetLwfWinSGtoNB(PVPOXNETLWF_MODULE pModule, PINTNETSG pSG)
{
    AssertReturn(pSG->cSegsUsed >= 1, NULL);
    LogFlow(("==>vpoxNetLwfWinSGtoNB: segments=%d hPool=%p cb=%u\n", pSG->cSegsUsed,
             pModule->hPool, pSG->cbTotal));
    AssertReturn(pModule->hPool, NULL);

#ifdef VPOXNETLWF_SYNC_SEND
    PINTNETSEG pSeg = pSG->aSegs;
    PMDL pMdl = NdisAllocateMdl(pModule->hFilter, pSeg->pv, pSeg->cb);
    if (!pMdl)
    {
        LogError(("vpoxNetLwfWinSGtoNB: failed to allocate an MDL\n"));
        LogFlow(("<==vpoxNetLwfWinSGtoNB: return NULL\n"));
        return NULL;
    }
    Log4(("vpoxNetLwfWinSGtoNB: allocated Mdl 0x%p\n", pMdl));
    PMDL pMdlCurr = pMdl;
    for (int i = 1; i < pSG->cSegsUsed; i++)
    {
        pSeg = &pSG->aSegs[i];
        pMdlCurr->Next = NdisAllocateMdl(pModule->hFilter, pSeg->pv, pSeg->cb);
        if (!pMdlCurr->Next)
        {
            LogError(("vpoxNetLwfWinSGtoNB: failed to allocate an MDL\n"));
            /* Tear down all MDL we chained so far */
            vpoxNetLwfWinFreeMdlChain(pMdl);
            return NULL;
        }
        pMdlCurr = pMdlCurr->Next;
        Log4(("vpoxNetLwfWinSGtoNB: allocated Mdl 0x%p\n", pMdlCurr));
    }
    PNET_BUFFER_LIST pBufList = NdisAllocateNetBufferAndNetBufferList(pModule->hPool,
                                                                      0 /* ContextSize */,
                                                                      0 /* ContextBackFill */,
                                                                      pMdl,
                                                                      0 /* DataOffset */,
                                                                      pSG->cbTotal);
    if (pBufList)
    {
        Log4(("vpoxNetLwfWinSGtoNB: allocated NBL+NB 0x%p\n", pBufList));
        pBufList->SourceHandle = pModule->hFilter;
        /** @todo Do we need to initialize anything else? */
    }
    else
    {
        LogError(("vpoxNetLwfWinSGtoNB: failed to allocate an NBL+NB\n"));
        vpoxNetLwfWinFreeMdlChain(pMdl);
    }
#else /* !VPOXNETLWF_SYNC_SEND */

# ifdef VPOXNETLWF_FIXED_SIZE_POOLS
    int iPool = 0;
    ULONG cbFrame = VPOXNETLWF_MAX_FRAME_SIZE(pSG->cbTotal);
    /* Let's find the appropriate pool first */
    for (iPool = 0; iPool < RT_ELEMENTS(g_cbPool); ++iPool)
        if (cbFrame <= g_cbPool[iPool])
            break;
    if (iPool >= RT_ELEMENTS(g_cbPool))
    {
        LogError(("vpoxNetLwfWinSGtoNB: frame is too big (%u > %u), drop it.\n", cbFrame, g_cbPool[RT_ELEMENTS(g_cbPool)-1]));
        LogFlow(("<==vpoxNetLwfWinSGtoNB: return NULL\n"));
        return NULL;
    }
    PNET_BUFFER_LIST pBufList = NdisAllocateNetBufferList(pModule->hPool[iPool],
                                                          0 /** @todo ContextSize */,
                                                          0 /** @todo ContextBackFill */);
    if (!pBufList)
    {
        LogError(("vpoxNetLwfWinSGtoNB: failed to allocate netbuffer (cb=%u) from pool %d\n", cbFrame, iPool));
        LogFlow(("<==vpoxNetLwfWinSGtoNB: return NULL\n"));
        return NULL;
    }
    const ULONG cbAlignmentMask = sizeof(USHORT) - 1; /* Microsoft LB/FO provider expects packets to be aligned at word boundary. */
    ULONG cbAlignedFrame = (pSG->cbTotal + cbAlignmentMask) & ~cbAlignmentMask;
    Assert(cbAlignedFrame >= pSG->cbTotal);
    Assert(cbFrame >= cbAlignedFrame);
    NET_BUFFER *pBuffer = NET_BUFFER_LIST_FIRST_NB(pBufList);
    NDIS_STATUS Status = NdisRetreatNetBufferDataStart(pBuffer, cbAlignedFrame, 0 /** @todo DataBackfill */, NULL);
    if (cbAlignedFrame - pSG->cbTotal > 0)
    {
        /* Make sure padding zeros do not get to the wire. */
        if (NET_BUFFER_DATA_LENGTH(pBuffer) != cbAlignedFrame)
            vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, STATUS_SUCCESS, 11);
        else
            NET_BUFFER_DATA_LENGTH(pBuffer) = pSG->cbTotal;
    }
    if (Status == NDIS_STATUS_SUCCESS)
    {
        uint8_t *pDst = (uint8_t*)NdisGetDataBuffer(pBuffer, pSG->cbTotal, NULL, 1, 0);
        if (pDst)
        {
            for (int i = 0; i < pSG->cSegsUsed; i++)
            {
                NdisMoveMemory(pDst, pSG->aSegs[i].pv, pSG->aSegs[i].cb);
                pDst += pSG->aSegs[i].cb;
            }
            Log4(("vpoxNetLwfWinSGtoNB: allocated NBL+NB 0x%p\n", pBufList));
            pBufList->SourceHandle = pModule->hFilter;
        }
        else
        {
            LogError(("vpoxNetLwfWinSGtoNB: failed to obtain the buffer pointer (size=%u)\n", pSG->cbTotal));
            NdisAdvanceNetBufferDataStart(pBuffer, cbAlignedFrame, false, NULL); /** @todo why bother? */
            NdisFreeNetBufferList(pBufList);
            pBufList = NULL;
        }
    }
    else
    {
        LogError(("vpoxNetLwfWinSGtoNB: NdisRetreatNetBufferDataStart failed with 0x%x (size=%u)\n", Status, pSG->cbTotal));
        NdisFreeNetBufferList(pBufList);
        pBufList = NULL;
    }
# else /* !VPOXNETLWF_FIXED_SIZE_POOLS */
    PNET_BUFFER_LIST pBufList = NULL;
    ULONG cbMdl = VPOXNETLWF_MAX_FRAME_SIZE(pSG->cbTotal);
    ULONG uDataOffset = cbMdl - pSG->cbTotal;
    PUCHAR pDataBuf = (PUCHAR)NdisAllocateMemoryWithTagPriority(pModule->hFilter, cbMdl,
                                                                VPOXNETLWF_MEM_TAG, NormalPoolPriority);
    if (pDataBuf)
    {
        Log4(("vpoxNetLwfWinSGtoNB: allocated data buffer (cb=%u) 0x%p\n", cbMdl, pDataBuf));
        PMDL pMdl = NdisAllocateMdl(pModule->hFilter, pDataBuf, cbMdl);
        if (!pMdl)
        {
            NdisFreeMemory(pDataBuf, 0, 0);
            Log4(("vpoxNetLwfWinSGtoNB: freed data buffer 0x%p\n", pDataBuf));
            LogError(("vpoxNetLwfWinSGtoNB: failed to allocate an MDL (cb=%u)\n", cbMdl));
            LogFlow(("<==vpoxNetLwfWinSGtoNB: return NULL\n"));
            return NULL;
        }
        PUCHAR pDst = pDataBuf + uDataOffset;
        for (int i = 0; i < pSG->cSegsUsed; i++)
        {
            NdisMoveMemory(pDst, pSG->aSegs[i].pv, pSG->aSegs[i].cb);
            pDst += pSG->aSegs[i].cb;
        }
        pBufList = NdisAllocateNetBufferAndNetBufferList(pModule->hPool,
                                                         0 /* ContextSize */,
                                                         0 /* ContextBackFill */,
                                                         pMdl,
                                                         uDataOffset,
                                                         pSG->cbTotal);
        if (pBufList)
        {
            Log4(("vpoxNetLwfWinSGtoNB: allocated NBL+NB 0x%p\n", pBufList));
            pBufList->SourceHandle = pModule->hFilter;
            /** @todo Do we need to initialize anything else? */
        }
        else
        {
            LogError(("vpoxNetLwfWinSGtoNB: failed to allocate an NBL+NB\n"));
            vpoxNetLwfWinFreeMdlChain(pMdl);
        }
    }
    else
    {
        LogError(("vpoxNetLwfWinSGtoNB: failed to allocate data buffer (size=%u)\n", cbMdl));
    }
# endif /* !VPOXNETLWF_FIXED_SIZE_POOLS */

#endif /* !VPOXNETLWF_SYNC_SEND */
    LogFlow(("<==vpoxNetLwfWinSGtoNB: return %p\n", pBufList));
    return pBufList;
}

/**
 * @note vpoxNetAdpWinNBtoSG() is a copy of this code.
 */
static PINTNETSG vpoxNetLwfWinNBtoSG(PVPOXNETLWF_MODULE pModule, PNET_BUFFER pNetBuf)
{
    ULONG cbPacket = NET_BUFFER_DATA_LENGTH(pNetBuf);
    ULONG cSegs = vpoxNetLwfWinCalcSegments(pNetBuf);
    /* Allocate and initialize SG */
    PINTNETSG pSG = (PINTNETSG)NdisAllocateMemoryWithTagPriority(pModule->hFilter,
                                                                 RT_UOFFSETOF_DYN(INTNETSG, aSegs[cSegs]),
                                                                 VPOXNETLWF_MEM_TAG,
                                                                 NormalPoolPriority);
    AssertReturn(pSG, pSG);
    Log4(("vpoxNetLwfWinNBtoSG: allocated SG 0x%p\n", pSG));
    IntNetSgInitTempSegs(pSG, cbPacket /*cbTotal*/, cSegs, cSegs /*cSegsUsed*/);

    ULONG uOffset = NET_BUFFER_CURRENT_MDL_OFFSET(pNetBuf);
    cSegs = 0;
    for (PMDL pMdl = NET_BUFFER_CURRENT_MDL(pNetBuf);
         pMdl != NULL && cbPacket > 0;
         pMdl = NDIS_MDL_LINKAGE(pMdl))
    {
        ULONG cbSrc = MmGetMdlByteCount(pMdl);
        if (cbSrc == 0)
            continue; /* Skip empty MDLs (see @bugref{9233}) */

        PUCHAR pSrc = (PUCHAR)MmGetSystemAddressForMdlSafe(pMdl, LowPagePriority);
        if (!pSrc)
        {
            vpoxNetLwfWinDestroySG(pSG);
            return NULL;
        }

        /* Handle the offset in the current (which is the first for us) MDL */
        if (uOffset)
        {
            if (uOffset < cbSrc)
            {
                pSrc  += uOffset;
                cbSrc -= uOffset;
                uOffset = 0;
            }
            else
            {
                /* This is an invalid MDL chain */
                vpoxNetLwfWinDestroySG(pSG);
                return NULL;
            }
        }

        /* Do not read the last MDL beyond packet's end */
        if (cbSrc > cbPacket)
            cbSrc = cbPacket;

        Assert(cSegs < pSG->cSegsAlloc);
        pSG->aSegs[cSegs].pv = pSrc;
        pSG->aSegs[cSegs].cb = cbSrc;
        pSG->aSegs[cSegs].Phys = NIL_RTHCPHYS;
        cSegs++;
        cbPacket -= cbSrc;
    }

    Assert(cbPacket == 0);
    Assert(cSegs <= pSG->cSegsUsed);

    /* Update actual segment count in case we used fewer than anticipated. */
    pSG->cSegsUsed = (uint16_t)cSegs;

    return pSG;
}

VOID vpoxNetLwfWinStatus(IN NDIS_HANDLE hModuleCtx, IN PNDIS_STATUS_INDICATION pIndication)
{
    LogFlow(("==>vpoxNetLwfWinStatus: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)hModuleCtx;
    Log(("vpoxNetLwfWinStatus: Got status indication: %s\n", vpoxNetLwfWinStatusToText(pIndication->StatusCode)));
    switch (pIndication->StatusCode)
    {
        case NDIS_STATUS_PACKET_FILTER:
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pIndication->StatusBuffer);
            vpoxNetLwfWinOverridePacketFiltersUp(pModuleCtx, (ULONG*)pIndication->StatusBuffer);
            Log(("vpoxNetLwfWinStatus: Reporting status: %s\n", vpoxNetLwfWinStatusToText(pIndication->StatusCode)));
            vpoxNetLwfWinDumpFilterTypes(*(ULONG*)pIndication->StatusBuffer);
            break;
        case NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG:
            Log5(("vpoxNetLwfWinStatus: offloading currently set to:\n"));
            vpoxNetLwfWinDumpOffloadSettings((PNDIS_OFFLOAD)pIndication->StatusBuffer);
            vpoxNetLwfWinUpdateSavedOffloadConfig(pModuleCtx, (PNDIS_OFFLOAD)pIndication->StatusBuffer);
            if (ASMAtomicReadBool(&pModuleCtx->fActive))
                vpoxNetLwfWinDisableOffloading((PNDIS_OFFLOAD)pIndication->StatusBuffer);
            Log5(("vpoxNetLwfWinStatus: reporting offloading up as:\n"));
            vpoxNetLwfWinDumpOffloadSettings((PNDIS_OFFLOAD)pIndication->StatusBuffer);
            break;
    }
    NdisFIndicateStatus(pModuleCtx->hFilter, pIndication);
    LogFlow(("<==vpoxNetLwfWinStatus\n"));
}

static bool vpoxNetLwfWinForwardToIntNet(PVPOXNETLWF_MODULE pModuleCtx, PNET_BUFFER_LIST pBufLists, uint32_t fSrc)
{
    /* We must not forward anything to the trunk unless it is ready to receive. */
    if (!ASMAtomicReadBool(&pModuleCtx->fActive))
    {
        Log(("vpoxNetLwfWinForwardToIntNet: trunk is inactive, won't forward\n"));
        return false;
    }
    /* Some NPF protocols make NDIS to loop back packets at miniport level, we must ignore those. */
    if (NdisTestNblFlag(pBufLists, NDIS_NBL_FLAGS_IS_LOOPBACK_PACKET))
    {
        if (pBufLists->SourceHandle == pModuleCtx->hFilter && !pModuleCtx->fPassVmTrafficToHost)
        {
            /* Drop the packets we've injected. */
            vpoxNetLwfWinDumpPackets("vpoxNetLwfWinForwardToIntNet: dropping loopback", pBufLists);
            return true;
        }
        vpoxNetLwfWinDumpPackets("vpoxNetLwfWinForwardToIntNet: passing through loopback", pBufLists);
        return false;
    }

    AssertReturn(pModuleCtx->pNetFlt, false);
    AssertReturn(pModuleCtx->pNetFlt->pSwitchPort, false);
    AssertReturn(pModuleCtx->pNetFlt->pSwitchPort->pfnRecv, false);
    LogFlow(("==>vpoxNetLwfWinForwardToIntNet: module=%p\n", pModuleCtx));
    Assert(pBufLists);                                                   /* The chain must contain at least one list */
    Assert(NET_BUFFER_LIST_NEXT_NBL(pBufLists) == NULL); /* The caller is supposed to unlink the list from the chain */
    /*
     * Even if NBL contains more than one buffer we are prepared to deal with it.
     * When any of buffers should not be dropped we keep the whole list. It is
     * better to leak some "unexpected" packets to the wire/host than to loose any.
     */
    bool fDropIt   = false;
    bool fDontDrop = false;
    int nLists = 0;
    for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
    {
        int nBuffers = 0;
        nLists++;
        for (PNET_BUFFER pBuf = NET_BUFFER_LIST_FIRST_NB(pList); pBuf; pBuf = NET_BUFFER_NEXT_NB(pBuf))
        {
            nBuffers++;
            PINTNETSG pSG = vpoxNetLwfWinNBtoSG(pModuleCtx, pBuf);
            if (pSG)
            {
                vpoxNetLwfWinDumpPacket(pSG, (fSrc & INTNETTRUNKDIR_WIRE)?"intnet <-- wire":"intnet <-- host");
                /* A bit paranoid, but we do not use any locks, so... */
                if (ASMAtomicReadBool(&pModuleCtx->fActive))
                    if (pModuleCtx->pNetFlt->pSwitchPort->pfnRecv(pModuleCtx->pNetFlt->pSwitchPort, NULL, pSG, fSrc))
                        fDropIt = true;
                    else
                        fDontDrop = true;
                vpoxNetLwfWinDestroySG(pSG);
            }
        }
        Log(("vpoxNetLwfWinForwardToIntNet: list=%d buffers=%d\n", nLists, nBuffers));
    }
    Log(("vpoxNetLwfWinForwardToIntNet: lists=%d drop=%s don't=%s\n", nLists, fDropIt ? "true":"false", fDontDrop ? "true":"false"));

    /* If the host (and the user) wants to see all packets we must not drop any. */
    if (pModuleCtx->fPassVmTrafficToHost && vpoxNetLwfWinIsPromiscuous(pModuleCtx))
        fDropIt = false;

    LogFlow(("<==vpoxNetLwfWinForwardToIntNet: return '%s'\n",
             fDropIt ? (fDontDrop ? "do not drop (some)" : "drop it") : "do not drop (any)"));
    return fDropIt && !fDontDrop; /* Drop the list if ALL its buffers are being dropped! */
}

DECLINLINE(bool) vpoxNetLwfWinIsRunning(PVPOXNETLWF_MODULE pModule)
{
    Log(("vpoxNetLwfWinIsRunning: state=%d\n", ASMAtomicReadU32(&pModule->enmState)));
    return ASMAtomicReadU32(&pModule->enmState) == LwfState_Running;
}

VOID vpoxNetLwfWinSendNetBufferLists(IN NDIS_HANDLE hModuleCtx, IN PNET_BUFFER_LIST pBufLists, IN NDIS_PORT_NUMBER nPort, IN ULONG fFlags)
{
    LogFlow(("==>vpoxNetLwfWinSendNetBufferLists: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModule = (PVPOXNETLWF_MODULE)hModuleCtx;
    vpoxNetLwfWinDumpPackets("vpoxNetLwfWinSendNetBufferLists: got", pBufLists);

    if (!ASMAtomicReadBool(&pModule->fActive))
    {
        /*
         * The trunk is inactive, jusp pass along all packets to the next
         * underlying driver.
         */
        NdisFSendNetBufferLists(pModule->hFilter, pBufLists, nPort, fFlags);
        return;
    }

    if (vpoxNetLwfWinIsRunning(pModule))
    {
        PNET_BUFFER_LIST pNext     = NULL;
        PNET_BUFFER_LIST pDropHead = NULL;
        PNET_BUFFER_LIST pDropTail = NULL;
        PNET_BUFFER_LIST pPassHead = NULL;
        PNET_BUFFER_LIST pPassTail = NULL;
        for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = pNext)
        {
            pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
            NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink */
            if (vpoxNetLwfWinForwardToIntNet(pModule, pList, INTNETTRUNKDIR_HOST))
            {
                NET_BUFFER_LIST_STATUS(pList) = NDIS_STATUS_SUCCESS;
                if (pDropHead)
                {
                    NET_BUFFER_LIST_NEXT_NBL(pDropTail) = pList;
                    pDropTail = pList;
                }
                else
                    pDropHead = pDropTail = pList;
            }
            else
            {
                if (pPassHead)
                {
                    NET_BUFFER_LIST_NEXT_NBL(pPassTail) = pList;
                    pPassTail = pList;
                }
                else
                    pPassHead = pPassTail = pList;
            }
        }
        Assert((pBufLists == pPassHead) || (pBufLists == pDropHead));
        if (pPassHead)
        {
            vpoxNetLwfWinDumpPackets("vpoxNetLwfWinSendNetBufferLists: passing down", pPassHead);
            NdisFSendNetBufferLists(pModule->hFilter, pBufLists, nPort, fFlags);
        }
        if (pDropHead)
        {
            vpoxNetLwfWinDumpPackets("vpoxNetLwfWinSendNetBufferLists: consumed", pDropHead);
            NdisFSendNetBufferListsComplete(pModule->hFilter, pDropHead,
                                            fFlags & NDIS_SEND_FLAGS_DISPATCH_LEVEL ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
        }
    }
    else
    {
        for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
        {
            NET_BUFFER_LIST_STATUS(pList) = NDIS_STATUS_PAUSED;
        }
        vpoxNetLwfWinDumpPackets("vpoxNetLwfWinSendNetBufferLists: consumed", pBufLists);
        NdisFSendNetBufferListsComplete(pModule->hFilter, pBufLists,
                                        fFlags & NDIS_SEND_FLAGS_DISPATCH_LEVEL ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);

    }
    LogFlow(("<==vpoxNetLwfWinSendNetBufferLists\n"));
}

VOID vpoxNetLwfWinSendNetBufferListsComplete(IN NDIS_HANDLE hModuleCtx, IN PNET_BUFFER_LIST pBufLists, IN ULONG fFlags)
{
    LogFlow(("==>vpoxNetLwfWinSendNetBufferListsComplete: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModule = (PVPOXNETLWF_MODULE)hModuleCtx;
    PNET_BUFFER_LIST pList = pBufLists;
    PNET_BUFFER_LIST pNextList;
    PNET_BUFFER_LIST pPrevList = NULL;
    while (pList)
    {
        pNextList = NET_BUFFER_LIST_NEXT_NBL(pList);
        if (pList->SourceHandle == pModule->hFilter)
        {
            /* We allocated this NET_BUFFER_LIST, let's free it up */
            Assert(NET_BUFFER_LIST_FIRST_NB(pList));
            Assert(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /*
             * All our NBLs hold a single NB each, no need to iterate over a list.
             * There is no need to free an associated NB explicitly either, as it was
             * preallocated with NBL structure.
             */
            Assert(!NET_BUFFER_NEXT_NB(NET_BUFFER_LIST_FIRST_NB(pList)));
            vpoxNetLwfWinFreeMdlChain(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /* Unlink this list from the chain */
            if (pPrevList)
                NET_BUFFER_LIST_NEXT_NBL(pPrevList) = pNextList;
            else
                pBufLists = pNextList;
            Log(("vpoxNetLwfWinSendNetBufferListsComplete: our list %p, next=%p, previous=%p, head=%p\n", pList, pNextList, pPrevList, pBufLists));
            NdisFreeNetBufferList(pList);
#ifdef VPOXNETLWF_SYNC_SEND
            Log4(("vpoxNetLwfWinSendNetBufferListsComplete: freed NBL+NB 0x%p\n", pList));
            KeSetEvent(&pModule->EventWire, 0, FALSE);
#else /* !VPOXNETLWF_SYNC_SEND */
            Log4(("vpoxNetLwfWinSendNetBufferListsComplete: freed NBL+NB+MDL+Data 0x%p\n", pList));
            Assert(ASMAtomicReadS32(&pModule->cPendingBuffers) > 0);
            if (ASMAtomicDecS32(&pModule->cPendingBuffers) == 0)
                NdisSetEvent(&pModule->EventSendComplete);
#endif /* !VPOXNETLWF_SYNC_SEND */
        }
        else
        {
            pPrevList = pList;
            Log(("vpoxNetLwfWinSendNetBufferListsComplete: passing list %p, next=%p, previous=%p, head=%p\n", pList, pNextList, pPrevList, pBufLists));
        }
        pList = pNextList;
    }
    if (pBufLists)
    {
        /* There are still lists remaining in the chain, pass'em up */
        NdisFSendNetBufferListsComplete(pModule->hFilter, pBufLists, fFlags);
    }
    LogFlow(("<==vpoxNetLwfWinSendNetBufferListsComplete\n"));
}

VOID vpoxNetLwfWinReceiveNetBufferLists(IN NDIS_HANDLE hModuleCtx,
                                        IN PNET_BUFFER_LIST pBufLists,
                                        IN NDIS_PORT_NUMBER nPort,
                                        IN ULONG nBufLists,
                                        IN ULONG fFlags)
{
    /// @todo Do we need loopback handling?
    LogFlow(("==>vpoxNetLwfWinReceiveNetBufferLists: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModule = (PVPOXNETLWF_MODULE)hModuleCtx;
    vpoxNetLwfWinDumpPackets("vpoxNetLwfWinReceiveNetBufferLists: got", pBufLists);

    if (!ASMAtomicReadBool(&pModule->fActive))
    {
        /*
         * The trunk is inactive, just pass along all packets to the next
         * overlying driver.
         */
        NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pBufLists, nPort, nBufLists, fFlags);
        LogFlow(("<==vpoxNetLwfWinReceiveNetBufferLists: inactive trunk\n"));
        return;
    }

    if (vpoxNetLwfWinIsRunning(pModule))
    {
        if (NDIS_TEST_RECEIVE_CANNOT_PEND(fFlags))
        {
            for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
            {
                PNET_BUFFER_LIST pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
                NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink temporarily */
                if (!vpoxNetLwfWinForwardToIntNet(pModule, pList, INTNETTRUNKDIR_WIRE))
                {
                    vpoxNetLwfWinDumpPackets("vpoxNetLwfWinReceiveNetBufferLists: passing up", pList);
                    NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pList, nPort, nBufLists, fFlags);
                }
                NET_BUFFER_LIST_NEXT_NBL(pList) = pNext; /* Restore the link */
            }
        }
        else
        {
            /* We collect dropped NBLs in a separate list in order to "return" them. */
            PNET_BUFFER_LIST pNext     = NULL;
            PNET_BUFFER_LIST pDropHead = NULL;
            PNET_BUFFER_LIST pDropTail = NULL;
            PNET_BUFFER_LIST pPassHead = NULL;
            PNET_BUFFER_LIST pPassTail = NULL;
            ULONG nDrop = 0, nPass = 0;
            for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = pNext)
            {
                pNext = NET_BUFFER_LIST_NEXT_NBL(pList);
                NET_BUFFER_LIST_NEXT_NBL(pList) = NULL; /* Unlink */
                if (vpoxNetLwfWinForwardToIntNet(pModule, pList, INTNETTRUNKDIR_WIRE))
                {
                    if (nDrop++)
                    {
                        NET_BUFFER_LIST_NEXT_NBL(pDropTail) = pList;
                        pDropTail = pList;
                    }
                    else
                        pDropHead = pDropTail = pList;
                }
                else
                {
                    if (nPass++)
                    {
                        NET_BUFFER_LIST_NEXT_NBL(pPassTail) = pList;
                        pPassTail = pList;
                    }
                    else
                        pPassHead = pPassTail = pList;
                }
            }
            Assert((pBufLists == pPassHead) || (pBufLists == pDropHead));
            Assert(nDrop + nPass == nBufLists);
            if (pPassHead)
            {
                vpoxNetLwfWinDumpPackets("vpoxNetLwfWinReceiveNetBufferLists: passing up", pPassHead);
                NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pPassHead, nPort, nPass, fFlags);
            }
            if (pDropHead)
            {
                vpoxNetLwfWinDumpPackets("vpoxNetLwfWinReceiveNetBufferLists: consumed", pDropHead);
                NdisFReturnNetBufferLists(pModule->hFilter, pDropHead,
                                          fFlags & NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL ? NDIS_RETURN_FLAGS_DISPATCH_LEVEL : 0);
            }
        }

    }
    else
    {
        vpoxNetLwfWinDumpPackets("vpoxNetLwfWinReceiveNetBufferLists: consumed", pBufLists);
        if ((fFlags & NDIS_RECEIVE_FLAGS_RESOURCES) == 0)
            NdisFReturnNetBufferLists(pModule->hFilter, pBufLists,
                                      fFlags & NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL ? NDIS_RETURN_FLAGS_DISPATCH_LEVEL : 0);
    }
    LogFlow(("<==vpoxNetLwfWinReceiveNetBufferLists\n"));
}

VOID vpoxNetLwfWinReturnNetBufferLists(IN NDIS_HANDLE hModuleCtx, IN PNET_BUFFER_LIST pBufLists, IN ULONG fFlags)
{
    LogFlow(("==>vpoxNetLwfWinReturnNetBufferLists: module=%p\n", hModuleCtx));
    PVPOXNETLWF_MODULE pModule = (PVPOXNETLWF_MODULE)hModuleCtx;
    PNET_BUFFER_LIST pList = pBufLists;
    PNET_BUFFER_LIST pNextList;
    PNET_BUFFER_LIST pPrevList = NULL;
    /** @todo Move common part into a separate function to be used by vpoxNetLwfWinSendNetBufferListsComplete() as well */
    while (pList)
    {
        pNextList = NET_BUFFER_LIST_NEXT_NBL(pList);
        if (pList->SourceHandle == pModule->hFilter)
        {
            /* We allocated this NET_BUFFER_LIST, let's free it up */
            Assert(NET_BUFFER_LIST_FIRST_NB(pList));
            Assert(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /*
             * All our NBLs hold a single NB each, no need to iterate over a list.
             * There is no need to free an associated NB explicitly either, as it was
             * preallocated with NBL structure.
             */
            vpoxNetLwfWinFreeMdlChain(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
            /* Unlink this list from the chain */
            if (pPrevList)
                NET_BUFFER_LIST_NEXT_NBL(pPrevList) = pNextList;
            else
                pBufLists = pNextList;
            NdisFreeNetBufferList(pList);
#ifdef VPOXNETLWF_SYNC_SEND
            Log4(("vpoxNetLwfWinReturnNetBufferLists: freed NBL+NB 0x%p\n", pList));
            KeSetEvent(&pModule->EventHost, 0, FALSE);
#else /* !VPOXNETLWF_SYNC_SEND */
            Log4(("vpoxNetLwfWinReturnNetBufferLists: freed NBL+NB+MDL+Data 0x%p\n", pList));
            Assert(ASMAtomicReadS32(&pModule->cPendingBuffers) > 0);
            if (ASMAtomicDecS32(&pModule->cPendingBuffers) == 0)
                NdisSetEvent(&pModule->EventSendComplete);
#endif /* !VPOXNETLWF_SYNC_SEND */
        }
        else
            pPrevList = pList;
        pList = pNextList;
    }
    if (pBufLists)
    {
        /* There are still lists remaining in the chain, pass'em up */
        NdisFReturnNetBufferLists(pModule->hFilter, pBufLists, fFlags);
    }
    LogFlow(("<==vpoxNetLwfWinReturnNetBufferLists\n"));
}

/**
 * register the filter driver
 */
DECLHIDDEN(NDIS_STATUS) vpoxNetLwfWinRegister(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPathStr)
{
    RT_NOREF1(pRegistryPathStr);
    NDIS_FILTER_DRIVER_CHARACTERISTICS FChars;
    NDIS_STRING FriendlyName;
    NDIS_STRING UniqueName;
    NDIS_STRING ServiceName;

    NdisInitUnicodeString(&FriendlyName, VPOXNETLWF_NAME_FRIENDLY);
    NdisInitUnicodeString(&UniqueName, VPOXNETLWF_NAME_UNIQUE);
    NdisInitUnicodeString(&ServiceName, VPOXNETLWF_NAME_SERVICE);

    NdisZeroMemory(&FChars, sizeof (FChars));

    FChars.Header.Type = NDIS_OBJECT_TYPE_FILTER_DRIVER_CHARACTERISTICS;
    FChars.Header.Size = sizeof(NDIS_FILTER_DRIVER_CHARACTERISTICS);
    FChars.Header.Revision = NDIS_FILTER_CHARACTERISTICS_REVISION_1;

    FChars.MajorNdisVersion = VPOXNETLWF_VERSION_NDIS_MAJOR;
    FChars.MinorNdisVersion = VPOXNETLWF_VERSION_NDIS_MINOR;

    FChars.FriendlyName = FriendlyName;
    FChars.UniqueName = UniqueName;
    FChars.ServiceName = ServiceName;

    /* Mandatory functions */
    FChars.AttachHandler = vpoxNetLwfWinAttach;
    FChars.DetachHandler = vpoxNetLwfWinDetach;
    FChars.RestartHandler = vpoxNetLwfWinRestart;
    FChars.PauseHandler = vpoxNetLwfWinPause;

    /* Optional functions, non changeble at run-time */
    FChars.OidRequestHandler = vpoxNetLwfWinOidRequest;
    FChars.OidRequestCompleteHandler = vpoxNetLwfWinOidRequestComplete;
    //FChars.CancelOidRequestHandler = vpoxNetLwfWinCancelOidRequest;
    FChars.StatusHandler = vpoxNetLwfWinStatus;
    //FChars.NetPnPEventHandler = vpoxNetLwfWinPnPEvent;

    /* Datapath functions */
    FChars.SendNetBufferListsHandler = vpoxNetLwfWinSendNetBufferLists;
    FChars.SendNetBufferListsCompleteHandler = vpoxNetLwfWinSendNetBufferListsComplete;
    FChars.ReceiveNetBufferListsHandler = vpoxNetLwfWinReceiveNetBufferLists;
    FChars.ReturnNetBufferListsHandler = vpoxNetLwfWinReturnNetBufferLists;

    pDriverObject->DriverUnload = vpoxNetLwfWinUnloadDriver;

    NDIS_STATUS Status;
    g_VPoxNetLwfGlobals.hFilterDriver = NULL;
    Log(("vpoxNetLwfWinRegister: registering filter driver...\n"));
    Status = NdisFRegisterFilterDriver(pDriverObject,
                                       (NDIS_HANDLE)&g_VPoxNetLwfGlobals,
                                       &FChars,
                                       &g_VPoxNetLwfGlobals.hFilterDriver);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Log(("vpoxNetLwfWinRegister: successfully registered filter driver; registering device...\n"));
        Status = vpoxNetLwfWinDevCreate(&g_VPoxNetLwfGlobals);
        Assert(Status == STATUS_SUCCESS);
        Log(("vpoxNetLwfWinRegister: vpoxNetLwfWinDevCreate() returned 0x%x\n", Status));
    }
    else
    {
        LogError(("vpoxNetLwfWinRegister: failed to register filter driver, status=0x%x", Status));
    }
    return Status;
}

static int vpoxNetLwfWinStartInitIdcThread()
{
    int rc = VERR_INVALID_STATE;

    if (ASMAtomicCmpXchgU32(&g_VPoxNetLwfGlobals.enmIdcState, LwfIdcState_Connecting, LwfIdcState_Disconnected))
    {
        Log(("vpoxNetLwfWinStartInitIdcThread: IDC state change Diconnected -> Connecting\n"));

        NTSTATUS Status = PsCreateSystemThread(&g_VPoxNetLwfGlobals.hInitIdcThread,
                                               THREAD_ALL_ACCESS,
                                               NULL,
                                               NULL,
                                               NULL,
                                               vpoxNetLwfWinInitIdcWorker,
                                               &g_VPoxNetLwfGlobals);
        Log(("vpoxNetLwfWinStartInitIdcThread: create IDC initialization thread, status=0x%x\n", Status));
        if (Status != STATUS_SUCCESS)
        {
            LogError(("vpoxNetLwfWinStartInitIdcThread: IDC initialization failed (system thread creation, status=0x%x)\n", Status));
            /*
             * We failed to init IDC and there will be no second chance.
             */
            Log(("vpoxNetLwfWinStartInitIdcThread: IDC state change Connecting -> Diconnected\n"));
            ASMAtomicWriteU32(&g_VPoxNetLwfGlobals.enmIdcState, LwfIdcState_Disconnected);
        }
        rc = RTErrConvertFromNtStatus(Status);
    }
    return rc;
}

static void vpoxNetLwfWinStopInitIdcThread()
{
}


RT_C_DECLS_BEGIN

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);

RT_C_DECLS_END

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    int rc;

    /* the idc registration is initiated via IOCTL since our driver
     * can be loaded when the VPoxDrv is not in case we are a Ndis IM driver */
    rc = vpoxNetLwfWinInitBase();
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        NdisZeroMemory(&g_VPoxNetLwfGlobals, sizeof (g_VPoxNetLwfGlobals));
        RTListInit(&g_VPoxNetLwfGlobals.listModules);
        NdisAllocateSpinLock(&g_VPoxNetLwfGlobals.Lock);
        /*
         * We choose to ignore IDC initialization errors here because if we fail to load
         * our filter the upper protocols won't bind to the associated adapter, causing
         * network failure at the host. Better to have non-working filter than broken
         * networking on the host.
         */
        rc = vpoxNetLwfWinStartInitIdcThread();
        AssertRC(rc);

        Status = vpoxNetLwfWinRegister(pDriverObject, pRegistryPath);
        Assert(Status == STATUS_SUCCESS);
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Log(("NETLWF: started successfully\n"));
            return STATUS_SUCCESS;
        }
        NdisFreeSpinLock(&g_VPoxNetLwfGlobals.Lock);
        vpoxNetLwfWinFini();
    }
    else
    {
        Status = NDIS_STATUS_FAILURE;
    }

    return Status;
}


static VOID vpoxNetLwfWinUnloadDriver(IN PDRIVER_OBJECT pDriver)
{
    RT_NOREF1(pDriver);
    LogFlow(("==>vpoxNetLwfWinUnloadDriver: driver=%p\n", pDriver));
    vpoxNetLwfWinDevDestroy(&g_VPoxNetLwfGlobals);
    NdisFDeregisterFilterDriver(g_VPoxNetLwfGlobals.hFilterDriver);
    NdisFreeSpinLock(&g_VPoxNetLwfGlobals.Lock);
    LogFlow(("<==vpoxNetLwfWinUnloadDriver\n"));
    vpoxNetLwfWinFini();
}

static const char *vpoxNetLwfWinIdcStateToText(uint32_t enmState)
{
    switch (enmState)
    {
        case LwfIdcState_Disconnected: return "Disconnected";
        case LwfIdcState_Connecting: return "Connecting";
        case LwfIdcState_Connected: return "Connected";
        case LwfIdcState_Stopping: return "Stopping";
    }
    return "Unknown";
}

static VOID vpoxNetLwfWinInitIdcWorker(PVOID pvContext)
{
    int rc;
    PVPOXNETLWFGLOBALS pGlobals = (PVPOXNETLWFGLOBALS)pvContext;

    while (ASMAtomicReadU32(&pGlobals->enmIdcState) == LwfIdcState_Connecting)
    {
        rc = vpoxNetFltInitIdc(&g_VPoxNetFltGlobals);
        if (RT_SUCCESS(rc))
        {
            if (!ASMAtomicCmpXchgU32(&pGlobals->enmIdcState, LwfIdcState_Connected, LwfIdcState_Connecting))
            {
                /* The state has been changed (the only valid transition is to "Stopping"), undo init */
                rc = vpoxNetFltTryDeleteIdc(&g_VPoxNetFltGlobals);
                Log(("vpoxNetLwfWinInitIdcWorker: state change (Connecting -> %s) while initializing IDC, deleted IDC, rc=0x%x\n",
                     vpoxNetLwfWinIdcStateToText(ASMAtomicReadU32(&pGlobals->enmIdcState)), rc));
            }
            else
            {
                Log(("vpoxNetLwfWinInitIdcWorker: IDC state change Connecting -> Connected\n"));
            }
        }
        else
        {
            LARGE_INTEGER WaitIn100nsUnits;
            WaitIn100nsUnits.QuadPart = -(LONGLONG)10000000; /* 1 sec */
            KeDelayExecutionThread(KernelMode, FALSE /* non-alertable */, &WaitIn100nsUnits);
        }
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static int vpoxNetLwfWinTryFiniIdc()
{
    int rc = VINF_SUCCESS;
    NTSTATUS Status;
    PKTHREAD pThread = NULL;
    uint32_t enmPrevState = ASMAtomicXchgU32(&g_VPoxNetLwfGlobals.enmIdcState, LwfIdcState_Stopping);

    Log(("vpoxNetLwfWinTryFiniIdc: IDC state change %s -> Stopping\n", vpoxNetLwfWinIdcStateToText(enmPrevState)));

    switch (enmPrevState)
    {
        case LwfIdcState_Disconnected:
            /* Have not even attempted to connect -- nothing to do. */
            break;
        case LwfIdcState_Stopping:
            /* Impossible, but another thread is alreading doing FiniIdc, bail out */
            LogError(("vpoxNetLwfWinTryFiniIdc: called in 'Stopping' state\n"));
            rc = VERR_INVALID_STATE;
            break;
        case LwfIdcState_Connecting:
            /* the worker thread is running, let's wait for it to stop */
            Status = ObReferenceObjectByHandle(g_VPoxNetLwfGlobals.hInitIdcThread,
                                               THREAD_ALL_ACCESS, NULL, KernelMode,
                                               (PVOID*)&pThread, NULL);
            if (Status == STATUS_SUCCESS)
            {
                KeWaitForSingleObject(pThread, Executive, KernelMode, FALSE, NULL);
                ObDereferenceObject(pThread);
            }
            else
            {
                LogError(("vpoxNetLwfWinTryFiniIdc: ObReferenceObjectByHandle(%p) failed with 0x%x\n",
                     g_VPoxNetLwfGlobals.hInitIdcThread, Status));
            }
            rc = RTErrConvertFromNtStatus(Status);
            break;
        case LwfIdcState_Connected:
            /* the worker succeeded in IDC init and terminated */
            rc = vpoxNetFltTryDeleteIdc(&g_VPoxNetFltGlobals);
            Log(("vpoxNetLwfWinTryFiniIdc: deleted IDC, rc=0x%x\n", rc));
            break;
    }
    return rc;
}

static void vpoxNetLwfWinFiniBase()
{
    vpoxNetFltDeleteGlobals(&g_VPoxNetFltGlobals);

    /*
     * Undo the work done during start (in reverse order).
     */
    memset(&g_VPoxNetFltGlobals, 0, sizeof(g_VPoxNetFltGlobals));

    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));

    RTR0Term();
}

static int vpoxNetLwfWinInitBase()
{
    int rc = RTR0Init(0);
    if (!RT_SUCCESS(rc))
        return rc;

    memset(&g_VPoxNetFltGlobals, 0, sizeof(g_VPoxNetFltGlobals));
    rc = vpoxNetFltInitGlobals(&g_VPoxNetFltGlobals);
    if (!RT_SUCCESS(rc))
        RTR0Term();

    return rc;
}

static int vpoxNetLwfWinFini()
{
    int rc = vpoxNetLwfWinTryFiniIdc();
    if (RT_SUCCESS(rc))
    {
        vpoxNetLwfWinFiniBase();
    }
    return rc;
}


/*
 *
 * The OS specific interface definition
 *
 */


bool vpoxNetFltOsMaybeRediscovered(PVPOXNETFLTINS pThis)
{
    LogFlow(("==>vpoxNetFltOsMaybeRediscovered: instance=%p\n", pThis));
    LogFlow(("<==vpoxNetFltOsMaybeRediscovered: return %RTbool\n", !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost)));
    /* AttachToInterface true if disconnected */
    return !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
}

int vpoxNetFltPortOsXmit(PVPOXNETFLTINS pThis, void *pvIfData, PINTNETSG pSG, uint32_t fDst)
{
    RT_NOREF1(pvIfData);
    int rc = VINF_SUCCESS;

    PVPOXNETLWF_MODULE pModule = (PVPOXNETLWF_MODULE)pThis->u.s.WinIf.hModuleCtx;
    LogFlow(("==>vpoxNetFltPortOsXmit: instance=%p module=%p\n", pThis, pModule));
    if (!pModule)
    {
        LogFlow(("<==vpoxNetFltPortOsXmit: pModule is null, return %d\n", VERR_INTERNAL_ERROR));
        return VERR_INTERNAL_ERROR;
    }
    /* Prevent going into "paused" state until all transmissions have been completed. */
    NDIS_WAIT_FOR_MUTEX(&pModule->InTransmit);
    /* Ignore all sends if the stack is paused or being paused, etc... */
    if (!vpoxNetLwfWinIsRunning(pModule))
    {
        NDIS_RELEASE_MUTEX(&pModule->InTransmit);
        return VINF_SUCCESS;
    }

    vpoxNetLwfWinDumpPacket(pSG,   !(fDst & INTNETTRUNKDIR_WIRE) ? "intnet --> host"
                                 : !(fDst & INTNETTRUNKDIR_HOST) ? "intnet --> wire" : "intnet --> all");

    /*
     * There are two possible strategies to deal with incoming SGs:
     * 1) make a copy of data and complete asynchronously;
     * 2) complete synchronously using the original data buffers.
     * Before we consider implementing (1) it is quite interesting to see
     * how well (2) performs. So we block until our requests are complete.
     * Actually there is third possibility -- to use SG retain/release
     * callbacks, but those seem not be fully implemented yet.
     * Note that ansynchronous completion will require different implementation
     * of vpoxNetLwfWinPause(), not relying on InTransmit mutex.
     */
#ifdef VPOXNETLWF_SYNC_SEND
    PVOID aEvents[2]; /* To wire and to host */
    ULONG nEvents = 0;
    LARGE_INTEGER timeout;
    timeout.QuadPart = -(LONGLONG)10000000; /* 1 sec */
#endif /* VPOXNETLWF_SYNC_SEND */
    if (fDst & INTNETTRUNKDIR_WIRE)
    {
        PNET_BUFFER_LIST pBufList = vpoxNetLwfWinSGtoNB(pModule, pSG);
        if (pBufList)
        {
            vpoxNetLwfWinDumpPackets("vpoxNetFltPortOsXmit: sending down", pBufList);
#ifdef VPOXNETLWF_SYNC_SEND
            aEvents[nEvents++] = &pModule->EventWire;
#else /* !VPOXNETLWF_SYNC_SEND */
            if (ASMAtomicIncS32(&pModule->cPendingBuffers) == 1)
                NdisResetEvent(&pModule->EventSendComplete);
#endif /* !VPOXNETLWF_SYNC_SEND */
            NdisFSendNetBufferLists(pModule->hFilter, pBufList, NDIS_DEFAULT_PORT_NUMBER, 0); /** @todo sendFlags! */
        }
    }
    if (fDst & INTNETTRUNKDIR_HOST)
    {
        PNET_BUFFER_LIST pBufList = vpoxNetLwfWinSGtoNB(pModule, pSG);
        if (pBufList)
        {
            vpoxNetLwfWinDumpPackets("vpoxNetFltPortOsXmit: sending up", pBufList);
#ifdef VPOXNETLWF_SYNC_SEND
            aEvents[nEvents++] = &pModule->EventHost;
#else /* !VPOXNETLWF_SYNC_SEND */
            if (ASMAtomicIncS32(&pModule->cPendingBuffers) == 1)
                NdisResetEvent(&pModule->EventSendComplete);
#endif /* !VPOXNETLWF_SYNC_SEND */
            NdisFIndicateReceiveNetBufferLists(pModule->hFilter, pBufList, NDIS_DEFAULT_PORT_NUMBER, 1, 0);
        }
    }
#ifdef VPOXNETLWF_SYNC_SEND
    if (nEvents)
    {
        NTSTATUS Status = KeWaitForMultipleObjects(nEvents, aEvents, WaitAll, Executive, KernelMode, FALSE, &timeout, NULL);
        if (Status != STATUS_SUCCESS)
        {
            LogError(("vpoxNetFltPortOsXmit: KeWaitForMultipleObjects() failed with 0x%x\n", Status));
            if (Status == STATUS_TIMEOUT)
                rc = VERR_TIMEOUT;
            else
                rc = RTErrConvertFromNtStatus(Status);
        }
    }
#endif /* VPOXNETLWF_SYNC_SEND */
    NDIS_RELEASE_MUTEX(&pModule->InTransmit);

    LogFlow(("<==vpoxNetFltPortOsXmit: return %d\n", rc));
    return rc;
}


NDIS_IO_WORKITEM_FUNCTION vpoxNetLwfWinToggleOffloading;

VOID vpoxNetLwfWinToggleOffloading(PVOID WorkItemContext, NDIS_HANDLE NdisIoWorkItemHandle)
{
    /* WARNING! Call this with IRQL=Passive! */
    RT_NOREF1(NdisIoWorkItemHandle);
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)WorkItemContext;

    if (ASMAtomicReadBool(&pModuleCtx->fActive))
    {
        /* Disable offloading temporarily by indicating offload config change. */
        /** @todo Be sure to revise this when implementing offloading support! */
        vpoxNetLwfWinIndicateOffload(pModuleCtx, pModuleCtx->pDisabledOffloadConfig);
        Log(("vpoxNetLwfWinToggleOffloading: set offloading off\n"));
    }
    else
    {
        /* The filter is inactive -- restore offloading configuration. */
        if (pModuleCtx->fOffloadConfigValid)
        {
            vpoxNetLwfWinIndicateOffload(pModuleCtx, pModuleCtx->pSavedOffloadConfig);
            Log(("vpoxNetLwfWinToggleOffloading: restored offloading config\n"));
        }
        else
            DbgPrint("VPoxNetLwf: no saved offload config to restore for %s\n", pModuleCtx->szMiniportName);
    }
}


void vpoxNetFltPortOsSetActive(PVPOXNETFLTINS pThis, bool fActive)
{
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)pThis->u.s.WinIf.hModuleCtx;
    LogFlow(("==>vpoxNetFltPortOsSetActive: instance=%p module=%p fActive=%RTbool\n", pThis, pModuleCtx, fActive));
    if (!pModuleCtx)
    {
        LogFlow(("<==vpoxNetFltPortOsSetActive: pModuleCtx is null\n"));
        return;
    }

    NDIS_STATUS Status = STATUS_SUCCESS;
    bool fOldActive = ASMAtomicXchgBool(&pModuleCtx->fActive, fActive);
    if (fOldActive != fActive)
    {
        NdisQueueIoWorkItem(pModuleCtx->hWorkItem, vpoxNetLwfWinToggleOffloading, pModuleCtx);
        Status = vpoxNetLwfWinSetPacketFilter(pModuleCtx, fActive);
        LogFlow(("<==vpoxNetFltPortOsSetActive: vpoxNetLwfWinSetPacketFilter() returned 0x%x\n", Status));
    }
    else
        LogFlow(("<==vpoxNetFltPortOsSetActive: no change, remain %sactive\n", fActive ? "":"in"));
}

int vpoxNetFltOsDisconnectIt(PVPOXNETFLTINS pThis)
{
    RT_NOREF1(pThis);
    LogFlow(("==>vpoxNetFltOsDisconnectIt: instance=%p\n", pThis));
    LogFlow(("<==vpoxNetFltOsDisconnectIt: return 0\n"));
    return VINF_SUCCESS;
}

int vpoxNetFltOsConnectIt(PVPOXNETFLTINS pThis)
{
    RT_NOREF1(pThis);
    LogFlow(("==>vpoxNetFltOsConnectIt: instance=%p\n", pThis));
    LogFlow(("<==vpoxNetFltOsConnectIt: return 0\n"));
    return VINF_SUCCESS;
}

/*
 * Uncommenting the following line produces debug log messages on IP address changes,
 * including wired interfaces. No actual calls to a switch port are made. This is for
 * debug purposes only!
 * #define VPOXNETLWFWIN_DEBUGIPADDRNOTIF 1
 */
static void __stdcall vpoxNetLwfWinIpAddrChangeCallback(IN PVOID pvCtx,
                                                        IN PMIB_UNICASTIPADDRESS_ROW pRow,
                                                        IN MIB_NOTIFICATION_TYPE enmNotifType)
{
    PVPOXNETFLTINS pThis = (PVPOXNETFLTINS)pvCtx;

    /* We are only interested in add or remove notifications. */
    bool fAdded;
    if (enmNotifType == MibAddInstance)
        fAdded = true;
    else if (enmNotifType == MibDeleteInstance)
        fAdded = false;
    else
        return;

    if (   pRow
#ifndef VPOXNETLWFWIN_DEBUGIPADDRNOTIF
        && pThis->pSwitchPort->pfnNotifyHostAddress
#endif /* !VPOXNETLWFWIN_DEBUGIPADDRNOTIF */
       )
    {
        switch (pRow->Address.si_family)
        {
            case AF_INET:
                if (   IN4_IS_ADDR_LINKLOCAL(&pRow->Address.Ipv4.sin_addr)
                    || pRow->Address.Ipv4.sin_addr.s_addr == IN4ADDR_LOOPBACK)
                {
                    Log(("vpoxNetLwfWinIpAddrChangeCallback: ignoring %s address (%RTnaipv4)\n",
                         pRow->Address.Ipv4.sin_addr.s_addr == IN4ADDR_LOOPBACK ? "loopback" : "link-local",
                         pRow->Address.Ipv4.sin_addr));
                    break;
                }
                Log(("vpoxNetLwfWinIpAddrChangeCallback: %s IPv4 addr=%RTnaipv4 on luid=(%u,%u)\n",
                     fAdded ? "add" : "remove", pRow->Address.Ipv4.sin_addr,
                     pRow->InterfaceLuid.Info.IfType, pRow->InterfaceLuid.Info.NetLuidIndex));
#ifndef VPOXNETLWFWIN_DEBUGIPADDRNOTIF
                pThis->pSwitchPort->pfnNotifyHostAddress(pThis->pSwitchPort, fAdded, kIntNetAddrType_IPv4,
                                                         &pRow->Address.Ipv4.sin_addr);
#endif /* !VPOXNETLWFWIN_DEBUGIPADDRNOTIF */
                break;
            case AF_INET6:
                if (Ipv6AddressScope(pRow->Address.Ipv6.sin6_addr.u.Byte) <= ScopeLevelLink)
                {
                    Log(("vpoxNetLwfWinIpAddrChangeCallback: ignoring link-local address (%RTnaipv6)\n",
                         &pRow->Address.Ipv6.sin6_addr));
                    break;
                }
                Log(("vpoxNetLwfWinIpAddrChangeCallback: %s IPv6 addr=%RTnaipv6 scope=%d luid=(%u,%u)\n",
                     fAdded ? "add" : "remove", &pRow->Address.Ipv6.sin6_addr,
                     Ipv6AddressScope(pRow->Address.Ipv6.sin6_addr.u.Byte),
                     pRow->InterfaceLuid.Info.IfType, pRow->InterfaceLuid.Info.NetLuidIndex));
#ifndef VPOXNETLWFWIN_DEBUGIPADDRNOTIF
                pThis->pSwitchPort->pfnNotifyHostAddress(pThis->pSwitchPort, fAdded, kIntNetAddrType_IPv6,
                                                         &pRow->Address.Ipv6.sin6_addr);
#endif /* !VPOXNETLWFWIN_DEBUGIPADDRNOTIF */
                break;
        }
    }
    else
        Log(("vpoxNetLwfWinIpAddrChangeCallback: pRow=%p pfnNotifyHostAddress=%p\n",
             pRow, pThis->pSwitchPort->pfnNotifyHostAddress));
}

void vpoxNetLwfWinRegisterIpAddrNotifier(PVPOXNETFLTINS pThis)
{
    LogFlow(("==>vpoxNetLwfWinRegisterIpAddrNotifier: instance=%p\n", pThis));
    if (   pThis->pSwitchPort
#ifndef VPOXNETLWFWIN_DEBUGIPADDRNOTIF
        && pThis->pSwitchPort->pfnNotifyHostAddress
#endif /* !VPOXNETLWFWIN_DEBUGIPADDRNOTIF */
       )
    {
        NETIO_STATUS Status;
        /* First we need to go over all host IP addresses and add them via pfnNotifyHostAddress. */
        PMIB_UNICASTIPADDRESS_TABLE HostIpAddresses = NULL;
        Status = GetUnicastIpAddressTable(AF_UNSPEC, &HostIpAddresses);
        if (NETIO_SUCCESS(Status))
        {
            for (unsigned i = 0; i < HostIpAddresses->NumEntries; i++)
                vpoxNetLwfWinIpAddrChangeCallback(pThis, &HostIpAddresses->Table[i], MibAddInstance);
        }
        else
            LogError(("vpoxNetLwfWinRegisterIpAddrNotifier: GetUnicastIpAddressTable failed with %x\n", Status));
        /* Now we can register a callback function to keep track of address changes. */
        Status = NotifyUnicastIpAddressChange(AF_UNSPEC, vpoxNetLwfWinIpAddrChangeCallback,
                                              pThis, false, &pThis->u.s.WinIf.hNotifier);
        if (NETIO_SUCCESS(Status))
            Log(("vpoxNetLwfWinRegisterIpAddrNotifier: notifier=%p\n", pThis->u.s.WinIf.hNotifier));
        else
            LogError(("vpoxNetLwfWinRegisterIpAddrNotifier: NotifyUnicastIpAddressChange failed with %x\n", Status));
    }
    else
        pThis->u.s.WinIf.hNotifier = NULL;
    LogFlow(("<==vpoxNetLwfWinRegisterIpAddrNotifier\n"));
}

void vpoxNetLwfWinUnregisterIpAddrNotifier(PVPOXNETFLTINS pThis)
{
    Log(("vpoxNetLwfWinUnregisterIpAddrNotifier: notifier=%p\n", pThis->u.s.WinIf.hNotifier));
    if (pThis->u.s.WinIf.hNotifier)
        CancelMibChangeNotify2(pThis->u.s.WinIf.hNotifier);
}

void vpoxNetFltOsDeleteInstance(PVPOXNETFLTINS pThis)
{
    PVPOXNETLWF_MODULE pModuleCtx = (PVPOXNETLWF_MODULE)pThis->u.s.WinIf.hModuleCtx;
    LogFlow(("==>vpoxNetFltOsDeleteInstance: instance=%p module=%p\n", pThis, pModuleCtx));
    /* Cancel IP address change notifications */
    vpoxNetLwfWinUnregisterIpAddrNotifier(pThis);
    /* Technically it is possible that the module has already been gone by now. */
    if (pModuleCtx)
    {
        Assert(!pModuleCtx->fActive); /* Deactivation ensures bypass mode */
        pModuleCtx->pNetFlt = NULL;
        pThis->u.s.WinIf.hModuleCtx = NULL;
    }
    LogFlow(("<==vpoxNetFltOsDeleteInstance\n"));
}

static void vpoxNetLwfWinReportCapabilities(PVPOXNETFLTINS pThis, PVPOXNETLWF_MODULE pModuleCtx)
{
    if (pThis->pSwitchPort
        && vpoxNetFltTryRetainBusyNotDisconnected(pThis))
    {
        pThis->pSwitchPort->pfnReportMacAddress(pThis->pSwitchPort, &pModuleCtx->MacAddr);
        pThis->pSwitchPort->pfnReportPromiscuousMode(pThis->pSwitchPort,
                                                     vpoxNetLwfWinIsPromiscuous(pModuleCtx));
        pThis->pSwitchPort->pfnReportGsoCapabilities(pThis->pSwitchPort, 0,
                                                     INTNETTRUNKDIR_WIRE | INTNETTRUNKDIR_HOST);
        pThis->pSwitchPort->pfnReportNoPreemptDsts(pThis->pSwitchPort, 0 /* none */);
        vpoxNetFltRelease(pThis, true /*fBusy*/);
    }
}

int vpoxNetFltOsInitInstance(PVPOXNETFLTINS pThis, void *pvContext)
{
    RT_NOREF1(pvContext);
    LogFlow(("==>vpoxNetFltOsInitInstance: instance=%p context=%p\n", pThis, pvContext));
    AssertReturn(pThis, VERR_INVALID_PARAMETER);
    Log(("vpoxNetFltOsInitInstance: trunk name=%s\n", pThis->szName));
    NdisAcquireSpinLock(&g_VPoxNetLwfGlobals.Lock);
    PVPOXNETLWF_MODULE pModuleCtx;
    RTListForEach(&g_VPoxNetLwfGlobals.listModules, pModuleCtx, VPOXNETLWF_MODULE, node)
    {
        DbgPrint("vpoxNetFltOsInitInstance: evaluating module, name=%s\n", pModuleCtx->szMiniportName);
        if (!RTStrICmp(pThis->szName, pModuleCtx->szMiniportName))
        {
            NdisReleaseSpinLock(&g_VPoxNetLwfGlobals.Lock);
            Log(("vpoxNetFltOsInitInstance: found matching module, name=%s\n", pThis->szName));
            pThis->u.s.WinIf.hModuleCtx = pModuleCtx;
            pModuleCtx->pNetFlt = pThis;
            vpoxNetLwfWinReportCapabilities(pThis, pModuleCtx);
            vpoxNetLwfWinRegisterIpAddrNotifier(pThis);
            LogFlow(("<==vpoxNetFltOsInitInstance: return 0\n"));
            return VINF_SUCCESS;
        }
    }
    NdisReleaseSpinLock(&g_VPoxNetLwfGlobals.Lock);
    // Internal network code will try to reconnect periodically, we should not spam in event log
    //vpoxNetLwfLogErrorEvent(IO_ERR_INTERNAL_ERROR, STATUS_SUCCESS, 6);
    LogFlow(("<==vpoxNetFltOsInitInstance: return VERR_INTNET_FLT_IF_NOT_FOUND\n"));
    return VERR_INTNET_FLT_IF_NOT_FOUND;
}

int vpoxNetFltOsPreInitInstance(PVPOXNETFLTINS pThis)
{
    LogFlow(("==>vpoxNetFltOsPreInitInstance: instance=%p\n", pThis));
    pThis->u.s.WinIf.hModuleCtx = 0;
    pThis->u.s.WinIf.hNotifier  = NULL;
    LogFlow(("<==vpoxNetFltOsPreInitInstance: return 0\n"));
    return VINF_SUCCESS;
}

void vpoxNetFltPortOsNotifyMacAddress(PVPOXNETFLTINS pThis, void *pvIfData, PCRTMAC pMac)
{
    RT_NOREF3(pThis, pvIfData, pMac);
    LogFlow(("==>vpoxNetFltPortOsNotifyMacAddress: instance=%p data=%p mac=%RTmac\n", pThis, pvIfData, pMac));
    LogFlow(("<==vpoxNetFltPortOsNotifyMacAddress\n"));
}

int vpoxNetFltPortOsConnectInterface(PVPOXNETFLTINS pThis, void *pvIf, void **ppvIfData)
{
    RT_NOREF3(pThis, pvIf, ppvIfData);
    LogFlow(("==>vpoxNetFltPortOsConnectInterface: instance=%p if=%p data=%p\n", pThis, pvIf, ppvIfData));
    LogFlow(("<==vpoxNetFltPortOsConnectInterface: return 0\n"));
    /* Nothing to do */
    return VINF_SUCCESS;
}

int vpoxNetFltPortOsDisconnectInterface(PVPOXNETFLTINS pThis, void *pvIfData)
{
    RT_NOREF2(pThis, pvIfData);
    LogFlow(("==>vpoxNetFltPortOsDisconnectInterface: instance=%p data=%p\n", pThis, pvIfData));
    LogFlow(("<==vpoxNetFltPortOsDisconnectInterface: return 0\n"));
    /* Nothing to do */
    return VINF_SUCCESS;
}

