/* $Id: VPoxNetLwf-win.h $ */
/** @file
 * VPoxNetLwf-win.h - Bridged Networking Driver, Windows-specific code.
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetLwf_win_h
#define VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetLwf_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOXNETLWF_VERSION_NDIS_MAJOR        6
#define VPOXNETLWF_VERSION_NDIS_MINOR        0

#define VPOXNETLWF_NAME_FRIENDLY             L"VirtualPox NDIS Light-Weight Filter"
#define VPOXNETLWF_NAME_UNIQUE               L"{7af6b074-048d-4444-bfce-1ecc8bc5cb76}"
#define VPOXNETLWF_NAME_SERVICE              L"VPoxNetLwf"

#define VPOXNETLWF_NAME_LINK                 L"\\DosDevices\\Global\\VPoxNetLwf"
#define VPOXNETLWF_NAME_DEVICE               L"\\Device\\VPoxNetLwf"

#define VPOXNETLWF_MEM_TAG                   'FLBV'
#define VPOXNETLWF_REQ_ID                    'fLBV'

#endif /* !VPOX_INCLUDED_SRC_VPoxNetFlt_win_drv_VPoxNetLwf_win_h */
