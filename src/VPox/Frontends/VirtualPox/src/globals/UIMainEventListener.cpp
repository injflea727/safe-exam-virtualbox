/* $Id: UIMainEventListener.cpp $ */
/** @file
 * VPox Qt GUI - UIMainEventListener class implementation.
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
 */

/* Qt includes: */
#include <QMutex>
#include <QThread>

/* GUI includes: */
#include "UICommon.h"
#include "UIMainEventListener.h"
#include "UIMousePointerShapeData.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCanShowWindowEvent.h"
#include "CClipboardModeChangedEvent.h"
#include "CCursorPositionChangedEvent.h"
#include "CDnDModeChangedEvent.h"
#include "CEvent.h"
#include "CEventSource.h"
#include "CEventListener.h"
#include "CExtraDataCanChangeEvent.h"
#include "CExtraDataChangedEvent.h"
#include "CGuestMonitorChangedEvent.h"
#include "CGuestProcessIOEvent.h"
#include "CGuestProcessRegisteredEvent.h"
#include "CGuestProcessStateChangedEvent.h"
#include "CGuestSessionRegisteredEvent.h"
#include "CGuestSessionStateChangedEvent.h"
#include "CKeyboardLedsChangedEvent.h"
#include "CMachineDataChangedEvent.h"
#include "CMachineStateChangedEvent.h"
#include "CMachineRegisteredEvent.h"
#include "CMediumChangedEvent.h"
#include "CMediumConfigChangedEvent.h"
#include "CMediumRegisteredEvent.h"
#include "CMouseCapabilityChangedEvent.h"
#include "CMousePointerShapeChangedEvent.h"
#include "CNetworkAdapterChangedEvent.h"
#include "CProgressPercentageChangedEvent.h"
#include "CProgressTaskCompletedEvent.h"
#include "CRuntimeErrorEvent.h"
#include "CSessionStateChangedEvent.h"
#include "CShowWindowEvent.h"
#include "CSnapshotChangedEvent.h"
#include "CSnapshotDeletedEvent.h"
#include "CSnapshotRestoredEvent.h"
#include "CSnapshotTakenEvent.h"
#include "CStateChangedEvent.h"
#include "CStorageControllerChangedEvent.h"
#include "CStorageDeviceChangedEvent.h"
#include "CUSBDevice.h"
#include "CUSBDeviceStateChangedEvent.h"
#include "CVPoxSVCAvailabilityChangedEvent.h"
#include "CVirtualPoxErrorInfo.h"


/** Private QThread extension allowing to listen for Main events in separate thread.
  * This thread listens for a Main events infinitely unless creator calls for #setShutdown. */
class UIMainEventListeningThread : public QThread
{
    Q_OBJECT;

public:

    /** Constructs Main events listener thread redirecting events from @a comSource to @a comListener. */
    UIMainEventListeningThread(const CEventSource &comSource, const CEventListener &comListener);
    /** Destructs Main events listener thread. */
    ~UIMainEventListeningThread();

protected:

    /** Contains the thread excution body. */
    virtual void run() /* override */;

    /** Returns whether the thread asked to shutdown prematurely. */
    bool isShutdown() const;
    /** Defines whether the thread asked to @a fShutdown prematurely. */
    void setShutdown(bool fShutdown);

private:

    /** Holds the Main event source reference. */
    CEventSource m_comSource;
    /** Holds the Main event listener reference. */
    CEventListener m_comListener;

    /** Holds the mutex instance which protects thread access. */
    mutable QMutex m_mutex;
    /** Holds whether the thread asked to shutdown prematurely. */
    bool m_fShutdown;
};


/*********************************************************************************************************************************
*   Class UIMainEventListeningThread implementation.                                                                             *
*********************************************************************************************************************************/

UIMainEventListeningThread::UIMainEventListeningThread(const CEventSource &comSource, const CEventListener &comListener)
    : m_comSource(comSource)
    , m_comListener(comListener)
    , m_fShutdown(false)
{
    setObjectName("UIMainEventListeningThread");
}

