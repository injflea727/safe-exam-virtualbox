/* $Id: UIVirtualPoxEventHandler.cpp $ */
/** @file
 * VPox Qt GUI - UIVirtualPoxEventHandler class implementation.
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

/* GUI includes: */
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIMainEventListener.h"
#include "UIVirtualPoxEventHandler.h"

/* COM includes: */
#include "CEventListener.h"
#include "CEventSource.h"
#include "CVirtualPox.h"
#include "CVirtualPoxClient.h"


/** Private QObject extension
  * providing UIVirtualPoxEventHandler with the CVirtualPoxClient and CVirtualPox event-sources. */
class UIVirtualPoxEventHandlerProxy : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about the VPoxSVC become @a fAvailable. */
    void sigVPoxSVCAvailabilityChange(bool fAvailable);

    /** Notifies about @a state change event for the machine with @a uId. */
    void sigMachineStateChange(const QUuid &uId, const KMachineState state);
    /** Notifies about data change event for the machine with @a uId. */
    void sigMachineDataChange(const QUuid &uId);
    /** Notifies about machine with @a uId was @a fRegistered. */
    void sigMachineRegistered(const QUuid &uId, const bool fRegistered);
    /** Notifies about @a state change event for the session of the machine with @a uId. */
    void sigSessionStateChange(const QUuid &uId, const KSessionState state);
    /** Notifies about snapshot with @a uSnapshotId was taken for the machine with @a uId. */
    void sigSnapshotTake(const QUuid &uId, const QUuid &uSnapshotId);
    /** Notifies about snapshot with @a uSnapshotId was deleted for the machine with @a uId. */
    void sigSnapshotDelete(const QUuid &uId, const QUuid &uSnapshotId);
    /** Notifies about snapshot with @a uSnapshotId was changed for the machine with @a uId. */
    void sigSnapshotChange(const QUuid &uId, const QUuid &uSnapshotId);
    /** Notifies about snapshot with @a uSnapshotId was restored for the machine with @a uId. */
    void sigSnapshotRestore(const QUuid &uId, const QUuid &uSnapshotId);

    /** Notifies about storage controller change.
      * @param  uMachineId         Brings the ID of machine corresponding controller belongs to.
      * @param  strControllerName  Brings the name of controller this event is related to. */
    void sigStorageControllerChange(const QUuid &uMachineId, const QString &strControllerName);
    /** Notifies about storage device change.
      * @param  comAttachment  Brings corresponding attachment.
      * @param  fRemoved       Brings whether medium is removed or added.
      * @param  fSilent        Brings whether this change has gone silent for guest. */
    void sigStorageDeviceChange(CMediumAttachment comAttachment, bool fRemoved, bool fSilent);
    /** Notifies about storage medium @a comAttachment state change. */
    void sigMediumChange(CMediumAttachment comAttachment);
    /** Notifies about storage @a comMedium config change. */
    void sigMediumConfigChange(CMedium comMedium);
    /** Notifies about storage medium is (un)registered.
      * @param  uMediumId      Brings corresponding medium ID.
      * @param  enmMediumType  Brings corresponding medium type.
      * @param  fRegistered    Brings whether medium is registered or unregistered. */
    void sigMediumRegistered(const QUuid &uMediumId, KDeviceType enmMediumType, bool fRegistered);

public:

    /** Constructs event proxy object on the basis of passed @a pParent. */
    UIVirtualPoxEventHandlerProxy(QObject *pParent);
    /** Destructs event proxy object. */
    ~UIVirtualPoxEventHandlerProxy();

protected:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares listener. */
        void prepareListener();
        /** Prepares connections. */
        void prepareConnections();

        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups listener. */
        void cleanupListener();
        /** Cleanups all. */
        void cleanup();
    /** @} */

private:

    /** Holds the COM event source instance. */
    CEventSource m_comEventSource;

    /** Holds the Qt event listener instance. */
    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;
    /** Holds the COM event listener instance. */
    CEventListener m_comEventListener;
};


/*********************************************************************************************************************************
*   Class UIVirtualPoxEventHandlerProxy implementation.                                                                          *
*********************************************************************************************************************************/

UIVirtualPoxEventHandlerProxy::UIVirtualPoxEventHandlerProxy(QObject *pParent)
    : QObject(pParent)
{
    /* Prepare: */
    prepare();
}

UIVirtualPoxEventHandlerProxy::~UIVirtualPoxEventHandlerProxy()
{
    /* Cleanup: */
    cleanup();
}

void UIVirtualPoxEventHandlerProxy::prepare()
{
    /* Prepare: */
    prepareListener();
    prepareConnections();
}

void UIVirtualPoxEventHandlerProxy::prepareListener()
{
    /* Create Main event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Get VirtualPoxClient: */
    const CVirtualPoxClient comVPoxClient = uiCommon().virtualPoxClient();
    AssertWrapperOk(comVPoxClient);
    /* Get VirtualPoxClient event source: */
    CEventSource comEventSourceVPoxClient = comVPoxClient.GetEventSource();
    AssertWrapperOk(comEventSourceVPoxClient);

    /* Get VirtualPox: */
    const CVirtualPox comVPox = uiCommon().virtualPox();
    AssertWrapperOk(comVPox);
    /* Get VirtualPox event source: */
    CEventSource comEventSourceVPox = comVPox.GetEventSource();
    AssertWrapperOk(comEventSourceVPox);

    /* Create event source aggregator: */
    m_comEventSource = comEventSourceVPoxClient.CreateAggregator(QVector<CEventSource>()
                                                                 << comEventSourceVPoxClient
                                                                 << comEventSourceVPox);

    /* Enumerate all the required event-types: */
    QVector<KVPoxEventType> eventTypes;
    eventTypes
        /* For VirtualPoxClient: */
        << KVPoxEventType_OnVPoxSVCAvailabilityChanged
        /* For VirtualPox: */
        << KVPoxEventType_OnMachineStateChanged
        << KVPoxEventType_OnMachineDataChanged
        << KVPoxEventType_OnMachineRegistered
        << KVPoxEventType_OnSessionStateChanged
        << KVPoxEventType_OnSnapshotTaken
        << KVPoxEventType_OnSnapshotDeleted
        << KVPoxEventType_OnSnapshotChanged
        << KVPoxEventType_OnSnapshotRestored
        << KVPoxEventType_OnStorageControllerChanged
        << KVPoxEventType_OnStorageDeviceChanged
        << KVPoxEventType_OnMediumChanged
        << KVPoxEventType_OnMediumConfigChanged
        << KVPoxEventType_OnMediumRegistered;

    /* Register event listener for event source aggregator: */
    m_comEventSource.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(m_comEventSource);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(m_comEventSource, m_comEventListener);
    }
}

