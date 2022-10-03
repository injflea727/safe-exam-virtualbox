/* $Id: UIVirtualPoxManager.cpp $ */
/** @file
 * VPox Qt GUI - UIVirtualPoxManager class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QMenuBar>
#include <QStandardPaths>
#include <QStatusBar>
//# include <QToolButton>

/* GUI includes: */
#include "QIFileDialog.h"
#include "UIActionPoolManager.h"
#include "UICloudProfileManager.h"
#include "UIDesktopServices.h"
#include "UIExtraDataManager.h"
#include "UIHostNetworkManager.h"
#include "UIMedium.h"
#include "UIMediumManager.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIVirtualPoxManager.h"
#include "UIVirtualPoxManagerWidget.h"
#include "UISettingsDialogSpecific.h"
#include "UIVMLogViewerDialog.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIVirtualMachineItemLocal.h"
#ifdef VPOX_GUI_WITH_NETWORK_MANAGER
# include "UIUpdateManager.h"
#endif
#include "UIVirtualPoxEventHandler.h"
#include "UIWizardCloneVM.h"
#include "UIWizardExportApp.h"
#include "UIWizardImportApp.h"
#include "UIWizardNewCloudVM.h"
#ifdef VPOX_WS_MAC
# include "UIImageTools.h"
# include "UIWindowMenuManager.h"
# include "VPoxUtils.h"
#endif
#ifdef VPOX_WS_X11
# include "UIDesktopWidgetWatchdog.h"
#endif
#ifndef VPOX_WS_MAC
# include "UIMenuBar.h"
#endif

/* COM includes: */
#include "CSystemProperties.h"

/* Other VPox stuff: */
#include <iprt/buildconfig.h>
#include <VPox/version.h>
#ifdef VPOX_WS_X11
# include <iprt/env.h>
#endif /* VPOX_WS_X11 */


/* static */
UIVirtualPoxManager *UIVirtualPoxManager::s_pInstance = 0;

/* static */
void UIVirtualPoxManager::create()
{
    /* Make sure VirtualPox Manager isn't created: */
    AssertReturnVoid(s_pInstance == 0);

    /* Create VirtualPox Manager: */
    new UIVirtualPoxManager;
    /* Prepare VirtualPox Manager: */
    s_pInstance->prepare();
    /* Show VirtualPox Manager: */
    s_pInstance->show();
    /* Register in the modal window manager: */
    windowManager().setMainWindowShown(s_pInstance);
}

/* static */
void UIVirtualPoxManager::destroy()
{
    /* Make sure VirtualPox Manager is created: */
    AssertPtrReturnVoid(s_pInstance);

    /* Unregister in the modal window manager: */
    windowManager().setMainWindowShown(0);
    /* Cleanup VirtualPox Manager: */
    s_pInstance->cleanup();
    /* Destroy machine UI: */
    delete s_pInstance;
}

UIVirtualPoxManager::UIVirtualPoxManager()
    : m_fPolished(false)
    , m_fFirstMediumEnumerationHandled(false)
    , m_pActionPool(0)
    , m_pManagerVirtualMedia(0)
    , m_pManagerHostNetwork(0)
    , m_pManagerCloudProfile(0)
{
    s_pInstance = this;
}

UIVirtualPoxManager::~UIVirtualPoxManager()
{
    s_pInstance = 0;
}

bool UIVirtualPoxManager::shouldBeMaximized() const
{
    return gEDataManager->selectorWindowShouldBeMaximized();
}

