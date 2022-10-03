/* $Id: VPoxUsbPnP.cpp $ */
/** @file
 * USB PnP Handling
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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

#include "VPoxUsbCmn.h"

static NTSTATUS vpoxUsbPnPMnStartDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    IoCopyCurrentIrpStackLocationToNext(pIrp);
    NTSTATUS Status = VPoxDrvToolIoPostSync(pDevExt->pLowerDO, pIrp);
    Assert(NT_SUCCESS(Status) || Status == STATUS_NOT_SUPPORTED);
    if (NT_SUCCESS(Status))
    {
        Status = vpoxUsbRtStart(pDevExt);
        Assert(Status == STATUS_SUCCESS);
        if (NT_SUCCESS(Status))
        {
            vpoxUsbPnPStateSet(pDevExt, ENMVPOXUSB_PNPSTATE_STARTED);
        }
    }

    VPoxDrvToolIoComplete(pIrp, Status, 0);
    vpoxUsbDdiStateRelease(pDevExt);
    return Status;
}

static NTSTATUS vpoxUsbPnPMnQueryStopDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    vpoxUsbPnPStateSet(pDevExt, ENMVPOXUSB_PNPSTATE_STOP_PENDING);

    vpoxUsbDdiStateReleaseAndWaitCompleted(pDevExt);

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    IoSkipCurrentIrpStackLocation(pIrp);
    return IoCallDriver(pDevExt->pLowerDO, pIrp);
}

static NTSTATUS vpoxUsbPnPMnStopDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    vpoxUsbPnPStateSet(pDevExt, ENMVPOXUSB_PNPSTATE_STOPPED);

    vpoxUsbRtClear(pDevExt);

    NTSTATUS Status = VPoxUsbToolDevUnconfigure(pDevExt->pLowerDO);
    Assert(NT_SUCCESS(Status));

    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information = 0;
    IoSkipCurrentIrpStackLocation(pIrp);
    Status = IoCallDriver(pDevExt->pLowerDO, pIrp);

    vpoxUsbDdiStateRelease(pDevExt);
    return Status;
}

static NTSTATUS vpoxUsbPnPMnCancelStopDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    ENMVPOXUSB_PNPSTATE enmState = vpoxUsbPnPStateGet(pDevExt);
    NTSTATUS Status = STATUS_SUCCESS;

    IoCopyCurrentIrpStackLocationToNext(pIrp);
    Status = VPoxDrvToolIoPostSync(pDevExt->pLowerDO, pIrp);
    if (NT_SUCCESS(Status) && enmState == ENMVPOXUSB_PNPSTATE_STOP_PENDING)
    {
        vpoxUsbPnPStateRestore(pDevExt);
    }

    Status = STATUS_SUCCESS;
    VPoxDrvToolIoComplete(pIrp, Status, 0);
    vpoxUsbDdiStateRelease(pDevExt);

    return Status;
}

static NTSTATUS vpoxUsbPnPMnQueryRemoveDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    vpoxUsbPnPStateSet(pDevExt, ENMVPOXUSB_PNPSTATE_REMOVE_PENDING);

    vpoxUsbDdiStateReleaseAndWaitCompleted(pDevExt);

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    IoSkipCurrentIrpStackLocation(pIrp);
    return IoCallDriver(pDevExt->pLowerDO, pIrp);
}

static NTSTATUS vpoxUsbPnPRmDev(PVPOXUSBDEV_EXT pDevExt)
{
    NTSTATUS Status = vpoxUsbRtRm(pDevExt);
    Assert(Status == STATUS_SUCCESS);

    return Status;
}

static NTSTATUS vpoxUsbPnPMnRemoveDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    ENMVPOXUSB_PNPSTATE enmState = vpoxUsbPnPStateGet(pDevExt);
    NTSTATUS Status = STATUS_SUCCESS;
    if (enmState != ENMVPOXUSB_PNPSTATE_SURPRISE_REMOVED)
    {
        Status = vpoxUsbPnPRmDev(pDevExt);
        Assert(Status == STATUS_SUCCESS);
    }

    vpoxUsbPnPStateSet(pDevExt, ENMVPOXUSB_PNPSTATE_REMOVED);

    vpoxUsbDdiStateRelease(pDevExt);

    vpoxUsbDdiStateReleaseAndWaitRemoved(pDevExt);

    vpoxUsbRtClear(pDevExt);

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    IoSkipCurrentIrpStackLocation(pIrp);
    Status = IoCallDriver(pDevExt->pLowerDO, pIrp);

    IoDetachDevice(pDevExt->pLowerDO);
    IoDeleteDevice(pDevExt->pFDO);

    return Status;
}

static NTSTATUS vpoxUsbPnPMnCancelRemoveDevice(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    ENMVPOXUSB_PNPSTATE enmState = vpoxUsbPnPStateGet(pDevExt);
    NTSTATUS Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(pIrp);

    Status = VPoxDrvToolIoPostSync(pDevExt->pLowerDO, pIrp);

    if (NT_SUCCESS(Status) &&
        enmState == ENMVPOXUSB_PNPSTATE_REMOVE_PENDING)
    {
        vpoxUsbPnPStateRestore(pDevExt);
    }

    Status = STATUS_SUCCESS;
    VPoxDrvToolIoComplete(pIrp, Status, 0);
    vpoxUsbDdiStateRelease(pDevExt);

    return Status;
}

static NTSTATUS vpoxUsbPnPMnSurpriseRemoval(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    vpoxUsbPnPStateSet(pDevExt, ENMVPOXUSB_PNPSTATE_SURPRISE_REMOVED);

    NTSTATUS Status = vpoxUsbPnPRmDev(pDevExt);
    Assert(Status == STATUS_SUCCESS);

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    IoSkipCurrentIrpStackLocation(pIrp);
    Status = IoCallDriver(pDevExt->pLowerDO, pIrp);

    vpoxUsbDdiStateRelease(pDevExt);

    return Status;
}

static NTSTATUS vpoxUsbPnPMnQueryCapabilities(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    PIO_STACK_LOCATION pSl = IoGetCurrentIrpStackLocation(pIrp);
    PDEVICE_CAPABILITIES pDevCaps = pSl->Parameters.DeviceCapabilities.Capabilities;

    if (pDevCaps->Version < 1 || pDevCaps->Size < sizeof (*pDevCaps))
    {
        AssertFailed();
        /** @todo return more appropriate status ?? */
        return STATUS_UNSUCCESSFUL;
    }

    pDevCaps->SurpriseRemovalOK = TRUE;
    pIrp->IoStatus.Status = STATUS_SUCCESS;

    IoCopyCurrentIrpStackLocationToNext(pIrp);
    NTSTATUS Status = VPoxDrvToolIoPostSync(pDevExt->pLowerDO, pIrp);
    Assert(NT_SUCCESS(Status));
    if (NT_SUCCESS(Status))
    {
        pDevCaps->SurpriseRemovalOK = 1;
        pDevExt->DdiState.DevCaps = *pDevCaps;
    }

    VPoxDrvToolIoComplete(pIrp, Status, 0);
    vpoxUsbDdiStateRelease(pDevExt);

    return Status;
}