void UIVirtualPoxEventHandlerProxy::prepareConnections()
{
    /* Create direct (sync) connections for signals of main event listener.
     * Keep in mind that the abstract Qt4 connection notation should be used here. */
    connect(m_pQtListener->getWrapped(), SIGNAL(sigVPoxSVCAvailabilityChange(bool)),
            this, SIGNAL(sigVPoxSVCAvailabilityChange(bool)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            this, SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMachineDataChange(QUuid)),
            this, SIGNAL(sigMachineDataChange(QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMachineRegistered(QUuid, bool)),
            this, SIGNAL(sigMachineRegistered(QUuid, bool)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            this, SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigStorageControllerChange(QUuid, QString)),
            this, SIGNAL(sigStorageControllerChange(QUuid, QString)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigStorageDeviceChange(CMediumAttachment, bool, bool)),
            this, SIGNAL(sigStorageDeviceChange(CMediumAttachment, bool, bool)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMediumChange(CMediumAttachment)),
            this, SIGNAL(sigMediumChange(CMediumAttachment)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMediumConfigChange(CMedium)),
            this, SIGNAL(sigMediumConfigChange(CMedium)),
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), SIGNAL(sigMediumRegistered(QUuid, KDeviceType, bool)),
            this, SIGNAL(sigMediumRegistered(QUuid, KDeviceType, bool)),
            Qt::DirectConnection);
}

void UIVirtualPoxEventHandlerProxy::cleanupConnections()
{
    /* Nothing for now. */
}

void UIVirtualPoxEventHandlerProxy::cleanupListener()
{
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        m_pQtListener->getWrapped()->unregisterSources();
    }

    /* Unregister event listener for event source aggregator: */
    m_comEventSource.UnregisterListener(m_comEventListener);
    m_comEventSource.detach();
}

void UIVirtualPoxEventHandlerProxy::cleanup()
{
    /* Cleanup: */
    cleanupConnections();
    cleanupListener();
}


/*********************************************************************************************************************************
*   Class UIVirtualPoxEventHandler implementation.                                                                               *
*********************************************************************************************************************************/

/* static */
UIVirtualPoxEventHandler *UIVirtualPoxEventHandler::s_pInstance = 0;

/* static */
UIVirtualPoxEventHandler *UIVirtualPoxEventHandler::instance()
{
    if (!s_pInstance)
        s_pInstance = new UIVirtualPoxEventHandler;
    return s_pInstance;
}

/* static */
void UIVirtualPoxEventHandler::destroy()
{
    if (s_pInstance)
    {
        delete s_pInstance;
        s_pInstance = 0;
    }
}

UIVirtualPoxEventHandler::UIVirtualPoxEventHandler()
    : m_pProxy(new UIVirtualPoxEventHandlerProxy(this))
{
    /* Prepare: */
    prepare();
}

void UIVirtualPoxEventHandler::prepare()
{
    /* Prepare connections: */
    prepareConnections();
}

void UIVirtualPoxEventHandler::prepareConnections()
{
    /* Create queued (async) connections for signals of event proxy object.
     * Keep in mind that the abstract Qt4 connection notation should be used here. */
    connect(m_pProxy, SIGNAL(sigVPoxSVCAvailabilityChange(bool)),
            this, SIGNAL(sigVPoxSVCAvailabilityChange(bool)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            this, SIGNAL(sigMachineStateChange(QUuid, KMachineState)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMachineDataChange(QUuid)),
            this, SIGNAL(sigMachineDataChange(QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMachineRegistered(QUuid, bool)),
            this, SIGNAL(sigMachineRegistered(QUuid, bool)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            this, SIGNAL(sigSessionStateChange(QUuid, KSessionState)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotTake(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotDelete(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotChange(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            this, SIGNAL(sigSnapshotRestore(QUuid, QUuid)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigStorageControllerChange(QUuid, QString)),
            this, SIGNAL(sigStorageControllerChange(QUuid, QString)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigStorageDeviceChange(CMediumAttachment, bool, bool)),
            this, SIGNAL(sigStorageDeviceChange(CMediumAttachment, bool, bool)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMediumChange(CMediumAttachment)),
            this, SIGNAL(sigMediumChange(CMediumAttachment)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMediumConfigChange(CMedium)),
            this, SIGNAL(sigMediumConfigChange(CMedium)),
            Qt::QueuedConnection);
    connect(m_pProxy, SIGNAL(sigMediumRegistered(QUuid, KDeviceType, bool)),
            this, SIGNAL(sigMediumRegistered(QUuid, KDeviceType, bool)),
            Qt::QueuedConnection);
}


#include "UIVirtualPoxEventHandler.moc"

