/* $Id: VPoxGuestInternal.h $ */
/** @file
 * VPoxGuest - Guest Additions Driver, Internal Header.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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

#ifndef GA_INCLUDED_SRC_common_VPoxGuest_VPoxGuestInternal_h
#define GA_INCLUDED_SRC_common_VPoxGuest_VPoxGuestInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/list.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/timer.h>
#include <VPox/VMMDev.h>
#include <VPox/VPoxGuest.h>
#include <VPox/VPoxGuestLib.h>

/** @def VPOXGUEST_USE_DEFERRED_WAKE_UP
 * Defer wake-up of waiting thread when defined. */
#if defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
# define VPOXGUEST_USE_DEFERRED_WAKE_UP
#endif

/** @def VPOXGUEST_MOUSE_NOTIFY_CAN_PREEMPT
 * The mouse notification callback can cause preemption and must not be invoked
 * while holding a high-level spinlock.
 */
#if defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
# define VPOXGUEST_MOUSE_NOTIFY_CAN_PREEMPT
#endif

/** Pointer to the VPoxGuest per session data. */
typedef struct VPOXGUESTSESSION *PVPOXGUESTSESSION;

/** Pointer to a wait-for-event entry. */
typedef struct VPOXGUESTWAIT *PVPOXGUESTWAIT;

/**
 * VPox guest wait for event entry.
 *
 * Each waiting thread allocates one of these items and adds
 * it to the wait list before going to sleep on the event sem.
 */
typedef struct VPOXGUESTWAIT
{
    /** The list node. */
    RTLISTNODE                  ListNode;
    /** The events we are waiting on. */
    uint32_t                    fReqEvents;
    /** The events we received. */
    uint32_t volatile           fResEvents;
#ifdef VPOXGUEST_USE_DEFERRED_WAKE_UP
    /** Set by VGDrvCommonWaitDoWakeUps before leaving the spinlock to call
     *  RTSemEventMultiSignal. */
    bool volatile               fPendingWakeUp;
    /** Set by the requestor thread if it got the spinlock before the
     * signaller.  Deals with the race in VGDrvCommonWaitDoWakeUps. */
    bool volatile               fFreeMe;
#endif
    /** The event semaphore. */
    RTSEMEVENTMULTI             Event;
    /** The session that's waiting. */
    PVPOXGUESTSESSION           pSession;
#ifdef VPOX_WITH_HGCM
    /** The HGCM request we're waiting for to complete. */
    VMMDevHGCMRequestHeader volatile *pHGCMReq;
#endif
} VPOXGUESTWAIT;


/**
 * VPox guest memory balloon.
 */
typedef struct VPOXGUESTMEMBALLOON
{
    /** Mutex protecting the members below from concurrent access. */
    RTSEMFASTMUTEX              hMtx;
    /** The current number of chunks in the balloon. */
    uint32_t                    cChunks;
    /** The maximum number of chunks in the balloon (typically the amount of guest
     * memory / chunksize). */
    uint32_t                    cMaxChunks;
    /** This is true if we are using RTR0MemObjAllocPhysNC() / RTR0MemObjGetPagePhysAddr()
     * and false otherwise. */
    bool                        fUseKernelAPI;
    /** The current owner of the balloon.
     * This is automatically assigned to the first session using the ballooning
     * API and first released when the session closes. */
    PVPOXGUESTSESSION           pOwner;
    /** The pointer to the array of memory objects holding the chunks of the
     *  balloon.  This array is cMaxChunks in size when present. */
    PRTR0MEMOBJ                 paMemObj;
} VPOXGUESTMEMBALLOON;
/** Pointer to a memory balloon. */
typedef VPOXGUESTMEMBALLOON *PVPOXGUESTMEMBALLOON;


/**
 * Per bit usage tracker for a uint32_t mask.
 *
 * Used for optimal handling of guest properties, mouse status and event filter.
 */
typedef struct VPOXGUESTBITUSAGETRACER
{
    /** Per bit usage counters. */
    uint32_t        acPerBitUsage[32];
    /** The current mask according to acPerBitUsage. */
    uint32_t        fMask;
} VPOXGUESTBITUSAGETRACER;
/** Pointer to a per bit usage tracker.  */
typedef VPOXGUESTBITUSAGETRACER *PVPOXGUESTBITUSAGETRACER;
/** Pointer to a const per bit usage tracker.  */
typedef VPOXGUESTBITUSAGETRACER const *PCVPOXGUESTBITUSAGETRACER;


/**
 * VPox guest device (data) extension.
 */