#ifdef VPOX_WS_MAC
bool UIVirtualPoxManager::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore for non-active window except for FileOpen event which should be always processed: */
    if (!isActiveWindow() && pEvent->type() != QEvent::FileOpen)
        return QMainWindowWithRestorableGeometryAndRetranslateUi::eventFilter(pObject, pEvent);

    /* Ignore for other objects: */
    if (qobject_cast<QWidget*>(pObject) &&
        qobject_cast<QWidget*>(pObject)->window() != this)
        return QMainWindowWithRestorableGeometryAndRetranslateUi::eventFilter(pObject, pEvent);

    /* Which event do we have? */
    switch (pEvent->type())
    {
        case QEvent::FileOpen:
        {
            sltHandleOpenUrlCall(QList<QUrl>() << static_cast<QFileOpenEvent*>(pEvent)->url());
            pEvent->accept();
            return true;
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QMainWindowWithRestorableGeometryAndRetranslateUi::eventFilter(pObject, pEvent);
}
#endif /* VPOX_WS_MAC */

void UIVirtualPoxManager::retranslateUi()
{
    /* Set window title: */
    QString strTitle(VPOX_PRODUCT);
    strTitle += " " + tr("Manager", "Note: main window title which is prepended by the product name.");
#ifdef VPOX_BLEEDING_EDGE
    strTitle += QString(" EXPERIMENTAL build ")
             +  QString(RTBldCfgVersion())
             +  QString(" r")
             +  QString(RTBldCfgRevisionStr())
             +  QString(" - " VPOX_BLEEDING_EDGE);
#endif /* VPOX_BLEEDING_EDGE */
    setWindowTitle(strTitle);
}

bool UIVirtualPoxManager::event(QEvent *pEvent)
{
    /* Which event do we have? */
    switch (pEvent->type())
    {
        /* Handle every ScreenChangeInternal event to notify listeners: */
        case QEvent::ScreenChangeInternal:
        {
            emit sigWindowRemapped();
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QMainWindowWithRestorableGeometryAndRetranslateUi::event(pEvent);
}

void UIVirtualPoxManager::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindowWithRestorableGeometryAndRetranslateUi::showEvent(pEvent);

    /* Is polishing required? */
    if (!m_fPolished)
    {
        /* Pass the show-event to polish-event: */
        polishEvent(pEvent);
        /* Mark as polished: */
        m_fPolished = true;
    }
}

void UIVirtualPoxManager::polishEvent(QShowEvent *)
{
    /* Make sure user warned about inaccessible media: */
    QMetaObject::invokeMethod(this, "sltHandleMediumEnumerationFinish", Qt::QueuedConnection);
}

void UIVirtualPoxManager::closeEvent(QCloseEvent *pEvent)
{
    /* Call to base-class: */
    QMainWindowWithRestorableGeometryAndRetranslateUi::closeEvent(pEvent);

    /* Quit application: */
    QApplication::quit();
}

#ifdef VPOX_WS_X11
void UIVirtualPoxManager::sltHandleHostScreenAvailableAreaChange()
{
    /* Prevent handling if fake screen detected: */
    if (gpDesktop->isFakeScreenDetected())
        return;

    /* Restore the geometry cached by the window: */
    const QRect geo = currentGeometry();
    resize(geo.size());
    move(geo.topLeft());
}
#endif /* VPOX_WS_X11 */

void UIVirtualPoxManager::sltHandleMediumEnumerationFinish()
{
#if 0 // ohh, come on!
    /* To avoid annoying the user, we check for inaccessible media just once, after
     * the first media emumeration [started from main() at startup] is complete. */
    if (m_fFirstMediumEnumerationHandled)
        return;
    m_fFirstMediumEnumerationHandled = true;

    /* Make sure MM window/tool is not opened,
     * otherwise user sees everything himself: */
    if (   m_pManagerVirtualMedia
        || m_pWidget->isGlobalToolOpened(UIToolType_Media))
        return;

    /* Look for at least one inaccessible medium: */
    bool fIsThereAnyInaccessibleMedium = false;
    foreach (const QUuid &uMediumID, uiCommon().mediumIDs())
    {
        if (uiCommon().medium(uMediumID).state() == KMediumState_Inaccessible)
        {
            fIsThereAnyInaccessibleMedium = true;
            break;
        }
    }
    /* Warn the user about inaccessible medium, propose to open MM window/tool: */
    if (fIsThereAnyInaccessibleMedium && msgCenter().warnAboutInaccessibleMedia())
    {
        /* Open the MM window: */
        sltOpenVirtualMediumManagerWindow();
    }
#endif
}

void UIVirtualPoxManager::sltHandleOpenUrlCall(QList<QUrl> list /* = QList<QUrl>() */)
{
    /* If passed list is empty, we take the one from UICommon: */
    if (list.isEmpty())
        list = uiCommon().takeArgumentUrls();

    /* Check if we are can handle the dropped urls: */
    for (int i = 0; i < list.size(); ++i)
    {
#ifdef VPOX_WS_MAC
        const QString strFile = ::darwinResolveAlias(list.at(i).toLocalFile());
#else
        const QString strFile = list.at(i).toLocalFile();
#endif
        /* If there is such file exists: */
        if (!strFile.isEmpty() && QFile::exists(strFile))
        {
            /* And has allowed VPox config file extension: */
            if (UICommon::hasAllowedExtension(strFile, VPoxFileExts))
            {
                /* Handle VPox config file: */
                CVirtualPox comVPox = uiCommon().virtualPox();
                CMachine comMachine = comVPox.FindMachine(strFile);
                if (comVPox.isOk() && comMachine.isNotNull())
                    uiCommon().launchMachine(comMachine);
                else
                    sltOpenAddMachineDialog(strFile);
            }
            /* And has allowed VPox OVF file extension: */
            else if (UICommon::hasAllowedExtension(strFile, OVFFileExts))
            {
                /* Allow only one file at the time: */
                sltOpenImportApplianceWizard(strFile);
                break;
            }
            /* And has allowed VPox extension pack file extension: */
            else if (UICommon::hasAllowedExtension(strFile, VPoxExtPackFileExts))
            {
#ifdef VPOX_GUI_WITH_NETWORK_MANAGER
                /* Prevent update manager from proposing us to update EP: */
                gUpdateManager->setEPInstallationRequested(true);
#endif
                /* Propose the user to install EP described by the arguments @a list. */
                uiCommon().doExtPackInstallation(strFile, QString(), this, NULL);
#ifdef VPOX_GUI_WITH_NETWORK_MANAGER
                /* Allow update manager to propose us to update EP: */
                gUpdateManager->setEPInstallationRequested(false);
#endif
            }
        }
    }
}

void UIVirtualPoxManager::sltHandleChooserPaneIndexChange()
{
    updateActionsVisibility();
    updateActionsAppearance();
}

void UIVirtualPoxManager::sltHandleGroupSavingProgressChange()
{
    updateActionsAppearance();
}

void UIVirtualPoxManager::sltHandleToolTypeChange()
{
    updateActionsVisibility();
    updateActionsAppearance();

    /* Make sure separate dialogs are closed when corresponding tools are opened: */
    switch (m_pWidget->toolsType())
    {
        case UIToolType_Media:   sltCloseVirtualMediumManagerWindow(); break;
        case UIToolType_Network: sltCloseHostNetworkManagerWindow(); break;
        case UIToolType_Cloud:   sltCloseCloudProfileManagerWindow(); break;
        case UIToolType_Logs:    sltCloseLogViewerWindow(); break;
        default: break;
    }
}

void UIVirtualPoxManager::sltCurrentSnapshotItemChange()
{
    updateActionsAppearance();
}

void UIVirtualPoxManager::sltHandleCloudMachineStateChange(const QString /* strMachineId */)
{
    updateActionsAppearance();
}

void UIVirtualPoxManager::sltHandleStateChange(const QUuid &)
{
    updateActionsAppearance();
}

void UIVirtualPoxManager::sltOpenVirtualMediumManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isGlobalToolOpened(UIToolType_Media))
    {
        m_pWidget->setToolsType(UIToolType_Welcome);
        m_pWidget->closeGlobalTool(UIToolType_Media);
    }

    /* Create instance if not yet created: */
    if (!m_pManagerVirtualMedia)
    {
        UIMediumManagerFactory(m_pActionPool).prepare(m_pManagerVirtualMedia, this);
        connect(m_pManagerVirtualMedia, &QIManagerDialog::sigClose,
                this, &UIVirtualPoxManager::sltCloseVirtualMediumManagerWindow);
    }

    /* Show instance: */
    m_pManagerVirtualMedia->show();
    m_pManagerVirtualMedia->setWindowState(m_pManagerVirtualMedia->windowState() & ~Qt::WindowMinimized);
    m_pManagerVirtualMedia->activateWindow();
}

void UIVirtualPoxManager::sltCloseVirtualMediumManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerVirtualMedia)
        UIMediumManagerFactory().cleanup(m_pManagerVirtualMedia);
}

void UIVirtualPoxManager::sltOpenHostNetworkManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isGlobalToolOpened(UIToolType_Network))
    {
        m_pWidget->setToolsType(UIToolType_Welcome);
        m_pWidget->closeGlobalTool(UIToolType_Network);
    }

    /* Create instance if not yet created: */
    if (!m_pManagerHostNetwork)
    {
        UIHostNetworkManagerFactory(m_pActionPool).prepare(m_pManagerHostNetwork, this);
        connect(m_pManagerHostNetwork, &QIManagerDialog::sigClose,
                this, &UIVirtualPoxManager::sltCloseHostNetworkManagerWindow);
    }

    /* Show instance: */
    m_pManagerHostNetwork->show();
    m_pManagerHostNetwork->setWindowState(m_pManagerHostNetwork->windowState() & ~Qt::WindowMinimized);
    m_pManagerHostNetwork->activateWindow();
}

void UIVirtualPoxManager::sltCloseHostNetworkManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerHostNetwork)
        UIHostNetworkManagerFactory().cleanup(m_pManagerHostNetwork);
}

void UIVirtualPoxManager::sltOpenCloudProfileManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isGlobalToolOpened(UIToolType_Cloud))
    {
        m_pWidget->setToolsType(UIToolType_Welcome);
        m_pWidget->closeGlobalTool(UIToolType_Cloud);
    }

    /* Create instance if not yet created: */
    if (!m_pManagerCloudProfile)
    {
        UICloudProfileManagerFactory(m_pActionPool).prepare(m_pManagerCloudProfile, this);
        connect(m_pManagerCloudProfile, &QIManagerDialog::sigClose,
                this, &UIVirtualPoxManager::sltCloseCloudProfileManagerWindow);
        connect(m_pManagerCloudProfile, &QIManagerDialog::sigChange,
                this, &UIVirtualPoxManager::sigCloudProfileManagerChange);
    }

    /* Show instance: */
    m_pManagerCloudProfile->show();
    m_pManagerCloudProfile->setWindowState(m_pManagerCloudProfile->windowState() & ~Qt::WindowMinimized);
    m_pManagerCloudProfile->activateWindow();
}

void UIVirtualPoxManager::sltCloseCloudProfileManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerCloudProfile)
        UIHostNetworkManagerFactory().cleanup(m_pManagerCloudProfile);
}

void UIVirtualPoxManager::sltOpenImportApplianceWizard(const QString &strFileName /* = QString() */)
{
    /* Initialize variables: */
#ifdef VPOX_WS_MAC
    const QString strTmpFile = ::darwinResolveAlias(strFileName);
#else
    const QString strTmpFile = strFileName;
#endif

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance)->setProperty("opened", true);
    updateActionsAppearance();

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizardImportApp pWizard = new UIWizardImportApp(pWizardParent, false /* OCI by default? */, strTmpFile);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    if (strFileName.isEmpty() || pWizard->isValid())
        pWizard->exec();
    delete pWizard;

    /* Unlock the action allowing further calls: */
    if (actionPool())
    {
        actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance)->setProperty("opened", QVariant());
        updateActionsAppearance();
    }
}

