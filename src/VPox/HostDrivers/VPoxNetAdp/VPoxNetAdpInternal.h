/* $Id: VPoxNetAdpInternal.h $ */
/** @file
 * VPoxNetAdp - Network Filter Driver (Host), Internal Header.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
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

#ifndef VPOX_INCLUDED_SRC_VPoxNetAdp_VPoxNetAdpInternal_h
#define VPOX_INCLUDED_SRC_VPoxNetAdp_VPoxNetAdpInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/sup.h>
#include <VPox/intnet.h>
#include <iprt/semaphore.h>
#include <iprt/assert.h>


RT_C_DECLS_BEGIN

/** Pointer to the globals. */
typedef struct VPOXNETADPGLOBALS *PVPOXNETADPGLOBALS;

#define VPOXNETADP_MAX_INSTANCES   128
#define VPOXNETADP_MAX_UNITS       128
#define VPOXNETADP_NAME            "vpoxnet"
#define VPOXNETADP_MAX_NAME_LEN    32
#define VPOXNETADP_MTU             1500
#if defined(RT_OS_DARWIN)
# define VPOXNETADP_MAX_FAMILIES   4
# define VPOXNETADP_DETACH_TIMEOUT 500
#endif

#define VPOXNETADP_CTL_DEV_NAME    "vpoxnetctl"
#define VPOXNETADP_CTL_ADD   _IOWR('v', 1, VPOXNETADPREQ)
#define VPOXNETADP_CTL_REMOVE _IOW('v', 2, VPOXNETADPREQ)

typedef struct VPoxNetAdpReq
{
    char szName[VPOXNETADP_MAX_NAME_LEN];
} VPOXNETADPREQ;
typedef VPOXNETADPREQ *PVPOXNETADPREQ;

/**
 * Void entries mark vacant slots in adapter array. Valid entries are busy slots.
 * As soon as slot is being modified its state changes to transitional.
 * An entry in transitional state must only be accessed by the thread that
 * put it to this state.
 */
/**
 * To avoid races on adapter fields we stick to the following rules:
 * - rewrite: Int net port calls are serialized
 * - No modifications are allowed on busy adapters (deactivate first)
 *     Refuse to destroy adapter until it gets to available state
 * - No transfers (thus getting busy) on inactive adapters
 * - Init sequence: void->available->connected->active
     1) Create
     2) Connect
     3) Activate
 * - Destruction sequence: active->connected->available->void
     1) Deactivate
     2) Disconnect
     3) Destroy
*/

enum VPoxNetAdpState
{
    kVPoxNetAdpState_Invalid,
    kVPoxNetAdpState_Transitional,
    kVPoxNetAdpState_Active,
    kVPoxNetAdpState_32BitHack = 0x7FFFFFFF
};
typedef enum VPoxNetAdpState VPOXNETADPSTATE;

struct VPoxNetAdapter
{
    /** Denotes availability of this slot in adapter array. */
    VPOXNETADPSTATE   enmState;
    /** Corresponds to the digit at the end of device name. */
    int               iUnit;

    union
    {
#ifdef VPOXNETADP_OS_SPECFIC
        struct
        {
# if defined(RT_OS_DARWIN)
            /** @name Darwin instance data.
             * @{ */
            /** Event to signal detachment of interface. */
            RTSEMEVENT        hEvtDetached;
            /** Pointer to Darwin interface structure. */
            ifnet_t           pIface;
            /** MAC address. */
            RTMAC             Mac;
            /** @} */
# elif defined(RT_OS_LINUX)
            /** @name Darwin instance data.
             * @{ */
            /** Pointer to Linux network device structure. */
            struct net_device *pNetDev;
            /** @} */
# elif defined(RT_OS_FREEBSD)
            /** @name FreeBSD instance data.
             * @{ */
            struct ifnet *ifp;
            /** @} */
# else
# error PORTME
# endif
        } s;
#endif
        /** Union alignment to a pointer. */
        void *pvAlign;
        /** Padding. */
        uint8_t abPadding[64];
    } u;
    /** The interface name. */
    char szName[VPOXNETADP_MAX_NAME_LEN];
};
typedef struct VPoxNetAdapter VPOXNETADP;
typedef VPOXNETADP *PVPOXNETADP;
/* Paranoia checks for alignment and padding. */
AssertCompileMemberAlignment(VPOXNETADP, u, ARCH_BITS/8);
AssertCompileMemberAlignment(VPOXNETADP, szName, ARCH_BITS/8);
AssertCompileMembersSameSize(VPOXNETADP, u, VPOXNETADP, u.abPadding);

DECLHIDDEN(int) vpoxNetAdpInit(void);
DECLHIDDEN(void) vpoxNetAdpShutdown(void);
DECLHIDDEN(int) vpoxNetAdpCreate(PVPOXNETADP *ppNew, const char *pcszName);
DECLHIDDEN(int) vpoxNetAdpDestroy(PVPOXNETADP pThis);
DECLHIDDEN(PVPOXNETADP) vpoxNetAdpFindByName(const char *pszName);
DECLHIDDEN(void) vpoxNetAdpComposeMACAddress(PVPOXNETADP pThis, PRTMAC pMac);


/**
 * This is called to perform OS-specific structure initializations.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks Owns no locks.
 */
DECLHIDDEN(int) vpoxNetAdpOsInit(PVPOXNETADP pThis);

/**
 * Counter part to vpoxNetAdpOsCreate().
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks May own the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(void) vpoxNetAdpOsDestroy(PVPOXNETADP pThis);

/**
 * This is called to attach to the actual host interface
 * after linking the instance into the list.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 * @param   pMac            The MAC address to use for this instance.
 *
 * @remarks Owns no locks.
 */
DECLHIDDEN(int) vpoxNetAdpOsCreate(PVPOXNETADP pThis, PCRTMAC pMac);



RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_SRC_VPoxNetAdp_VPoxNetAdpInternal_h */

