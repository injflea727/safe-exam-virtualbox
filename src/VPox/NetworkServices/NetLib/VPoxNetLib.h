/* $Id: VPoxNetLib.h $ */
/** @file
 * VPoxNetUDP - IntNet Client Library.
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
 */

#ifndef VPOX_INCLUDED_SRC_NetLib_VPoxNetLib_h
#define VPOX_INCLUDED_SRC_NetLib_VPoxNetLib_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/net.h>
#include <VPox/intnet.h>

RT_C_DECLS_BEGIN


/**
 * Header pointers optionally returned by VPoxNetUDPMatch.
 */
typedef struct VPOXNETUDPHDRS
{
    PCRTNETETHERHDR     pEth;           /**< Pointer to the ethernet header. */
    PCRTNETIPV4         pIpv4;          /**< Pointer to the IPV4 header if IPV4 packet. */
    PCRTNETUDP          pUdp;           /**< Pointer to the UDP header. */
} VPOXNETUDPHDRS;
/** Pointer to a VPOXNETUDPHDRS structure. */
typedef VPOXNETUDPHDRS *PVPOXNETUDPHDRS;


/** @name VPoxNetUDPMatch flags.
 * @{ */
#define VPOXNETUDP_MATCH_UNICAST            RT_BIT_32(0)
#define VPOXNETUDP_MATCH_BROADCAST          RT_BIT_32(1)
#define VPOXNETUDP_MATCH_CHECKSUM           RT_BIT_32(2)
#define VPOXNETUDP_MATCH_REQUIRE_CHECKSUM   RT_BIT_32(3)
#define VPOXNETUDP_MATCH_PRINT_STDERR       RT_BIT_32(31)
/** @}  */

void *  VPoxNetUDPMatch(PINTNETBUF pBuf, unsigned uDstPort, PCRTMAC pDstMac, uint32_t fFlags, PVPOXNETUDPHDRS pHdrs, size_t *pcb);
int     VPoxNetUDPUnicast(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf,
                          RTNETADDRIPV4 SrcIPv4Addr, PCRTMAC SrcMacAddr, unsigned uSrcPort,
                          RTNETADDRIPV4 DstIPv4Addr, PCRTMAC DstMacAddr, unsigned uDstPort,
                          void const *pvData, size_t cbData);
int     VPoxNetUDPBroadcast(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf,
                            RTNETADDRIPV4 SrcIPv4Addr, PCRTMAC SrcMacAddr, unsigned uSrcPort,
                            unsigned uDstPort,
                            void const *pvData, size_t cbData);

bool    VPoxNetArpHandleIt(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf, PCRTMAC pMacAddr, RTNETADDRIPV4 IPv4Addr);

int     VPoxNetIntIfFlush(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf);
int     VPoxNetIntIfRingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, size_t cSegs, PCINTNETSEG paSegs);
int     VPoxNetIntIfSend(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf, size_t cSegs, PCINTNETSEG paSegs, bool fFlush);


RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_SRC_NetLib_VPoxNetLib_h */