void UIVirtualPoxManager::sltOpenExportApplianceWizard()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();

    /* Populate the list of VM names: */
    QStringList names;
    for (int i = 0; i < items.size(); ++i)
        names << items.at(i)->name();

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)->setProperty("opened", true);
    actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI)->setProperty("opened", true);
    updateActionsAppearance();

    /* Check what was the action invoked us: */
    UIAction *pAction = qobject_cast<UIAction*>(sender());

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizard pWizard = new UIWizardExportApp(pWizardParent, names,
                                                        pAction &&
                                                        pAction == actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI));
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;

    /* Unlock the action allowing further calls: */
    if (actionPool())
    {
        actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)->setProperty("opened", QVariant());
        actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI)->setProperty("opened", QVariant());
        updateActionsAppearance();
    }
}

void UIVirtualPoxManager::sltOpenNewCloudVMWizard()
{
    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM)->setProperty("opened", true);
    updateActionsAppearance();

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizardNewCloudVM pWizard = new UIWizardNewCloudVM(pWizardParent);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;

    /* Unlock the action allowing further calls: */
    if (actionPool())
    {
        actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM)->setProperty("opened", QVariant());
        updateActionsAppearance();
    }
}

#ifdef VPOX_GUI_WITH_EXTRADATA_MANAGER_UI
void UIVirtualPoxManager::sltOpenExtraDataManagerWindow()
{
    gEDataManager->openWindow(this);
}
#endif /* VPOX_GUI_WITH_EXTRADATA_MANAGER_UI */

void UIVirtualPoxManager::sltOpenPreferencesDialog()
{
    /* Don't show the inaccessible warning
     * if the user tries to open global settings: */
    m_fFirstMediumEnumerationHandled = true;

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setProperty("opened", true);
    updateActionsAppearance();

    /* Create and execute global settings window: */
    QPointer<UISettingsDialogGlobal> pDlg = new UISettingsDialogGlobal(this);
    pDlg->execute();
    delete pDlg;

    /* Unlock the action allowing further calls: */
    if (actionPool())
    {
        actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setProperty("opened", QVariant());
        updateActionsAppearance();
    }
}

void UIVirtualPoxManager::sltPerformExit()
{
    close();
}

void UIVirtualPoxManager::sltOpenAddMachineDialog(const QString &strFileName /* = QString() */)
{
    /* Initialize variables: */
#ifdef VPOX_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFileName);
#else
    QString strTmpFile = strFileName;
#endif
    CVirtualPox comVPox = uiCommon().virtualPox();

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_Welcome_S_Add)->setProperty("opened", true);
    updateActionsAppearance();

    /* No file specified: */
    if (strTmpFile.isEmpty())
    {
        QString strBaseFolder = comVPox.GetSystemProperties().GetDefaultMachineFolder();
        QString strTitle = tr("Select a virtual machine file");
        QStringList extensions;
        for (int i = 0; i < VPoxFileExts.size(); ++i)
            extensions << QString("*.%1").arg(VPoxFileExts[i]);
        QString strFilter = tr("Virtual machine files (%1)").arg(extensions.join(" "));
        /* Create open file dialog: */
        QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);
        if (!fileNames.isEmpty())
            strTmpFile = fileNames.at(0);
    }

    /* Unlock the action allowing further calls: */
    if (actionPool())
    {
        actionPool()->action(UIActionIndexST_M_Welcome_S_Add)->setProperty("opened", QVariant());
        updateActionsAppearance();
    }

    /* Nothing was chosen? */
    if (strTmpFile.isEmpty())
        return;

    /* Make sure this machine can be opened: */
    CMachine comMachineNew = comVPox.OpenMachine(strTmpFile);
    if (!comVPox.isOk())
    {
        msgCenter().cannotOpenMachine(comVPox, strTmpFile);
        return;
    }

    /* Make sure this machine was NOT registered already: */
    CMachine comMachineOld = comVPox.FindMachine(comMachineNew.GetId().toString());
    if (!comMachineOld.isNull())
    {
        msgCenter().cannotReregisterExistingMachine(strTmpFile, comMachineOld.GetName());
        return;
    }

    /* Register that machine: */
    comVPox.RegisterMachine(comMachineNew);
}

void UIVirtualPoxManager::sltOpenMachineSettingsDialog(QString strCategory /* = QString() */,
                                                       QString strControl /* = QString() */,
                                                       const QUuid &uID /* = QString() */)
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setProperty("opened", true);
    updateActionsAppearance();

    /* Process href from VM details / description: */
    if (!strCategory.isEmpty() && strCategory[0] != '#')
    {
        uiCommon().openURL(strCategory);
    }
    else
    {
        /* Check if control is coded into the URL by %%: */
        if (strControl.isEmpty())
        {
            QStringList parts = strCategory.split("%%");
            if (parts.size() == 2)
            {
                strCategory = parts.at(0);
                strControl = parts.at(1);
            }
        }

        /* Don't show the inaccessible warning
         * if the user tries to open VM settings: */
        m_fFirstMediumEnumerationHandled = true;

        /* Create and execute corresponding VM settings window: */
        QPointer<UISettingsDialogMachine> pDlg = new UISettingsDialogMachine(this,
                                                                             uID.isNull() ? pItem->id() : uID,
                                                                             strCategory, strControl);
        pDlg->execute();
        delete pDlg;
    }

    /* Unlock the action allowing further calls: */
    if (actionPool())
    {
        actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setProperty("opened", QVariant());
        updateActionsAppearance();
    }
}

void UIVirtualPoxManager::sltOpenCloneMachineWizard()
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));
    /* Make sure current item is local one: */
    UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
    AssertMsgReturnVoid(pItemLocal, ("Current item should be local one!\n"));

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    const QStringList &machineGroupNames = pItemLocal->groups();
    const QString strGroup = !machineGroupNames.isEmpty() ? machineGroupNames.at(0) : QString();
    UISafePointerWizard pWizard = new UIWizardCloneVM(pWizardParent, pItemLocal->machine(), strGroup);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;
}

void UIVirtualPoxManager::sltPerformMachineMove()
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Open a session thru which we will modify the machine: */
    CSession comSession = uiCommon().openSession(pItem->id(), KLockType_Write);
    if (comSession.isNull())
        return;

    /* Get session machine: */
    CMachine comMachine = comSession.GetMachine();
    AssertMsgReturnVoid(comSession.isOk() && comMachine.isNotNull(), ("Unable to acquire machine!\n"));

    /* Open a file dialog for the user to select a destination folder. Start with the default machine folder: */
    CVirtualPox comVPox = uiCommon().virtualPox();
    QString strBaseFolder = comVPox.GetSystemProperties().GetDefaultMachineFolder();
    QString strTitle = tr("Select a destination folder to move the selected virtual machine");
    QString strDestinationFolder = QIFileDialog::getExistingDirectory(strBaseFolder, this, strTitle);
    if (!strDestinationFolder.isEmpty())
    {
        /* Prepare machine move progress: */
        CProgress comProgress = comMachine.MoveTo(strDestinationFolder, "basic");
        if (comMachine.isOk() && comProgress.isNotNull())
        {
            /* Show machine move progress: */
            msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(), ":/progress_clone_90px.png");
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                msgCenter().cannotMoveMachine(comProgress, comMachine.GetName());
        }
        else
            msgCenter().cannotMoveMachine(comMachine);
    }
    comSession.UnlockMachine();
}

