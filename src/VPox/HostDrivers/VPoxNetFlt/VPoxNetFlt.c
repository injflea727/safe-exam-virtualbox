/* $Id: VPoxNetFlt.c $ */
/** @file
 * VPoxNetFlt - Network Filter Driver (Host), Common Code.
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

/** @page pg_netflt     VPoxNetFlt - Network Interface Filter
 *
 * This is a kernel module that attaches to a real interface on the host and
 * filters and injects packets.
 *
 * In the big picture we're one of the three trunk interface on the internal
 * network, the one named "NIC Filter Driver": @image html Networking_Overview.gif
 *
 *
 * @section  sec_netflt_locking     Locking and Potential Races
 *
 * The main challenge here is to make sure the netfilter and internal network
 * instances won't be destroyed while someone is calling into them.
 *
 * The main calls into or out of of the filter driver are:
 *      - Send.
 *      - Async send completion (not implemented yet)
 *      - Release by the internal network.
 *      - Receive.
 *      - Disappearance of the host networking interface.
 *      - Reappearance of the host networking interface.
 *
 * The latter two calls are can be caused by driver unloading/loading or the
 * device being physical unplugged (e.g. a USB network device).  Actually, the
 * unload scenario must fervently be prevent as it will cause panics because the
 * internal network will assume the trunk is around until it releases it.
 * @todo Need to figure which host allow unloading and block/fix it.
 *
 * Currently the netfilter instance lives until the internal network releases
 * it. So, it is the internal networks responsibility to make sure there are no
 * active calls when it releases the trunk and destroys the network.  The
 * netfilter assists in this by providing INTNETTRUNKIFPORT::pfnSetState and
 * INTNETTRUNKIFPORT::pfnWaitForIdle.  The trunk state is used to enable/disable
 * promiscuous mode on the hardware NIC (or similar activation) as well
 * indicating that disconnect is imminent and no further calls shall be made
 * into the internal network.  After changing the state to disconnecting and
 * prior to invoking INTNETTRUNKIFPORT::pfnDisconnectAndRelease, the internal
 * network will use INTNETTRUNKIFPORT::pfnWaitForIdle to wait for any still
 * active calls to complete.
 *
 * The netfilter employs a busy counter and an internal state in addition to the
 * public trunk state.  All these variables are protected using a spinlock.
 *
 *
 * @section  sec_netflt_msc     Locking / Sequence Diagrams - OBSOLETE
 *
 * !OBSOLETE! - THIS WAS THE OLD APPROACH!
 *
 * This secion contains a few sequence diagrams describing the problematic
 * transitions of a host interface filter instance.
 *
 * The thing that makes it all a bit problematic is that multiple events may
 * happen at the same time, and that we have to be very careful to avoid
 * deadlocks caused by mixing our locks with the ones in the host kernel. The
 * main events are receive, send, async send completion, disappearance of the
 * host networking interface and its reappearance.  The latter two events are
 * can be caused by driver unloading/loading or the device being physical
 * unplugged (e.g. a USB network device).
 *
 * The strategy for dealing with these issues are:
 *    - Use a simple state machine.
 *    - Require the user (IntNet) to serialize all its calls to us,
 *      while at the same time not owning any lock used by any of the
 *      the callbacks we might call on receive and async send completion.
 *    - Make sure we're 100% idle before disconnecting, and have a
 *      disconnected status on both sides to fend off async calls.
 *    - Protect the host specific interface handle and the state variables
 *      using a spinlock.
 *
 *
 * @subsection subsec_netflt_msc_dis_rel    Disconnect from the network and release - OBSOLETE
 *
 * @msc
 *      VM, IntNet, NetFlt, Kernel, Wire;
 *
 *      VM->IntNet      [label="pkt0", linecolor="green", textcolor="green"];
 *      IntNet=>IntNet  [label="Lock Network", linecolor="green", textcolor="green" ];
 *      IntNet=>IntNet  [label="Route packet -> wire", linecolor="green", textcolor="green" ];
 *      IntNet=>IntNet  [label="Unlock Network", linecolor="green", textcolor="green" ];
 *      IntNet=>NetFlt  [label="pkt0 to wire", linecolor="green", textcolor="green" ];
 *      NetFlt=>Kernel  [label="pkt0 to wire", linecolor="green", textcolor="green"];
 *      Kernel->Wire    [label="pkt0 to wire", linecolor="green", textcolor="green"];
 *
 *      ---             [label="Suspending the trunk interface"];
 *      IntNet=>IntNet  [label="Lock Network"];
 *
 *      Wire->Kernel    [label="pkt1 - racing us", linecolor="red", textcolor="red"];
 *      Kernel=>>NetFlt [label="pkt1 - racing us", linecolor="red", textcolor="red"];
 *      NetFlt=>>IntNet [label="pkt1 recv - blocks", linecolor="red", textcolor="red"];
 *
 *      IntNet=>IntNet  [label="Mark Trunk Suspended"];
 *      IntNet=>IntNet  [label="Unlock Network"];
 *
 *      IntNet=>NetFlt  [label="pfnSetActive(false)"];
 *      NetFlt=>NetFlt  [label="Mark inactive (atomic)"];
 *      IntNet<<NetFlt;
 *      IntNet=>NetFlt  [label="pfnWaitForIdle(forever)"];
 *
 *      IntNet=>>NetFlt [label="pkt1 to host", linecolor="red", textcolor="red"];
 *      NetFlt=>>Kernel [label="pkt1 to host", linecolor="red", textcolor="red"];
 *
 *      Kernel<-Wire    [label="pkt0 on wire", linecolor="green", textcolor="green"];
 *      NetFlt<<Kernel  [label="pkt0 on wire", linecolor="green", textcolor="green"];
 *      IntNet<<=NetFlt [label="pfnSGRelease", linecolor="green", textcolor="green"];
 *      IntNet<<=IntNet [label="Lock Net, free SG, Unlock Net", linecolor="green", textcolor="green"];
 *      IntNet>>NetFlt  [label="pfnSGRelease", linecolor="green", textcolor="green"];
 *      NetFlt<-NetFlt  [label="idle", linecolor="green", textcolor="green"];
 *
 *      IntNet<<NetFlt  [label="idle (pfnWaitForIdle)"];
 *
 *      Wire->Kernel    [label="pkt2", linecolor="red", textcolor="red"];
 *      Kernel=>>NetFlt [label="pkt2", linecolor="red", textcolor="red"];
 *      NetFlt=>>Kernel [label="pkt2 to host", linecolor="red", textcolor="red"];
 *
 *      VM->IntNet      [label="pkt3", linecolor="green", textcolor="green"];
 *      IntNet=>IntNet  [label="Lock Network", linecolor="green", textcolor="green" ];
 *      IntNet=>IntNet  [label="Route packet -> drop", linecolor="green", textcolor="green" ];
 *      IntNet=>IntNet  [label="Unlock Network", linecolor="green", textcolor="green" ];
 *
 *      ---             [label="The trunk interface is idle now, disconnect it"];
 *      IntNet=>IntNet  [label="Lock Network"];
 *      IntNet=>IntNet  [label="Unlink Trunk"];
 *      IntNet=>IntNet  [label="Unlock Network"];
 *      IntNet=>NetFlt  [label="pfnDisconnectAndRelease"];
 *      NetFlt=>Kernel  [label="iflt_detach"];
 *      NetFlt<<=Kernel [label="iff_detached"];
 *      NetFlt>>Kernel  [label="iff_detached"];
 *      NetFlt<<Kernel  [label="iflt_detach"];
 *      NetFlt=>NetFlt  [label="Release"];
 *      IntNet<<NetFlt  [label="pfnDisconnectAndRelease"];
 *
 * @endmsc
 *
 *
 *
 * @subsection subsec_netflt_msc_hif_rm    Host Interface Removal - OBSOLETE
 *
 * The ifnet_t (pIf) is a tricky customer as any reference to it can potentially
 * race the filter detaching. The simple way of solving it on Darwin is to guard
 * all access to the pIf member with a spinlock. The other host systems will
 * probably have similar race conditions, so the spinlock is a generic thing.
 *
 * @msc
 *      VM, IntNet, NetFlt, Kernel;
 *
 *      VM->IntNet      [label="pkt0", linecolor="green", textcolor="green"];
 *      IntNet=>IntNet  [label="Lock Network", linecolor="green", textcolor="green" ];
 *      IntNet=>IntNet  [label="Route packet -> wire", linecolor="green", textcolor="green" ];
 *      IntNet=>IntNet  [label="Unlock Network", linecolor="green", textcolor="green" ];
 *      IntNet=>NetFlt  [label="pkt0 to wire", linecolor="green", textcolor="green" ];
 *      NetFlt=>Kernel  [label="ifnet_reference w/ spinlock", linecolor="green", textcolor="green" ];
 *      NetFlt<<Kernel  [label="ifnet_reference", linecolor="green", textcolor="green" ];
 *      NetFlt=>Kernel  [label="pkt0 to wire (blocks)", linecolor="green", textcolor="green" ];
 *
 *      ---             [label="The host interface is being disconnected"];
 *      Kernel->NetFlt  [label="iff_detached"];
 *      NetFlt=>Kernel  [label="ifnet_release w/ spinlock"];
 *      NetFlt<<Kernel  [label="ifnet_release"];
 *      NetFlt=>NetFlt  [label="fDisconnectedFromHost=true"];
 *      NetFlt>>Kernel  [label="iff_detached"];
 *
 *      NetFlt<<Kernel  [label="dropped", linecolor="green", textcolor="green"];
 *      NetFlt=>NetFlt  [label="Acquire spinlock", linecolor="green", textcolor="green"];
 *      NetFlt=>Kernel  [label="ifnet_release", linecolor="green", textcolor="green"];
 *      NetFlt<<Kernel  [label="ifnet_release", linecolor="green", textcolor="green"];
 *      NetFlt=>NetFlt  [label="pIf=NULL", linecolor="green", textcolor="green"];
 *      NetFlt=>NetFlt  [label="Release spinlock", linecolor="green", textcolor="green"];
 *      IntNet<=NetFlt  [label="pfnSGRelease", linecolor="green", textcolor="green"];
 *      IntNet>>NetFlt  [label="pfnSGRelease", linecolor="green", textcolor="green"];
 *      IntNet<<NetFlt  [label="pkt0 to wire", linecolor="green", textcolor="green"];
 *
 * @endmsc
 *
 *
 *
 * @subsection subsec_netflt_msc_hif_rd    Host Interface Rediscovery - OBSOLETE
 *
 * The rediscovery is performed when we receive a send request and a certain
 * period have elapsed since the last attempt, i.e. we're polling it. We
 * synchronize the rediscovery with disconnection from the internal network
 * by means of the pfnWaitForIdle call, so no special handling is required.
 *
 * @msc
 *      VM2, VM1, IntNet, NetFlt, Kernel, Wire;
 *
 *      ---             [label="Rediscovery conditions are not met"];
 *      VM1->IntNet     [label="pkt0"];
 *      IntNet=>IntNet  [label="Lock Network"];
 *      IntNet=>IntNet  [label="Route packet -> wire"];
 *      IntNet=>IntNet  [label="Unlock Network"];
 *      IntNet=>NetFlt  [label="pkt0 to wire"];
 *      NetFlt=>NetFlt  [label="Read pIf(==NULL) w/ spinlock"];
 *      IntNet<<NetFlt  [label="pkt0 to wire (dropped)"];
 *
 *      ---             [label="Rediscovery conditions"];
 *      VM1->IntNet     [label="pkt1"];
 *      IntNet=>IntNet  [label="Lock Network"];
 *      IntNet=>IntNet  [label="Route packet -> wire"];
 *      IntNet=>IntNet  [label="Unlock Network"];
 *      IntNet=>NetFlt  [label="pkt1 to wire"];
 *      NetFlt=>NetFlt  [label="Read pIf(==NULL) w/ spinlock"];
 *      NetFlt=>NetFlt  [label="fRediscoveryPending=true w/ spinlock"];
 *      NetFlt=>Kernel  [label="ifnet_find_by_name"];
 *      NetFlt<<Kernel  [label="ifnet_find_by_name (success)"];
 *
 *      VM2->IntNet     [label="pkt2", linecolor="red", textcolor="red"];
 *      IntNet=>IntNet  [label="Lock Network", linecolor="red", textcolor="red"];
 *      IntNet=>IntNet  [label="Route packet -> wire", linecolor="red", textcolor="red"];
 *      IntNet=>IntNet  [label="Unlock Network", linecolor="red", textcolor="red"];
 *      IntNet=>NetFlt  [label="pkt2 to wire", linecolor="red", textcolor="red"];
 *      NetFlt=>NetFlt  [label="!pIf || fRediscoveryPending (w/ spinlock)", linecolor="red", textcolor="red"];
 *      IntNet<<NetFlt  [label="pkt2 to wire (dropped)", linecolor="red", textcolor="red"];

 *      NetFlt=>Kernel  [label="iflt_attach"];
 *      NetFlt<<Kernel  [label="iflt_attach (success)"];
 *      NetFlt=>NetFlt  [label="Acquire spinlock"];
 *      NetFlt=>NetFlt  [label="Set pIf and update flags"];
 *      NetFlt=>NetFlt  [label="Release spinlock"];
 *
 *      NetFlt=>Kernel  [label="pkt1 to wire"];
 *      Kernel->Wire    [label="pkt1 to wire"];
 *      NetFlt<<Kernel  [label="pkt1 to wire"];
 *      IntNet<<NetFlt  [label="pkt1 to wire"];
 *
 *
 * @endmsc
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NET_FLT_DRV
#include "VPoxNetFltInternal.h"

#include <VPox/sup.h>
#include <VPox/log.h>
#include <VPox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/spinlock.h>
#include <iprt/uuid.h>
#include <iprt/mem.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define IFPORT_2_VPOXNETFLTINS(pIfPort) \
    ( (PVPOXNETFLTINS)((uint8_t *)(pIfPort) - RT_UOFFSETOF(VPOXNETFLTINS, MyPort)) )


AssertCompileMemberSize(VPOXNETFLTINS, enmState, sizeof(uint32_t));

/**
 * Sets the enmState member atomically.
 *
 * Used for all updates.
 *
 * @param   pThis           The instance.
 * @param   enmNewState     The new value.
 */