typedef struct VPOXGUESTDEVEXT
{
    /** VPOXGUESTDEVEXT_INIT_STATE_XXX.   */
    uint32_t                    uInitState;
    /** The base of the adapter I/O ports. */
    RTIOPORT                    IOPortBase;
    /** Pointer to the mapping of the VMMDev adapter memory. */
    VMMDevMemory volatile      *pVMMDevMemory;
    /** The memory object reserving space for the guest mappings. */
    RTR0MEMOBJ                  hGuestMappings;
    /** Spinlock protecting the signaling and resetting of the wait-for-event
     * semaphores as well as the event acking in the ISR. */
    RTSPINLOCK                  EventSpinlock;
    /** Host feature flags (VMMDEV_HVF_XXX).   */
    uint32_t                    fHostFeatures;
    /** Preallocated VMMDevEvents for the IRQ handler. */
    VMMDevEvents               *pIrqAckEvents;
    /** The physical address of pIrqAckEvents. */
    RTCCPHYS                    PhysIrqAckEvents;
    /** Wait-for-event list for threads waiting for multiple events
     * (VPOXGUESTWAIT). */
    RTLISTANCHOR                WaitList;
#ifdef VPOX_WITH_HGCM
    /** Wait-for-event list for threads waiting on HGCM async completion
     * (VPOXGUESTWAIT).
     *
     * The entire list is evaluated upon the arrival of an HGCM event, unlike
     * the other lists which are only evaluated till the first thread has
     * been woken up. */
    RTLISTANCHOR                HGCMWaitList;
#endif
#ifdef VPOXGUEST_USE_DEFERRED_WAKE_UP
    /** List of wait-for-event entries that needs waking up
     * (VPOXGUESTWAIT). */
    RTLISTANCHOR                WakeUpList;
#endif
    /** List of wait-for-event entries that has been woken up
     * (VPOXGUESTWAIT). */
    RTLISTANCHOR                WokenUpList;
    /** List of free wait-for-event entries (VPOXGUESTWAIT). */
    RTLISTANCHOR                FreeList;
    /** Mask of pending events. */
    uint32_t volatile           f32PendingEvents;
    /** Current VMMDEV_EVENT_MOUSE_POSITION_CHANGED sequence number.
     * Used to implement polling.  */
    uint32_t volatile           u32MousePosChangedSeq;

    /** Spinlock various items in the VPOXGUESTSESSION. */
    RTSPINLOCK                  SessionSpinlock;
    /** List of guest sessions (VPOXGUESTSESSION).  We currently traverse this
     * but do not search it, so a list data type should be fine.  Use under the
     * #SessionSpinlock lock. */
    RTLISTANCHOR                SessionList;
    /** Number of session. */
    uint32_t                    cSessions;
    /** Flag indicating whether logging to the release log
     *  is enabled. */
    bool                        fLoggingEnabled;
    /** Memory balloon information for RTR0MemObjAllocPhysNC(). */
    VPOXGUESTMEMBALLOON         MemBalloon;
    /** Mouse notification callback function. */
    PFNVPOXGUESTMOUSENOTIFY     pfnMouseNotifyCallback;
    /** The callback argument for the mouse ntofication callback. */
    void                       *pvMouseNotifyCallbackArg;

    /** @name Host Event Filtering
     * @{ */
    /** Events we won't permit anyone to filter out. */
    uint32_t                    fFixedEvents;
    /** Usage counters for the host events. (Fixed events are not included.) */
    VPOXGUESTBITUSAGETRACER     EventFilterTracker;
    /** The event filter last reported to the host (UINT32_MAX on failure). */
    uint32_t                    fEventFilterHost;
    /** @} */

    /** @name Mouse Status
     * @{ */
    /** Usage counters for the mouse statuses (VMMDEV_MOUSE_XXX). */
    VPOXGUESTBITUSAGETRACER     MouseStatusTracker;
    /** The mouse status last reported to the host (UINT32_MAX on failure). */
    uint32_t                    fMouseStatusHost;
    /** @} */

    /** @name Guest Capabilities
     * @{ */
    /** Guest capabilities which have been set to "acquire" mode.  This means
     * that only one session can use them at a time, and that they will be
     * automatically cleaned up if that session exits without doing so.
     *
     * Protected by VPOXGUESTDEVEXT::SessionSpinlock, but is unfortunately read
     * without holding the lock in a couple of places. */
    uint32_t volatile           fAcquireModeGuestCaps;
    /** Guest capabilities which have been set to "set" mode.  This just means
     * that they have been blocked from ever being set to "acquire" mode. */
    uint32_t                    fSetModeGuestCaps;
    /** Mask of all capabilities which are currently acquired by some session
     * and as such reported to the host. */
    uint32_t                    fAcquiredGuestCaps;
    /** Usage counters for guest capabilities in "set" mode. Indexed by
     *  capability bit number, one count per session using a capability. */
    VPOXGUESTBITUSAGETRACER     SetGuestCapsTracker;
    /** The guest capabilities last reported to the host (UINT32_MAX on failure). */
    uint32_t                    fGuestCapsHost;
    /** @} */

    /** Heartbeat timer which fires with interval
      * cNsHearbeatInterval and its handler sends
      * VMMDevReq_GuestHeartbeat to VMMDev. */
    PRTTIMER                    pHeartbeatTimer;
    /** Heartbeat timer interval in nanoseconds. */
    uint64_t                    cNsHeartbeatInterval;
    /** Preallocated VMMDevReq_GuestHeartbeat request. */
    VMMDevRequestHeader         *pReqGuestHeartbeat;
} VPOXGUESTDEVEXT;
/** Pointer to the VPoxGuest driver data. */
typedef VPOXGUESTDEVEXT *PVPOXGUESTDEVEXT;