void UIVirtualPoxManager::sltPerformStartOrShowMachine()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Invalid);
}

void UIVirtualPoxManager::sltPerformStartMachineNormal()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Default);
}

void UIVirtualPoxManager::sltPerformStartMachineHeadless()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Headless);
}

void UIVirtualPoxManager::sltPerformStartMachineDetachable()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, UICommon::LaunchMode_Separate);
}

void UIVirtualPoxManager::sltPerformDiscardMachineState()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be discarded: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToDiscard;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Group_S_Discard, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToDiscard << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm discarding saved VM state: */
    if (!msgCenter().confirmDiscardSavedState(machineNames.join(", ")))
        return;

    /* For every confirmed item: */
    foreach (UIVirtualMachineItem *pItem, itemsToDiscard)
    {
        /* Open a session to modify VM: */
        CSession comSession = uiCommon().openSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session machine: */
        CMachine comMachine = comSession.GetMachine();
        comMachine.DiscardSavedState(true);
        if (!comMachine.isOk())
            msgCenter().cannotDiscardSavedState(comMachine);

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualPoxManager::sltPerformPauseOrResumeMachine(bool fPause)
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Get item state: */
        const KMachineState enmState = pItem->machineState();

        /* Check if current item could be paused/resumed: */
        if (!isActionEnabled(UIActionIndexST_M_Group_T_Pause, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Check if current item already paused: */
        if (fPause &&
            (enmState == KMachineState_Paused ||
             enmState == KMachineState_TeleportingPausedVM))
            continue;

        /* Check if current item already resumed: */
        if (!fPause &&
            (enmState == KMachineState_Running ||
             enmState == KMachineState_Teleporting ||
             enmState == KMachineState_LiveSnapshotting))
            continue;

        /* For local machine: */
        if (pItem->itemType() == UIVirtualMachineItem::ItemType_Local)
        {
            /* Open a session to modify VM state: */
            CSession comSession = uiCommon().openExistingSession(pItem->id());
            if (comSession.isNull())
                return;

            /* Get session console: */
            CConsole comConsole = comSession.GetConsole();
            /* Pause/resume VM: */
            if (fPause)
                comConsole.Pause();
            else
                comConsole.Resume();
            if (!comConsole.isOk())
            {
                if (fPause)
                    msgCenter().cannotPauseMachine(comConsole);
                else
                    msgCenter().cannotResumeMachine(comConsole);
            }

            /* Unlock machine finally: */
            comSession.UnlockMachine();
        }
        /* For real cloud machine: */
        else if (pItem->itemType() == UIVirtualMachineItem::ItemType_CloudReal)
        {
            if (fPause)
                pItem->toCloud()->pause(this);
            else
                pItem->toCloud()->resume(this);
        }
    }
}

void UIVirtualPoxManager::sltPerformResetMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be reseted: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToReset;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Group_S_Reset, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToReset << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm reseting VM: */
    if (!msgCenter().confirmResetMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToReset)
    {
        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* Reset VM: */
        comConsole.Reset();

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualPoxManager::sltPerformDetachMachineUI()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if current item could be detached: */
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Detach, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /// @todo Detach separate UI process..
        AssertFailed();
    }
}

void UIVirtualPoxManager::sltPerformSaveMachineState()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if current item could be saved: */
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* Get session machine: */
        CMachine comMachine = comSession.GetMachine();
        /* Pause VM first if necessary: */
        if (pItem->machineState() != KMachineState_Paused)
            comConsole.Pause();
        if (comConsole.isOk())
        {
            /* Prepare machine state saving progress: */
            CProgress comProgress = comMachine.SaveState();
            if (comMachine.isOk())
            {
                /* Show machine state saving progress: */
                msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(), ":/progress_state_save_90px.png");
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotSaveMachineState(comProgress, comMachine.GetName());
            }
            else
                msgCenter().cannotSaveMachineState(comMachine);
        }
        else
            msgCenter().cannotPauseMachine(comConsole);

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualPoxManager::sltPerformShutdownMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be shutdowned: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToShutdown;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToShutdown << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm ACPI shutdown current VM: */
    if (!msgCenter().confirmACPIShutdownMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToShutdown)
    {
        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* ACPI Shutdown: */
        comConsole.PowerButton();
        if (!comConsole.isOk())
            msgCenter().cannotACPIShutdownMachine(comConsole);

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualPoxManager::sltPerformPowerOffMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be powered off: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToPowerOff;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToPowerOff << pItem;
        }
    }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm Power Off current VM: */
    if (!msgCenter().confirmPowerOffMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToPowerOff)
    {
        /* Open a session to modify VM state: */
        CSession comSession = uiCommon().openExistingSession(pItem->id());
        if (comSession.isNull())
            return;

        /* Get session console: */
        CConsole comConsole = comSession.GetConsole();
        /* Prepare machine power down: */
        CProgress comProgress = comConsole.PowerDown();
        if (comConsole.isOk())
        {
            /* Show machine power down progress: */
            CMachine machine = comSession.GetMachine();
            msgCenter().showModalProgressDialog(comProgress, machine.GetName(), ":/progress_poweroff_90px.png");
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                msgCenter().cannotPowerDownMachine(comProgress, machine.GetName());
        }
        else
            msgCenter().cannotPowerDownMachine(comConsole);

        /* Unlock machine finally: */
        comSession.UnlockMachine();
    }
}

void UIVirtualPoxManager::sltPerformShowMachineTool(QAction *pAction)
{
    AssertPtrReturnVoid(pAction);
    AssertPtrReturnVoid(m_pWidget);
    m_pWidget->setToolsType(pAction->property("UIToolType").value<UIToolType>());
}

void UIVirtualPoxManager::sltOpenLogViewerWindow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* First check if instance of widget opened the embedded way: */
    if (m_pWidget->isMachineToolOpened(UIToolType_Logs))
    {
        m_pWidget->setToolsType(UIToolType_Details);
        m_pWidget->closeMachineTool(UIToolType_Logs);
    }

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Make sure current item is local one: */
        UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
        if (!pItemLocal)
            continue;

        /* Check if log could be show for the current item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        QIManagerDialog *pLogViewerDialog = 0;
        /* Create and Show VM Log Viewer: */
        if (!m_logViewers[pItemLocal->machine().GetHardwareUUID().toString()])
        {
            UIVMLogViewerDialogFactory dialogFactory(actionPool(), pItemLocal->machine());
            dialogFactory.prepare(pLogViewerDialog, this);
            if (pLogViewerDialog)
            {
                m_logViewers[pItemLocal->machine().GetHardwareUUID().toString()] = pLogViewerDialog;
                connect(pLogViewerDialog, &QIManagerDialog::sigClose,
                        this, &UIVirtualPoxManager::sltCloseLogViewerWindow);
            }
        }
        else
        {
            pLogViewerDialog = m_logViewers[pItemLocal->machine().GetHardwareUUID().toString()];
        }
        if (pLogViewerDialog)
        {
            /* Show instance: */
            pLogViewerDialog->show();
            pLogViewerDialog->setWindowState(pLogViewerDialog->windowState() & ~Qt::WindowMinimized);
            pLogViewerDialog->activateWindow();
        }
    }
}