DECLINLINE(void) vpoxNetFltSetState(PVPOXNETFLTINS pThis, VPOXNETFTLINSSTATE enmNewState)
{
    ASMAtomicWriteU32((uint32_t volatile *)&pThis->enmState, enmNewState);
}


/**
 * Gets the enmState member atomically.
 *
 * Used for all reads.
 *
 * @returns The enmState value.
 * @param   pThis           The instance.
 */
DECLINLINE(VPOXNETFTLINSSTATE) vpoxNetFltGetState(PVPOXNETFLTINS pThis)
{
    return (VPOXNETFTLINSSTATE)ASMAtomicUoReadU32((uint32_t volatile *)&pThis->enmState);
}


/**
 * Finds a instance by its name, the caller does the locking.
 *
 * @returns Pointer to the instance by the given name. NULL if not found.
 * @param   pGlobals        The globals.
 * @param   pszName         The name of the instance.
 */
static PVPOXNETFLTINS vpoxNetFltFindInstanceLocked(PVPOXNETFLTGLOBALS pGlobals, const char *pszName)
{
    PVPOXNETFLTINS pCur;
    for (pCur = pGlobals->pInstanceHead; pCur; pCur = pCur->pNext)
        if (!strcmp(pszName, pCur->szName))
            return pCur;
    return NULL;
}


