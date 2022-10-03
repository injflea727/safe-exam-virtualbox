/* $Id: VPoxNetAdp-win.cpp $ */
/** @file
 * VPoxNetAdp-win.cpp - NDIS6 Host-only Networking Driver, Windows-specific code.
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

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV

#include <VPox/log.h>
#include <VPox/version.h>
#include <VPox/err.h>
#include <VPox/sup.h>
#include <VPox/intnet.h>
#include <VPox/intnetinline.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/list.h>
#include <iprt/net.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include <iprt/nt/ntddk.h>
#include <iprt/nt/ndis.h>

#include "VPoxNetAdp-win.h"
#include "VPox/VPoxNetCmn-win.h"

#define VPOXNETADP_MEM_TAG                   'OHBV'

/*
 * By default the link speed reported to be 1Gbps. We may wish to lower
 * it to 100Mbps to work around issues with multi-cast traffic on the host.
 * See @bugref{6379}.
 */
#define VPOXNETADPWIN_LINK_SPEED 1000000000ULL

#define LogError LogRel

/* Forward declarations */
MINIPORT_INITIALIZE                vpoxNetAdpWinInitializeEx;
MINIPORT_HALT                      vpoxNetAdpWinHaltEx;
MINIPORT_UNLOAD                    vpoxNetAdpWinUnload;
MINIPORT_PAUSE                     vpoxNetAdpWinPause;
MINIPORT_RESTART                   vpoxNetAdpWinRestart;
MINIPORT_OID_REQUEST               vpoxNetAdpWinOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS     vpoxNetAdpWinSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS   vpoxNetAdpWinReturnNetBufferLists;
MINIPORT_CANCEL_SEND               vpoxNetAdpWinCancelSend;
MINIPORT_CHECK_FOR_HANG            vpoxNetAdpWinCheckForHangEx;
MINIPORT_RESET                     vpoxNetAdpWinResetEx;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY   vpoxNetAdpWinDevicePnPEventNotify;
MINIPORT_SHUTDOWN                  vpoxNetAdpWinShutdownEx;
MINIPORT_CANCEL_OID_REQUEST        vpoxNetAdpWinCancelOidRequest;


/* Packet types by destination address; used in statistics. */
typedef enum {
    kVPoxNetAdpWinPacketType_Unicast,
    kVPoxNetAdpWinPacketType_Multicast,
    kVPoxNetAdpWinPacketType_Broadcast,
    kVPoxNetAdpWinPacketType_ArraySize /* Must be the last one */
} VPOXNETADPWIN_PACKET_TYPE;


/* Miniport states as defined by NDIS. */
typedef enum {
    kVPoxNetAdpWinState_Initializing,
    kVPoxNetAdpWinState_Paused,
    kVPoxNetAdpWinState_Restarting,
    kVPoxNetAdpWinState_Running,
    kVPoxNetAdpWinState_Pausing,
    kVPoxNetAdpWinState_32BitHack = 0x7fffffff
} VPOXNETADPWIN_ADAPTER_STATE;


/*
 * Valid state transitions are:
 * 1) Disconnected -> Connecting   : start the worker thread, attempting to init IDC;
 * 2) Connecting   -> Disconnected : failed to start IDC init worker thread;
 * 3) Connecting   -> Connected    : IDC init successful, terminate the worker;
 * 4) Connecting   -> Stopping     : IDC init incomplete, but the driver is being unloaded, terminate the worker;
 * 5) Connected    -> Stopping     : IDC init was successful, no worker, the driver is being unloaded;
 *
 * Driver terminates in either in Disconnected or in Stopping state.
 */
typedef enum {
    kVPoxNetAdpWinIdcState_Disconnected = 0, /* Initial state */
    kVPoxNetAdpWinIdcState_Connecting,       /* Attemping to init IDC, worker thread running */
    kVPoxNetAdpWinIdcState_Connected,        /* Successfully connected to IDC, worker thread terminated */
    kVPoxNetAdpWinIdcState_Stopping          /* Terminating the worker thread and disconnecting IDC */
} VPOXNETADPWIN_IDC_STATE;

typedef struct _VPOXNETADPGLOBALS
{
    /** Miniport driver handle. */
    NDIS_HANDLE hMiniportDriver;
    /** Power management capabilities, shared by all instances, do not change after init. */
    NDIS_PNP_CAPABILITIES PMCaps;
    /** The INTNET trunk network interface factory. */
    INTNETTRUNKFACTORY TrunkFactory;
    /** The SUPDRV component factory registration. */
    SUPDRVFACTORY SupDrvFactory;
    /** The SUPDRV IDC handle (opaque struct). */
    SUPDRVIDCHANDLE SupDrvIDC;
    /** IDC init thread handle. */
    HANDLE hInitIdcThread;
    /** Lock protecting the following members. */
    NDIS_SPIN_LOCK Lock;
    /** Lock-protected: the head of module list. */
    RTLISTANCHOR ListOfAdapters;
    /** Lock-protected: The number of current factory references. */
    int32_t volatile cFactoryRefs;
    /** Lock-protected: IDC initialization state. */
    volatile uint32_t enmIdcState;
    /** Lock-protected: event signaled when trunk factory is not in use. */
    NDIS_EVENT EventUnloadAllowed;
} VPOXNETADPGLOBALS, *PVPOXNETADPGLOBALS;

/* win-specific global data */
VPOXNETADPGLOBALS g_VPoxNetAdpGlobals;


typedef struct _VPOXNETADP_ADAPTER {
    /** Auxiliary member to link adapters into a list. */
    RTLISTNODE node;
    /** Adapter handle for NDIS. */
    NDIS_HANDLE hAdapter;
    /** Memory pool network buffers are allocated from. */
    NDIS_HANDLE hPool;
    /** Our RJ-45 port.
     * This is what the internal network plugs into. */
    INTNETTRUNKIFPORT MyPort;
    /** The RJ-45 port on the INTNET "switch".
     * This is what we're connected to. */
    PINTNETTRUNKSWPORT pSwitchPort;
    /** Pointer to global data */
    PVPOXNETADPGLOBALS pGlobals;
    /** Adapter state in NDIS, used for assertions only */
    VPOXNETADPWIN_ADAPTER_STATE volatile enmAdapterState; /// @todo do we need it really?
    /** The trunk state. */
    INTNETTRUNKIFSTATE volatile enmTrunkState;
    /** Number of pending operations, when it reaches zero we signal EventIdle. */
    int32_t volatile cBusy;
    /** The event that is signaled when we go idle and that pfnWaitForIdle blocks on. */
    NDIS_EVENT EventIdle;
    /** MAC address of adapter. */
    RTMAC MacAddr;
    /** Statistics: bytes received from internal network. */
    uint64_t au64StatsInOctets[kVPoxNetAdpWinPacketType_ArraySize];
    /** Statistics: packets received from internal network. */
    uint64_t au64StatsInPackets[kVPoxNetAdpWinPacketType_ArraySize];
    /** Statistics: bytes sent to internal network. */
    uint64_t au64StatsOutOctets[kVPoxNetAdpWinPacketType_ArraySize];
    /** Statistics: packets sent to internal network. */
    uint64_t au64StatsOutPackets[kVPoxNetAdpWinPacketType_ArraySize];
    /** Adapter friendly name. */
    char szName[1];
} VPOXNETADP_ADAPTER;
typedef VPOXNETADP_ADAPTER *PVPOXNETADP_ADAPTER;


/* Port */

#define IFPORT_2_VPOXNETADP_ADAPTER(pIfPort) \
    ( (PVPOXNETADP_ADAPTER)((uint8_t *)(pIfPort) - RT_UOFFSETOF(VPOXNETADP_ADAPTER, MyPort)) )

DECLINLINE(VPOXNETADPWIN_ADAPTER_STATE) vpoxNetAdpWinGetState(PVPOXNETADP_ADAPTER pThis)
{
    return (VPOXNETADPWIN_ADAPTER_STATE)ASMAtomicUoReadU32((uint32_t volatile *)&pThis->enmAdapterState);
}

DECLINLINE(VPOXNETADPWIN_ADAPTER_STATE) vpoxNetAdpWinSetState(PVPOXNETADP_ADAPTER pThis, VPOXNETADPWIN_ADAPTER_STATE enmNewState)
{
    return (VPOXNETADPWIN_ADAPTER_STATE)ASMAtomicXchgU32((uint32_t volatile *)&pThis->enmAdapterState, enmNewState);
}

DECLINLINE(bool) vpoxNetAdpWinSetState(PVPOXNETADP_ADAPTER pThis, VPOXNETADPWIN_ADAPTER_STATE enmNewState,
                                                              VPOXNETADPWIN_ADAPTER_STATE enmOldState)
{
    return ASMAtomicCmpXchgU32((uint32_t volatile *)&pThis->enmAdapterState, enmNewState, enmOldState);
}

#ifdef DEBUG