void UIVirtualPoxManager::sltCloseLogViewerWindow()
{
    /* If there is a proper sender: */
    if (qobject_cast<QIManagerDialog*>(sender()))
    {
        /* Search for the sender of the signal within the m_logViewers map: */
        QMap<QString, QIManagerDialog*>::iterator sendersIterator = m_logViewers.begin();
        while (sendersIterator != m_logViewers.end() && sendersIterator.value() != sender())
            ++sendersIterator;
        /* Do nothing if we cannot find it with the map: */
        if (sendersIterator == m_logViewers.end())
            return;

        /* Check whether we have found the proper dialog: */
        QIManagerDialog *pDialog = qobject_cast<QIManagerDialog*>(sendersIterator.value());
        if (!pDialog)
            return;

        /* First remove this log-viewer dialog from the map.
         * This should be done before closing the dialog which will incur
         * a second call to this function and result in double delete!!! */
        m_logViewers.erase(sendersIterator);
        UIVMLogViewerDialogFactory().cleanup(pDialog);
    }
    /* Otherwise: */
    else
    {
        /* Just wipe out everything: */
        foreach (const QString &strKey, m_logViewers.keys())
        {
            /* First remove each log-viewer dialog from the map.
             * This should be done before closing the dialog which will incur
             * a second call to this function and result in double delete!!! */
            QIManagerDialog *pDialog = m_logViewers.value(strKey);
            m_logViewers.remove(strKey);
            UIVMLogViewerDialogFactory().cleanup(pDialog);
        }
    }
}

void UIVirtualPoxManager::sltShowMachineInFileManager()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Make sure current item is local one: */
        UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
        if (!pItemLocal)
            continue;

        /* Check if that item could be shown in file-browser: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Show VM in filebrowser: */
        UIDesktopServices::openInFileManager(pItemLocal->machine().GetSettingsFilePath());
    }
}

void UIVirtualPoxManager::sltPerformCreateMachineShortcut()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Make sure current item is local one: */
        UIVirtualMachineItemLocal *pItemLocal = pItem->toLocal();
        if (!pItemLocal)
            continue;

        /* Check if shortcuts could be created for this item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Create shortcut for this VM: */
        const CMachine &comMachine = pItemLocal->machine();
        UIDesktopServices::createMachineShortcut(comMachine.GetSettingsFilePath(),
                                                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                 comMachine.GetName(), comMachine.GetId());
    }
}

void UIVirtualPoxManager::sltGroupCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
}

void UIVirtualPoxManager::sltMachineCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
}