/**
 * Finds a instance by its name, will request the mutex.
 *
 * No reference to the instance is retained, we're assuming the caller to
 * already have one but just for some reason doesn't have the pointer to it.
 *
 * @returns Pointer to the instance by the given name. NULL if not found.
 * @param   pGlobals        The globals.
 * @param   pszName         The name of the instance.
 */
DECLHIDDEN(PVPOXNETFLTINS) vpoxNetFltFindInstance(PVPOXNETFLTGLOBALS pGlobals, const char *pszName)
{
    PVPOXNETFLTINS pRet;
    int rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    AssertRCReturn(rc, NULL);

    pRet = vpoxNetFltFindInstanceLocked(pGlobals, pszName);

    rc = RTSemFastMutexRelease(pGlobals->hFastMtx);
    AssertRC(rc);
    return pRet;
}


/**
 * Unlinks an instance from the chain.
 *
 * @param   pGlobals        The globals.
 * @param   pToUnlink       The instance to unlink.
 */
static void vpoxNetFltUnlinkLocked(PVPOXNETFLTGLOBALS pGlobals, PVPOXNETFLTINS pToUnlink)
{
    if (pGlobals->pInstanceHead == pToUnlink)
        pGlobals->pInstanceHead = pToUnlink->pNext;
    else
    {
        PVPOXNETFLTINS pCur;
        for (pCur = pGlobals->pInstanceHead; pCur; pCur = pCur->pNext)
            if (pCur->pNext == pToUnlink)
            {
                pCur->pNext = pToUnlink->pNext;
                break;
            }
        Assert(pCur);
    }
    pToUnlink->pNext = NULL;
}


/**
 * Performs interface rediscovery if it was disconnected from the host.
 *
 * @returns true if successfully rediscovered and connected, false if not.
 * @param   pThis           The instance.
 */
static bool vpoxNetFltMaybeRediscovered(PVPOXNETFLTINS pThis)
{
    uint64_t        Now;
    bool            fRediscovered;
    bool            fDoIt;

    /*
     * Don't do rediscovery if we're called with preemption disabled.
     *
     * Note! This may cause trouble if we're always called with preemption
     *       disabled and vpoxNetFltOsMaybeRediscovered actually does some real
     *       work.  For the time being though, only Darwin and FreeBSD depends
     *       on these call outs and neither supports sending with preemption
     *       disabled.
     */
    if (!RTThreadPreemptIsEnabled(NIL_RTTHREAD))
        return false;

    /*
     * Rediscovered already? Time to try again?
     */
    Now = RTTimeNanoTS();
    RTSpinlockAcquire(pThis->hSpinlock);

    fRediscovered = !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
    fDoIt = !fRediscovered
         && !ASMAtomicUoReadBool(&pThis->fRediscoveryPending)
         && Now - ASMAtomicUoReadU64(&pThis->NanoTSLastRediscovery) > UINT64_C(5000000000); /* 5 sec */
    if (fDoIt)
        ASMAtomicWriteBool(&pThis->fRediscoveryPending, true);

    RTSpinlockRelease(pThis->hSpinlock);

    /*
     * Call the OS specific code to do the job.
     * Update the state when the call returns, that is everything except for
     * the fDisconnectedFromHost flag which the OS specific code shall set.
     */
    if (fDoIt)
    {
        fRediscovered = vpoxNetFltOsMaybeRediscovered(pThis);

        Assert(!fRediscovered || !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost));

        ASMAtomicUoWriteU64(&pThis->NanoTSLastRediscovery, RTTimeNanoTS());
        ASMAtomicWriteBool(&pThis->fRediscoveryPending, false);

        if (fRediscovered)
            /** @todo this isn't 100% serialized. */
            vpoxNetFltPortOsSetActive(pThis, pThis->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE);
    }

    return fRediscovered;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnXmit
 */
static DECLCALLBACK(int) vpoxNetFltPortXmit(PINTNETTRUNKIFPORT pIfPort, void *pvIfData, PINTNETSG pSG, uint32_t fDst)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    int rc = VINF_SUCCESS;

    /*
     * Input validation.
     */
    AssertPtr(pThis);
    AssertPtr(pSG);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    AssertReturn(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Connected, VERR_INVALID_STATE);

    /*
     * Do a busy retain and then make sure we're connected to the interface
     * before invoking the OS specific code.
     */
    if (RT_LIKELY(vpoxNetFltTryRetainBusyActive(pThis)))
    {
        if (    !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost)
            ||  vpoxNetFltMaybeRediscovered(pThis))
            rc = vpoxNetFltPortOsXmit(pThis, pvIfData, pSG, fDst);
        vpoxNetFltRelease(pThis, true /* fBusy */);
    }

    return rc;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnWaitForIdle
 */
static DECLCALLBACK(int) vpoxNetFltPortWaitForIdle(PINTNETTRUNKIFPORT pIfPort, uint32_t cMillies)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    int rc;

    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    AssertReturn(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Connected, VERR_INVALID_STATE);
    AssertReturn(pThis->enmTrunkState == INTNETTRUNKIFSTATE_DISCONNECTING, VERR_INVALID_STATE);

    /*
     * Go to sleep on the semaphore after checking the busy count.
     */
    vpoxNetFltRetain(pThis, false /* fBusy */);

    rc = VINF_SUCCESS;
    while (pThis->cBusy && RT_SUCCESS(rc))
        rc = RTSemEventWait(pThis->hEventIdle, cMillies); /** @todo make interruptible? */

    vpoxNetFltRelease(pThis, false /* fBusy */);

    return rc;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnSetState
 */
static DECLCALLBACK(INTNETTRUNKIFSTATE) vpoxNetFltPortSetState(PINTNETTRUNKIFPORT pIfPort, INTNETTRUNKIFSTATE enmState)
{
    PVPOXNETFLTINS      pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    INTNETTRUNKIFSTATE  enmOldTrunkState;

    /*
     * Input validation.
     */
    AssertPtr(pThis);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    AssertReturn(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Connected, INTNETTRUNKIFSTATE_INVALID);
    AssertReturn(enmState > INTNETTRUNKIFSTATE_INVALID && enmState < INTNETTRUNKIFSTATE_END,
                 INTNETTRUNKIFSTATE_INVALID);

    /*
     * Take the lock and change the state.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    enmOldTrunkState = pThis->enmTrunkState;
    if (enmOldTrunkState != enmState)
        ASMAtomicWriteU32((uint32_t volatile *)&pThis->enmTrunkState, enmState);
    RTSpinlockRelease(pThis->hSpinlock);

    /*
     * If the state change indicates that the trunk has become active or
     * inactive, call the OS specific part so they can work the promiscuous
     * settings and such.
     * Note! The caller makes sure there are no concurrent pfnSetState calls.
     */
    if ((enmOldTrunkState == INTNETTRUNKIFSTATE_ACTIVE) != (enmState == INTNETTRUNKIFSTATE_ACTIVE))
        vpoxNetFltPortOsSetActive(pThis, (enmState == INTNETTRUNKIFSTATE_ACTIVE));

    return enmOldTrunkState;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnNotifyMacAddress
 */
static DECLCALLBACK(void) vpoxNetFltPortNotifyMacAddress(PINTNETTRUNKIFPORT pIfPort, void *pvIfData, PCRTMAC pMac)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);

    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);

    vpoxNetFltRetain(pThis, false /* fBusy */);
    vpoxNetFltPortOsNotifyMacAddress(pThis, pvIfData, pMac);
    vpoxNetFltRelease(pThis, false /* fBusy */);
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnConnectInterface
 */