/** @name VPOXGUESTDEVEXT_INIT_STATE_XXX - magic values for validating init
 *        state of the device extension structur.
 * @{ */
#define VPOXGUESTDEVEXT_INIT_STATE_FUNDAMENT        UINT32_C(0x0badcafe)
#define VPOXGUESTDEVEXT_INIT_STATE_RESOURCES        UINT32_C(0xcafebabe)
#define VPOXGUESTDEVEXT_INIT_STATE_DELETED          UINT32_C(0xdeadd0d0)
/** @} */

/**
 * The VPoxGuest per session data.
 */
typedef struct VPOXGUESTSESSION
{
    /** The list node. */
    RTLISTNODE                  ListNode;
#if defined(RT_OS_DARWIN) || defined(RT_OS_FREEBSD) || defined(RT_OS_OS2) || defined(RT_OS_SOLARIS)
    /** Pointer to the next session with the same hash. */
    PVPOXGUESTSESSION           pNextHash;
#endif
#if defined(RT_OS_OS2)
    /** The system file number of this session. */
    uint16_t                    sfn;
    uint16_t                    Alignment; /**< Alignment */
#endif
    /** The requestor information to pass to the host for this session.
     * @sa VMMDevRequestHeader::fRequestor */
    uint32_t                    fRequestor;
    /** The process (id) of the session.
     * This is NIL if it's a kernel session. */
    RTPROCESS                   Process;
    /** Which process this session is associated with.
     * This is NIL if it's a kernel session. */
    RTR0PROCESS                 R0Process;
    /** Pointer to the device extension. */
    PVPOXGUESTDEVEXT            pDevExt;

#ifdef VPOX_WITH_HGCM
    /** Array containing HGCM client IDs associated with this session.
     * This will be automatically disconnected when the session is closed. */
    uint32_t volatile           aHGCMClientIds[64];
#endif
    /** The last consumed VMMDEV_EVENT_MOUSE_POSITION_CHANGED sequence number.
     * Used to implement polling.  */
    uint32_t volatile           u32MousePosChangedSeq;
    /** Host events requested by the session.
     * An event type requested in any guest session will be added to the host
     * filter.  Protected by VPOXGUESTDEVEXT::SessionSpinlock. */
    uint32_t                    fEventFilter;
    /** Guest capabilities held in "acquired" by this session.
     * Protected by VPOXGUESTDEVEXT::SessionSpinlock, but is unfortunately read
     * without holding the lock in a couple of places. */
    uint32_t volatile           fAcquiredGuestCaps;
    /** Guest capabilities in "set" mode for this session.
     * These accumulated for sessions via VPOXGUESTDEVEXT::acGuestCapsSet and
     * reported to the host.  Protected by VPOXGUESTDEVEXT::SessionSpinlock.  */
    uint32_t                    fCapabilities;
    /** Mouse features supported.  A feature enabled in any guest session will
     * be enabled for the host.
     * @note We invert the VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR feature in this
     * bitmap.  The logic of this is that the real feature is when the host
     * cursor is not needed, and we tell the host it is not needed if any
     * session explicitly fails to assert it.  Storing it inverted simplifies
     * the checks.
     * Use under the VPOXGUESTDEVEXT#SessionSpinlock lock. */
    uint32_t                    fMouseStatus;
#ifdef RT_OS_DARWIN
    /** Pointer to the associated org_virtualpox_VPoxGuestClient object. */
    void                       *pvVPoxGuestClient;
    /** Whether this session has been opened or not. */
    bool                        fOpened;
#endif
    /** Whether a CANCEL_ALL_WAITEVENTS is pending.  This happens when
     * CANCEL_ALL_WAITEVENTS is called, but no call to WAITEVENT is in process
     * in the current session.  In that case the next call will be interrupted
     * at once. */
    bool volatile               fPendingCancelWaitEvents;
    /** Does this session belong to a root process or a user one? */
    bool                        fUserSession;
} VPOXGUESTSESSION;