void UIVirtualPoxManager::prepare()
{
#ifdef VPOX_WS_X11
    /* Assign same name to both WM_CLASS name & class for now: */
    UICommon::setWMClass(this, "VirtualPox Manager", "VirtualPox Manager");
#endif

#ifdef VPOX_WS_MAC
    /* We have to make sure that we are getting the front most process: */
    ::darwinSetFrontMostProcess();
    /* Install global event-filter, since vmstarter.app can send us FileOpen events,
     * see UIVirtualPoxManager::eventFilter for handler implementation. */
    qApp->installEventFilter(this);
#endif

    /* Cache media data early if necessary: */
    if (uiCommon().agressiveCaching())
        uiCommon().enumerateMedia();

    /* Prepare: */
    prepareIcon();
    prepareMenuBar();
    prepareStatusBar();
    prepareWidgets();
    prepareConnections();

    /* Update actions initially: */
    updateActionsVisibility();
    updateActionsAppearance();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    retranslateUi();

#ifdef VPOX_WS_MAC
    /* Beta label? */
    if (uiCommon().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* VPOX_WS_MAC */

    /* If there are unhandled URLs we should handle them after manager is shown: */
    if (uiCommon().argumentUrlsPresent())
        QMetaObject::invokeMethod(this, "sltHandleOpenUrlCall", Qt::QueuedConnection);
}

void UIVirtualPoxManager::prepareIcon()
{
    /* Prepare application icon.
     * On Win host it's built-in to the executable.
     * On Mac OS X the icon referenced in info.plist is used.
     * On X11 we will provide as much icons as we can. */
#if !defined(VPOX_WS_WIN) && !defined(VPOX_WS_MAC)
    QIcon icon(":/VirtualPox.svg");
    icon.addFile(":/VirtualPox_48px.png");
    icon.addFile(":/VirtualPox_64px.png");
    setWindowIcon(icon);
#endif /* !VPOX_WS_WIN && !VPOX_WS_MAC */
}

void UIVirtualPoxManager::prepareMenuBar()
{
#ifndef VPOX_WS_MAC
    /* Create menu-bar: */
    setMenuBar(new UIMenuBar);
    if (menuBar())
    {
        /* Make sure menu-bar fills own solid background: */
        menuBar()->setAutoFillBackground(true);
        QPalette pal = menuBar()->palette();
        const QColor color = pal.color(QPalette::Active, QPalette::Mid).lighter(160);
        pal.setColor(QPalette::Active, QPalette::Button, color);
        menuBar()->setPalette(pal);
    }
#endif

    /* Create action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Manager);

    /* Build menu-bar: */
    foreach (QMenu *pMenu, actionPool()->menus())
    {
#ifdef VPOX_WS_MAC
        /* Before 'Help' menu we should: */
        if (pMenu == actionPool()->action(UIActionIndex_Menu_Help)->menu())
        {
            /* Insert 'Window' menu: */
            UIWindowMenuManager::create();
            menuBar()->addMenu(gpWindowMenuManager->createMenu(this));
            gpWindowMenuManager->addWindow(this);
        }
#endif
        menuBar()->addMenu(pMenu);
    }

    /* Setup menu-bar policy: */
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void UIVirtualPoxManager::prepareStatusBar()
{
    /* We are not using status-bar anymore: */
    statusBar()->setHidden(true);
}

void UIVirtualPoxManager::prepareWidgets()
{
    /* Create central-widget: */
    m_pWidget = new UIVirtualPoxManagerWidget(this);
    if (m_pWidget)
    {
        /* Configure central-widget: */
        connect(m_pWidget, &UIVirtualPoxManagerWidget::sigCloudProfileManagerChange,
                this, &UIVirtualPoxManager::sigCloudProfileManagerChange);
        connect(m_pWidget, &UIVirtualPoxManagerWidget::sigCurrentSnapshotItemChange,
                this, &UIVirtualPoxManager::sltCurrentSnapshotItemChange);
        connect(m_pWidget, &UIVirtualPoxManagerWidget::sigCloudMachineStateChange,
                this, &UIVirtualPoxManager::sltHandleCloudMachineStateChange);
        setCentralWidget(m_pWidget);
    }
}

void UIVirtualPoxManager::prepareConnections()
{
#ifdef VPOX_WS_X11
    /* Desktop event handlers: */
    connect(gpDesktop, &UIDesktopWidgetWatchdog::sigHostScreenWorkAreaResized,
            this, &UIVirtualPoxManager::sltHandleHostScreenAvailableAreaChange);
#endif

    /* Medium enumeration connections: */
    connect(&uiCommon(), &UICommon::sigMediumEnumerationFinished,
            this, &UIVirtualPoxManager::sltHandleMediumEnumerationFinish);

    /* Widget connections: */
    connect(m_pWidget, &UIVirtualPoxManagerWidget::sigChooserPaneIndexChange,
            this, &UIVirtualPoxManager::sltHandleChooserPaneIndexChange);
    connect(m_pWidget, &UIVirtualPoxManagerWidget::sigGroupSavingStateChanged,
            this, &UIVirtualPoxManager::sltHandleGroupSavingProgressChange);
    connect(m_pWidget, &UIVirtualPoxManagerWidget::sigMachineSettingsLinkClicked,
            this, &UIVirtualPoxManager::sltOpenMachineSettingsDialog);
    connect(m_pWidget, &UIVirtualPoxManagerWidget::sigToolTypeChange,
            this, &UIVirtualPoxManager::sltHandleToolTypeChange);
    connect(menuBar(), &QMenuBar::customContextMenuRequested,
            m_pWidget, &UIVirtualPoxManagerWidget::sltHandleContextMenuRequest);

    /* Global VPox event handlers: */
    connect(gVPoxEvents, &UIVirtualPoxEventHandler::sigMachineStateChange,
            this, &UIVirtualPoxManager::sltHandleStateChange);
    connect(gVPoxEvents, &UIVirtualPoxEventHandler::sigSessionStateChange,
            this, &UIVirtualPoxManager::sltHandleStateChange);

    /* 'File' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenVirtualMediumManagerWindow);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenHostNetworkManagerWindow);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowCloudProfileManager), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenCloudProfileManagerWindow);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenImportApplianceWizardDefault);
    connect(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenExportApplianceWizard);
    connect(actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenNewCloudVMWizard);
#ifdef VPOX_GUI_WITH_EXTRADATA_MANAGER_UI
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenExtraDataManagerWindow);
#endif /* VPOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    connect(actionPool()->action(UIActionIndex_M_Application_S_Preferences), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenPreferencesDialog);
    connect(actionPool()->action(UIActionIndexST_M_File_S_Close), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformExit);

    /* 'Welcome' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Welcome_S_Add), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenAddMachineDialogDefault);

    /* 'Group' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Add), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenAddMachineDialogDefault);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartOrShowMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_T_Pause), &UIAction::toggled,
            this, &UIVirtualPoxManager::sltPerformPauseOrResumeMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Reset), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformResetMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Discard), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformDiscardMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenLogViewerWindow);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltShowMachineInFileManager);
    connect(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformCreateMachineShortcut);

    /* 'Machine' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Add), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenAddMachineDialogDefault);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenMachineSettingsDialogDefault);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Clone), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenCloneMachineWizard);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Move), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformMachineMove);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenExportApplianceWizard);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartOrShowMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_T_Pause), &UIAction::toggled,
            this, &UIVirtualPoxManager::sltPerformPauseOrResumeMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Reset), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformResetMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Discard), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformDiscardMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltOpenLogViewerWindow);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltShowMachineInFileManager);
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformCreateMachineShortcut);

    /* 'Group/Start or Show' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartMachineNormal);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartMachineHeadless);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartMachineDetachable);

    /* 'Machine/Start or Show' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartMachineNormal);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartMachineHeadless);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformStartMachineDetachable);

    /* 'Group/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu(), &UIMenu::aboutToShow,
            this, &UIVirtualPoxManager::sltGroupCloseMenuAboutToShow);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformDetachMachineUI);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformSaveMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformShutdownMachine);
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformPowerOffMachine);

    /* 'Machine/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu(), &UIMenu::aboutToShow,
            this, &UIVirtualPoxManager::sltMachineCloseMenuAboutToShow);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformDetachMachineUI);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformSaveMachineState);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformShutdownMachine);
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff), &UIAction::triggered,
            this, &UIVirtualPoxManager::sltPerformPowerOffMachine);

    /* 'Group/Tools' menu connections: */
    connect(actionPool()->actionGroup(UIActionIndexST_M_Group_M_Tools), &QActionGroup::triggered,
            this, &UIVirtualPoxManager::sltPerformShowMachineTool);

    /* 'Machine/Tools' menu connections: */
    connect(actionPool()->actionGroup(UIActionIndexST_M_Machine_M_Tools), &QActionGroup::triggered,
            this, &UIVirtualPoxManager::sltPerformShowMachineTool);
}

void UIVirtualPoxManager::loadSettings()
{
    /* Load window geometry: */
    {
        const QRect geo = gEDataManager->selectorWindowGeometry(this);
        LogRel2(("GUI: UIVirtualPoxManager: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
                 geo.x(), geo.y(), geo.width(), geo.height()));
        restoreGeometry(geo);
    }
}

void UIVirtualPoxManager::saveSettings()
{
    /* Save window geometry: */
    {
        const QRect geo = currentGeometry();
        LogRel2(("GUI: UIVirtualPoxManager: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
                 geo.x(), geo.y(), geo.width(), geo.height()));
        gEDataManager->setSelectorWindowGeometry(geo, isCurrentlyMaximized());
    }
}

void UIVirtualPoxManager::cleanupConnections()
{
    /* Honestly we should disconnect everything here,
     * but for now it's enough to disconnect the most critical. */
    m_pWidget->disconnect(this);
}

void UIVirtualPoxManager::cleanupWidgets()
{
    /* Deconfigure central-widget: */
    setCentralWidget(0);
    /* Destroy central-widget: */
    delete m_pWidget;
    m_pWidget = 0;
}

void UIVirtualPoxManager::cleanupMenuBar()
{
#ifdef VPOX_WS_MAC
    /* Cleanup 'Window' menu: */
    UIWindowMenuManager::destroy();
#endif

    /* Destroy action-pool: */
    UIActionPool::destroy(m_pActionPool);
    m_pActionPool = 0;
}

void UIVirtualPoxManager::cleanup()
{
    /* Close the sub-dialogs first: */
    sltCloseVirtualMediumManagerWindow();
    sltCloseHostNetworkManagerWindow();
    sltCloseCloudProfileManagerWindow();

    /* Save settings: */
    saveSettings();

    /* Cleanup: */
    cleanupConnections();
    cleanupWidgets();
    cleanupMenuBar();
}

UIVirtualMachineItem *UIVirtualPoxManager::currentItem() const
{
    return m_pWidget->currentItem();
}

QList<UIVirtualMachineItem*> UIVirtualPoxManager::currentItems() const
{
    return m_pWidget->currentItems();
}

bool UIVirtualPoxManager::isGroupSavingInProgress() const
{
    return m_pWidget->isGroupSavingInProgress();
}

bool UIVirtualPoxManager::isAllItemsOfOneGroupSelected() const
{
    return m_pWidget->isAllItemsOfOneGroupSelected();
}

bool UIVirtualPoxManager::isSingleGroupSelected() const
{
    return m_pWidget->isSingleGroupSelected();
}

void UIVirtualPoxManager::performStartOrShowVirtualMachines(const QList<UIVirtualMachineItem*> &items, UICommon::LaunchMode enmLaunchMode)
{
    /* Do nothing while group saving is in progress: */
    if (isGroupSavingInProgress())
        return;

    /* Compose the list of startable items: */
    QStringList startableMachineNames;
    QList<UIVirtualMachineItem*> startableItems;
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (isAtLeastOneItemCanBeStarted(QList<UIVirtualMachineItem*>() << pItem))
        {
            startableItems << pItem;
            startableMachineNames << pItem->name();
        }
    }

    /* Initially we have start auto-confirmed: */
    bool fStartConfirmed = true;
    /* But if we have more than one item to start =>
     * We should still ask user for a confirmation: */
    if (startableItems.size() > 1)
        fStartConfirmed = msgCenter().confirmStartMultipleMachines(startableMachineNames.join(", "));

    /* For every item => check if it could be launched: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   isAtLeastOneItemCanBeShown(QList<UIVirtualMachineItem*>() << pItem)
            || (   isAtLeastOneItemCanBeStarted(QList<UIVirtualMachineItem*>() << pItem)
                && fStartConfirmed))
        {
            /* Make sure item is local one: */
            if (pItem->itemType() == UIVirtualMachineItem::ItemType_Local)
            {
                /* Fetch item launch mode: */
                UICommon::LaunchMode enmItemLaunchMode = enmLaunchMode;
                if (enmItemLaunchMode == UICommon::LaunchMode_Invalid)
                    enmItemLaunchMode = pItem->isItemRunningHeadless()
                                      ? UICommon::LaunchMode_Separate
                                      : qApp->keyboardModifiers() == Qt::ShiftModifier
                                      ? UICommon::LaunchMode_Headless
                                      : UICommon::LaunchMode_Default;

                /* Launch current VM: */
                CMachine machine = pItem->toLocal()->machine();
                uiCommon().launchMachine(machine, enmItemLaunchMode);
            }
        }
    }
}