UIMainEventListeningThread::~UIMainEventListeningThread()
{
    /* Make a request to shutdown: */
    setShutdown(true);

    /* And wait 30 seconds for run() to finish (1 sec increments to help with
       delays incurred debugging and prevent suicidal use-after-free behaviour): */
    uint32_t i = 30000;
    do
        wait(1000);
    while (i-- > 0 && !isFinished());
}

void UIMainEventListeningThread::run()
{
    /* Initialize COM: */
    COMBase::InitializeCOM(false);

    /* Copy source wrapper to this thread: */
    CEventSource comSource = m_comSource;
    /* Copy listener wrapper to this thread: */
    CEventListener comListener = m_comListener;

    /* While we are not in shutdown: */
    while (!isShutdown())
    {
        /* Fetch the event from the queue: */
        CEvent comEvent = comSource.GetEvent(comListener, 500);
        if (!comEvent.isNull())
        {
            /* Process the event and tell the listener: */
            comListener.HandleEvent(comEvent);
            if (comEvent.GetWaitable())
                comSource.EventProcessed(comListener, comEvent);
        }
    }

    /* Cleanup COM: */
    COMBase::CleanupCOM();
}

bool UIMainEventListeningThread::isShutdown() const
{
    m_mutex.lock();
    bool fShutdown = m_fShutdown;
    m_mutex.unlock();
    return fShutdown;
}

void UIMainEventListeningThread::setShutdown(bool fShutdown)
{
    m_mutex.lock();
    m_fShutdown = fShutdown;
    m_mutex.unlock();
}


/*********************************************************************************************************************************
*   Class UIMainEventListener implementation.                                                                                    *
*********************************************************************************************************************************/

UIMainEventListener::UIMainEventListener()
{
    /* Register meta-types for required enums. */
    qRegisterMetaType<KDeviceType>("KDeviceType");
    qRegisterMetaType<KMachineState>("KMachineState");
    qRegisterMetaType<KSessionState>("KSessionState");
    qRegisterMetaType< QVector<uint8_t> >("QVector<uint8_t>");
    qRegisterMetaType<CNetworkAdapter>("CNetworkAdapter");
    qRegisterMetaType<CMedium>("CMedium");
    qRegisterMetaType<CMediumAttachment>("CMediumAttachment");
    qRegisterMetaType<CUSBDevice>("CUSBDevice");
    qRegisterMetaType<CVirtualPoxErrorInfo>("CVirtualPoxErrorInfo");
    qRegisterMetaType<KGuestMonitorChangedEventType>("KGuestMonitorChangedEventType");
    qRegisterMetaType<CGuestSession>("CGuestSession");
}

void UIMainEventListener::registerSource(const CEventSource &comSource, const CEventListener &comListener)
{
    /* Make sure source and listener are valid: */
    AssertReturnVoid(!comSource.isNull());
    AssertReturnVoid(!comListener.isNull());

    /* Create thread for passed source: */
    m_threads << new UIMainEventListeningThread(comSource, comListener);
    /* And start it: */
    m_threads.last()->start();
}

void UIMainEventListener::unregisterSources()
{
    /* Wipe out the threads: */
    /** @todo r=bird: The use of qDeleteAll here is unsafe because it won't take
     * QThread::wait() timeouts into account, and may delete the QThread object
     * while the thread is still running, causing heap corruption/crashes once
     * the thread awakens and gets on with its termination.
     * Observed with debugger + paged heap.
     *
     * Should use specialized thread list which only deletes the threads after
     * isFinished() returns true, leaving them alone on timeout failures. */
    qDeleteAll(m_threads);
}