static NTSTATUS vpoxUsbPnPMnDefault(PVPOXUSBDEV_EXT pDevExt, PIRP pIrp)
{
    NTSTATUS Status;
    IoSkipCurrentIrpStackLocation(pIrp);
    Status = IoCallDriver(pDevExt->pLowerDO, pIrp);
    vpoxUsbDdiStateRelease(pDevExt);
    return Status;
}

DECLHIDDEN(NTSTATUS) vpoxUsbDispatchPnP(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
    PVPOXUSBDEV_EXT pDevExt = (PVPOXUSBDEV_EXT)pDeviceObject->DeviceExtension;
    if (!vpoxUsbDdiStateRetainIfNotRemoved(pDevExt))
        return VPoxDrvToolIoComplete(pIrp, STATUS_DELETE_PENDING, 0);

    PIO_STACK_LOCATION pSl = IoGetCurrentIrpStackLocation(pIrp);
    switch (pSl->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            return vpoxUsbPnPMnStartDevice(pDevExt, pIrp);

        case IRP_MN_QUERY_STOP_DEVICE:
            return vpoxUsbPnPMnQueryStopDevice(pDevExt, pIrp);

        case IRP_MN_STOP_DEVICE:
            return vpoxUsbPnPMnStopDevice(pDevExt, pIrp);

        case IRP_MN_CANCEL_STOP_DEVICE:
            return vpoxUsbPnPMnCancelStopDevice(pDevExt, pIrp);

        case IRP_MN_QUERY_REMOVE_DEVICE:
            return vpoxUsbPnPMnQueryRemoveDevice(pDevExt, pIrp);

        case IRP_MN_REMOVE_DEVICE:
            return vpoxUsbPnPMnRemoveDevice(pDevExt, pIrp);

        case IRP_MN_CANCEL_REMOVE_DEVICE:
            return vpoxUsbPnPMnCancelRemoveDevice(pDevExt, pIrp);

        case IRP_MN_SURPRISE_REMOVAL:
            return vpoxUsbPnPMnSurpriseRemoval(pDevExt, pIrp);

        case IRP_MN_QUERY_CAPABILITIES:
            return vpoxUsbPnPMnQueryCapabilities(pDevExt, pIrp);

        default:
            return vpoxUsbPnPMnDefault(pDevExt, pIrp);
    }
}