void UIVirtualPoxManager::updateActionsVisibility()
{
    /* Determine whether Machine or Group menu should be shown at all: */
    const bool fGlobalMenuShown  = m_pWidget->isGlobalItemSelected();
    const bool fGroupMenuShown   = m_pWidget->isGroupItemSelected()   &&  isSingleGroupSelected();
    const bool fMachineMenuShown = m_pWidget->isMachineItemSelected() && !isSingleGroupSelected();
    actionPool()->action(UIActionIndexST_M_Welcome)->setVisible(fGlobalMenuShown);
    actionPool()->action(UIActionIndexST_M_Group)->setVisible(fGroupMenuShown);
    actionPool()->action(UIActionIndexST_M_Machine)->setVisible(fMachineMenuShown);

    /* Determine whether Media menu should be visible: */
    const bool fMediumMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_Media;
    actionPool()->action(UIActionIndexST_M_Medium)->setVisible(fMediumMenuShown);
    /* Determine whether Network menu should be visible: */
    const bool fNetworkMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_Network;
    actionPool()->action(UIActionIndexST_M_Network)->setVisible(fNetworkMenuShown);
    /* Determine whether Cloud menu should be visible: */
    const bool fCloudMenuShown = fGlobalMenuShown && m_pWidget->currentGlobalTool() == UIToolType_Cloud;
    actionPool()->action(UIActionIndexST_M_Cloud)->setVisible(fCloudMenuShown);

    /* Determine whether Snapshots menu should be visible: */
    const bool fSnapshotMenuShown = (fMachineMenuShown || fGroupMenuShown) &&
                                    m_pWidget->currentMachineTool() == UIToolType_Snapshots;
    actionPool()->action(UIActionIndexST_M_Snapshot)->setVisible(fSnapshotMenuShown);
    /* Determine whether Logs menu should be visible: */
    const bool fLogViewerMenuShown = (fMachineMenuShown || fGroupMenuShown) &&
                                     m_pWidget->currentMachineTool() == UIToolType_Logs;
    actionPool()->action(UIActionIndex_M_Log)->setVisible(fLogViewerMenuShown);

    /* Hide action shortcuts: */
    if (!fGlobalMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Welcome, false);
    if (!fGroupMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Group, false);
    if (!fMachineMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Machine, false);

    /* Show action shortcuts: */
    if (fGlobalMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Welcome, true);
    if (fGroupMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Group, true);
    if (fMachineMenuShown)
        actionPool()->setShortcutsVisible(UIActionIndexST_M_Machine, true);
}

void UIVirtualPoxManager::updateActionsAppearance()
{
    /* Get current items: */
    QList<UIVirtualMachineItem*> items = currentItems();

    /* Enable/disable File/Application actions: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setEnabled(isActionEnabled(UIActionIndex_M_Application_S_Preferences, items));
    actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)->setEnabled(isActionEnabled(UIActionIndexST_M_File_S_ExportAppliance, items));
    actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance)->setEnabled(isActionEnabled(UIActionIndexST_M_File_S_ImportAppliance, items));
    actionPool()->action(UIActionIndexST_M_File_S_NewCloudVM)->setEnabled(isActionEnabled(UIActionIndexST_M_File_S_NewCloudVM, items));

    /* Enable/disable welcome actions: */
    actionPool()->action(UIActionIndexST_M_Welcome_S_Add)->setEnabled(isActionEnabled(UIActionIndexST_M_Welcome_S_Add, items));

    /* Enable/disable group actions: */
    actionPool()->action(UIActionIndexST_M_Group_S_New)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_New, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Add)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Add, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Rename)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Rename, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Sort)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Sort, items));

    /* Enable/disable machine actions: */
    actionPool()->action(UIActionIndexST_M_Machine_S_New)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_New, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Add)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Add, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Settings, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Clone)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Clone, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Move)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Move, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ExportToOCI)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ExportToOCI, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_AddGroup, items));
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_SortParent)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_SortParent, items));

    /* Enable/disable group-start-or-show actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable, items));

    /* Enable/disable machine-start-or-show actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable, items));

    /* Enable/disable group-close actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Detach, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_PowerOff, items));

    /* Enable/disable machine-close actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Detach, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, items));

    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* Start/Show action is deremined by 1st item: */
    if (pItem && pItem->accessible())
    {
        actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->toActionPolymorphicMenu()->setState(pItem->isItemPoweredOff() ? 0 : 1);
        actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->toActionPolymorphicMenu()->setState(pItem->isItemPoweredOff() ? 0 : 1);
        /// @todo Hmm, fix it?
//        QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)));
//        if (pButton)
//            pButton->setPopupMode(pItem->isItemPoweredOff() ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
    }
    else
    {
        actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->toActionPolymorphicMenu()->setState(0);
        actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->toActionPolymorphicMenu()->setState(0);
        /// @todo Hmm, fix it?
//        QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)));
//        if (pButton)
//            pButton->setPopupMode(pItem->isItemPoweredOff() ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
    }

    /* Pause/Resume action is deremined by 1st started item: */
    UIVirtualMachineItem *pFirstStartedAction = 0;
    foreach (UIVirtualMachineItem *pSelectedItem, items)
    {
        if (pSelectedItem->isItemStarted())
        {
            pFirstStartedAction = pSelectedItem;
            break;
        }
    }
    /* Update the group Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setChecked(pFirstStartedAction && pFirstStartedAction->isItemPaused());
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(false);
    /* Update the machine Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setChecked(pFirstStartedAction && pFirstStartedAction->isItemPaused());
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->blockSignals(false);

    /* Update action toggle states: */
    if (m_pWidget)
    {
        switch (m_pWidget->currentMachineTool())
        {
            case UIToolType_Details:
            {
                actionPool()->action(UIActionIndexST_M_Group_M_Tools_T_Details)->setChecked(true);
                actionPool()->action(UIActionIndexST_M_Machine_M_Tools_T_Details)->setChecked(true);
                break;
            }
            case UIToolType_Snapshots:
            {
                actionPool()->action(UIActionIndexST_M_Group_M_Tools_T_Snapshots)->setChecked(true);
                actionPool()->action(UIActionIndexST_M_Machine_M_Tools_T_Snapshots)->setChecked(true);
                break;
            }
            case UIToolType_Logs:
            {
                actionPool()->action(UIActionIndexST_M_Group_M_Tools_T_Logs)->setChecked(true);
                actionPool()->action(UIActionIndexST_M_Machine_M_Tools_T_Logs)->setChecked(true);
                break;
            }
            default:
                break;
        }
    }
}

