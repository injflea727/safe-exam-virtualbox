/* $Id: VPoxNetAdp.c $ */
/** @file
 * VPoxNetAdp - Virtual Network Adapter Driver (Host), Common Code.
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

/** @page pg_netadp     VPoxNetAdp - Network Adapter
 *
 * This is a kernel module that creates a virtual interface that can be attached
 * to an internal network.
 *
 * In the big picture we're one of the three trunk interface on the internal
 * network, the one named "TAP Interface": @image html Networking_Overview.gif
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NET_ADP_DRV
#include "VPoxNetAdpInternal.h"

#include <VPox/log.h>
#include <VPox/err.h>
#include <iprt/string.h>


VPOXNETADP g_aAdapters[VPOXNETADP_MAX_INSTANCES];
static uint8_t g_aUnits[VPOXNETADP_MAX_UNITS/8];


DECLINLINE(int) vpoxNetAdpGetUnitByName(const char *pcszName)
{
    uint32_t iUnit = RTStrToUInt32(pcszName + sizeof(VPOXNETADP_NAME) - 1);
    bool fOld;

    if (iUnit >= VPOXNETADP_MAX_UNITS)
        return -1;

    fOld = ASMAtomicBitTestAndSet(g_aUnits, iUnit);
    return fOld ? -1 : (int)iUnit;
}

DECLINLINE(int) vpoxNetAdpGetNextAvailableUnit(void)
{
    bool fOld;
    int iUnit;
    /* There is absolutely no chance that all units are taken */
    do {
        iUnit = ASMBitFirstClear(g_aUnits, VPOXNETADP_MAX_UNITS);
        if (iUnit < 0)
            break;
        fOld = ASMAtomicBitTestAndSet(g_aUnits, iUnit);
    } while (fOld);

    return iUnit;
}

DECLINLINE(void) vpoxNetAdpReleaseUnit(int iUnit)
{
    bool fSet = ASMAtomicBitTestAndClear(g_aUnits, iUnit);
    NOREF(fSet);
    Assert(fSet);
}

/**
 * Generate a suitable MAC address.
 *
 * @param   pThis       The instance.
 * @param   pMac        Where to return the MAC address.
 */
DECLHIDDEN(void) vpoxNetAdpComposeMACAddress(PVPOXNETADP pThis, PRTMAC pMac)
{
    /* Use a locally administered version of the OUI we use for the guest NICs. */
    pMac->au8[0] = 0x08 | 2;
    pMac->au8[1] = 0x00;
    pMac->au8[2] = 0x27;

    pMac->au8[3] = 0; /* pThis->iUnit >> 16; */
    pMac->au8[4] = 0; /* pThis->iUnit >> 8; */
    pMac->au8[5] = pThis->iUnit;
}