static DECLCALLBACK(int) vpoxNetFltPortConnectInterface(PINTNETTRUNKIFPORT pIfPort, void *pvIf, void **ppvIfData)
{
    PVPOXNETFLTINS  pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    int             rc;

    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);

    vpoxNetFltRetain(pThis, false /* fBusy */);
    rc = vpoxNetFltPortOsConnectInterface(pThis, pvIf, ppvIfData);
    vpoxNetFltRelease(pThis, false /* fBusy */);

    return rc;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnDisconnectInterface
 */
static DECLCALLBACK(void) vpoxNetFltPortDisconnectInterface(PINTNETTRUNKIFPORT pIfPort, void *pvIfData)
{
    PVPOXNETFLTINS  pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    int             rc;

    /*
     * Input validation.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);

    vpoxNetFltRetain(pThis, false /* fBusy */);
    rc = vpoxNetFltPortOsDisconnectInterface(pThis, pvIfData);
    vpoxNetFltRelease(pThis, false /* fBusy */);
    AssertRC(rc); /** @todo fix vpoxNetFltPortOsDisconnectInterface. */
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnDisconnectAndRelease
 */
static DECLCALLBACK(void) vpoxNetFltPortDisconnectAndRelease(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);

    /*
     * Serious paranoia.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    Assert(pThis->MyPort.u32VersionEnd == INTNETTRUNKIFPORT_VERSION);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->hEventIdle != NIL_RTSEMEVENT);
    Assert(pThis->hSpinlock != NIL_RTSPINLOCK);
    Assert(pThis->szName[0]);

    Assert(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Connected);
    Assert(pThis->enmTrunkState == INTNETTRUNKIFSTATE_DISCONNECTING);
    Assert(!pThis->fRediscoveryPending);
    Assert(!pThis->cBusy);

    /*
     * Disconnect and release it.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    vpoxNetFltSetState(pThis, kVPoxNetFltInsState_Disconnecting);
    RTSpinlockRelease(pThis->hSpinlock);

    vpoxNetFltOsDisconnectIt(pThis);
    pThis->pSwitchPort = NULL;

#ifdef VPOXNETFLT_STATIC_CONFIG
    RTSpinlockAcquire(pThis->hSpinlock);
    vpoxNetFltSetState(pThis, kVPoxNetFltInsState_Unconnected);
    RTSpinlockRelease(pThis->hSpinlock);
#endif

    vpoxNetFltRelease(pThis, false /* fBusy */);
}


/**
 * Destroy a device that has been disconnected from the switch.
 *
 * @returns true if the instance is destroyed, false otherwise.
 * @param   pThis               The instance to be destroyed. This is
 *                              no longer valid when this function returns.
 */
static bool vpoxNetFltDestroyInstance(PVPOXNETFLTINS pThis)
{
    PVPOXNETFLTGLOBALS pGlobals = pThis->pGlobals;
    uint32_t cRefs = ASMAtomicUoReadU32((uint32_t volatile *)&pThis->cRefs);
    int rc;
    LogFlow(("vpoxNetFltDestroyInstance: pThis=%p (%s)\n", pThis, pThis->szName));

    /*
     * Validate the state.
     */
#ifdef VPOXNETFLT_STATIC_CONFIG
    Assert(   vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Disconnecting
           || vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Unconnected);
#else
    Assert(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Disconnecting);
#endif
    Assert(pThis->enmTrunkState == INTNETTRUNKIFSTATE_DISCONNECTING);
    Assert(!pThis->fRediscoveryPending);
    Assert(!pThis->cRefs);
    Assert(!pThis->cBusy);
    Assert(!pThis->pSwitchPort);

    /*
     * Make sure the state is 'disconnecting' / 'destroying' and let the OS
     * specific code do its part of the cleanup outside the mutex.
     */
    rc = RTSemFastMutexRequest(pGlobals->hFastMtx); AssertRC(rc);
    vpoxNetFltSetState(pThis, kVPoxNetFltInsState_Disconnecting);
    RTSemFastMutexRelease(pGlobals->hFastMtx);

    vpoxNetFltOsDeleteInstance(pThis);

    /*
     * Unlink the instance and free up its resources.
     */
    rc = RTSemFastMutexRequest(pGlobals->hFastMtx); AssertRC(rc);
    vpoxNetFltSetState(pThis, kVPoxNetFltInsState_Destroyed);
    vpoxNetFltUnlinkLocked(pGlobals, pThis);
    RTSemFastMutexRelease(pGlobals->hFastMtx);

    RTSemEventDestroy(pThis->hEventIdle);
    pThis->hEventIdle = NIL_RTSEMEVENT;
    RTSpinlockDestroy(pThis->hSpinlock);
    pThis->hSpinlock = NIL_RTSPINLOCK;
    RTMemFree(pThis);

    NOREF(cRefs);

    return true;
}


/**
 * Releases a reference to the specified instance.
 *
 * This method will destroy the instance when the count reaches 0.
 * It will also take care of decrementing the counter and idle wakeup.
 *
 * @param   pThis           The instance.
 * @param   fBusy           Whether the busy counter should be decremented too.
 */
DECLHIDDEN(void) vpoxNetFltRelease(PVPOXNETFLTINS pThis, bool fBusy)
{
    uint32_t cRefs;

    /*
     * Paranoid Android.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    Assert(pThis->MyPort.u32VersionEnd == INTNETTRUNKIFPORT_VERSION);
    Assert(   vpoxNetFltGetState(pThis) > kVPoxNetFltInsState_Invalid
           && vpoxNetFltGetState(pThis) < kVPoxNetFltInsState_Destroyed);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->hEventIdle != NIL_RTSEMEVENT);
    Assert(pThis->hSpinlock != NIL_RTSPINLOCK);
    Assert(pThis->szName[0]);

    /*
     * Work the busy counter.
     */
    if (fBusy)
    {
        cRefs = ASMAtomicDecU32(&pThis->cBusy);
        if (!cRefs)
        {
            int rc = RTSemEventSignal(pThis->hEventIdle);
            AssertRC(rc);
        }
        else
            Assert(cRefs < UINT32_MAX / 2);
    }

    /*
     * The object reference counting.
     */
    cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (!cRefs)
        vpoxNetFltDestroyInstance(pThis);
    else
        Assert(cRefs < UINT32_MAX / 2);
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnRelease
 */
static DECLCALLBACK(void) vpoxNetFltPortRelease(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    vpoxNetFltRelease(pThis, false /* fBusy */);
}


/**
 * @callback_method_impl{FNINTNETTRUNKIFPORTRELEASEBUSY}
 */
DECLHIDDEN(DECLCALLBACK(void)) vpoxNetFltPortReleaseBusy(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    vpoxNetFltRelease(pThis, true /*fBusy*/);
}


/**
 * Retains a reference to the specified instance and a busy reference too.
 *
 * @param   pThis           The instance.
 * @param   fBusy           Whether the busy counter should be incremented as well.
 */
DECLHIDDEN(void) vpoxNetFltRetain(PVPOXNETFLTINS pThis, bool fBusy)
{
    uint32_t cRefs;

    /*
     * Paranoid Android.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    Assert(pThis->MyPort.u32VersionEnd == INTNETTRUNKIFPORT_VERSION);
    Assert(   vpoxNetFltGetState(pThis) > kVPoxNetFltInsState_Invalid
           && vpoxNetFltGetState(pThis) < kVPoxNetFltInsState_Destroyed);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->hEventIdle != NIL_RTSEMEVENT);
    Assert(pThis->hSpinlock != NIL_RTSPINLOCK);
    Assert(pThis->szName[0]);

    /*
     * Retain the object.
     */
    cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1 && cRefs < UINT32_MAX / 2);

    /*
     * Work the busy counter.
     */
    if (fBusy)
    {
        cRefs = ASMAtomicIncU32(&pThis->cBusy);
        Assert(cRefs > 0 && cRefs < UINT32_MAX / 2);
    }

    NOREF(cRefs);
}


