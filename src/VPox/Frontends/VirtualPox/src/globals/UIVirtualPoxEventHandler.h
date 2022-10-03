/* $Id: UIVirtualPoxEventHandler.h $ */
/** @file
 * VPox Qt GUI - UIVirtualPoxEventHandler class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIVirtualPoxEventHandler_h
#define FEQT_INCLUDED_SRC_globals_UIVirtualPoxEventHandler_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"
#include "CMediumAttachment.h"

/* Forward declarations: */
class UIVirtualPoxEventHandlerProxy;

/** Singleton QObject extension
  * providing GUI with the CVirtualPoxClient and CVirtualPox event-sources. */
class SHARED_LIBRARY_STUFF UIVirtualPoxEventHandler : public QObject
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

    /** Returns singleton instance. */
    static UIVirtualPoxEventHandler *instance();
    /** Destroys singleton instance. */
    static void destroy();

protected:

    /** Constructs VirtualPox event handler. */
    UIVirtualPoxEventHandler();

    /** Prepares all. */
    void prepare();
    /** Prepares connections. */
    void prepareConnections();

private:

    /** Holds the singleton instance. */
    static UIVirtualPoxEventHandler *s_pInstance;

    /** Holds the VirtualPox event proxy instance. */
    UIVirtualPoxEventHandlerProxy *m_pProxy;
};

/** Singleton VirtualPox Event Handler 'official' name. */
#define gVPoxEvents UIVirtualPoxEventHandler::instance()

#endif /* !FEQT_INCLUDED_SRC_globals_UIVirtualPoxEventHandler_h */