STDMETHODIMP UIMainEventListener::HandleEvent(VPoxEventType_T, IEvent *pEvent)
{
    /* Try to acquire COM cleanup protection token first: */
    if (!uiCommon().comTokenTryLockForRead())
        return S_OK;

    CEvent comEvent(pEvent);
    switch (comEvent.GetType())
    {
        case KVPoxEventType_OnVPoxSVCAvailabilityChanged:
        {
            CVPoxSVCAvailabilityChangedEvent comEventSpecific(pEvent);
            emit sigVPoxSVCAvailabilityChange(comEventSpecific.GetAvailable());
            break;
        }

        case KVPoxEventType_OnMachineStateChanged:
        {
            CMachineStateChangedEvent comEventSpecific(pEvent);
            emit sigMachineStateChange(comEventSpecific.GetMachineId(), comEventSpecific.GetState());
            break;
        }
        case KVPoxEventType_OnMachineDataChanged:
        {
            CMachineDataChangedEvent comEventSpecific(pEvent);
            emit sigMachineDataChange(comEventSpecific.GetMachineId());
            break;
        }
        case KVPoxEventType_OnMachineRegistered:
        {
            CMachineRegisteredEvent comEventSpecific(pEvent);
            emit sigMachineRegistered(comEventSpecific.GetMachineId(), comEventSpecific.GetRegistered());
            break;
        }
        case KVPoxEventType_OnSessionStateChanged:
        {
            CSessionStateChangedEvent comEventSpecific(pEvent);
            emit sigSessionStateChange(comEventSpecific.GetMachineId(), comEventSpecific.GetState());
            break;
        }
        case KVPoxEventType_OnSnapshotTaken:
        {
            CSnapshotTakenEvent comEventSpecific(pEvent);
            emit sigSnapshotTake(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
        case KVPoxEventType_OnSnapshotDeleted:
        {
            CSnapshotDeletedEvent comEventSpecific(pEvent);
            emit sigSnapshotDelete(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
        case KVPoxEventType_OnSnapshotChanged:
        {
            CSnapshotChangedEvent comEventSpecific(pEvent);
            emit sigSnapshotChange(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }
        case KVPoxEventType_OnSnapshotRestored:
        {
            CSnapshotRestoredEvent comEventSpecific(pEvent);
            emit sigSnapshotRestore(comEventSpecific.GetMachineId(), comEventSpecific.GetSnapshotId());
            break;
        }

        case KVPoxEventType_OnExtraDataCanChange:
        {
            CExtraDataCanChangeEvent comEventSpecific(pEvent);
            /* Has to be done in place to give an answer: */
            bool fVeto = false;
            QString strReason;
            emit sigExtraDataCanChange(comEventSpecific.GetMachineId(), comEventSpecific.GetKey(),
                                       comEventSpecific.GetValue(), fVeto, strReason);
            if (fVeto)
                comEventSpecific.AddVeto(strReason);
            break;
        }
        case KVPoxEventType_OnExtraDataChanged:
        {
            CExtraDataChangedEvent comEventSpecific(pEvent);
            emit sigExtraDataChange(comEventSpecific.GetMachineId(), comEventSpecific.GetKey(), comEventSpecific.GetValue());
            break;
        }

        case KVPoxEventType_OnStorageControllerChanged:
        {
            CStorageControllerChangedEvent comEventSpecific(pEvent);
            emit sigStorageControllerChange(comEventSpecific.GetMachinId(),
                                            comEventSpecific.GetControllerName());
            break;
        }
        case KVPoxEventType_OnStorageDeviceChanged:
        {
            CStorageDeviceChangedEvent comEventSpecific(pEvent);
            emit sigStorageDeviceChange(comEventSpecific.GetStorageDevice(),
                                        comEventSpecific.GetRemoved(),
                                        comEventSpecific.GetSilent());
            break;
        }
        case KVPoxEventType_OnMediumChanged:
        {
            CMediumChangedEvent comEventSpecific(pEvent);
            emit sigMediumChange(comEventSpecific.GetMediumAttachment());
            break;
        }
        case KVPoxEventType_OnMediumConfigChanged:
        {
            CMediumConfigChangedEvent comEventSpecific(pEvent);
            emit sigMediumConfigChange(comEventSpecific.GetMedium());
            break;
        }
        case KVPoxEventType_OnMediumRegistered:
        {
            CMediumRegisteredEvent comEventSpecific(pEvent);
            emit sigMediumRegistered(comEventSpecific.GetMediumId(),
                                     comEventSpecific.GetMediumType(),
                                     comEventSpecific.GetRegistered());
            break;
        }

        case KVPoxEventType_OnMousePointerShapeChanged:
        {
            CMousePointerShapeChangedEvent comEventSpecific(pEvent);
            UIMousePointerShapeData shapeData(comEventSpecific.GetVisible(),
                                              comEventSpecific.GetAlpha(),
                                              QPoint(comEventSpecific.GetXhot(), comEventSpecific.GetYhot()),
                                              QSize(comEventSpecific.GetWidth(), comEventSpecific.GetHeight()),
                                              comEventSpecific.GetShape());
            emit sigMousePointerShapeChange(shapeData);
            break;
        }
        case KVPoxEventType_OnMouseCapabilityChanged:
        {
            CMouseCapabilityChangedEvent comEventSpecific(pEvent);
            emit sigMouseCapabilityChange(comEventSpecific.GetSupportsAbsolute(), comEventSpecific.GetSupportsRelative(),
                                          comEventSpecific.GetSupportsMultiTouch(), comEventSpecific.GetNeedsHostCursor());
            break;
        }
        case KVPoxEventType_OnCursorPositionChanged:
        {
            CCursorPositionChangedEvent comEventSpecific(pEvent);
            emit sigCursorPositionChange(comEventSpecific.GetHasData(),
                                         (unsigned long)comEventSpecific.GetX(), (unsigned long)comEventSpecific.GetY());
            break;
        }
        case KVPoxEventType_OnKeyboardLedsChanged:
        {
            CKeyboardLedsChangedEvent comEventSpecific(pEvent);
            emit sigKeyboardLedsChangeEvent(comEventSpecific.GetNumLock(),
                                            comEventSpecific.GetCapsLock(),
                                            comEventSpecific.GetScrollLock());
            break;
        }
        case KVPoxEventType_OnStateChanged:
        {
            CStateChangedEvent comEventSpecific(pEvent);
            emit sigStateChange(comEventSpecific.GetState());
            break;
        }
        case KVPoxEventType_OnAdditionsStateChanged:
        {
            emit sigAdditionsChange();
            break;
        }
        case KVPoxEventType_OnNetworkAdapterChanged:
        {
            CNetworkAdapterChangedEvent comEventSpecific(pEvent);
            emit sigNetworkAdapterChange(comEventSpecific.GetNetworkAdapter());
            break;
        }
        case KVPoxEventType_OnVRDEServerChanged:
        case KVPoxEventType_OnVRDEServerInfoChanged:
        {
            emit sigVRDEChange();
            break;
        }
        case KVPoxEventType_OnRecordingChanged:
        {
            emit sigRecordingChange();
            break;
        }
        case KVPoxEventType_OnUSBControllerChanged:
        {
            emit sigUSBControllerChange();
            break;
        }
        case KVPoxEventType_OnUSBDeviceStateChanged:
        {
            CUSBDeviceStateChangedEvent comEventSpecific(pEvent);
            emit sigUSBDeviceStateChange(comEventSpecific.GetDevice(),
                                         comEventSpecific.GetAttached(),
                                         comEventSpecific.GetError());
            break;
        }
        case KVPoxEventType_OnSharedFolderChanged:
        {
            emit sigSharedFolderChange();
            break;
        }
        case KVPoxEventType_OnCPUExecutionCapChanged:
        {
            emit sigCPUExecutionCapChange();
            break;
        }
        case KVPoxEventType_OnGuestMonitorChanged:
        {
            CGuestMonitorChangedEvent comEventSpecific(pEvent);
            emit sigGuestMonitorChange(comEventSpecific.GetChangeType(), comEventSpecific.GetScreenId(),
                                       QRect(comEventSpecific.GetOriginX(), comEventSpecific.GetOriginY(),
                                             comEventSpecific.GetWidth(), comEventSpecific.GetHeight()));
            break;
        }
        case KVPoxEventType_OnRuntimeError:
        {
            CRuntimeErrorEvent comEventSpecific(pEvent);
            emit sigRuntimeError(comEventSpecific.GetFatal(), comEventSpecific.GetId(), comEventSpecific.GetMessage());
            break;
        }
        case KVPoxEventType_OnCanShowWindow:
        {
            CCanShowWindowEvent comEventSpecific(pEvent);
            /* Has to be done in place to give an answer: */
            bool fVeto = false;
            QString strReason;
            emit sigCanShowWindow(fVeto, strReason);
            if (fVeto)
                comEventSpecific.AddVeto(strReason);
            else
                comEventSpecific.AddApproval(strReason);
            break;
        }
        case KVPoxEventType_OnShowWindow:
        {
            CShowWindowEvent comEventSpecific(pEvent);
            /* Has to be done in place to give an answer: */
            qint64 winId = comEventSpecific.GetWinId();
            if (winId != 0)
                break; /* Already set by some listener. */
            emit sigShowWindow(winId);
            comEventSpecific.SetWinId(winId);
            break;
        }
        case KVPoxEventType_OnAudioAdapterChanged:
        {
            emit sigAudioAdapterChange();
            break;
        }

        case KVPoxEventType_OnProgressPercentageChanged:
        {
            CProgressPercentageChangedEvent comEventSpecific(pEvent);
            emit sigProgressPercentageChange(comEventSpecific.GetProgressId(), (int)comEventSpecific.GetPercent());
            break;
        }
        case KVPoxEventType_OnProgressTaskCompleted:
        {
            CProgressTaskCompletedEvent comEventSpecific(pEvent);
            emit sigProgressTaskComplete(comEventSpecific.GetProgressId());
            break;
        }

        case KVPoxEventType_OnGuestSessionRegistered:
        {
            CGuestSessionRegisteredEvent comEventSpecific(pEvent);
            if (comEventSpecific.GetRegistered())
                emit sigGuestSessionRegistered(comEventSpecific.GetSession());
            else
                emit sigGuestSessionUnregistered(comEventSpecific.GetSession());
            break;
        }
        case KVPoxEventType_OnGuestProcessRegistered:
        {
            CGuestProcessRegisteredEvent comEventSpecific(pEvent);
            if (comEventSpecific.GetRegistered())
                emit sigGuestProcessRegistered(comEventSpecific.GetProcess());
            else
                emit sigGuestProcessUnregistered(comEventSpecific.GetProcess());
            break;
        }
        case KVPoxEventType_OnGuestSessionStateChanged:
        {
            CGuestSessionStateChangedEvent comEventSpecific(pEvent);
            emit sigGuestSessionStatedChanged(comEventSpecific);
            break;
        }
        case KVPoxEventType_OnGuestProcessInputNotify:
        case KVPoxEventType_OnGuestProcessOutput:
        {
            break;
        }
        case KVPoxEventType_OnGuestProcessStateChanged:
        {
            CGuestProcessStateChangedEvent comEventSpecific(pEvent);
            comEventSpecific.GetError();
            emit sigGuestProcessStateChanged(comEventSpecific);
            break;
        }
        case KVPoxEventType_OnGuestFileRegistered:
        case KVPoxEventType_OnGuestFileStateChanged:
        case KVPoxEventType_OnGuestFileOffsetChanged:
        case KVPoxEventType_OnGuestFileRead:
        case KVPoxEventType_OnGuestFileWrite:
        {
            break;
        }
        case KVPoxEventType_OnClipboardModeChanged:
        {
            CClipboardModeChangedEvent comEventSpecific(pEvent);
            emit sigClipboardModeChange(comEventSpecific.GetClipboardMode());
            break;
        }
        case KVPoxEventType_OnDnDModeChanged:
        {
            CDnDModeChangedEvent comEventSpecific(pEvent);
            emit sigDnDModeChange(comEventSpecific.GetDndMode());
            break;
        }
        default: break;
    }

    /* Unlock COM cleanup protection token: */
    uiCommon().comTokenUnlock();

    return S_OK;
}

#include "UIMainEventListener.moc"