/**
 * Tries to retain the device as busy if the trunk is active.
 *
 * This is used before calling pfnRecv or pfnPreRecv.
 *
 * @returns true if we succeeded in retaining a busy reference to the active
 *          device.  false if we failed.
 * @param   pThis           The instance.
 */
DECLHIDDEN(bool) vpoxNetFltTryRetainBusyActive(PVPOXNETFLTINS pThis)
{
    uint32_t        cRefs;
    bool            fRc;

    /*
     * Paranoid Android.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    Assert(pThis->MyPort.u32VersionEnd == INTNETTRUNKIFPORT_VERSION);
    Assert(   vpoxNetFltGetState(pThis) > kVPoxNetFltInsState_Invalid
           && vpoxNetFltGetState(pThis) < kVPoxNetFltInsState_Destroyed);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->hEventIdle != NIL_RTSEMEVENT);
    Assert(pThis->hSpinlock != NIL_RTSPINLOCK);
    Assert(pThis->szName[0]);

    /*
     * Do the retaining and checking behind the spinlock.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    fRc = pThis->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE;
    if (fRc)
    {
        cRefs = ASMAtomicIncU32(&pThis->cRefs);
        AssertMsg(cRefs > 1 && cRefs < UINT32_MAX / 2, ("%d\n", cRefs)); NOREF(cRefs);

        cRefs = ASMAtomicIncU32(&pThis->cBusy);
        AssertMsg(cRefs >= 1 && cRefs < UINT32_MAX / 2, ("%d\n", cRefs)); NOREF(cRefs);
    }
    RTSpinlockRelease(pThis->hSpinlock);

    return fRc;
}


/**
 * Tries to retain the device as busy if the trunk is not disconnecting.
 *
 * This is used before reporting stuff to the internal network.
 *
 * @returns true if we succeeded in retaining a busy reference to the active
 *          device.  false if we failed.
 * @param   pThis           The instance.
 */
DECLHIDDEN(bool) vpoxNetFltTryRetainBusyNotDisconnected(PVPOXNETFLTINS pThis)
{
    uint32_t        cRefs;
    bool            fRc;

    /*
     * Paranoid Android.
     */
    AssertPtr(pThis);
    Assert(pThis->MyPort.u32Version == INTNETTRUNKIFPORT_VERSION);
    Assert(pThis->MyPort.u32VersionEnd == INTNETTRUNKIFPORT_VERSION);
    Assert(   vpoxNetFltGetState(pThis) > kVPoxNetFltInsState_Invalid
           && vpoxNetFltGetState(pThis) < kVPoxNetFltInsState_Destroyed);
    AssertPtr(pThis->pGlobals);
    Assert(pThis->hEventIdle != NIL_RTSEMEVENT);
    Assert(pThis->hSpinlock != NIL_RTSPINLOCK);
    Assert(pThis->szName[0]);

    /*
     * Do the retaining and checking behind the spinlock.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    fRc =  pThis->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE
        || pThis->enmTrunkState == INTNETTRUNKIFSTATE_INACTIVE;
    if (fRc)
    {
        cRefs = ASMAtomicIncU32(&pThis->cRefs);
        AssertMsg(cRefs > 1 && cRefs < UINT32_MAX / 2, ("%d\n", cRefs)); NOREF(cRefs);

        cRefs = ASMAtomicIncU32(&pThis->cBusy);
        AssertMsg(cRefs >= 1 && cRefs < UINT32_MAX / 2, ("%d\n", cRefs)); NOREF(cRefs);
    }
    RTSpinlockRelease(pThis->hSpinlock);

    return fRc;
}


/**
 * @copydoc INTNETTRUNKIFPORT::pfnRetain
 */
static DECLCALLBACK(void) vpoxNetFltPortRetain(PINTNETTRUNKIFPORT pIfPort)
{
    PVPOXNETFLTINS pThis = IFPORT_2_VPOXNETFLTINS(pIfPort);
    vpoxNetFltRetain(pThis, false /* fBusy */);
}


/**
 * Connects the instance to the specified switch port.
 *
 * Called while owning the lock. We're ASSUMING that the internal
 * networking code is already owning an recursive mutex, so, there
 * will be no deadlocks when vpoxNetFltOsConnectIt calls back into
 * it for setting preferences.
 *
 * @returns VPox status code.
 * @param   pThis               The instance.
 * @param   pSwitchPort         The port on the internal network 'switch'.
 * @param   ppIfPort            Where to return our port interface.
 */
static int vpoxNetFltConnectIt(PVPOXNETFLTINS pThis, PINTNETTRUNKSWPORT pSwitchPort, PINTNETTRUNKIFPORT *ppIfPort)
{
    int rc;

    /*
     * Validate state.
     */
    Assert(!pThis->fRediscoveryPending);
    Assert(!pThis->cBusy);
#ifdef VPOXNETFLT_STATIC_CONFIG
    Assert(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Unconnected);
#else
    Assert(vpoxNetFltGetState(pThis) == kVPoxNetFltInsState_Initializing);
#endif
    Assert(pThis->enmTrunkState == INTNETTRUNKIFSTATE_INACTIVE);

    /*
     * Do the job.
     * Note that we're calling the os stuff while owning the semaphore here.
     */
    pThis->pSwitchPort = pSwitchPort;
    rc = vpoxNetFltOsConnectIt(pThis);
    if (RT_SUCCESS(rc))
    {
        vpoxNetFltSetState(pThis, kVPoxNetFltInsState_Connected);
        *ppIfPort = &pThis->MyPort;
    }
    else
        pThis->pSwitchPort = NULL;

    Assert(pThis->enmTrunkState == INTNETTRUNKIFSTATE_INACTIVE);
    return rc;
}


/**
 * Creates a new instance.
 *
 * The new instance will be in the suspended state in a dynamic config and in
 * the inactive in a static one.
 *
 * Called without owning the lock, but will request is several times.
 *
 * @returns VPox status code.
 * @param   pGlobals            The globals.
 * @param   pszName             The instance name.
 * @param   pSwitchPort         The port on the switch that we're connected with (dynamic only).
 * @param   fNoPromisc          Do not attempt going into promiscuous mode.
 * @param   pvContext           Context argument for vpoxNetFltOsInitInstance.
 * @param   ppIfPort            Where to store the pointer to our port interface (dynamic only).
 */
static int vpoxNetFltNewInstance(PVPOXNETFLTGLOBALS pGlobals, const char *pszName, PINTNETTRUNKSWPORT pSwitchPort,
                                 bool fNoPromisc, void *pvContext, PINTNETTRUNKIFPORT *ppIfPort)
{
    /*
     * Allocate and initialize a new instance before requesting the mutex.
     * Note! That in a static config we'll initialize the trunk state to
     *       disconnecting and flip it in vpoxNetFltFactoryCreateAndConnect
     *       later on.  This better reflext the state and it works better with
     *       assertions in the destruction path.
     */
    int             rc;
    size_t const    cchName = strlen(pszName);
    PVPOXNETFLTINS  pNew = (PVPOXNETFLTINS)RTMemAllocZVar(RT_UOFFSETOF_DYN(VPOXNETFLTINS, szName[cchName + 1]));
    if (!pNew)
        return VERR_INTNET_FLT_IF_FAILED;
    AssertMsg(((uintptr_t)pNew & 7) == 0, ("%p LB %#x\n", pNew, RT_UOFFSETOF_DYN(VPOXNETFLTINS, szName[cchName + 1])));
    pNew->pNext                         = NULL;
    pNew->MyPort.u32Version             = INTNETTRUNKIFPORT_VERSION;
    pNew->MyPort.pfnRetain              = vpoxNetFltPortRetain;
    pNew->MyPort.pfnRelease             = vpoxNetFltPortRelease;
    pNew->MyPort.pfnDisconnectAndRelease= vpoxNetFltPortDisconnectAndRelease;
    pNew->MyPort.pfnSetState            = vpoxNetFltPortSetState;
    pNew->MyPort.pfnWaitForIdle         = vpoxNetFltPortWaitForIdle;
    pNew->MyPort.pfnXmit                = vpoxNetFltPortXmit;
    pNew->MyPort.pfnNotifyMacAddress    = vpoxNetFltPortNotifyMacAddress;
    pNew->MyPort.pfnConnectInterface    = vpoxNetFltPortConnectInterface;
    pNew->MyPort.pfnDisconnectInterface = vpoxNetFltPortDisconnectInterface;
    pNew->MyPort.u32VersionEnd          = INTNETTRUNKIFPORT_VERSION;
    pNew->pSwitchPort                   = pSwitchPort;
    pNew->pGlobals                      = pGlobals;
    pNew->hSpinlock                     = NIL_RTSPINLOCK;
    pNew->enmState                      = kVPoxNetFltInsState_Initializing;
#ifdef VPOXNETFLT_STATIC_CONFIG
    pNew->enmTrunkState                 = INTNETTRUNKIFSTATE_DISCONNECTING;
#else
    pNew->enmTrunkState                 = INTNETTRUNKIFSTATE_INACTIVE;
#endif
    pNew->fDisconnectedFromHost         = false;
    pNew->fRediscoveryPending           = false;
    pNew->fDisablePromiscuous           = fNoPromisc;
    pNew->NanoTSLastRediscovery         = INT64_MAX;
    pNew->cRefs                         = 1;
    pNew->cBusy                         = 0;
    pNew->hEventIdle                    = NIL_RTSEMEVENT;
    memcpy(pNew->szName, pszName, cchName + 1);

    rc = RTSpinlockCreate(&pNew->hSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VPoxNetFltNewInstance");
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pNew->hEventIdle);
        if (RT_SUCCESS(rc))
        {
            rc = vpoxNetFltOsPreInitInstance(pNew);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Insert the instance into the chain, checking for
                 * duplicates first of course (race).
                 */
                rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
                if (RT_SUCCESS(rc))
                {
                    if (!vpoxNetFltFindInstanceLocked(pGlobals, pszName))
                    {
                        pNew->pNext = pGlobals->pInstanceHead;
                        pGlobals->pInstanceHead = pNew;
                        RTSemFastMutexRelease(pGlobals->hFastMtx);

                        /*
                         * Call the OS specific initialization code.
                         */
                        rc = vpoxNetFltOsInitInstance(pNew, pvContext);
                        RTSemFastMutexRequest(pGlobals->hFastMtx);
                        if (RT_SUCCESS(rc))
                        {
#ifdef VPOXNETFLT_STATIC_CONFIG
                            /*
                             * Static instances are unconnected at birth.
                             */
                            Assert(!pSwitchPort);
                            pNew->enmState = kVPoxNetFltInsState_Unconnected;
                            RTSemFastMutexRelease(pGlobals->hFastMtx);
                            *ppIfPort = &pNew->MyPort;
                            return rc;

#else  /* !VPOXNETFLT_STATIC_CONFIG */
                            /*
                             * Connect it as well, the OS specific bits has to be done outside
                             * the lock as they may call back to into intnet.
                             */
                            rc = vpoxNetFltConnectIt(pNew, pSwitchPort, ppIfPort);
                            if (RT_SUCCESS(rc))
                            {
                                RTSemFastMutexRelease(pGlobals->hFastMtx);
                                Assert(*ppIfPort == &pNew->MyPort);
                                return rc;
                            }

                            /* Bail out (failed). */
                            vpoxNetFltOsDeleteInstance(pNew);
#endif /* !VPOXNETFLT_STATIC_CONFIG */
                        }
                        vpoxNetFltUnlinkLocked(pGlobals, pNew);
                    }
                    else
                        rc = VERR_INTNET_FLT_IF_BUSY;
                    RTSemFastMutexRelease(pGlobals->hFastMtx);
                }
            }
            RTSemEventDestroy(pNew->hEventIdle);
        }
        RTSpinlockDestroy(pNew->hSpinlock);
    }

    RTMemFree(pNew);
    return rc;
}