bool UIVirtualPoxManager::isActionEnabled(int iActionIndex, const QList<UIVirtualMachineItem*> &items)
{
    /* For known *global* action types: */
    switch (iActionIndex)
    {
        case UIActionIndex_M_Application_S_Preferences:
        case UIActionIndexST_M_File_S_ExportAppliance:
        case UIActionIndexST_M_File_S_ImportAppliance:
        case UIActionIndexST_M_File_S_NewCloudVM:
        case UIActionIndexST_M_Welcome_S_Add:
        {
            return !actionPool()->action(iActionIndex)->property("opened").toBool();
        }
        default:
            break;
    }

    /* No *machine* actions enabled for empty item list: */
    if (items.isEmpty())
        return false;

    /* Get first item: */
    UIVirtualMachineItem *pItem = items.first();

    /* For known *machine* action types: */
    switch (iActionIndex)
    {
        case UIActionIndexST_M_Group_S_New:
        {
            return !isGroupSavingInProgress() &&
                   isSingleGroupSelected();
        }
        case UIActionIndexST_M_Group_S_Add:
        case UIActionIndexST_M_Group_S_Sort:
        {
            return !isGroupSavingInProgress() &&
                   isSingleGroupSelected() &&
                   isItemsLocal(items);
        }
        case UIActionIndexST_M_Group_S_Rename:
        case UIActionIndexST_M_Group_S_Remove:
        {
            return !isGroupSavingInProgress() &&
                   isSingleGroupSelected() &&
                   isItemsLocal(items) &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Machine_S_New:
        case UIActionIndexST_M_Machine_S_Add:
        {
            return !isGroupSavingInProgress() &&
                   isItemsLocal(items);
        }
        case UIActionIndexST_M_Machine_S_Settings:
        {
            return !actionPool()->action(iActionIndex)->property("opened").toBool() &&
                   !isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   pItem->toLocal() &&
                   pItem->configurationAccessLevel() != ConfigurationAccessLevel_Null &&
                   (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                    m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Machine_S_Clone:
        case UIActionIndexST_M_Machine_S_Move:
        {
            return !isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   pItem->toLocal() &&
                   pItem->isItemEditable();
        }
        case UIActionIndexST_M_Machine_S_ExportToOCI:
        {
            return !actionPool()->action(iActionIndex)->property("opened").toBool() &&
                   items.size() == 1 &&
                   pItem->toLocal();
        }
        case UIActionIndexST_M_Machine_S_Remove:
        {
            return !isGroupSavingInProgress() &&
                   isAtLeastOneItemRemovable(items) &&
                   isItemsLocal(items);
        }
        case UIActionIndexST_M_Machine_S_AddGroup:
        {
            return !isGroupSavingInProgress() &&
                   !isAllItemsOfOneGroupSelected() &&
                   isItemsLocal(items) &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Group_M_StartOrShow:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable:
        case UIActionIndexST_M_Machine_M_StartOrShow:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable:
        {
            return !isGroupSavingInProgress() &&
                   isAtLeastOneItemCanBeStartedOrShown(items) &&
                    (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                     m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Group_S_Discard:
        case UIActionIndexST_M_Machine_S_Discard:
        {
            return !isGroupSavingInProgress() &&
                   isAtLeastOneItemDiscardable(items) &&
                    (m_pWidget->currentMachineTool() != UIToolType_Snapshots ||
                     m_pWidget->isCurrentStateItemSelected());
        }
        case UIActionIndexST_M_Group_S_ShowLogDialog:
        case UIActionIndexST_M_Machine_S_ShowLogDialog:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Group_T_Pause:
        case UIActionIndexST_M_Machine_T_Pause:
        {
            return isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_S_Reset:
        case UIActionIndexST_M_Machine_S_Reset:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemRunning(items);
        }
        case UIActionIndexST_M_Group_S_Refresh:
        case UIActionIndexST_M_Machine_S_Refresh:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemInaccessible(items);
        }
        case UIActionIndexST_M_Group_S_ShowInFileManager:
        case UIActionIndexST_M_Machine_S_ShowInFileManager:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Machine_S_SortParent:
        {
            return !isGroupSavingInProgress() &&
                   isItemsLocal(items);
        }
        case UIActionIndexST_M_Group_S_CreateShortcut:
        case UIActionIndexST_M_Machine_S_CreateShortcut:
        {
            return isAtLeastOneItemSupportsShortcuts(items);
        }
        case UIActionIndexST_M_Group_M_Close:
        case UIActionIndexST_M_Machine_M_Close:
        {
            return isItemsLocal(items) &&
                   isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_M_Close_S_Detach:
        case UIActionIndexST_M_Machine_M_Close_S_Detach:
        {
            return isItemsLocal(items) &&
                   isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Group_M_Close_S_SaveState:
        case UIActionIndexST_M_Machine_M_Close_S_SaveState:
        {
            return isItemsLocal(items) &&
                   isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Group_M_Close_S_Shutdown:
        case UIActionIndexST_M_Machine_M_Close_S_Shutdown:
        {
            return isItemsLocal(items) &&
                   isActionEnabled(UIActionIndexST_M_Machine_M_Close, items) &&
                   isAtLeastOneItemAbleToShutdown(items);
        }
        case UIActionIndexST_M_Group_M_Close_S_PowerOff:
        case UIActionIndexST_M_Machine_M_Close_S_PowerOff:
        {
            return isItemsLocal(items) &&
                   isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        default:
            break;
    }

    /* Unknown actions are disabled: */
    return false;
}

/* static */
bool UIVirtualPoxManager::isItemsLocal(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->toLocal())
            return false;
    return true;
}

/* static */
bool UIVirtualPoxManager::isItemsPoweredOff(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->isItemPoweredOff())
            return false;
    return true;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemAbleToShutdown(const QList<UIVirtualMachineItem*> &items)
{
    /* Enumerate all the passed items: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Skip non-running machines: */
        if (!pItem->isItemRunning())
            continue;
        /* Skip session failures: */
        CSession session = uiCommon().openExistingSession(pItem->id());
        if (session.isNull())
            continue;
        /* Skip console failures: */
        CConsole console = session.GetConsole();
        if (console.isNull())
        {
            /* Do not forget to release machine: */
            session.UnlockMachine();
            continue;
        }
        /* Is the guest entered ACPI mode? */
        bool fGuestEnteredACPIMode = console.GetGuestEnteredACPIMode();
        /* Do not forget to release machine: */
        session.UnlockMachine();
        /* True if the guest entered ACPI mode: */
        if (fGuestEnteredACPIMode)
            return true;
    }
    /* False by default: */
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemSupportsShortcuts(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   pItem->accessible()
            && pItem->toLocal()
#ifdef VPOX_WS_MAC
            /* On Mac OS X this are real alias files, which don't work with the old legacy xml files. */
            && pItem->toLocal()->settingsFile().endsWith(".vpox", Qt::CaseInsensitive)
#endif
            )
            return true;
    }
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemAccessible(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->accessible())
            return true;
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemInaccessible(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->accessible())
            return true;
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemRemovable(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->accessible() || pItem->isItemEditable())
            return true;
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemCanBeStarted(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (pItem->isItemPoweredOff() && pItem->isItemEditable())
            return true;
    }
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemCanBeShown(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   pItem->toLocal()
            && pItem->isItemStarted()
            && (   pItem->toLocal()->canSwitchTo()
                || pItem->isItemRunningHeadless()))
            return true;
    }
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemCanBeStartedOrShown(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (   pItem->toLocal()
            && (   (   pItem->isItemPoweredOff()
                    && pItem->isItemEditable())
                || (   pItem->isItemStarted()
                    && (   pItem->toLocal()->canSwitchTo()
                        || pItem->isItemRunningHeadless()))))
            return true;
    }
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemDiscardable(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->isItemSaved() && pItem->isItemEditable())
            return true;
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemStarted(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->isItemStarted())
            return true;
    return false;
}

/* static */
bool UIVirtualPoxManager::isAtLeastOneItemRunning(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->isItemRunning())
            return true;
    return false;
}