int vpoxNetAdpCreate(PVPOXNETADP *ppNew, const char *pcszName)
{
    int rc;
    unsigned i;
    for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
    {
        PVPOXNETADP pThis = &g_aAdapters[i];

        if (ASMAtomicCmpXchgU32((uint32_t volatile *)&pThis->enmState, kVPoxNetAdpState_Transitional, kVPoxNetAdpState_Invalid))
        {
            RTMAC Mac;
            /* Found an empty slot -- use it. */
            Log(("vpoxNetAdpCreate: found empty slot: %d\n", i));
            if (pcszName)
            {
                Log(("vpoxNetAdpCreate: using name: %s\n", pcszName));
                pThis->iUnit = vpoxNetAdpGetUnitByName(pcszName);
                strncpy(pThis->szName, pcszName, sizeof(pThis->szName) - 1);
                pThis->szName[sizeof(pThis->szName) - 1] = '\0';
            }
            else
            {
                pThis->iUnit = vpoxNetAdpGetNextAvailableUnit();
                pThis->szName[0] = '\0';
            }
            if (pThis->iUnit < 0)
                rc = VERR_INVALID_PARAMETER;
            else
            {
                vpoxNetAdpComposeMACAddress(pThis, &Mac);
                rc = vpoxNetAdpOsCreate(pThis, &Mac);
                Log(("vpoxNetAdpCreate: pThis=%p pThis->iUnit=%d, pThis->szName=%s\n",
                     pThis, pThis->iUnit, pThis->szName));
            }
            if (RT_SUCCESS(rc))
            {
                *ppNew = pThis;
                ASMAtomicWriteU32((uint32_t volatile *)&pThis->enmState, kVPoxNetAdpState_Active);
                Log2(("VPoxNetAdpCreate: Created %s\n", g_aAdapters[i].szName));
            }
            else
            {
                ASMAtomicWriteU32((uint32_t volatile *)&pThis->enmState, kVPoxNetAdpState_Invalid);
                Log(("vpoxNetAdpCreate: vpoxNetAdpOsCreate failed with '%Rrc'.\n", rc));
            }
            for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
                Log2(("VPoxNetAdpCreate: Scanning entry: state=%d unit=%d name=%s\n",
                      g_aAdapters[i].enmState, g_aAdapters[i].iUnit, g_aAdapters[i].szName));
            return rc;
        }
    }
    Log(("vpoxNetAdpCreate: no empty slots!\n"));

    /* All slots in adapter array are busy. */
    return VERR_OUT_OF_RESOURCES;
}

int vpoxNetAdpDestroy(PVPOXNETADP pThis)
{
    int rc = VINF_SUCCESS;

    if (!ASMAtomicCmpXchgU32((uint32_t volatile *)&pThis->enmState, kVPoxNetAdpState_Transitional, kVPoxNetAdpState_Active))
        return VERR_INTNET_FLT_IF_BUSY;

    Assert(pThis->iUnit >= 0 && pThis->iUnit < VPOXNETADP_MAX_UNITS);
    vpoxNetAdpOsDestroy(pThis);
    vpoxNetAdpReleaseUnit(pThis->iUnit);
    pThis->iUnit = -1;
    pThis->szName[0] = '\0';

    ASMAtomicWriteU32((uint32_t volatile *)&pThis->enmState, kVPoxNetAdpState_Invalid);

    return rc;
}

int  vpoxNetAdpInit(void)
{
    unsigned i;
    /*
     * Init common members and call OS-specific init.
     */
    memset(g_aUnits, 0, sizeof(g_aUnits));
    memset(g_aAdapters, 0, sizeof(g_aAdapters));
    LogFlow(("vpoxnetadp: max host-only interfaces supported: %d (%d bytes)\n",
             VPOXNETADP_MAX_INSTANCES, sizeof(g_aAdapters)));
    for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
    {
        g_aAdapters[i].enmState = kVPoxNetAdpState_Invalid;
        g_aAdapters[i].iUnit    = -1;
        vpoxNetAdpOsInit(&g_aAdapters[i]);
    }

    return VINF_SUCCESS;
}

/**
 * Finds an adapter by its name.
 *
 * @returns Pointer to the instance by the given name. NULL if not found.
 * @param   pszName         The name of the instance.
 */
PVPOXNETADP vpoxNetAdpFindByName(const char *pszName)
{
    unsigned i;

    for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
    {
        PVPOXNETADP pThis = &g_aAdapters[i];
        Log2(("VPoxNetAdp: Scanning entry: state=%d name=%s\n", pThis->enmState, pThis->szName));
        if (   strcmp(pThis->szName, pszName) == 0
            && ASMAtomicReadU32((uint32_t volatile *)&pThis->enmState) == kVPoxNetAdpState_Active)
            return pThis;
    }
    return NULL;
}

void vpoxNetAdpShutdown(void)
{
    unsigned i;

    /* Remove virtual adapters */
    for (i = 0; i < RT_ELEMENTS(g_aAdapters); i++)
        vpoxNetAdpDestroy(&g_aAdapters[i]);
}