#ifdef VPOXNETFLT_STATIC_CONFIG
/**
 * Searches for the NetFlt instance by its name and creates the new one if not found.
 *
 * @returns VPox status code.
 * @retval  VINF_SUCCESS and *ppInstance if a new instance was created.
 * @retval  VINF_ALREADY_INITIALIZED and *ppInstance if an instance already exists.
 *
 * @param   pGlobal     Pointer to the globals.
 * @param   pszName     The instance name.
 * @param   ppInstance  Where to return the instance pointer on success.
 * @param   pvContext   Context which needs to be passed along to vpoxNetFltOsInitInstance.
 */
DECLHIDDEN(int) vpoxNetFltSearchCreateInstance(PVPOXNETFLTGLOBALS pGlobals, const char *pszName, PVPOXNETFLTINS *ppInstance, void *pvContext)
{
    PINTNETTRUNKIFPORT pIfPort;
    PVPOXNETFLTINS pCur;
    VPOXNETFTLINSSTATE enmState;
    int rc;

    *ppInstance = NULL;
    rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    AssertRCReturn(rc, rc);

    /*
     * Look for an existing instance in the list.
     *
     * There might be an existing one in the list if the driver was unbound
     * while it was connected to an internal network. We're running into
     * a destruction race that is a bit similar to the one in
     * vpoxNetFltFactoryCreateAndConnect, only the roles are reversed
     * and we're not in a position to back down. Instead of backing down
     * we'll delay a bit giving the other thread time to complete the
     * destructor.
     */
    pCur = vpoxNetFltFindInstanceLocked(pGlobals, pszName);
    while (pCur)
    {
        uint32_t cRefs = ASMAtomicIncU32(&pCur->cRefs);
        if (cRefs > 1)
        {
            enmState = vpoxNetFltGetState(pCur);
            switch (enmState)
            {
                case kVPoxNetFltInsState_Unconnected:
                case kVPoxNetFltInsState_Connected:
                case kVPoxNetFltInsState_Disconnecting:
                    if (pCur->fDisconnectedFromHost)
                    {
                        /* Wait for it to exit the transitional disconnecting
                           state. It might otherwise be running the risk of
                           upsetting the OS specific code...  */
                        /** @todo This reconnect stuff should be serialized correctly for static
                         *        devices. Shouldn't it? In the dynamic case we're using the INTNET
                         *        outbound thunk lock, but that doesn't quite cut it here, or does
                         *        it? We could either transition to initializing  or make a callback
                         *        while owning the mutex here... */
                        if (enmState == kVPoxNetFltInsState_Disconnecting)
                        {
                            do
                            {
                                RTSemFastMutexRelease(pGlobals->hFastMtx);
                                RTThreadSleep(2); /* (2ms) */
                                RTSemFastMutexRequest(pGlobals->hFastMtx);
                                enmState = vpoxNetFltGetState(pCur);
                            }
                            while (enmState == kVPoxNetFltInsState_Disconnecting);
                            AssertMsg(enmState == kVPoxNetFltInsState_Unconnected, ("%d\n", enmState));
                            Assert(pCur->fDisconnectedFromHost);
                        }

                        RTSemFastMutexRelease(pGlobals->hFastMtx);
                        *ppInstance = pCur;
                        return VINF_ALREADY_INITIALIZED;
                    }
                    /* fall thru */

                default:
                {
                    bool fDfH = pCur->fDisconnectedFromHost;
                    RTSemFastMutexRelease(pGlobals->hFastMtx);
                    vpoxNetFltRelease(pCur, false /* fBusy */);
                    LogRel(("VPoxNetFlt: Huh? An instance of '%s' already exists! [pCur=%p cRefs=%d fDfH=%RTbool enmState=%d]\n",
                            pszName, pCur, cRefs - 1, fDfH, enmState));
                    *ppInstance = NULL;
                    return VERR_INTNET_FLT_IF_BUSY;
                }
            }
        }

        /* Zero references, it's being destroyed. Delay a bit so the destructor
           can finish its work and try again. (vpoxNetFltNewInstance will fail
           with duplicate name if we don't.) */
# ifdef RT_STRICT
        Assert(cRefs == 1);
        enmState = vpoxNetFltGetState(pCur);
        AssertMsg(   enmState == kVPoxNetFltInsState_Unconnected
                  || enmState == kVPoxNetFltInsState_Disconnecting
                  || enmState == kVPoxNetFltInsState_Destroyed, ("%d\n", enmState));
# endif
        ASMAtomicDecU32(&pCur->cRefs);
        RTSemFastMutexRelease(pGlobals->hFastMtx);
        RTThreadSleep(2); /* (2ms) */
        rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
        AssertRCReturn(rc, rc);

        /* try again */
        pCur = vpoxNetFltFindInstanceLocked(pGlobals, pszName);
    }

    RTSemFastMutexRelease(pGlobals->hFastMtx);

    /*
     * Try create a new instance.
     * (fNoPromisc is overridden in the vpoxNetFltFactoryCreateAndConnect path, so pass true here.)
     */
    rc = vpoxNetFltNewInstance(pGlobals, pszName, NULL, true /* fNoPromisc */, pvContext, &pIfPort);
    if (RT_SUCCESS(rc))
        *ppInstance = IFPORT_2_VPOXNETFLTINS(pIfPort);
    else
        *ppInstance = NULL;

    return rc;
}
#endif /* VPOXNETFLT_STATIC_CONFIG */