RT_C_DECLS_BEGIN

int  VGDrvCommonInitDevExt(PVPOXGUESTDEVEXT pDevExt, uint16_t IOPortBase, void *pvMMIOBase, uint32_t cbMMIO,
                           VPOXOSTYPE enmOSType, uint32_t fEvents);
void VGDrvCommonDeleteDevExt(PVPOXGUESTDEVEXT pDevExt);

int  VGDrvCommonInitLoggers(void);
void VGDrvCommonDestroyLoggers(void);
int  VGDrvCommonInitDevExtFundament(PVPOXGUESTDEVEXT pDevExt);
void VGDrvCommonDeleteDevExtFundament(PVPOXGUESTDEVEXT pDevExt);
int  VGDrvCommonInitDevExtResources(PVPOXGUESTDEVEXT pDevExt, uint16_t IOPortBase,
                                    void *pvMMIOBase, uint32_t cbMMIO, VPOXOSTYPE enmOSType, uint32_t fFixedEvents);
void VGDrvCommonDeleteDevExtResources(PVPOXGUESTDEVEXT pDevExt);
int  VGDrvCommonReinitDevExtAfterHibernation(PVPOXGUESTDEVEXT pDevExt, VPOXOSTYPE enmOSType);

bool VBDrvCommonIsOptionValueTrue(const char *pszValue);
void VGDrvCommonProcessOption(PVPOXGUESTDEVEXT pDevExt, const char *pszName, const char *pszValue);
void VGDrvCommonProcessOptionsFromHost(PVPOXGUESTDEVEXT pDevExt);
bool VGDrvCommonIsOurIRQ(PVPOXGUESTDEVEXT pDevExt);
bool VGDrvCommonISR(PVPOXGUESTDEVEXT pDevExt);

#ifdef VPOXGUEST_USE_DEFERRED_WAKE_UP
void VGDrvCommonWaitDoWakeUps(PVPOXGUESTDEVEXT pDevExt);
#endif

int  VGDrvCommonCreateUserSession(PVPOXGUESTDEVEXT pDevExt, uint32_t fRequestor, PVPOXGUESTSESSION *ppSession);
int  VGDrvCommonCreateKernelSession(PVPOXGUESTDEVEXT pDevExt, PVPOXGUESTSESSION *ppSession);
void VGDrvCommonCloseSession(PVPOXGUESTDEVEXT pDevExt, PVPOXGUESTSESSION pSession);

int  VGDrvCommonIoCtlFast(uintptr_t iFunction, PVPOXGUESTDEVEXT pDevExt, PVPOXGUESTSESSION pSession);
int  VGDrvCommonIoCtl(uintptr_t iFunction, PVPOXGUESTDEVEXT pDevExt, PVPOXGUESTSESSION pSession,
                      PVBGLREQHDR pReqHdr, size_t cbReq);

/**
 * ISR callback for notifying threads polling for mouse events.
 *
 * This is called at the end of the ISR, after leaving the event spinlock, if
 * VMMDEV_EVENT_MOUSE_POSITION_CHANGED was raised by the host.
 *
 * @param   pDevExt     The device extension.
 */
void VGDrvNativeISRMousePollEvent(PVPOXGUESTDEVEXT pDevExt);

/**
 * Hook for handling OS specfic options from the host.
 *
 * @returns true if handled, false if not.
 * @param   pDevExt         The device extension.
 * @param   pszName         The option name.
 * @param   pszValue        The option value.
 */
bool VGDrvNativeProcessOption(PVPOXGUESTDEVEXT pDevExt, const char *pszName, const char *pszValue);


#ifdef VPOX_WITH_DPC_LATENCY_CHECKER
int VGDrvNtIOCtl_DpcLatencyChecker(void);
#endif

#ifdef VPOXGUEST_MOUSE_NOTIFY_CAN_PREEMPT
int VGDrvNativeSetMouseNotifyCallback(PVPOXGUESTDEVEXT pDevExt, PVBGLIOCSETMOUSENOTIFYCALLBACK pNotify);
#endif

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_common_VPoxGuest_VPoxGuestInternal_h */

