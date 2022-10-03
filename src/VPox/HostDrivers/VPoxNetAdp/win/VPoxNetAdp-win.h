/* $Id: VPoxNetAdp-win.h $ */
/** @file
 * VPoxNetAdp-win.h - Host-only Miniport Driver, Windows-specific code.
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetAdp_win_VPoxNetAdp_win_h
#define VPOX_INCLUDED_SRC_VPoxNetAdp_win_VPoxNetAdp_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOXNETADP_VERSION_NDIS_MAJOR        6
#define VPOXNETADP_VERSION_NDIS_MINOR        0

#define VPOXNETADP_VERSION_MAJOR             1
#define VPOXNETADP_VERSION_MINOR             0

#define VPOXNETADP_VENDOR_NAME               "Oracle"
#define VPOXNETADP_VENDOR_ID                 0xFFFFFF
#define VPOXNETADP_MCAST_LIST_SIZE           32
#define VPOXNETADP_MAX_FRAME_SIZE            1518 // TODO: 14+4+1500

//#define VPOXNETADP_NAME_UNIQUE               L"{7af6b074-048d-4444-bfce-1ecc8bc5cb76}"
#define VPOXNETADP_NAME_SERVICE              L"VPoxNetAdp"

#define VPOXNETADP_NAME_LINK                 L"\\DosDevices\\Global\\VPoxNetAdp"
#define VPOXNETADP_NAME_DEVICE               L"\\Device\\VPoxNetAdp"

#define VPOXNETADPWIN_TAG                    'ANBV'

#define VPOXNETADPWIN_ATTR_FLAGS             NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM | NDIS_MINIPORT_ATTRIBUTES_NO_HALT_ON_SUSPEND
#define VPOXNETADP_MAC_OPTIONS               NDIS_MAC_OPTION_NO_LOOPBACK
#define VPOXNETADP_SUPPORTED_FILTERS         (NDIS_PACKET_TYPE_DIRECTED | \
                                              NDIS_PACKET_TYPE_MULTICAST | \
                                              NDIS_PACKET_TYPE_BROADCAST | \
                                              NDIS_PACKET_TYPE_PROMISCUOUS | \
                                              NDIS_PACKET_TYPE_ALL_MULTICAST)
#define VPOXNETADPWIN_SUPPORTED_STATISTICS   0 //TODO!
#define VPOXNETADPWIN_HANG_CHECK_TIME        4

#endif /* !VPOX_INCLUDED_SRC_VPoxNetAdp_win_VPoxNetAdp_win_h */