DECLHIDDEN(void) vpoxNetAdpWinDumpPackets(const char *pszMsg, PNET_BUFFER_LIST pBufLists)
{
    for (PNET_BUFFER_LIST pList = pBufLists; pList; pList = NET_BUFFER_LIST_NEXT_NBL(pList))
    {
        for (PNET_BUFFER pBuf = NET_BUFFER_LIST_FIRST_NB(pList); pBuf; pBuf = NET_BUFFER_NEXT_NB(pBuf))
        {
            Log6(("%s packet: cb=%d offset=%d", pszMsg, NET_BUFFER_DATA_LENGTH(pBuf), NET_BUFFER_DATA_OFFSET(pBuf)));
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

DECLINLINE(const char *) vpoxNetAdpWinEthTypeStr(uint16_t uType)
{
    switch (uType)
    {
        case RTNET_ETHERTYPE_IPV4: return "IP";
        case RTNET_ETHERTYPE_IPV6: return "IPv6";
        case RTNET_ETHERTYPE_ARP:  return "ARP";
    }
    return "unknown";
}

#define VPOXNETADP_PKTDMPSIZE 0x50

/**
 * Dump a packet to debug log.
 *
 * @param   cpPacket    The packet.
 * @param   cb          The size of the packet.
 * @param   cszText     A string denoting direction of packet transfer.
 */
DECLINLINE(void) vpoxNetAdpWinDumpPacket(PCINTNETSG pSG, const char *cszText)
{
    uint8_t bPacket[VPOXNETADP_PKTDMPSIZE];

    uint32_t cb = pSG->cbTotal < VPOXNETADP_PKTDMPSIZE ? pSG->cbTotal : VPOXNETADP_PKTDMPSIZE;
    IntNetSgReadEx(pSG, 0, cb, bPacket);

    AssertReturnVoid(cb >= 14);

    uint8_t *pHdr = bPacket;
    uint8_t *pEnd = bPacket + cb;
    AssertReturnVoid(pEnd - pHdr >= 14);
    uint16_t uEthType = RT_N2H_U16(*(uint16_t*)(pHdr+12));
    Log2(("NetADP: %s (%d bytes), %RTmac => %RTmac, EthType=%s(0x%x)\n",
          cszText, pSG->cbTotal, pHdr+6, pHdr, vpoxNetAdpWinEthTypeStr(uEthType), uEthType));
    pHdr += sizeof(RTNETETHERHDR);
    if (uEthType == RTNET_ETHERTYPE_VLAN)
    {
        AssertReturnVoid(pEnd - pHdr >= 4);
        uEthType = RT_N2H_U16(*(uint16_t*)(pHdr+2));
        Log2((" + VLAN: id=%d EthType=%s(0x%x)\n", RT_N2H_U16(*(uint16_t*)(pHdr)) & 0xFFF,
              vpoxNetAdpWinEthTypeStr(uEthType), uEthType));
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
//# define vpoxNetAdpWinDumpFilterTypes(uFlags)    do { } while (0)
//# define vpoxNetAdpWinDumpOffloadSettings(p)     do { } while (0)
//# define vpoxNetAdpWinDumpSetOffloadSettings(p)  do { } while (0)
# define vpoxNetAdpWinDumpPackets(m,l)           do { } while (0)
# define vpoxNetAdpWinDumpPacket(p,t)            do { } while (0)
#endif /* !DEBUG */


DECLHIDDEN(VPOXNETADPWIN_PACKET_TYPE) vpoxNetAdpWinPacketType(PINTNETSG pSG)
{
    static const uint8_t g_abBcastAddr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    AssertReturn(pSG->cbTotal >= sizeof(g_abBcastAddr), kVPoxNetAdpWinPacketType_Unicast);
    AssertReturn(pSG->cSegsUsed > 0, kVPoxNetAdpWinPacketType_Unicast);
    AssertReturn(pSG->aSegs[0].cb >= sizeof(g_abBcastAddr), kVPoxNetAdpWinPacketType_Unicast);
    if (!memcmp(pSG->aSegs[0].pv, g_abBcastAddr, sizeof(g_abBcastAddr)))
        return kVPoxNetAdpWinPacketType_Broadcast;
    if ((*(uint8_t*)pSG->aSegs[0].pv) & 1)
        return kVPoxNetAdpWinPacketType_Multicast;
    return kVPoxNetAdpWinPacketType_Unicast;
}

DECLINLINE(void) vpoxNetAdpWinUpdateStats(uint64_t *pPacketStats, uint64_t *pOctetStats, PINTNETSG pSG)
{
    VPOXNETADPWIN_PACKET_TYPE enmPktType = vpoxNetAdpWinPacketType(pSG);
    ASMAtomicIncU64(&pPacketStats[enmPktType]);
    ASMAtomicAddU64(&pOctetStats[enmPktType], pSG->cbTotal);
}

DECLINLINE(void) vpoxNetAdpWinFreeMdlChain(PMDL pMdl)
{
    PMDL pMdlNext;
    while (pMdl)
    {
        pMdlNext = pMdl->Next;
        PUCHAR pDataBuf;
        ULONG cb = 0;
        NdisQueryMdl(pMdl, &pDataBuf, &cb, NormalPagePriority);
        NdisFreeMdl(pMdl);
        Log4(("vpoxNetAdpWinFreeMdlChain: freed MDL 0x%p\n", pMdl));
        NdisFreeMemory(pDataBuf, 0, 0);
        Log4(("vpoxNetAdpWinFreeMdlChain: freed data buffer 0x%p\n", pDataBuf));
        pMdl = pMdlNext;
    }
}

DECLHIDDEN(PNET_BUFFER_LIST) vpoxNetAdpWinSGtoNB(PVPOXNETADP_ADAPTER pThis, PINTNETSG pSG)
{
    AssertReturn(pSG->cSegsUsed >= 1, NULL);
    LogFlow(("==>vpoxNetAdpWinSGtoNB: segments=%d hPool=%p cb=%u\n", pSG->cSegsUsed,
             pThis->hPool, pSG->cbTotal));
    AssertReturn(pThis->hPool, NULL);


    PNET_BUFFER_LIST pBufList = NULL;
    ULONG cbMdl = pSG->cbTotal;
    ULONG uDataOffset = cbMdl - pSG->cbTotal;
    PUCHAR pDataBuf = (PUCHAR)NdisAllocateMemoryWithTagPriority(pThis->hAdapter, cbMdl,
                                                                VPOXNETADP_MEM_TAG, NormalPoolPriority);
    if (pDataBuf)
    {
        Log4(("vpoxNetAdpWinSGtoNB: allocated data buffer (cb=%u) 0x%p\n", cbMdl, pDataBuf));
        PMDL pMdl = NdisAllocateMdl(pThis->hAdapter, pDataBuf, cbMdl);
        if (!pMdl)
        {
            NdisFreeMemory(pDataBuf, 0, 0);
            Log4(("vpoxNetAdpWinSGtoNB: freed data buffer 0x%p\n", pDataBuf));
            LogError(("vpoxNetAdpWinSGtoNB: failed to allocate an MDL (cb=%u)\n", cbMdl));
            LogFlow(("<==vpoxNetAdpWinSGtoNB: return NULL\n"));
            return NULL;
        }
        PUCHAR pDst = pDataBuf + uDataOffset;
        for (int i = 0; i < pSG->cSegsUsed; i++)
        {
            NdisMoveMemory(pDst, pSG->aSegs[i].pv, pSG->aSegs[i].cb);
            pDst += pSG->aSegs[i].cb;
        }
        pBufList = NdisAllocateNetBufferAndNetBufferList(pThis->hPool,
                                                         0 /* ContextSize */,
                                                         0 /* ContextBackFill */,
                                                         pMdl,
                                                         uDataOffset,
                                                         pSG->cbTotal);
        if (pBufList)
        {
            Log4(("vpoxNetAdpWinSGtoNB: allocated NBL+NB 0x%p\n", pBufList));
            pBufList->SourceHandle = pThis->hAdapter;
            /** @todo Do we need to initialize anything else? */
        }
        else
        {
            LogError(("vpoxNetAdpWinSGtoNB: failed to allocate an NBL+NB\n"));
            vpoxNetAdpWinFreeMdlChain(pMdl);
        }
    }
    else
    {
        LogError(("vpoxNetAdpWinSGtoNB: failed to allocate data buffer (size=%u)\n", cbMdl));
    }

    LogFlow(("<==vpoxNetAdpWinSGtoNB: return %p\n", pBufList));
    return pBufList;
}

DECLINLINE(void) vpoxNetAdpWinDestroySG(PINTNETSG pSG)
{
    NdisFreeMemory(pSG, 0, 0);
    Log4(("vpoxNetAdpWinDestroySG: freed SG 0x%p\n", pSG));
}

/**
 * Worker for vpoxNetAdpWinNBtoSG() that gets the max segment count needed.
 * @note vpoxNetAdpWinNBtoSG may use fewer depending on cbPacket and offset!
 * @note vpoxNetLwfWinCalcSegments() is a copy of this code.
 */
DECLINLINE(ULONG) vpoxNetAdpWinCalcSegments(PNET_BUFFER pNetBuf)
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

/**
 * @note vpoxNetLwfWinNBtoSG() is a copy of this code.
 */
DECLHIDDEN(PINTNETSG) vpoxNetAdpWinNBtoSG(PVPOXNETADP_ADAPTER pThis, PNET_BUFFER pNetBuf)
{
    ULONG cbPacket = NET_BUFFER_DATA_LENGTH(pNetBuf);
    ULONG cSegs = vpoxNetAdpWinCalcSegments(pNetBuf);
    /* Allocate and initialize SG */
    PINTNETSG pSG = (PINTNETSG)NdisAllocateMemoryWithTagPriority(pThis->hAdapter,
                                                                 RT_UOFFSETOF_DYN(INTNETSG, aSegs[cSegs]),
                                                                 VPOXNETADP_MEM_TAG,
                                                                 NormalPoolPriority);
    AssertReturn(pSG, pSG);
    Log4(("vpoxNetAdpWinNBtoSG: allocated SG 0x%p\n", pSG));
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
            vpoxNetAdpWinDestroySG(pSG);
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
                vpoxNetAdpWinDestroySG(pSG);
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

DECLINLINE(bool) vpoxNetAdpWinIsActive(PVPOXNETADP_ADAPTER pThis)
{
    if (vpoxNetAdpWinGetState(pThis) != kVPoxNetAdpWinState_Running)
        return false;
    if (pThis->enmTrunkState != INTNETTRUNKIFSTATE_ACTIVE)
        return false;
    AssertPtrReturn(pThis->pSwitchPort, false);
    return true;
}

DECLHIDDEN(bool) vpoxNetAdpWinForwardToIntNet(PVPOXNETADP_ADAPTER pThis, PNET_BUFFER_LIST pList, uint32_t fSrc)
{
    if (!vpoxNetAdpWinIsActive(pThis))
    {
        LogFlow(("vpoxNetAdpWinForwardToIntNet: not active\n"));
        return false;
    }
    AssertReturn(pThis->pSwitchPort, false);
    AssertReturn(pThis->pSwitchPort->pfnRecv, false);
    LogFlow(("==>vpoxNetAdpWinForwardToIntNet\n"));

    if (ASMAtomicIncS32(&pThis->cBusy) == 1)
        NdisResetEvent(&pThis->EventIdle);
    for (PNET_BUFFER pBuf = NET_BUFFER_LIST_FIRST_NB(pList); pBuf; pBuf = NET_BUFFER_NEXT_NB(pBuf))
    {
        PINTNETSG pSG = vpoxNetAdpWinNBtoSG(pThis, pBuf);
        if (pSG)
        {
            vpoxNetAdpWinUpdateStats(pThis->au64StatsOutPackets, pThis->au64StatsOutOctets, pSG);
            vpoxNetAdpWinDumpPacket(pSG, (fSrc & INTNETTRUNKDIR_WIRE)?"intnet <-- wire":"intnet <-- host");
            pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, NULL, pSG, fSrc);
            vpoxNetAdpWinDestroySG(pSG);
        }
    }
    if (ASMAtomicDecS32(&pThis->cBusy) == 0)
        NdisSetEvent(&pThis->EventIdle);

    return true;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnRetain
 */
static DECLCALLBACK(void) vpoxNetAdpWinPortRetain(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETADP_ADAPTER pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    RT_NOREF1(pThis);
    LogFlow(("vpoxNetAdpWinPortRetain: pThis=%p, pIfPort=%p\n", pThis, pIfPort));
}

/**
 * @copydoc INTNETTRUNKIFPORT::pfnRelease
 */
static DECLCALLBACK(void) vpoxNetAdpWinPortRelease(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETADP_ADAPTER pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    RT_NOREF1(pThis);
    LogFlow(("vpoxNetAdpWinPortRelease: pThis=%p, pIfPort=%p\n", pThis, pIfPort));
}

/**
 * @copydoc INTNETTRUNKIFPORT::pfnDisconnectAndRelease
 */
static DECLCALLBACK(void) vpoxNetAdpWinPortDisconnectAndRelease(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETADP_ADAPTER pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);

    LogFlow(("vpoxNetAdpWinPortDisconnectAndRelease: pThis=%p, pIfPort=%p\n", pThis, pIfPort));
    /*
     * Serious paranoia.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    Assert(pThis->MyPort.u32VersionEnd == INTNETTRUNKIFPORT_VERSION);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->szName[0]);

    AssertPtr(pThis->pSwitchPort);
    Assert(pThis->enmTrunkState == INTNETTRUNKIFSTATE_DISCONNECTING);

    pThis->pSwitchPort = NULL;
}

/**
 * @copydoc INTNETTRUNKIFPORT::pfnSetState
 */
static DECLCALLBACK(INTNETTRUNKIFSTATE) vpoxNetAdpWinPortSetState(PINTNETTRUNKIFPORT pIfPort, INTNETTRUNKIFSTATE enmState)
{
    PVPOXNETADP_ADAPTER      pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    INTNETTRUNKIFSTATE  enmOldTrunkState;

    LogFlow(("vpoxNetAdpWinPortSetState: pThis=%p, pIfPort=%p, enmState=%d\n", pThis, pIfPort, enmState));
    /*
     * Input validation.
     */
    AssertPtr(pThis);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    AssertPtrReturn(pThis->pSwitchPort, INTNETTRUNKIFSTATE_INVALID);
    AssertReturn(enmState > INTNETTRUNKIFSTATE_INVALID && enmState < INTNETTRUNKIFSTATE_END,
                 INTNETTRUNKIFSTATE_INVALID);

    enmOldTrunkState = pThis->enmTrunkState;
    if (enmOldTrunkState != enmState)
        ASMAtomicWriteU32((uint32_t volatile *)&pThis->enmTrunkState, enmState);

    return enmOldTrunkState;
}

/**
 * @copydoc INTNETTRUNKIFPORT::pfnWaitForIdle
 */
static DECLCALLBACK(int) vpoxNetAdpWinPortWaitForIdle(PINTNETTRUNKIFPORT pIfPort, uint32_t cMillies)
{
    PVPOXNETADP_ADAPTER pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    int rc;

    LogFlow(("vpoxNetAdpWinPortWaitForIdle: pThis=%p, pIfPort=%p, cMillies=%u\n", pThis, pIfPort, cMillies));
    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    AssertPtrReturn(pThis->pSwitchPort, VERR_INVALID_STATE);
    AssertReturn(pThis->enmTrunkState == INTNETTRUNKIFSTATE_DISCONNECTING, VERR_INVALID_STATE);

    rc = NdisWaitEvent(&pThis->EventIdle, cMillies) ? VINF_SUCCESS : VERR_TIMEOUT;

    return rc;
}

/**
 * @copydoc INTNETTRUNKIFPORT::pfnXmit
 */
static DECLCALLBACK(int) vpoxNetAdpWinPortXmit(PINTNETTRUNKIFPORT pIfPort, void *pvIfData, PINTNETSG pSG, uint32_t fDst)
{
    RT_NOREF1(fDst);
    PVPOXNETADP_ADAPTER pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    int rc = VINF_SUCCESS;

    LogFlow(("vpoxNetAdpWinPortXmit: pThis=%p, pIfPort=%p, pvIfData=%p, pSG=%p, fDst=0x%x\n", pThis, pIfPort, pvIfData, pSG, fDst));
    RT_NOREF1(pvIfData);
    /*
     * Input validation.
     */
    AssertPtr(pThis);
    AssertPtr(pSG);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    AssertPtrReturn(pThis->pSwitchPort, VERR_INVALID_STATE);

    vpoxNetAdpWinDumpPacket(pSG, "intnet --> host");

    /*
     * First of all, indicate we are busy. It is possible the trunk or the adapter
     * will get paused or even disconnected, so we need to check the state after
     * we have marked ourselves busy.
     * Later, when NDIS returns all buffers, we will mark ourselves idle.
     */
    if (ASMAtomicIncS32(&pThis->cBusy) == 1)
        NdisResetEvent(&pThis->EventIdle);

    if (vpoxNetAdpWinIsActive(pThis))
    {
        PNET_BUFFER_LIST pBufList = vpoxNetAdpWinSGtoNB(pThis, pSG);
        if (pBufList)
        {
            NdisMIndicateReceiveNetBufferLists(pThis->hAdapter, pBufList, NDIS_DEFAULT_PORT_NUMBER, 1, 0);
            vpoxNetAdpWinUpdateStats(pThis->au64StatsInPackets, pThis->au64StatsInOctets, pSG);
        }
    }

    return rc;
}

/**
 * @copydoc INTNETTRUNKIFPORT::pfnNotifyMacAddress
 */
static DECLCALLBACK(void) vpoxNetAdpWinPortNotifyMacAddress(PINTNETTRUNKIFPORT pIfPort, void *pvIfData, PCRTMAC pMac)
{
    PVPOXNETADP_ADAPTER pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);

    LogFlow(("vpoxNetAdpWinPortNotifyMacAddress: pThis=%p, pIfPort=%p, pvIfData=%p, pMac=%p\n", pThis, pIfPort, pvIfData, pMac));
    RT_NOREF3(pThis, pvIfData, pMac);
    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);

    /// @todo Do we really need to handle this?
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnConnectInterface
 */
static DECLCALLBACK(int) vpoxNetAdpWinPortConnectInterface(PINTNETTRUNKIFPORT pIfPort, void *pvIf, void **ppvIfData)
{
    PVPOXNETADP_ADAPTER  pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    int             rc;

    LogFlow(("vpoxNetAdpWinPortConnectInterface: pThis=%p, pIfPort=%p, pvIf=%p, ppvIfData=%p\n", pThis, pIfPort, pvIf, ppvIfData));
    RT_NOREF3(pThis, pvIf, ppvIfData);
    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);

    rc = VINF_SUCCESS;

    return rc;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnDisconnectInterface
 */
static DECLCALLBACK(void) vpoxNetAdpWinPortDisconnectInterface(PINTNETTRUNKIFPORT pIfPort, void *pvIfData)
{
    PVPOXNETADP_ADAPTER  pThis = IFPORT_2_VPOXNETADP_ADAPTER(pIfPort);
    int             rc;

    LogFlow(("vpoxNetAdpWinPortDisconnectInterface: pThis=%p, pIfPort=%p, pvIfData=%p\n", pThis, pIfPort, pvIfData));
    RT_NOREF2(pThis, pvIfData);
    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);

    rc = VINF_SUCCESS;
    AssertRC(rc);
}



/**
 * Implements the SUPDRV component factor interface query method.
 *
 * @returns Pointer to an interface. NULL if not supported.
 *
 * @param   pSupDrvFactory      Pointer to the component factory registration structure.
 * @param   pSession            The session - unused.
 * @param   pszInterfaceUuid    The factory interface id.
 */
static DECLCALLBACK(void *) vpoxNetAdpWinQueryFactoryInterface(PCSUPDRVFACTORY pSupDrvFactory, PSUPDRVSESSION pSession,
                                                               const char *pszInterfaceUuid)
{
    PVPOXNETADPGLOBALS pGlobals = (PVPOXNETADPGLOBALS)((uint8_t *)pSupDrvFactory - RT_UOFFSETOF(VPOXNETADPGLOBALS, SupDrvFactory));

    /*
     * Convert the UUID strings and compare them.
     */
    RTUUID UuidReq;
    int rc = RTUuidFromStr(&UuidReq, pszInterfaceUuid);
    if (RT_SUCCESS(rc))
    {
        if (!RTUuidCompareStr(&UuidReq, INTNETTRUNKFACTORY_UUID_STR))
        {
            NdisAcquireSpinLock(&pGlobals->Lock);
            if (pGlobals->enmIdcState == kVPoxNetAdpWinIdcState_Connected)
            {
                pGlobals->cFactoryRefs++;
                NdisResetEvent(&pGlobals->EventUnloadAllowed);
            }
            NdisReleaseSpinLock(&pGlobals->Lock);
            return &pGlobals->TrunkFactory;
        }
#ifdef LOG_ENABLED
        else
            Log(("VPoxNetFlt: unknown factory interface query (%s)\n", pszInterfaceUuid));
#endif
    }
    else
        Log(("VPoxNetFlt: rc=%Rrc, uuid=%s\n", rc, pszInterfaceUuid));

    RT_NOREF1(pSession);
    return NULL;
}


DECLHIDDEN(void) vpoxNetAdpWinReportCapabilities(PVPOXNETADP_ADAPTER pThis)
{
    if (pThis->pSwitchPort)
    {
        pThis->pSwitchPort->pfnReportMacAddress(pThis->pSwitchPort, &pThis->MacAddr);
        /* Promiscuous mode makes no sense for host-only adapters, does it? */
        pThis->pSwitchPort->pfnReportGsoCapabilities(pThis->pSwitchPort, 0,
                                                     INTNETTRUNKDIR_WIRE | INTNETTRUNKDIR_HOST);
        pThis->pSwitchPort->pfnReportNoPreemptDsts(pThis->pSwitchPort, 0 /* none */);
    }
}

/**
 * @copydoc INTNETTRUNKFACTORY::pfnCreateAndConnect
 */
static DECLCALLBACK(int) vpoxNetAdpWinFactoryCreateAndConnect(PINTNETTRUNKFACTORY pIfFactory, const char *pszName,
                                                           PINTNETTRUNKSWPORT pSwitchPort, uint32_t fFlags,
                                                           PINTNETTRUNKIFPORT *ppIfPort)
{
    PVPOXNETADPGLOBALS pGlobals = (PVPOXNETADPGLOBALS)((uint8_t *)pIfFactory - RT_UOFFSETOF(VPOXNETADPGLOBALS, TrunkFactory));

    LogFlow(("==>vpoxNetAdpWinFactoryCreateAndConnect: pszName=%p:{%s} fFlags=%#x\n", pszName, pszName, fFlags));
    Assert(pGlobals->cFactoryRefs > 0);
    AssertMsgReturn(!(fFlags & ~(INTNETTRUNKFACTORY_FLAG_NO_PROMISC)),
                    ("%#x\n", fFlags), VERR_INVALID_PARAMETER);

    DbgPrint("vpoxNetAdpWinFactoryCreateAndConnect: looking for %s...\n", pszName);
    PVPOXNETADP_ADAPTER pAdapter = NULL;
    NdisAcquireSpinLock(&pGlobals->Lock);
    RTListForEach(&g_VPoxNetAdpGlobals.ListOfAdapters, pAdapter, VPOXNETADP_ADAPTER, node)
    {
        Log(("vpoxNetAdpWinFactoryCreateAndConnect: evaluating adapter=%s\n", pAdapter->szName));
        DbgPrint("vpoxNetAdpWinFactoryCreateAndConnect: evaluating %s...\n", pAdapter->szName);
        if (!RTStrICmp(pszName, pAdapter->szName))
        {
            pAdapter->pSwitchPort = pSwitchPort;
            *ppIfPort = &pAdapter->MyPort;
            NdisReleaseSpinLock(&g_VPoxNetAdpGlobals.Lock); /// @todo too early? adp should have been connected by the time we do this
            Log(("vpoxNetAdpWinFactoryCreateAndConnect: found matching adapter, name=%s\n", pszName));
            vpoxNetAdpWinReportCapabilities(pAdapter);
            /// @todo I guess there is no need in vpoxNetAdpWinRegisterIpAddrNotifier(pThis);
            LogFlow(("<==vpoxNetAdpWinFactoryCreateAndConnect: return VINF_SUCCESS\n"));
            return VINF_SUCCESS;
        }
    }
    NdisReleaseSpinLock(&pGlobals->Lock);
    /// @todo vpoxNetAdpLogErrorEvent(IO_ERR_INTERNAL_ERROR, STATUS_SUCCESS, 6);
    DbgPrint("vpoxNetAdpWinFactoryCreateAndConnect: could not find %s\n", pszName);
    LogFlow(("<==vpoxNetAdpWinFactoryCreateAndConnect: return VERR_INTNET_FLT_IF_NOT_FOUND\n"));
    return VERR_INTNET_FLT_IF_NOT_FOUND;
}


/**
 * @copydoc INTNETTRUNKFACTORY::pfnRelease
 */
static DECLCALLBACK(void) vpoxNetAdpWinFactoryRelease(PINTNETTRUNKFACTORY pIfFactory)
{
    PVPOXNETADPGLOBALS pGlobals = (PVPOXNETADPGLOBALS)((uint8_t *)pIfFactory - RT_OFFSETOF(VPOXNETADPGLOBALS, TrunkFactory));

    NdisAcquireSpinLock(&pGlobals->Lock);
    int32_t cRefs = ASMAtomicDecS32(&pGlobals->cFactoryRefs);
    if (cRefs == 0)
        NdisSetEvent(&pGlobals->EventUnloadAllowed);
    NdisReleaseSpinLock(&pGlobals->Lock);
    Assert(cRefs >= 0); NOREF(cRefs);
    LogFlow(("vpoxNetAdpWinFactoryRelease: cRefs=%d (new)\n", cRefs));
}



/* IDC */

DECLINLINE(const char *) vpoxNetAdpWinIdcStateToText(uint32_t enmState)
{
    switch (enmState)
    {
        case kVPoxNetAdpWinIdcState_Disconnected: return "Disconnected";
        case kVPoxNetAdpWinIdcState_Connecting: return "Connecting";
        case kVPoxNetAdpWinIdcState_Connected: return "Connected";
        case kVPoxNetAdpWinIdcState_Stopping: return "Stopping";
    }
    return "Unknown";
}

static VOID vpoxNetAdpWinInitIdcWorker(PVOID pvContext)
{
    int rc;
    PVPOXNETADPGLOBALS pGlobals = (PVPOXNETADPGLOBALS)pvContext;

    /*
     * Note that we break the rules here and access IDC state wihout acquiring
     * the lock. This is ok because vpoxNetAdpWinUnload will wait for this
     * thread to terminate itself and we always use atomic access to IDC state.
     * We check the state (while holding the lock) further when we have succeeded
     * to connect. We cannot take the lock here and release it later as we will
     * be holding it for too long.
     */
    while (ASMAtomicReadU32(&pGlobals->enmIdcState) == kVPoxNetAdpWinIdcState_Connecting)
    {
        /*
         * Establish a connection to SUPDRV and register our component factory.
         */
        rc = SUPR0IdcOpen(&pGlobals->SupDrvIDC, 0 /* iReqVersion = default */, 0 /* iMinVersion = default */, NULL, NULL, NULL);
        if (RT_SUCCESS(rc))
        {
            rc = SUPR0IdcComponentRegisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
            if (RT_SUCCESS(rc))
            {
                /*
                 * At this point we should take the lock to access IDC state as
                 * we technically may now race with factory methods.
                 */
                NdisAcquireSpinLock(&pGlobals->Lock);
                bool fSuccess = ASMAtomicCmpXchgU32(&pGlobals->enmIdcState,
                                                    kVPoxNetAdpWinIdcState_Connected,
                                                    kVPoxNetAdpWinIdcState_Connecting);
                NdisReleaseSpinLock(&pGlobals->Lock);
                if (!fSuccess)
                {
                    /* The state has been changed (the only valid transition is to "Stopping"), undo init */
                    rc = SUPR0IdcComponentDeregisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
                    AssertRC(rc);
                    SUPR0IdcClose(&pGlobals->SupDrvIDC);
                    Log(("vpoxNetAdpWinInitIdcWorker: state change (Connecting -> %s) while initializing IDC, closed IDC, rc=0x%x\n",
                         vpoxNetAdpWinIdcStateToText(ASMAtomicReadU32(&pGlobals->enmIdcState)), rc));
                }
                else
                {
                    Log(("vpoxNetAdpWinInitIdcWorker: IDC state change Connecting -> Connected\n"));
                }
            }
        }
        else
        {
            LARGE_INTEGER WaitIn100nsUnits;
            WaitIn100nsUnits.QuadPart = -(LONGLONG)5000000; /* 0.5 sec */
            KeDelayExecutionThread(KernelMode, FALSE /* non-alertable */, &WaitIn100nsUnits);
        }
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}


DECLHIDDEN(int) vpoxNetAdpWinStartInitIdcThread(PVPOXNETADPGLOBALS pGlobals)
{
    int rc = VERR_INVALID_STATE;

    /* No locking needed yet */
    if (ASMAtomicCmpXchgU32(&pGlobals->enmIdcState, kVPoxNetAdpWinIdcState_Connecting, kVPoxNetAdpWinIdcState_Disconnected))
    {
        Log(("vpoxNetAdpWinStartInitIdcThread: IDC state change Diconnected -> Connecting\n"));

        NTSTATUS Status = PsCreateSystemThread(&g_VPoxNetAdpGlobals.hInitIdcThread,
                                               THREAD_ALL_ACCESS,
                                               NULL,
                                               NULL,
                                               NULL,
                                               vpoxNetAdpWinInitIdcWorker,
                                               &g_VPoxNetAdpGlobals);
        Log(("vpoxNetAdpWinStartInitIdcThread: create IDC initialization thread, status=0x%x\n", Status));
        if (Status != STATUS_SUCCESS)
        {
            LogError(("vpoxNetAdpWinStartInitIdcThread: IDC initialization failed (system thread creation, status=0x%x)\n", Status));
            /*
             * We failed to init IDC and there will be no second chance.
             */
            Log(("vpoxNetAdpWinStartInitIdcThread: IDC state change Connecting -> Diconnected\n"));
            ASMAtomicWriteU32(&g_VPoxNetAdpGlobals.enmIdcState, kVPoxNetAdpWinIdcState_Disconnected);
        }
        rc = RTErrConvertFromNtStatus(Status);
    }
    return rc;
}



/* === !!!! */


NDIS_OID g_SupportedOids[] =
{
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_INTERRUPT_MODERATION,
    OID_GEN_LINK_PARAMETERS,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_RCV_OK,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_STATISTICS,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_VENDOR_ID,
    OID_GEN_XMIT_OK,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_PNP_CAPABILITIES,
    OID_PNP_QUERY_POWER,
    OID_PNP_SET_POWER
};

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinAllocAdapter(NDIS_HANDLE hAdapter, PVPOXNETADP_ADAPTER *ppAdapter, ULONG uIfIndex)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PVPOXNETADP_ADAPTER pAdapter = NULL;
    PVPOXNETADPGLOBALS pGlobals = &g_VPoxNetAdpGlobals;

    LogFlow(("==>vpoxNetAdpWinAllocAdapter: adapter handle=%p\n", hAdapter));

    /* Get the name */
    UNICODE_STRING strUnicodeName;
    Status = NdisMQueryAdapterInstanceName(&strUnicodeName, hAdapter);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        LogError(("vpoxNetAdpWinAllocAdapter: NdisMQueryAdapterInstanceName failed with 0x%x\n", Status));
        return Status;
    }

    ANSI_STRING strAnsiName;
    /* We use the miniport name to associate this filter module with the netflt instance */
    NTSTATUS rc = RtlUnicodeStringToAnsiString(&strAnsiName,
                                               &strUnicodeName,
                                               TRUE);
    if (rc != STATUS_SUCCESS)
    {
        LogError(("vpoxNetAdpWinAllocAdapter: RtlUnicodeStringToAnsiString(%ls) failed with 0x%x\n",
                  strUnicodeName, rc));
        //vpoxNetAdpLogErrorEvent(IO_ERR_INTERNAL_ERROR, NDIS_STATUS_FAILURE, 2);
        NdisFreeMemory(strUnicodeName.Buffer, 0, 0);
        return NDIS_STATUS_FAILURE;
    }
    NdisFreeMemory(strUnicodeName.Buffer, 0, 0);
    DbgPrint("vpoxNetAdpWinAllocAdapter: name=%Z\n", &strAnsiName);

    *ppAdapter = NULL;

    UINT cbAdapterWithNameExtra = sizeof(VPOXNETADP_ADAPTER) + strAnsiName.Length;
    pAdapter = (PVPOXNETADP_ADAPTER)NdisAllocateMemoryWithTagPriority(pGlobals->hMiniportDriver,
                                                                      cbAdapterWithNameExtra,
                                                                      VPOXNETADPWIN_TAG,
                                                                      NormalPoolPriority);
    if (!pAdapter)
    {
        RtlFreeAnsiString(&strAnsiName);
        Status = NDIS_STATUS_RESOURCES;
        Log(("vpoxNetAdpWinAllocAdapter: Out of memory while allocating adapter context (size=%d)\n", sizeof(VPOXNETADP_ADAPTER)));
    }
    else
    {
        NdisZeroMemory(pAdapter, cbAdapterWithNameExtra);
        NdisMoveMemory(pAdapter->szName, strAnsiName.Buffer, strAnsiName.Length);
        RtlFreeAnsiString(&strAnsiName);

        /* Allocate buffer pool */
        NET_BUFFER_LIST_POOL_PARAMETERS PoolParams;
        NdisZeroMemory(&PoolParams, sizeof(PoolParams));
        PoolParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
        PoolParams.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
        PoolParams.Header.Size = sizeof(PoolParams);
        PoolParams.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
        PoolParams.fAllocateNetBuffer = TRUE;
        PoolParams.ContextSize = 0;
        PoolParams.PoolTag = VPOXNETADP_MEM_TAG;
        pAdapter->hPool = NdisAllocateNetBufferListPool(hAdapter, &PoolParams);
        if (!pAdapter->hPool)
        {
            LogError(("vpoxNetAdpWinAllocAdapter: NdisAllocateNetBufferListPool failed\n"));
            NdisFreeMemory(pAdapter, 0, 0);
            return NDIS_STATUS_RESOURCES;
        }
        Log4(("vpoxNetAdpWinAllocAdapter: allocated NBL+NB pool 0x%p\n", pAdapter->hPool));

        pAdapter->hAdapter = hAdapter;
        pAdapter->MyPort.u32Version              = INTNETTRUNKIFPORT_VERSION;
        pAdapter->MyPort.pfnRetain               = vpoxNetAdpWinPortRetain;
        pAdapter->MyPort.pfnRelease              = vpoxNetAdpWinPortRelease;
        pAdapter->MyPort.pfnDisconnectAndRelease = vpoxNetAdpWinPortDisconnectAndRelease;
        pAdapter->MyPort.pfnSetState             = vpoxNetAdpWinPortSetState;
        pAdapter->MyPort.pfnWaitForIdle          = vpoxNetAdpWinPortWaitForIdle;
        pAdapter->MyPort.pfnXmit                 = vpoxNetAdpWinPortXmit;
        pAdapter->MyPort.pfnNotifyMacAddress     = vpoxNetAdpWinPortNotifyMacAddress;
        pAdapter->MyPort.pfnConnectInterface     = vpoxNetAdpWinPortConnectInterface;
        pAdapter->MyPort.pfnDisconnectInterface  = vpoxNetAdpWinPortDisconnectInterface;
        pAdapter->MyPort.u32VersionEnd           = INTNETTRUNKIFPORT_VERSION;
        pAdapter->pGlobals = pGlobals;
        pAdapter->enmAdapterState = kVPoxNetAdpWinState_Initializing;
        pAdapter->enmTrunkState = INTNETTRUNKIFSTATE_INACTIVE;
        pAdapter->cBusy = 0;
        NdisInitializeEvent(&pAdapter->EventIdle);
        NdisSetEvent(&pAdapter->EventIdle); /* We are idle initially */

        /* Use a locally administered version of the OUI we use for the guest NICs. */
        pAdapter->MacAddr.au8[0] = 0x08 | 2;
        pAdapter->MacAddr.au8[1] = 0x00;
        pAdapter->MacAddr.au8[2] = 0x27;

        pAdapter->MacAddr.au8[3] = (uIfIndex >> 16) & 0xFF;
        pAdapter->MacAddr.au8[4] = (uIfIndex >> 8) & 0xFF;
        pAdapter->MacAddr.au8[5] = uIfIndex & 0xFF;

        NdisAcquireSpinLock(&pGlobals->Lock);
        RTListPrepend(&pGlobals->ListOfAdapters, &pAdapter->node);
        NdisReleaseSpinLock(&pGlobals->Lock);

        *ppAdapter = pAdapter;
    }
    LogFlow(("<==vpoxNetAdpWinAllocAdapter: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(void) vpoxNetAdpWinFreeAdapter(PVPOXNETADP_ADAPTER pAdapter)
{
    /* Remove from adapter chain */
    NdisAcquireSpinLock(&pAdapter->pGlobals->Lock);
    RTListNodeRemove(&pAdapter->node);
    NdisReleaseSpinLock(&pAdapter->pGlobals->Lock);

    NdisFreeMemory(pAdapter, 0, 0);
}

DECLINLINE(NDIS_MEDIA_CONNECT_STATE) vpoxNetAdpWinGetConnectState(PVPOXNETADP_ADAPTER pAdapter)
{
    RT_NOREF1(pAdapter);
    return MediaConnectStateConnected;
}


DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinInitializeEx(IN NDIS_HANDLE NdisMiniportHandle,
                                                  IN NDIS_HANDLE MiniportDriverContext,
                                                  IN PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters)
{
    RT_NOREF1(MiniportDriverContext);
    PVPOXNETADP_ADAPTER pAdapter = NULL;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    LogFlow(("==>vpoxNetAdpWinInitializeEx: miniport=0x%x\n", NdisMiniportHandle));

    do
    {
        NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES RAttrs = {0};
        NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES GAttrs = {0};

        Status = vpoxNetAdpWinAllocAdapter(NdisMiniportHandle, &pAdapter, MiniportInitParameters->IfIndex);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vpoxNetAdpWinInitializeEx: Failed to allocate the adapter context with 0x%x\n", Status));
            break;
        }

        RAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
        RAttrs.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
        RAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
        RAttrs.MiniportAdapterContext = pAdapter;
        RAttrs.AttributeFlags = VPOXNETADPWIN_ATTR_FLAGS; // NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM
        RAttrs.CheckForHangTimeInSeconds = VPOXNETADPWIN_HANG_CHECK_TIME;
        RAttrs.InterfaceType = NdisInterfaceInternal;

        Status = NdisMSetMiniportAttributes(NdisMiniportHandle,
                                            (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&RAttrs);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vpoxNetAdpWinInitializeEx: NdisMSetMiniportAttributes(registration) failed with 0x%x\n", Status));
            break;
        }

        /// @todo Registry?

        /// @todo WDM stack?

        /// @todo DPC?

        GAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
        GAttrs.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
        GAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;

        GAttrs.MediaType = NdisMedium802_3;
        GAttrs.PhysicalMediumType = NdisPhysicalMediumUnspecified;
        GAttrs.MtuSize = 1500; /// @todo
        GAttrs.MaxXmitLinkSpeed = VPOXNETADPWIN_LINK_SPEED;
        GAttrs.XmitLinkSpeed = VPOXNETADPWIN_LINK_SPEED;
        GAttrs.MaxRcvLinkSpeed = VPOXNETADPWIN_LINK_SPEED;
        GAttrs.RcvLinkSpeed = VPOXNETADPWIN_LINK_SPEED;
        GAttrs.MediaConnectState = vpoxNetAdpWinGetConnectState(pAdapter);
        GAttrs.MediaDuplexState = MediaDuplexStateFull;
        GAttrs.LookaheadSize = 1500; /// @todo
        GAttrs.MacOptions = VPOXNETADP_MAC_OPTIONS;
        GAttrs.SupportedPacketFilters = VPOXNETADP_SUPPORTED_FILTERS;
        GAttrs.MaxMulticastListSize = 32; /// @todo

        GAttrs.MacAddressLength = ETH_LENGTH_OF_ADDRESS;
        Assert(GAttrs.MacAddressLength == sizeof(pAdapter->MacAddr));
        memcpy(GAttrs.PermanentMacAddress, pAdapter->MacAddr.au8, GAttrs.MacAddressLength);
        memcpy(GAttrs.CurrentMacAddress, pAdapter->MacAddr.au8, GAttrs.MacAddressLength);

        GAttrs.RecvScaleCapabilities = NULL;
        GAttrs.AccessType = NET_IF_ACCESS_BROADCAST;
        GAttrs.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
        GAttrs.ConnectionType = NET_IF_CONNECTION_DEDICATED;
        GAttrs.IfType = IF_TYPE_ETHERNET_CSMACD;
        GAttrs.IfConnectorPresent = false;
        GAttrs.SupportedStatistics = VPOXNETADPWIN_SUPPORTED_STATISTICS;
        GAttrs.SupportedPauseFunctions = NdisPauseFunctionsUnsupported;
        GAttrs.DataBackFillSize = 0;
        GAttrs.ContextBackFillSize = 0;
        GAttrs.SupportedOidList = g_SupportedOids;
        GAttrs.SupportedOidListLength = sizeof(g_SupportedOids);
        GAttrs.AutoNegotiationFlags = NDIS_LINK_STATE_DUPLEX_AUTO_NEGOTIATED;
        GAttrs.PowerManagementCapabilities = &g_VPoxNetAdpGlobals.PMCaps;

        Status = NdisMSetMiniportAttributes(NdisMiniportHandle,
                                            (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&GAttrs);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("vpoxNetAdpWinInitializeEx: NdisMSetMiniportAttributes(general) failed with 0x%x\n", Status));
            break;
        }

        VPOXNETADPWIN_ADAPTER_STATE enmPrevState = vpoxNetAdpWinSetState(pAdapter, kVPoxNetAdpWinState_Paused);
        RT_NOREF1(enmPrevState);
        Assert(enmPrevState == kVPoxNetAdpWinState_Initializing);
    } while (false);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        if (pAdapter)
            vpoxNetAdpWinFreeAdapter(pAdapter);
    }

    LogFlow(("<==vpoxNetAdpWinInitializeEx: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(VOID) vpoxNetAdpWinHaltEx(IN NDIS_HANDLE MiniportAdapterContext,
                                     IN NDIS_HALT_ACTION HaltAction)
{
    RT_NOREF1(HaltAction);
    PVPOXNETADP_ADAPTER pThis = (PVPOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vpoxNetAdpWinHaltEx\n"));
    AssertPtr(pThis);
    Assert(vpoxNetAdpWinGetState(pThis) == kVPoxNetAdpWinState_Paused);
    /*
     * Check if the trunk is active which means the adapter gets disabled
     * while it is used by VM(s) and we need to disconnect the trunk.
     */
    if (pThis->pSwitchPort && pThis->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE)
        pThis->pSwitchPort->pfnDisconnect(pThis->pSwitchPort, &pThis->MyPort, NULL);
    /*
     * Since we are already in the paused state and we have disconnected
     * the trunk, we can safely destroy this adapter.
     */
    vpoxNetAdpWinFreeAdapter(pThis);
    LogFlow(("<==vpoxNetAdpWinHaltEx\n"));
}

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinPause(IN NDIS_HANDLE MiniportAdapterContext,
                                           IN PNDIS_MINIPORT_PAUSE_PARAMETERS MiniportPauseParameters)
{
    RT_NOREF1(MiniportPauseParameters);
    PVPOXNETADP_ADAPTER pThis = (PVPOXNETADP_ADAPTER)MiniportAdapterContext;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("==>vpoxNetAdpWinPause\n"));
    VPOXNETADPWIN_ADAPTER_STATE enmPrevState = vpoxNetAdpWinSetState(pThis, kVPoxNetAdpWinState_Pausing);
    Assert(enmPrevState == kVPoxNetAdpWinState_Running);
    if (!NdisWaitEvent(&pThis->EventIdle, 1000 /* ms */))
    {
        LogError(("vpoxNetAdpWinPause: timed out while pausing the adapter\n"));
        /// @todo implement NDIS_STATUS_PENDING case? probably not.
    }
    enmPrevState = vpoxNetAdpWinSetState(pThis, kVPoxNetAdpWinState_Paused);
    Assert(enmPrevState == kVPoxNetAdpWinState_Pausing);
    LogFlow(("<==vpoxNetAdpWinPause: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinRestart(IN NDIS_HANDLE MiniportAdapterContext,
                                             IN PNDIS_MINIPORT_RESTART_PARAMETERS MiniportRestartParameters)
{
    RT_NOREF1(MiniportRestartParameters);
    PVPOXNETADP_ADAPTER pThis = (PVPOXNETADP_ADAPTER)MiniportAdapterContext;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("==>vpoxNetAdpWinRestart\n"));
    VPOXNETADPWIN_ADAPTER_STATE enmPrevState = vpoxNetAdpWinSetState(pThis, kVPoxNetAdpWinState_Restarting);
    Assert(enmPrevState == kVPoxNetAdpWinState_Paused);
    /// @todo anything?
    enmPrevState = vpoxNetAdpWinSetState(pThis, kVPoxNetAdpWinState_Running);
    Assert(enmPrevState == kVPoxNetAdpWinState_Restarting);
    LogFlow(("<==vpoxNetAdpWinRestart: status=0x%x\n", Status));
    return Status;
}

DECLINLINE(uint64_t) vpoxNetAdpWinStatsTotals(uint64_t *pStats)
{
    return pStats[kVPoxNetAdpWinPacketType_Unicast]
        + pStats[kVPoxNetAdpWinPacketType_Multicast]
        + pStats[kVPoxNetAdpWinPacketType_Broadcast];
}

DECLINLINE(PVOID) vpoxNetAdpWinStatsU64(uint64_t *pTmp, ULONG *pcbTmp, uint64_t u64Stat)
{
    *pcbTmp = sizeof(*pTmp);
    *pTmp = u64Stat;
    return pTmp;
}

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinOidRqQuery(PVPOXNETADP_ADAPTER pThis,
                                                PNDIS_OID_REQUEST pRequest)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    struct _NDIS_OID_REQUEST::_REQUEST_DATA::_QUERY *pQuery = &pRequest->DATA.QUERY_INFORMATION;

    LogFlow(("==>vpoxNetAdpWinOidRqQuery\n"));

    uint64_t u64Tmp = 0;
    ULONG ulTmp = 0;
    PVOID pInfo = &ulTmp;
    ULONG cbInfo = sizeof(ulTmp);

    switch (pQuery->Oid)
    {
        case OID_GEN_INTERRUPT_MODERATION:
        {
            PNDIS_INTERRUPT_MODERATION_PARAMETERS pParams =
                (PNDIS_INTERRUPT_MODERATION_PARAMETERS)pQuery->InformationBuffer;
            cbInfo = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            if (cbInfo > pQuery->InformationBufferLength)
                break;
            pParams->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            pParams->Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            pParams->Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            pParams->Flags = 0;
            pParams->InterruptModeration = NdisInterruptModerationNotSupported;
            pInfo = NULL; /* Do not copy */
            break;
        }
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
            ulTmp = VPOXNETADP_MAX_FRAME_SIZE;
            break;
        case OID_GEN_RECEIVE_BUFFER_SPACE:
        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            /// @todo Make configurable
            ulTmp = VPOXNETADP_MAX_FRAME_SIZE * 40;
            break;
        case OID_GEN_RCV_OK:
            pInfo = vpoxNetAdpWinStatsU64(&u64Tmp, &cbInfo, vpoxNetAdpWinStatsTotals(pThis->au64StatsInPackets));
            break;
        case OID_GEN_XMIT_OK:
            pInfo = vpoxNetAdpWinStatsU64(&u64Tmp, &cbInfo, vpoxNetAdpWinStatsTotals(pThis->au64StatsOutPackets));
            break;
        case OID_GEN_STATISTICS:
        {
            PNDIS_STATISTICS_INFO pStats =
                (PNDIS_STATISTICS_INFO)pQuery->InformationBuffer;
            cbInfo = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
            if (cbInfo > pQuery->InformationBufferLength)
                break;
            pInfo = NULL; /* Do not copy */
            memset(pStats, 0, cbInfo);
            pStats->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            pStats->Header.Revision = NDIS_STATISTICS_INFO_REVISION_1;
            pStats->Header.Size = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
            pStats->SupportedStatistics =
                  NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS
                | NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR
                | NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT
                | NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT
                | NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT
                | NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT
                | NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR
                | NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS
                | NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV
                | NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT
                | NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT
                | NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT;

            pStats->ifHCInOctets = vpoxNetAdpWinStatsTotals(pThis->au64StatsInOctets);
            pStats->ifHCInUcastPkts = ASMAtomicReadU64(&pThis->au64StatsInPackets[kVPoxNetAdpWinPacketType_Unicast]);
            pStats->ifHCInMulticastPkts = ASMAtomicReadU64(&pThis->au64StatsInPackets[kVPoxNetAdpWinPacketType_Multicast]);
            pStats->ifHCInBroadcastPkts = ASMAtomicReadU64(&pThis->au64StatsInPackets[kVPoxNetAdpWinPacketType_Broadcast]);
            pStats->ifHCOutOctets = vpoxNetAdpWinStatsTotals(pThis->au64StatsOutOctets);;
            pStats->ifHCOutUcastPkts = ASMAtomicReadU64(&pThis->au64StatsOutPackets[kVPoxNetAdpWinPacketType_Unicast]);
            pStats->ifHCOutMulticastPkts = ASMAtomicReadU64(&pThis->au64StatsOutPackets[kVPoxNetAdpWinPacketType_Multicast]);
            pStats->ifHCOutBroadcastPkts = ASMAtomicReadU64(&pThis->au64StatsOutPackets[kVPoxNetAdpWinPacketType_Broadcast]);
            pStats->ifHCInUcastOctets = ASMAtomicReadU64(&pThis->au64StatsInOctets[kVPoxNetAdpWinPacketType_Unicast]);
            pStats->ifHCInMulticastOctets = ASMAtomicReadU64(&pThis->au64StatsInOctets[kVPoxNetAdpWinPacketType_Multicast]);
            pStats->ifHCInBroadcastOctets = ASMAtomicReadU64(&pThis->au64StatsInOctets[kVPoxNetAdpWinPacketType_Broadcast]);
            pStats->ifHCOutUcastOctets = ASMAtomicReadU64(&pThis->au64StatsOutOctets[kVPoxNetAdpWinPacketType_Unicast]);
            pStats->ifHCOutMulticastOctets = ASMAtomicReadU64(&pThis->au64StatsOutOctets[kVPoxNetAdpWinPacketType_Multicast]);
            pStats->ifHCOutBroadcastOctets = ASMAtomicReadU64(&pThis->au64StatsOutOctets[kVPoxNetAdpWinPacketType_Broadcast]);
            break;
        }
        case OID_GEN_VENDOR_DESCRIPTION:
            pInfo = VPOXNETADP_VENDOR_NAME;
            cbInfo = sizeof(VPOXNETADP_VENDOR_NAME);
            break;
        case OID_GEN_VENDOR_DRIVER_VERSION:
            ulTmp = (VPOXNETADP_VERSION_NDIS_MAJOR << 16) | VPOXNETADP_VERSION_NDIS_MINOR;
            break;
        case OID_GEN_VENDOR_ID:
            ulTmp = VPOXNETADP_VENDOR_ID;
            break;
        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
            pInfo = &pThis->MacAddr;
            cbInfo = sizeof(pThis->MacAddr);
            break;
            //case OID_802_3_MULTICAST_LIST:
        case OID_802_3_MAXIMUM_LIST_SIZE:
            ulTmp = VPOXNETADP_MCAST_LIST_SIZE;
            break;
        case OID_PNP_CAPABILITIES:
            pInfo = &pThis->pGlobals->PMCaps;
            cbInfo = sizeof(pThis->pGlobals->PMCaps);
            break;
        case OID_PNP_QUERY_POWER:
            pInfo = NULL; /* Do not copy */
            cbInfo = 0;
            break;
        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (cbInfo > pQuery->InformationBufferLength)
        {
            pQuery->BytesNeeded = cbInfo;
            Status = NDIS_STATUS_BUFFER_TOO_SHORT;
        }
        else
        {
            if (pInfo)
                NdisMoveMemory(pQuery->InformationBuffer, pInfo, cbInfo);
            pQuery->BytesWritten = cbInfo;
        }
    }

    LogFlow(("<==vpoxNetAdpWinOidRqQuery: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinOidRqSet(PVPOXNETADP_ADAPTER pAdapter,
                                              PNDIS_OID_REQUEST pRequest)
{
    RT_NOREF1(pAdapter);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    struct _NDIS_OID_REQUEST::_REQUEST_DATA::_SET *pSet = &pRequest->DATA.SET_INFORMATION;

    LogFlow(("==>vpoxNetAdpWinOidRqSet\n"));

    switch (pSet->Oid)
    {
        case OID_GEN_CURRENT_LOOKAHEAD:
            if (pSet->InformationBufferLength != sizeof(ULONG))
            {
                pSet->BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            /// @todo For the time being we simply ignore lookahead settings.
            pSet->BytesRead = sizeof(ULONG);
            Status = NDIS_STATUS_SUCCESS;
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            if (pSet->InformationBufferLength != sizeof(ULONG))
            {
                pSet->BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            /// @todo For the time being we simply ignore packet filter settings.
            pSet->BytesRead = pSet->InformationBufferLength;
            Status = NDIS_STATUS_SUCCESS;
            break;

        case OID_GEN_INTERRUPT_MODERATION:
            pSet->BytesNeeded = 0;
            pSet->BytesRead = 0;
            Status = NDIS_STATUS_INVALID_DATA;
            break;

        case OID_PNP_SET_POWER:
            if (pSet->InformationBufferLength < sizeof(NDIS_DEVICE_POWER_STATE))
            {
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }
            pSet->BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
            Status = NDIS_STATUS_SUCCESS;
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    LogFlow(("<==vpoxNetAdpWinOidRqSet: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinOidRequest(IN NDIS_HANDLE MiniportAdapterContext,
                                                IN PNDIS_OID_REQUEST NdisRequest)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PVPOXNETADP_ADAPTER pAdapter = (PVPOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vpoxNetAdpWinOidRequest\n"));
    vpoxNetCmnWinDumpOidRequest(__FUNCTION__, NdisRequest);

    switch (NdisRequest->RequestType)
    {
#if 0
        case NdisRequestMethod:
            Status = vpoxNetAdpWinOidRqMethod(pAdapter, NdisRequest);
            break;
#endif

        case NdisRequestSetInformation:
            Status = vpoxNetAdpWinOidRqSet(pAdapter, NdisRequest);
            break;

        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
            Status = vpoxNetAdpWinOidRqQuery(pAdapter, NdisRequest);
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }
    LogFlow(("<==vpoxNetAdpWinOidRequest: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(VOID) vpoxNetAdpWinSendNetBufferLists(IN NDIS_HANDLE MiniportAdapterContext,
                                                 IN PNET_BUFFER_LIST NetBufferLists,
                                                 IN NDIS_PORT_NUMBER PortNumber,
                                                 IN ULONG SendFlags)
{
    RT_NOREF1(PortNumber);
    PVPOXNETADP_ADAPTER pAdapter = (PVPOXNETADP_ADAPTER)MiniportAdapterContext;
    LogFlow(("==>vpoxNetAdpWinSendNetBufferLists\n"));
    PNET_BUFFER_LIST pNbl = NetBufferLists;
    vpoxNetAdpWinDumpPackets("vpoxNetAdpWinSendNetBufferLists: got", pNbl);

    /* We alwast complete all send requests. */
    for (pNbl = NetBufferLists; pNbl; pNbl = NET_BUFFER_LIST_NEXT_NBL(pNbl))
    {
        vpoxNetAdpWinForwardToIntNet(pAdapter, pNbl, INTNETTRUNKDIR_HOST);
        NET_BUFFER_LIST_STATUS(pNbl) = NDIS_STATUS_SUCCESS;
    }
    NdisMSendNetBufferListsComplete(pAdapter->hAdapter, NetBufferLists,
                                    (SendFlags & NDIS_SEND_FLAGS_DISPATCH_LEVEL) ?
                                    NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
    LogFlow(("<==vpoxNetAdpWinSendNetBufferLists\n"));
}

DECLHIDDEN(VOID) vpoxNetAdpWinReturnNetBufferLists(IN NDIS_HANDLE MiniportAdapterContext,
                                                   IN PNET_BUFFER_LIST NetBufferLists,
                                                   IN ULONG ReturnFlags)
{
    LogFlow(("==>vpoxNetAdpWinReturnNetBufferLists\n"));
    RT_NOREF1(ReturnFlags);
    PVPOXNETADP_ADAPTER pThis = (PVPOXNETADP_ADAPTER)MiniportAdapterContext;
    PNET_BUFFER_LIST pList = NetBufferLists;
    while (pList)
    {
        Assert(pList->SourceHandle == pThis->hAdapter);
        Assert(NET_BUFFER_LIST_FIRST_NB(pList));
        Assert(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));

        PNET_BUFFER_LIST pNextList = NET_BUFFER_LIST_NEXT_NBL(pList);

        vpoxNetAdpWinFreeMdlChain(NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(pList)));
        NdisFreeNetBufferList(pList);
        Log4(("vpoxNetLwfWinReturnNetBufferLists: freed NBL+NB+MDL+Data 0x%p\n", pList));
        Assert(ASMAtomicReadS32(&pThis->cBusy) > 0);
        if (ASMAtomicDecS32(&pThis->cBusy) == 0)
            NdisSetEvent(&pThis->EventIdle);

        pList = pNextList;
    }
    LogFlow(("<==vpoxNetAdpWinReturnNetBufferLists\n"));
}

DECLHIDDEN(VOID) vpoxNetAdpWinCancelSend(IN NDIS_HANDLE MiniportAdapterContext,
                                         IN PVOID CancelId)
{
    RT_NOREF2(MiniportAdapterContext, CancelId);
    LogFlow(("==>vpoxNetAdpWinCancelSend\n"));
    Log(("vpoxNetAdpWinCancelSend: We should not be here!\n"));
    LogFlow(("<==vpoxNetAdpWinCancelSend\n"));
}


DECLHIDDEN(BOOLEAN) vpoxNetAdpWinCheckForHangEx(IN NDIS_HANDLE MiniportAdapterContext)
{
    RT_NOREF1(MiniportAdapterContext);
    LogFlow(("==>vpoxNetAdpWinCheckForHangEx\n"));
    LogFlow(("<==vpoxNetAdpWinCheckForHangEx return false\n"));
    return FALSE;
}

DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinResetEx(IN NDIS_HANDLE MiniportAdapterContext,
                                             OUT PBOOLEAN AddressingReset)
{
    RT_NOREF2(MiniportAdapterContext, AddressingReset);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    LogFlow(("==>vpoxNetAdpWinResetEx\n"));
    LogFlow(("<==vpoxNetAdpWinResetEx: status=0x%x\n", Status));
    return Status;
}

DECLHIDDEN(VOID) vpoxNetAdpWinDevicePnPEventNotify(IN NDIS_HANDLE MiniportAdapterContext,
                                                   IN PNET_DEVICE_PNP_EVENT NetDevicePnPEvent)
{
    RT_NOREF2(MiniportAdapterContext, NetDevicePnPEvent);
    LogFlow(("==>vpoxNetAdpWinDevicePnPEventNotify\n"));
    Log(("vpoxNetAdpWinDevicePnPEventNotify: PnP event=%d\n", NetDevicePnPEvent->DevicePnPEvent));
    LogFlow(("<==vpoxNetAdpWinDevicePnPEventNotify\n"));
}


DECLHIDDEN(VOID) vpoxNetAdpWinShutdownEx(IN NDIS_HANDLE MiniportAdapterContext,
                                         IN NDIS_SHUTDOWN_ACTION ShutdownAction)
{
    RT_NOREF2(MiniportAdapterContext, ShutdownAction);
    LogFlow(("==>vpoxNetAdpWinShutdownEx\n"));
    Log(("vpoxNetAdpWinShutdownEx: action=%d\n", ShutdownAction));
    LogFlow(("<==vpoxNetAdpWinShutdownEx\n"));
}

DECLHIDDEN(VOID) vpoxNetAdpWinCancelOidRequest(IN NDIS_HANDLE MiniportAdapterContext,
                                               IN PVOID RequestId)
{
    RT_NOREF2(MiniportAdapterContext, RequestId);
    LogFlow(("==>vpoxNetAdpWinCancelOidRequest\n"));
    Log(("vpoxNetAdpWinCancelOidRequest: req id=%p\n", RequestId));
    LogFlow(("<==vpoxNetAdpWinCancelOidRequest\n"));
}



DECLHIDDEN(VOID) vpoxNetAdpWinUnload(IN PDRIVER_OBJECT DriverObject)
{
    RT_NOREF1(DriverObject);
    LogFlow(("==>vpoxNetAdpWinUnload\n"));
    PVPOXNETADPGLOBALS pGlobals = &g_VPoxNetAdpGlobals;
    int rc;
    NDIS_STATUS Status;
    PKTHREAD pThread = NULL;

    /* We are about to disconnect IDC, let's make it clear so the factories will know */
    NdisAcquireSpinLock(&pGlobals->Lock);
    uint32_t enmPrevState = ASMAtomicXchgU32(&g_VPoxNetAdpGlobals.enmIdcState, kVPoxNetAdpWinIdcState_Stopping);
    NdisReleaseSpinLock(&pGlobals->Lock);
    Log(("vpoxNetAdpWinUnload: IDC state change %s -> Stopping\n", vpoxNetAdpWinIdcStateToText(enmPrevState)));

    switch (enmPrevState)
    {
        case kVPoxNetAdpWinIdcState_Disconnected:
            /* Have not even attempted to connect -- nothing to do. */
            break;
        case kVPoxNetAdpWinIdcState_Stopping:
            /* Impossible, but another thread is alreading doing StopIdc, bail out */
            LogError(("vpoxNetAdpWinUnload: called in 'Stopping' state\n"));
            break;
        case kVPoxNetAdpWinIdcState_Connecting:
            /* the worker thread is running, let's wait for it to stop */
            Status = ObReferenceObjectByHandle(g_VPoxNetAdpGlobals.hInitIdcThread,
                                               THREAD_ALL_ACCESS, NULL, KernelMode,
                                               (PVOID*)&pThread, NULL);
            if (Status == STATUS_SUCCESS)
            {
                KeWaitForSingleObject(pThread, Executive, KernelMode, FALSE, NULL);
                ObDereferenceObject(pThread);
            }
            else
            {
                LogError(("vpoxNetAdpWinStopIdc: ObReferenceObjectByHandle(%p) failed with 0x%x\n",
                     g_VPoxNetAdpGlobals.hInitIdcThread, Status));
            }
            break;
        case kVPoxNetAdpWinIdcState_Connected:
            /* the worker succeeded in IDC init and terminated */
            /* Make sure nobody uses the trunk factory. Wait half a second if needed. */
            if (!NdisWaitEvent(&pGlobals->EventUnloadAllowed, 500))
                LogRel(("VPoxNetAdp: unloading driver while trunk factory is in use!\n"));
            rc = SUPR0IdcComponentDeregisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
            AssertRC(rc);
            SUPR0IdcClose(&pGlobals->SupDrvIDC);
            Log(("vpoxNetAdpWinUnload: closed IDC, rc=0x%x\n", rc));
            break;
    }
    if (pGlobals->hMiniportDriver)
        NdisMDeregisterMiniportDriver(pGlobals->hMiniportDriver);
    NdisFreeSpinLock(&pGlobals->Lock);
    LogFlow(("<==vpoxNetAdpWinUnload\n"));
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
    RTR0Term();
}


/**
 * register the miniport driver
 */
DECLHIDDEN(NDIS_STATUS) vpoxNetAdpWinRegister(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPathStr)
{
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS MChars;

    NdisZeroMemory(&MChars, sizeof (MChars));

    MChars.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    MChars.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
    MChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

    MChars.MajorNdisVersion = VPOXNETADP_VERSION_NDIS_MAJOR;
    MChars.MinorNdisVersion = VPOXNETADP_VERSION_NDIS_MINOR;

    MChars.MajorDriverVersion = VPOXNETADP_VERSION_MAJOR;
    MChars.MinorDriverVersion = VPOXNETADP_VERSION_MINOR;

    MChars.InitializeHandlerEx         = vpoxNetAdpWinInitializeEx;
    MChars.HaltHandlerEx               = vpoxNetAdpWinHaltEx;
    MChars.UnloadHandler               = vpoxNetAdpWinUnload;
    MChars.PauseHandler                = vpoxNetAdpWinPause;
    MChars.RestartHandler              = vpoxNetAdpWinRestart;
    MChars.OidRequestHandler           = vpoxNetAdpWinOidRequest;
    MChars.SendNetBufferListsHandler   = vpoxNetAdpWinSendNetBufferLists;
    MChars.ReturnNetBufferListsHandler = vpoxNetAdpWinReturnNetBufferLists;
    MChars.CancelSendHandler           = vpoxNetAdpWinCancelSend;
    MChars.CheckForHangHandlerEx       = vpoxNetAdpWinCheckForHangEx;
    MChars.ResetHandlerEx              = vpoxNetAdpWinResetEx;
    MChars.DevicePnPEventNotifyHandler = vpoxNetAdpWinDevicePnPEventNotify;
    MChars.ShutdownHandlerEx           = vpoxNetAdpWinShutdownEx;
    MChars.CancelOidRequestHandler     = vpoxNetAdpWinCancelOidRequest;

    NDIS_STATUS Status;
    g_VPoxNetAdpGlobals.hMiniportDriver = NULL;
    Log(("vpoxNetAdpWinRegister: registering miniport driver...\n"));
    Status = NdisMRegisterMiniportDriver(pDriverObject,
                                         pRegistryPathStr,
                                         (NDIS_HANDLE)&g_VPoxNetAdpGlobals,
                                         &MChars,
                                         &g_VPoxNetAdpGlobals.hMiniportDriver);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Log(("vpoxNetAdpWinRegister: successfully registered miniport driver; registering device...\n"));
    }
    else
    {
        Log(("ERROR! vpoxNetAdpWinRegister: failed to register miniport driver, status=0x%x", Status));
    }
    return Status;
}


RT_C_DECLS_BEGIN

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);

RT_C_DECLS_END

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    int rc;


    rc = RTR0Init(0);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        NdisZeroMemory(&g_VPoxNetAdpGlobals, sizeof (g_VPoxNetAdpGlobals));
        RTListInit(&g_VPoxNetAdpGlobals.ListOfAdapters);
        NdisAllocateSpinLock(&g_VPoxNetAdpGlobals.Lock);
        NdisInitializeEvent(&g_VPoxNetAdpGlobals.EventUnloadAllowed);
        //g_VPoxNetAdpGlobals.PMCaps.WakeUpCapabilities.Flags = NDIS_DEVICE_WAKE_UP_ENABLE;
        g_VPoxNetAdpGlobals.PMCaps.WakeUpCapabilities.MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
        g_VPoxNetAdpGlobals.PMCaps.WakeUpCapabilities.MinPatternWakeUp = NdisDeviceStateUnspecified;

        /* Initialize SupDrv interface */
        g_VPoxNetAdpGlobals.SupDrvFactory.pfnQueryFactoryInterface = vpoxNetAdpWinQueryFactoryInterface;
        memcpy(g_VPoxNetAdpGlobals.SupDrvFactory.szName, "VPoxNetAdp", sizeof("VPoxNetAdp"));
        /* Initialize trunk factory interface */
        g_VPoxNetAdpGlobals.TrunkFactory.pfnRelease = vpoxNetAdpWinFactoryRelease;
        g_VPoxNetAdpGlobals.TrunkFactory.pfnCreateAndConnect = vpoxNetAdpWinFactoryCreateAndConnect;

        rc = vpoxNetAdpWinStartInitIdcThread(&g_VPoxNetAdpGlobals);
        if (RT_SUCCESS(rc))
        {
            Status = vpoxNetAdpWinRegister(pDriverObject, pRegistryPath);
            Assert(Status == STATUS_SUCCESS);
            if (Status == NDIS_STATUS_SUCCESS)
            {
                Log(("NETADP: started successfully\n"));
                return STATUS_SUCCESS;
            }
        }
        else
            Status = NDIS_STATUS_FAILURE;
        NdisFreeSpinLock(&g_VPoxNetAdpGlobals.Lock);
        RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
        RTLogDestroy(RTLogSetDefaultInstance(NULL));

        RTR0Term();
    }
    else
    {
        Status = NDIS_STATUS_FAILURE;
    }

    return Status;
}