/**
 * @copydoc INTNETTRUNKFACTORY::pfnCreateAndConnect
 */
static DECLCALLBACK(int) vpoxNetFltFactoryCreateAndConnect(PINTNETTRUNKFACTORY pIfFactory, const char *pszName,
                                                           PINTNETTRUNKSWPORT pSwitchPort, uint32_t fFlags,
                                                           PINTNETTRUNKIFPORT *ppIfPort)
{
    PVPOXNETFLTGLOBALS pGlobals = (PVPOXNETFLTGLOBALS)((uint8_t *)pIfFactory - RT_UOFFSETOF(VPOXNETFLTGLOBALS, TrunkFactory));
    PVPOXNETFLTINS pCur;
    int rc;

    LogFlow(("vpoxNetFltFactoryCreateAndConnect: pszName=%p:{%s} fFlags=%#x\n", pszName, pszName, fFlags));
    Assert(pGlobals->cFactoryRefs > 0);
    AssertMsgReturn(!(fFlags & ~(INTNETTRUNKFACTORY_FLAG_NO_PROMISC)),
                    ("%#x\n", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Static: Find instance, check if busy, connect if not.
     * Dynamic: Check for duplicate / busy interface instance.
     */
    rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    AssertRCReturn(rc, rc);

//#if defined(VPOXNETADP) && defined(RT_OS_WINDOWS)
//    /* temporary hack to pick up the first adapter */
//    pCur = pGlobals->pInstanceHead; /** @todo Don't for get to remove this temporary hack... :-) */
//#else
    pCur = vpoxNetFltFindInstanceLocked(pGlobals, pszName);
//#endif
    if (pCur)
    {
#ifdef VPOXNETFLT_STATIC_CONFIG
        /* Try grab a reference. If the count had already reached zero we're racing the
           destructor code and must back down. */
        uint32_t cRefs = ASMAtomicIncU32(&pCur->cRefs);
        if (cRefs > 1)
        {
            if (vpoxNetFltGetState(pCur) == kVPoxNetFltInsState_Unconnected)
            {
                pCur->enmTrunkState = INTNETTRUNKIFSTATE_INACTIVE; /** @todo protect me? */
                pCur->fDisablePromiscuous = !!(fFlags & INTNETTRUNKFACTORY_FLAG_NO_PROMISC);
                rc = vpoxNetFltConnectIt(pCur, pSwitchPort, ppIfPort);
                if (RT_SUCCESS(rc))
                    pCur = NULL; /* Don't release it, reference given to the caller. */
                else
                    pCur->enmTrunkState = INTNETTRUNKIFSTATE_DISCONNECTING;
            }
            else
                rc = VERR_INTNET_FLT_IF_BUSY;
        }
        else
        {
            Assert(cRefs == 1);
            ASMAtomicDecU32(&pCur->cRefs);
            pCur = NULL; /* nothing to release */
            rc = VERR_INTNET_FLT_IF_NOT_FOUND;
        }

        RTSemFastMutexRelease(pGlobals->hFastMtx);
        if (pCur)
            vpoxNetFltRelease(pCur, false /* fBusy */);
#else
        rc = VERR_INTNET_FLT_IF_BUSY;
        RTSemFastMutexRelease(pGlobals->hFastMtx);
#endif
        LogFlow(("vpoxNetFltFactoryCreateAndConnect: returns %Rrc\n", rc));
        return rc;
    }

    RTSemFastMutexRelease(pGlobals->hFastMtx);

#ifdef VPOXNETFLT_STATIC_CONFIG
    rc = VERR_INTNET_FLT_IF_NOT_FOUND;
#else
    /*
     * Dynamically create a new instance.
     */
    rc = vpoxNetFltNewInstance(pGlobals,
                               pszName,
                               pSwitchPort,
                               !!(fFlags & INTNETTRUNKFACTORY_FLAG_NO_PROMISC),
                               NULL,
                               ppIfPort);
#endif
    LogFlow(("vpoxNetFltFactoryCreateAndConnect: returns %Rrc\n", rc));
    return rc;
}


/**
 * @copydoc INTNETTRUNKFACTORY::pfnRelease
 */
static DECLCALLBACK(void) vpoxNetFltFactoryRelease(PINTNETTRUNKFACTORY pIfFactory)
{
    PVPOXNETFLTGLOBALS pGlobals = (PVPOXNETFLTGLOBALS)((uint8_t *)pIfFactory - RT_UOFFSETOF(VPOXNETFLTGLOBALS, TrunkFactory));

    int32_t cRefs = ASMAtomicDecS32(&pGlobals->cFactoryRefs);
    Assert(cRefs >= 0); NOREF(cRefs);
    LogFlow(("vpoxNetFltFactoryRelease: cRefs=%d (new)\n", cRefs));
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
static DECLCALLBACK(void *) vpoxNetFltQueryFactoryInterface(PCSUPDRVFACTORY pSupDrvFactory, PSUPDRVSESSION pSession,
                                                            const char *pszInterfaceUuid)
{
    PVPOXNETFLTGLOBALS pGlobals = (PVPOXNETFLTGLOBALS)((uint8_t *)pSupDrvFactory - RT_UOFFSETOF(VPOXNETFLTGLOBALS, SupDrvFactory));

    /*
     * Convert the UUID strings and compare them.
     */
    RTUUID UuidReq;
    int rc = RTUuidFromStr(&UuidReq, pszInterfaceUuid);
    if (RT_SUCCESS(rc))
    {
        if (!RTUuidCompareStr(&UuidReq, INTNETTRUNKFACTORY_UUID_STR))
        {
            ASMAtomicIncS32(&pGlobals->cFactoryRefs);
            return &pGlobals->TrunkFactory;
        }
#ifdef LOG_ENABLED
        /* log legacy queries */
        /* else if (!RTUuidCompareStr(&UuidReq, INTNETTRUNKFACTORY_V1_UUID_STR))
            Log(("VPoxNetFlt: V1 factory query\n"));
        */
        else
            Log(("VPoxNetFlt: unknown factory interface query (%s)\n", pszInterfaceUuid));
#endif
    }
    else
        Log(("VPoxNetFlt: rc=%Rrc, uuid=%s\n", rc, pszInterfaceUuid));

    RT_NOREF1(pSession);
    return NULL;
}


/**
 * Checks whether the VPoxNetFlt wossname can be unloaded.
 *
 * This will return false if someone is currently using the module.
 *
 * @returns true if it's relatively safe to unload it, otherwise false.
 * @param   pGlobals        Pointer to the globals.
 */
DECLHIDDEN(bool) vpoxNetFltCanUnload(PVPOXNETFLTGLOBALS pGlobals)
{
    int rc = RTSemFastMutexRequest(pGlobals->hFastMtx);
    bool fRc = !pGlobals->pInstanceHead
            && pGlobals->cFactoryRefs <= 0;
    RTSemFastMutexRelease(pGlobals->hFastMtx);
    AssertRC(rc);
    return fRc;
}


/**
 * Try to close the IDC connection to SUPDRV if established.
 *
 * @returns VPox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_WRONG_ORDER if we're busy.
 *
 * @param   pGlobals        Pointer to the globals.
 *
 * @sa      vpoxNetFltTryDeleteIdcAndGlobals()
 */
DECLHIDDEN(int) vpoxNetFltTryDeleteIdc(PVPOXNETFLTGLOBALS pGlobals)
{
    int rc;

    Assert(pGlobals->hFastMtx != NIL_RTSEMFASTMUTEX);

    /*
     * Check before trying to deregister the factory.
     */
    if (!vpoxNetFltCanUnload(pGlobals))
        return VERR_WRONG_ORDER;

    if (!pGlobals->fIDCOpen)
        rc = VINF_SUCCESS;
    else
    {
        /*
         * Disconnect from SUPDRV and check that nobody raced us,
         * reconnect if that should happen.
         */
        rc = SUPR0IdcComponentDeregisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
        AssertRC(rc);
        if (!vpoxNetFltCanUnload(pGlobals))
        {
            rc = SUPR0IdcComponentRegisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
            AssertRC(rc);
            return VERR_WRONG_ORDER;
        }

        SUPR0IdcClose(&pGlobals->SupDrvIDC);
        pGlobals->fIDCOpen = false;
    }

    return rc;
}


/**
 * Establishes the IDC connection to SUPDRV and registers our component factory.
 *
 * @returns VPox status code.
 * @param   pGlobals    Pointer to the globals.
 * @sa      vpoxNetFltInitGlobalsAndIdc().
 */
DECLHIDDEN(int) vpoxNetFltInitIdc(PVPOXNETFLTGLOBALS pGlobals)
{
    int rc;
    Assert(!pGlobals->fIDCOpen);

    /*
     * Establish a connection to SUPDRV and register our component factory.
     */
    rc = SUPR0IdcOpen(&pGlobals->SupDrvIDC, 0 /* iReqVersion = default */, 0 /* iMinVersion = default */, NULL, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = SUPR0IdcComponentRegisterFactory(&pGlobals->SupDrvIDC, &pGlobals->SupDrvFactory);
        if (RT_SUCCESS(rc))
        {
            pGlobals->fIDCOpen = true;
            Log(("VPoxNetFlt: pSession=%p\n", SUPR0IdcGetSession(&pGlobals->SupDrvIDC)));
            return rc;
        }

        /* bail out. */
        LogRel(("VPoxNetFlt: Failed to register component factory, rc=%Rrc\n", rc));
        SUPR0IdcClose(&pGlobals->SupDrvIDC);
    }

    return rc;
}


/**
 * Deletes the globals.
 *
 * This must be called after the IDC connection has been closed,
 * see vpoxNetFltTryDeleteIdc().
 *
 * @param   pGlobals        Pointer to the globals.
 * @sa      vpoxNetFltTryDeleteIdcAndGlobals()
 */
DECLHIDDEN(void) vpoxNetFltDeleteGlobals(PVPOXNETFLTGLOBALS pGlobals)
{
    Assert(!pGlobals->fIDCOpen);

    /*
     * Release resources.
     */
    RTSemFastMutexDestroy(pGlobals->hFastMtx);
    pGlobals->hFastMtx = NIL_RTSEMFASTMUTEX;
}


/**
 * Initializes the globals.
 *
 * @returns VPox status code.
 * @param   pGlobals        Pointer to the globals.
 * @sa      vpoxNetFltInitGlobalsAndIdc().
 */
DECLHIDDEN(int) vpoxNetFltInitGlobals(PVPOXNETFLTGLOBALS pGlobals)
{
    /*
     * Initialize the common portions of the structure.
     */
    int rc = RTSemFastMutexCreate(&pGlobals->hFastMtx);
    if (RT_SUCCESS(rc))
    {
        pGlobals->pInstanceHead = NULL;

        pGlobals->TrunkFactory.pfnRelease = vpoxNetFltFactoryRelease;
        pGlobals->TrunkFactory.pfnCreateAndConnect = vpoxNetFltFactoryCreateAndConnect;
#if defined(RT_OS_WINDOWS) && defined(VPOXNETADP)
        memcpy(pGlobals->SupDrvFactory.szName, "VPoxNetAdp", sizeof("VPoxNetAdp"));
#else
        memcpy(pGlobals->SupDrvFactory.szName, "VPoxNetFlt", sizeof("VPoxNetFlt"));
#endif
        pGlobals->SupDrvFactory.pfnQueryFactoryInterface = vpoxNetFltQueryFactoryInterface;
        pGlobals->fIDCOpen = false;

        return rc;
    }

    return rc;
}


/**
 * Called by the native part when the OS wants the driver to unload.
 *
 * @returns VINF_SUCCESS on success, VERR_WRONG_ORDER if we're busy.
 *
 * @param   pGlobals        Pointer to the globals.
 */
DECLHIDDEN(int) vpoxNetFltTryDeleteIdcAndGlobals(PVPOXNETFLTGLOBALS pGlobals)
{
    int rc = vpoxNetFltTryDeleteIdc(pGlobals);
    if (RT_SUCCESS(rc))
        vpoxNetFltDeleteGlobals(pGlobals);
    return rc;
}


/**
 * Called by the native driver/kext module initialization routine.
 *
 * It will initialize the common parts of the globals, assuming the caller
 * has already taken care of the OS specific bits, and establish the IDC
 * connection to SUPDRV.
 *
 * @returns VPox status code.
 * @param   pGlobals    Pointer to the globals.
 */
DECLHIDDEN(int) vpoxNetFltInitGlobalsAndIdc(PVPOXNETFLTGLOBALS pGlobals)
{
    /*
     * Initialize the common portions of the structure.
     */
    int rc = vpoxNetFltInitGlobals(pGlobals);
    if (RT_SUCCESS(rc))
    {
        rc = vpoxNetFltInitIdc(pGlobals);
        if (RT_SUCCESS(rc))
            return rc;

        /* bail out. */
        vpoxNetFltDeleteGlobals(pGlobals);
    }

    return rc;
}

