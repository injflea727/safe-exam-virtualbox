/* $Id: VPoxUsbMon.cpp $ */
/** @file
 * VPox USB Monitor
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


/*
 *
 *                        Theory of Operation
 *                              - or -
 *        The Document I Wish The Original Author Had Written
 *
 *
 * The USB Monitor (VPoxUSBMon.sys) serves to capture and uncapture USB
 * devices. Its job is to ensure that the USB proxy (VPoxUSB.sys) gets installed
 * for captured devices and removed again when not needed, restoring the regular
 * driver (if any).
 *
 * The USB Monitor does not handle any actual USB traffic; that is the role of
 * VPoxUSB.sys, the USB proxy. A typical solution for installing such USB proxy
 * is using a filter driver, but that approach was rejected because filter drivers
 * cannot be dynamically added and removed. What VPoxUSBMon does instead is hook
 * into the dispatch routine of the bus driver, i.e. USB hub driver, and alter
 * the PnP information returned by the bus driver.
 *
 * The key functionality for capturing is cycling a USB port (which causes a USB
 * device reset and triggers re-enumeration in the Windows USB driver stack), and
 * then modifying IRP_MN_QUERY_ID / BusQueryHardwareIDs and related requests so
 * that they return the synthetic USB VID/PID that VPoxUSB.sys handles rather than
 * the true hardware VID/PID. That causes Windows to install VPoxUSB.sys for the
 * device.
 *
 * Uncapturing again cycles the USB port but returns unmodified hardware IDs,
 * causing Windows to load the normal driver for the device.
 *
 * Identifying devices to capture or release (uncapture) is done through USB filters,
 * a cross-platform concept which matches USB device based on their VID/PID, class,
 * and other criteria.
 *
 * There is an IOCTL interface for adding/removing USB filters and applying them.
 * The IOCTLs are normally issued by VPoxSVC.
 *
 * USB devices are enumerated by finding all USB hubs (GUID_DEVINTERFACE_USB_HUB)
 * and querying their child devices (i.e. USB devices or other hubs) by sending
 * IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / BusRelations. This is done when
 * applying existing filters.
 *
 * Newly arrived USB devices are intercepted early in their PnP enumeration
 * through the hooked bus driver dispatch routine. Devices which satisty the
 * filter matching criteria are morphed (see above) such that VPoxUSB.sys loads
 * for them before any default driver does.
 *
 * There is an IDC interface to VPoxUSB.sys which allows the USB proxy to report
 * that it's installed for a given USB device, and also report when the USB proxy
 * is unloaded (typically caused by either unplugging the device or uncapturing
 * and cycling the port). VPoxUSBMon.sys relies on these IDC calls to track
 * captured devices and be informed when VPoxUSB.sys unloads.
 *
 * Windows 8+ complicates the USB Monitor's life by automatically putting some
 * USB devices to a low-power state where they are unable to respond to any USB
 * requests and VPoxUSBMon can't read any of their descriptors (note that in
 * userland, the device descriptor can always be read, but string descriptors
 * can't). Such devices'  USB VID/PID/revision is recovered using the Windows
 * PnP Manager from their DevicePropertyHardwareID, but their USB class/subclass
 * and protocol unfortunately cannot be unambiguously recovered from their
 * DevicePropertyCompatibleIDs.
 *
 * Filter drivers add another complication. With filter drivers in place, the
 * device objects returned by the BusRelations query (or passing through the PnP
 * hooks) may not be PDOs but rather filter DOs higher in the stack. To avoid
 * confusion, we flatten the references to their base, i.e. the real PDO, which
 * should remain the same for the lifetime of a device. Note that VPoxUSB.sys
 * always passes its own PDO in the proxy startup IOCTL.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VPoxUsbMon.h"
#include "../cmn/VPoxUsbIdc.h"
#include <iprt/errcore.h>
#include <VPox/usblib.h>
#include <excpt.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/*
 * Note: Must match the VID & PID in the USB driver .inf file!!
 */
/*
  BusQueryDeviceID USB\Vid_90EE&Pid_CAFE
  BusQueryInstanceID 2
  BusQueryHardwareIDs USB\Vid_90EE&Pid_CAFE&Rev_0100
  BusQueryHardwareIDs USB\Vid_90EE&Pid_CAFE
  BusQueryCompatibleIDs USB\Class_ff&SubClass_00&Prot_00
  BusQueryCompatibleIDs USB\Class_ff&SubClass_00
  BusQueryCompatibleIDs USB\Class_ff
*/

#define szBusQueryDeviceId       L"USB\\Vid_90EE&Pid_CAFE"
#define szBusQueryHardwareIDs    L"USB\\Vid_90EE&Pid_CAFE&Rev_0100\0USB\\Vid_90EE&Pid_CAFE\0\0"
#define szBusQueryCompatibleIDs  L"USB\\Class_ff&SubClass_00&Prot_00\0USB\\Class_ff&SubClass_00\0USB\\Class_ff\0\0"

#define szDeviceTextDescription          L"VirtualPox USB"


#define VPOXUSBMON_MEMTAG 'MUBV'


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct VPOXUSBMONINS
{
    void * pvDummy;
} VPOXUSBMONINS, *PVPOXUSBMONINS;

typedef struct VPOXUSBMONCTX
{
    VPOXUSBFLTCTX FltCtx;
} VPOXUSBMONCTX, *PVPOXUSBMONCTX;

typedef struct VPOXUSBHUB_PNPHOOK
{
    VPOXUSBHOOK_ENTRY Hook;
    bool fUninitFailed;
} VPOXUSBHUB_PNPHOOK, *PVPOXUSBHUB_PNPHOOK;

typedef struct VPOXUSBHUB_PNPHOOK_COMPLETION
{
    VPOXUSBHOOK_REQUEST Rq;
} VPOXUSBHUB_PNPHOOK_COMPLETION, *PVPOXUSBHUB_PNPHOOK_COMPLETION;

#define VPOXUSBMON_MAXDRIVERS 5
typedef struct VPOXUSB_PNPDRIVER
{
    PDRIVER_OBJECT     DriverObject;
    VPOXUSBHUB_PNPHOOK UsbHubPnPHook;
    PDRIVER_DISPATCH   pfnHookStub;
} VPOXUSB_PNPDRIVER, *PVPOXUSB_PNPDRIVER;

typedef struct VPOXUSBMONGLOBALS
{
    PDEVICE_OBJECT pDevObj;
    VPOXUSB_PNPDRIVER pDrivers[VPOXUSBMON_MAXDRIVERS];
    KEVENT OpenSynchEvent;
    IO_REMOVE_LOCK RmLock;
    uint32_t cOpens;
    volatile LONG ulPreventUnloadOn;
    PFILE_OBJECT pPreventUnloadFileObj;
} VPOXUSBMONGLOBALS, *PVPOXUSBMONGLOBALS;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static VPOXUSBMONGLOBALS g_VPoxUsbMonGlobals;



PVOID VPoxUsbMonMemAlloc(SIZE_T cbBytes)
{
    PVOID pvMem = ExAllocatePoolWithTag(NonPagedPool, cbBytes, VPOXUSBMON_MEMTAG);
    Assert(pvMem);
    return pvMem;
}

PVOID VPoxUsbMonMemAllocZ(SIZE_T cbBytes)
{
    PVOID pvMem = VPoxUsbMonMemAlloc(cbBytes);
    if (pvMem)
    {
        RtlZeroMemory(pvMem, cbBytes);
    }
    return pvMem;
}

VOID VPoxUsbMonMemFree(PVOID pvMem)
{
    ExFreePoolWithTag(pvMem, VPOXUSBMON_MEMTAG);
}

#define VPOXUSBDBG_STRCASE(_t) \
        case _t: return #_t
#define VPOXUSBDBG_STRCASE_UNKNOWN(_v) \
        default: LOG((__FUNCTION__": Unknown Value (0n%d), (0x%x)", _v, _v)); return "Unknown"

/* These minor code are semi-undocumented. */
#ifndef IRP_MN_QUERY_LEGACY_BUS_INFORMATION
#define IRP_MN_QUERY_LEGACY_BUS_INFORMATION 0x18
#endif
#ifndef IRP_MN_DEVICE_ENUMERATED
#define IRP_MN_DEVICE_ENUMERATED 0x19
#endif

static const char* vpoxUsbDbgStrPnPMn(UCHAR uMn)
{
    switch (uMn)
    {
        VPOXUSBDBG_STRCASE(IRP_MN_START_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_REMOVE_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_REMOVE_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_CANCEL_REMOVE_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_STOP_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_STOP_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_CANCEL_STOP_DEVICE);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_DEVICE_RELATIONS);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_INTERFACE);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_CAPABILITIES);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_RESOURCES);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_RESOURCE_REQUIREMENTS);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_DEVICE_TEXT);
        VPOXUSBDBG_STRCASE(IRP_MN_FILTER_RESOURCE_REQUIREMENTS);
        VPOXUSBDBG_STRCASE(IRP_MN_READ_CONFIG);
        VPOXUSBDBG_STRCASE(IRP_MN_WRITE_CONFIG);
        VPOXUSBDBG_STRCASE(IRP_MN_EJECT);
        VPOXUSBDBG_STRCASE(IRP_MN_SET_LOCK);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_ID);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_PNP_DEVICE_STATE);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_BUS_INFORMATION);
        VPOXUSBDBG_STRCASE(IRP_MN_DEVICE_USAGE_NOTIFICATION);
        VPOXUSBDBG_STRCASE(IRP_MN_SURPRISE_REMOVAL);
        VPOXUSBDBG_STRCASE(IRP_MN_QUERY_LEGACY_BUS_INFORMATION);
        VPOXUSBDBG_STRCASE(IRP_MN_DEVICE_ENUMERATED);
        VPOXUSBDBG_STRCASE_UNKNOWN(uMn);
    }
}

/**
 * Send IRP_MN_QUERY_DEVICE_RELATIONS
 *
 * @returns NT Status
 * @param   pDevObj         USB device pointer
 * @param   pFileObj        Valid file object pointer
 * @param   pDevRelations   Pointer to DEVICE_RELATIONS pointer (out)
 */
NTSTATUS VPoxUsbMonQueryBusRelations(PDEVICE_OBJECT pDevObj, PFILE_OBJECT pFileObj, PDEVICE_RELATIONS *pDevRelations)
{
    IO_STATUS_BLOCK IoStatus;
    KEVENT Event;
    NTSTATUS Status;
    PIRP pIrp;
    PIO_STACK_LOCATION pSl;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Assert(pDevRelations);
    *pDevRelations = NULL;

    pIrp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pDevObj, NULL, 0, NULL, &Event, &IoStatus);
    if (!pIrp)
    {
        WARN(("IoBuildDeviceIoControlRequest failed!!"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    pSl = IoGetNextIrpStackLocation(pIrp);
    pSl->MajorFunction = IRP_MJ_PNP;
    pSl->MinorFunction = IRP_MN_QUERY_DEVICE_RELATIONS;
    pSl->Parameters.QueryDeviceRelations.Type = BusRelations;
    pSl->FileObject = pFileObj;

    Status = IoCallDriver(pDevObj, pIrp);
    if (Status == STATUS_PENDING)
    {
        LOG(("IoCallDriver returned STATUS_PENDING!!"));
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatus.Status;
    }

    if (Status == STATUS_SUCCESS)
    {
        PDEVICE_RELATIONS pRel = (PDEVICE_RELATIONS)IoStatus.Information;
        LOG(("pRel = %p", pRel));
        if (VALID_PTR(pRel))
        {
            *pDevRelations = pRel;
        }
        else
        {
            WARN(("Invalid pointer %p", pRel));
        }
    }
    else
    {
        WARN(("IRP_MN_QUERY_DEVICE_RELATIONS failed Status(0x%x)", Status));
    }

    LOG(("IoCallDriver returned %x", Status));
    return Status;
}

VOID vpoxUsbMonHubDevWalk(PFNVPOXUSBMONDEVWALKER pfnWalker, PVOID pvWalker)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PWSTR szwHubList;
    Status = IoGetDeviceInterfaces(&GUID_DEVINTERFACE_USB_HUB, NULL, 0, &szwHubList);
    if (Status != STATUS_SUCCESS)
    {
        LOG(("IoGetDeviceInterfaces failed with %d\n", Status));
        return;
    }
    if (szwHubList)
    {
        UNICODE_STRING  UnicodeName;
        PDEVICE_OBJECT  pHubDevObj;
        PFILE_OBJECT    pHubFileObj;
        PWSTR           szwHubName = szwHubList;
        while (*szwHubName != UNICODE_NULL)
        {
            RtlInitUnicodeString(&UnicodeName, szwHubName);
            Status = IoGetDeviceObjectPointer(&UnicodeName, FILE_READ_DATA, &pHubFileObj, &pHubDevObj);
            if (Status == STATUS_SUCCESS)
            {
                /* We could not log hub name here.
                 * It is the paged memory and we cannot use it in logger cause it increases the IRQL
                 */
                LOG(("IoGetDeviceObjectPointer returned %p %p", pHubDevObj, pHubFileObj));
                if (!pfnWalker(pHubFileObj, pHubDevObj, pvWalker))
                {
                    LOG(("the walker said to stop"));
                    ObDereferenceObject(pHubFileObj);
                    break;
                }

                LOG(("going forward.."));
                ObDereferenceObject(pHubFileObj);
            }
            szwHubName += wcslen(szwHubName) + 1;
        }
        ExFreePool(szwHubList);
    }
}

/* NOTE: the stack location data is not the "actual" IRP stack location,
 * but a copy being preserved on the IRP way down.
 * See the note in VPoxUsbPnPCompletion for detail */
static NTSTATUS vpoxUsbMonHandlePnPIoctl(PDEVICE_OBJECT pDevObj, PIO_STACK_LOCATION pSl, PIO_STATUS_BLOCK pIoStatus)
{
    LOG(("IRQL = %d", KeGetCurrentIrql()));
    switch(pSl->MinorFunction)
    {
        case IRP_MN_QUERY_DEVICE_TEXT:
        {
            LOG(("IRP_MN_QUERY_DEVICE_TEXT: pIoStatus->Status = %x", pIoStatus->Status));
            if (pIoStatus->Status == STATUS_SUCCESS)
            {
                WCHAR *pId = (WCHAR *)pIoStatus->Information;
                if (VALID_PTR(pId))
                {
                    KIRQL Iqrl = KeGetCurrentIrql();
                    /* IRQL should be always passive here */
                    ASSERT_WARN(Iqrl == PASSIVE_LEVEL, ("irql is not PASSIVE"));
                    switch(pSl->Parameters.QueryDeviceText.DeviceTextType)
                    {
                        case DeviceTextLocationInformation:
                            LOG(("DeviceTextLocationInformation"));
                            LOG_STRW(pId);
                            break;

                        case DeviceTextDescription:
                            LOG(("DeviceTextDescription"));
                            LOG_STRW(pId);
                            if (VPoxUsbFltPdoIsFiltered(pDevObj))
                            {
                                LOG(("PDO (0x%p) is filtered", pDevObj));
                                WCHAR *pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szDeviceTextDescription));
                                if (!pId)
                                {
                                    AssertFailed();
                                    break;
                                }
                                memcpy(pId, szDeviceTextDescription, sizeof(szDeviceTextDescription));
                                LOG(("NEW szDeviceTextDescription"));
                                LOG_STRW(pId);
                                ExFreePool((PVOID)pIoStatus->Information);
                                pIoStatus->Information = (ULONG_PTR)pId;
                            }
                            else
                            {
                                LOG(("PDO (0x%p) is NOT filtered", pDevObj));
                            }
                            break;
                        default:
                            LOG(("DeviceText %d", pSl->Parameters.QueryDeviceText.DeviceTextType));
                            break;
                    }
                }
                else
                    LOG(("Invalid pointer %p", pId));
            }
            break;
        }

        case IRP_MN_QUERY_ID:
        {
            LOG(("IRP_MN_QUERY_ID: Irp->pIoStatus->Status = %x", pIoStatus->Status));
            if (pIoStatus->Status == STATUS_SUCCESS &&  pDevObj)
            {
                WCHAR *pId = (WCHAR *)pIoStatus->Information;
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
                WCHAR *pTmp;
#endif
                if (VALID_PTR(pId))
                {
                    KIRQL Iqrl = KeGetCurrentIrql();
                    /* IRQL should be always passive here */
                    ASSERT_WARN(Iqrl == PASSIVE_LEVEL, ("irql is not PASSIVE"));

                    switch (pSl->Parameters.QueryId.IdType)
                    {
                        case BusQueryInstanceID:
                            LOG(("BusQueryInstanceID"));
                            LOG_STRW(pId);
                            break;

                        case BusQueryDeviceID:
                        {
                            LOG(("BusQueryDeviceID"));
                            pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryDeviceId));
                            if (!pId)
                            {
                                WARN(("ExAllocatePool failed"));
                                break;
                            }

                            BOOLEAN bFiltered = FALSE;
                            NTSTATUS Status = VPoxUsbFltPdoAdd(pDevObj, &bFiltered);
                            if (Status != STATUS_SUCCESS || !bFiltered)
                            {
                                if (Status == STATUS_SUCCESS)
                                {
                                    LOG(("PDO (0x%p) is NOT filtered", pDevObj));
                                }
                                else
                                {
                                    WARN(("VPoxUsbFltPdoAdd for PDO (0x%p) failed Status 0x%x", pDevObj, Status));
                                }
                                ExFreePool(pId);
                                break;
                            }

                            LOG(("PDO (0x%p) is filtered", pDevObj));
                            ExFreePool((PVOID)pIoStatus->Information);
                            memcpy(pId, szBusQueryDeviceId, sizeof(szBusQueryDeviceId));
                            pIoStatus->Information = (ULONG_PTR)pId;
                            break;
                        }
                    case BusQueryHardwareIDs:
                    {
                        LOG(("BusQueryHardwareIDs"));
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
                        while (*pId) //MULTI_SZ
                        {
                            LOG_STRW(pId);
                            while (*pId) pId++;
                            pId++;
                        }
#endif
                        pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryHardwareIDs));
                        if (!pId)
                        {
                            WARN(("ExAllocatePool failed"));
                            break;
                        }

                        BOOLEAN bFiltered = FALSE;
                        NTSTATUS Status = VPoxUsbFltPdoAdd(pDevObj, &bFiltered);
                        if (Status != STATUS_SUCCESS || !bFiltered)
                        {
                            if (Status == STATUS_SUCCESS)
                            {
                                LOG(("PDO (0x%p) is NOT filtered", pDevObj));
                            }
                            else
                            {
                                WARN(("VPoxUsbFltPdoAdd for PDO (0x%p) failed Status 0x%x", pDevObj, Status));
                            }
                            ExFreePool(pId);
                            break;
                        }

                        LOG(("PDO (0x%p) is filtered", pDevObj));

                        memcpy(pId, szBusQueryHardwareIDs, sizeof(szBusQueryHardwareIDs));
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
                        LOG(("NEW BusQueryHardwareIDs"));
                        pTmp = pId;
                        while (*pTmp) //MULTI_SZ
                        {

                            LOG_STRW(pTmp);
                            while (*pTmp) pTmp++;
                            pTmp++;
                        }
#endif
                        ExFreePool((PVOID)pIoStatus->Information);
                        pIoStatus->Information = (ULONG_PTR)pId;
                        break;
                    }
                    case BusQueryCompatibleIDs:
                        LOG(("BusQueryCompatibleIDs"));
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
                        while (*pId) //MULTI_SZ
                        {
                            LOG_STRW(pId);
                            while (*pId) pId++;
                            pId++;
                        }
#endif
                        if (VPoxUsbFltPdoIsFiltered(pDevObj))
                        {
                            LOG(("PDO (0x%p) is filtered", pDevObj));
                            pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryCompatibleIDs));
                            if (!pId)
                            {
                                WARN(("ExAllocatePool failed"));
                                break;
                            }
                            memcpy(pId, szBusQueryCompatibleIDs, sizeof(szBusQueryCompatibleIDs));
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
                            LOG(("NEW BusQueryCompatibleIDs"));
                            pTmp = pId;
                            while (*pTmp) //MULTI_SZ
                            {
                                LOG_STRW(pTmp);
                                while (*pTmp) pTmp++;
                                pTmp++;
                            }
#endif
                            ExFreePool((PVOID)pIoStatus->Information);
                            pIoStatus->Information = (ULONG_PTR)pId;
                        }
                        else
                        {
                            LOG(("PDO (0x%p) is NOT filtered", pDevObj));
                        }
                        break;

                        default:
                            /** @todo r=bird: handle BusQueryContainerID and whatever else we might see  */
                            break;
                    }
                }
                else
                {
                    LOG(("Invalid pointer %p", pId));
                }
            }
            break;
        }

#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
        case IRP_MN_QUERY_DEVICE_RELATIONS:
        {
            switch(pSl->Parameters.QueryDeviceRelations.Type)
            {
                case BusRelations:
                    LOG(("BusRelations"));

                    if (pIoStatus->Status == STATUS_SUCCESS)
                    {
                        PDEVICE_RELATIONS pRel = (PDEVICE_RELATIONS)pIoStatus->Information;
                        LOG(("pRel = %p", pRel));
                        if (VALID_PTR(pRel))
                        {
                            for (unsigned i=0;i<pRel->Count;i++)
                            {
                                if (VPoxUsbFltPdoIsFiltered(pDevObj))
                                    LOG(("New PDO %p", pRel->Objects[i]));
                            }
                        }
                        else
                            LOG(("Invalid pointer %p", pRel));
                    }
                    break;
                case TargetDeviceRelation:
                    LOG(("TargetDeviceRelation"));
                    break;
                case RemovalRelations:
                    LOG(("RemovalRelations"));
                    break;
                case EjectionRelations:
                    LOG(("EjectionRelations"));
                    break;
                default:
                    LOG(("QueryDeviceRelations.Type=%d", pSl->Parameters.QueryDeviceRelations.Type));
            }
            break;
        }

        case IRP_MN_QUERY_CAPABILITIES:
        {
            LOG(("IRP_MN_QUERY_CAPABILITIES: pIoStatus->Status = %x", pIoStatus->Status));
            if (pIoStatus->Status == STATUS_SUCCESS)
            {
                PDEVICE_CAPABILITIES pCaps = pSl->Parameters.DeviceCapabilities.Capabilities;
                if (VALID_PTR(pCaps))
                {
                    LOG(("Caps.SilentInstall  = %d", pCaps->SilentInstall));
                    LOG(("Caps.UniqueID       = %d", pCaps->UniqueID ));
                    LOG(("Caps.Address        = %d", pCaps->Address ));
                    LOG(("Caps.UINumber       = %d", pCaps->UINumber ));
                }
                else
                    LOG(("Invalid pointer %p", pCaps));
            }
            break;
        }

        default:
            break;
#endif
    } /*switch */

    LOG(("Done returns %x (IRQL = %d)", pIoStatus->Status, KeGetCurrentIrql()));
    return pIoStatus->Status;
}

NTSTATUS _stdcall VPoxUsbPnPCompletion(DEVICE_OBJECT *pDevObj, IRP *pIrp, void *pvContext)
{
    LOG(("Completion PDO(0x%p), IRP(0x%p), Status(0x%x)", pDevObj, pIrp, pIrp->IoStatus.Status));
    ASSERT_WARN(pvContext, ("zero context"));

    PVPOXUSBHOOK_REQUEST pRequest = (PVPOXUSBHOOK_REQUEST)pvContext;
    /* NOTE: despite a regular IRP processing the stack location in our completion
     * differs from those of the PnP hook since the hook is invoked in the "context" of the calle,
     * while the completion is in the "coller" context in terms of IRP,
     * so the completion stack location is one level "up" here.
     *
     * Moreover we CAN NOT access irp stack location in the completion because we might not have one at all
     * in case the hooked driver is at the top of the irp call stack
     *
     * This is why we use the stack location we saved on IRP way down.
     * */
    PIO_STACK_LOCATION pSl = &pRequest->OldLocation;
    ASSERT_WARN(pIrp == pRequest->pIrp, ("completed IRP(0x%x) not match request IRP(0x%x)", pIrp, pRequest->pIrp));
    /* NOTE: we can not rely on pDevObj passed in IoCompletion since it may be zero
     * in case IRP was created with extra stack locations and the caller did not initialize
     * the IO_STACK_LOCATION::DeviceObject */
    DEVICE_OBJECT *pRealDevObj = pRequest->pDevObj;
//    Assert(!pDevObj || pDevObj == pRealDevObj);
//    Assert(pSl->DeviceObject == pDevObj);

    switch(pSl->MinorFunction)
    {
        case IRP_MN_QUERY_DEVICE_TEXT:
        case IRP_MN_QUERY_ID:
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
        case IRP_MN_QUERY_DEVICE_RELATIONS:
        case IRP_MN_QUERY_CAPABILITIES:
#endif
            if (NT_SUCCESS(pIrp->IoStatus.Status))
            {
                vpoxUsbMonHandlePnPIoctl(pRealDevObj, pSl, &pIrp->IoStatus);
            }
            else
            {
                ASSERT_WARN(pIrp->IoStatus.Status == STATUS_NOT_SUPPORTED, ("Irp failed with status(0x%x)", pIrp->IoStatus.Status));
            }
            break;

        case IRP_MN_SURPRISE_REMOVAL:
        case IRP_MN_REMOVE_DEVICE:
            if (NT_SUCCESS(pIrp->IoStatus.Status))
            {
                VPoxUsbFltPdoRemove(pRealDevObj);
            }
            else
            {
                AssertFailed();
            }
            break;

        /* These two IRPs are received when the PnP subsystem has determined the id of the newly arrived device */
        /* IRP_MN_START_DEVICE only arrives if it's a USB device of a known class or with a present host driver */
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
        case IRP_MN_QUERY_RESOURCES:
            /* There used to be code to support SUPUSBFLT_IOCTL_SET_NOTIFY_EVENT but it was not reliable. */

        default:
            break;
    }

    LOG(("<==PnP: Mn(%s), PDO(0x%p), IRP(0x%p), Status(0x%x), Sl PDO(0x%p), Compl PDO(0x%p)",
                            vpoxUsbDbgStrPnPMn(pSl->MinorFunction),
                            pRealDevObj, pIrp, pIrp->IoStatus.Status,
                            pSl->DeviceObject, pDevObj));
#ifdef DEBUG_misha
    NTSTATUS tmpStatus = pIrp->IoStatus.Status;
#endif
    PVPOXUSBHOOK_ENTRY pHook = pRequest->pHook;
    NTSTATUS Status = VPoxUsbHookRequestComplete(pHook, pDevObj, pIrp, pRequest);
    VPoxUsbMonMemFree(pRequest);
#ifdef DEBUG_misha
    if (Status != STATUS_MORE_PROCESSING_REQUIRED)
    {
        Assert(pIrp->IoStatus.Status == tmpStatus);
    }
#endif
    VPoxUsbHookRelease(pHook);
    return Status;
}

/**
 * Device PnP hook
 *
 * @param   pDevObj     Device object.
 * @param   pIrp         Request packet.
 */
static NTSTATUS vpoxUsbMonPnPHook(IN PVPOXUSBHOOK_ENTRY pHook, IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    LOG(("==>PnP: Mn(%s), PDO(0x%p), IRP(0x%p), Status(0x%x)", vpoxUsbDbgStrPnPMn(IoGetCurrentIrpStackLocation(pIrp)->MinorFunction), pDevObj, pIrp, pIrp->IoStatus.Status));

    if (!VPoxUsbHookRetain(pHook))
    {
        WARN(("VPoxUsbHookRetain failed"));
        return VPoxUsbHookRequestPassDownHookSkip(pHook, pDevObj, pIrp);
    }

    PVPOXUSBHUB_PNPHOOK_COMPLETION pCompletion = (PVPOXUSBHUB_PNPHOOK_COMPLETION)VPoxUsbMonMemAlloc(sizeof (*pCompletion));
    if (!pCompletion)
    {
        WARN(("VPoxUsbMonMemAlloc failed"));
        VPoxUsbHookRelease(pHook);
        pIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        pIrp->IoStatus.Information = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS Status = VPoxUsbHookRequestPassDownHookCompletion(pHook, pDevObj, pIrp, VPoxUsbPnPCompletion, &pCompletion->Rq);
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
    if (Status != STATUS_PENDING)
    {
        LOG(("Request completed, Status(0x%x)", Status));
        VPoxUsbHookVerifyCompletion(pHook, &pCompletion->Rq, pIrp);
    }
    else
    {
        LOG(("Request pending"));
    }
#endif
    return Status;
}

/**
 * Device PnP hook stubs.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp         Request packet.
 */
#define VPOX_PNPHOOKSTUB(n) NTSTATUS _stdcall VPoxUsbMonPnPHook##n(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp) \
{ \
    return vpoxUsbMonPnPHook(&g_VPoxUsbMonGlobals.pDrivers[n].UsbHubPnPHook.Hook, pDevObj, pIrp); \
}

#define VPOX_PNPHOOKSTUB_INIT(n) g_VPoxUsbMonGlobals.pDrivers[n].pfnHookStub = VPoxUsbMonPnPHook##n

VPOX_PNPHOOKSTUB(0)
VPOX_PNPHOOKSTUB(1)
VPOX_PNPHOOKSTUB(2)
VPOX_PNPHOOKSTUB(3)
VPOX_PNPHOOKSTUB(4)
AssertCompile(VPOXUSBMON_MAXDRIVERS == 5);

typedef struct VPOXUSBMONHOOKDRIVERWALKER
{
    PDRIVER_OBJECT pDrvObj;
} VPOXUSBMONHOOKDRIVERWALKER, *PVPOXUSBMONHOOKDRIVERWALKER;

/**
 * Logs an error to the system event log.
 *
 * @param   ErrCode        Error to report to event log.
 * @param   ReturnedStatus Error that was reported by the driver to the caller.
 * @param   uErrId         Unique error id representing the location in the driver.
 * @param   cbDumpData     Number of bytes at pDumpData.
 * @param   pDumpData      Pointer to data that will be added to the message (see 'details' tab).
 *
 * NB: We only use IoLogMsg.dll as the message file, limiting
 * ErrCode to status codes and messages defined in ntiologc.h
 */
static void vpoxUsbMonLogError(NTSTATUS ErrCode, NTSTATUS ReturnedStatus, ULONG uErrId, USHORT cbDumpData, PVOID pDumpData)
{
    PIO_ERROR_LOG_PACKET pErrEntry;


    /* Truncate dumps that do not fit into IO_ERROR_LOG_PACKET. */
    if (FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + cbDumpData > ERROR_LOG_MAXIMUM_SIZE)
        cbDumpData = ERROR_LOG_MAXIMUM_SIZE - FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData);

    pErrEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(g_VPoxUsbMonGlobals.pDevObj,
                                                              FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + cbDumpData);
    if (pErrEntry)
    {
        uint8_t *pDump = (uint8_t *)pErrEntry->DumpData;
        if (cbDumpData)
            memcpy(pDump, pDumpData, cbDumpData);
        pErrEntry->MajorFunctionCode = 0;
        pErrEntry->RetryCount = 0;
        pErrEntry->DumpDataSize = cbDumpData;
        pErrEntry->NumberOfStrings = 0;
        pErrEntry->StringOffset = 0;
        pErrEntry->ErrorCode = ErrCode;
        pErrEntry->UniqueErrorValue = uErrId;
        pErrEntry->FinalStatus = ReturnedStatus;
        pErrEntry->IoControlCode = 0;
        IoWriteErrorLogEntry(pErrEntry);
    }
    else
    {
        LOG(("Failed to allocate error log entry (cb=%d)\n", FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + cbDumpData));
    }
}

static DECLCALLBACK(BOOLEAN) vpoxUsbMonHookDrvObjWalker(PFILE_OBJECT pHubFile, PDEVICE_OBJECT pHubDo, PVOID pvContext)
{
    RT_NOREF2(pHubFile, pvContext);
    PDRIVER_OBJECT pDrvObj = pHubDo->DriverObject;

    /* First we try to figure out if we are already hooked to this driver. */
    for (int i = 0; i < VPOXUSBMON_MAXDRIVERS; i++)
        if (pDrvObj == g_VPoxUsbMonGlobals.pDrivers[i].DriverObject)
        {
            LOG(("Found %p at pDrivers[%d]\n", pDrvObj, i));
            /* We've already hooked to this one -- nothing to do. */
            return TRUE;
        }
    /* We are not hooked yet, find an empty slot. */
    for (int i = 0; i < VPOXUSBMON_MAXDRIVERS; i++)
    {
        if (!g_VPoxUsbMonGlobals.pDrivers[i].DriverObject)
        {
            /* Found an emtpy slot, use it. */
            g_VPoxUsbMonGlobals.pDrivers[i].DriverObject = pDrvObj;
            ObReferenceObject(pDrvObj);
            LOG(("pDrivers[%d] = %p, installing the hook...\n", i, pDrvObj));
            VPoxUsbHookInit(&g_VPoxUsbMonGlobals.pDrivers[i].UsbHubPnPHook.Hook,
                            pDrvObj,
                            IRP_MJ_PNP,
                            g_VPoxUsbMonGlobals.pDrivers[i].pfnHookStub);
            VPoxUsbHookInstall(&g_VPoxUsbMonGlobals.pDrivers[i].UsbHubPnPHook.Hook);
            return TRUE; /* Must continue to find all drivers. */
        }
        if (pDrvObj == g_VPoxUsbMonGlobals.pDrivers[i].DriverObject)
        {
            LOG(("Found %p at pDrivers[%d]\n", pDrvObj, i));
            /* We've already hooked to this one -- nothing to do. */
            return TRUE;
        }
    }
    /* No empty slots! No reason to continue. */
    LOG(("No empty slots!\n"));
    ANSI_STRING ansiDrvName;
    NTSTATUS Status = RtlUnicodeStringToAnsiString(&ansiDrvName, &pDrvObj->DriverName, true);
    if (Status != STATUS_SUCCESS)
    {
        ansiDrvName.Length = 0;
        LOG(("RtlUnicodeStringToAnsiString failed with 0x%x", Status));
    }
    vpoxUsbMonLogError(IO_ERR_INSUFFICIENT_RESOURCES, STATUS_SUCCESS, 1, ansiDrvName.Length, ansiDrvName.Buffer);
    if (Status == STATUS_SUCCESS)
        RtlFreeAnsiString(&ansiDrvName);
    return FALSE;
}

/**
 * Finds all USB drivers in the system and installs hooks if haven't done already.
 */
static NTSTATUS vpoxUsbMonInstallAllHooks()
{
    vpoxUsbMonHubDevWalk(vpoxUsbMonHookDrvObjWalker, NULL);
    return STATUS_SUCCESS;
}

static NTSTATUS vpoxUsbMonHookCheckInit()
{
    static bool fIsHookInited = false;
    if (fIsHookInited)
    {
        LOG(("hook inited already, success"));
        return STATUS_SUCCESS;
    }
    return vpoxUsbMonInstallAllHooks();
}

static NTSTATUS vpoxUsbMonHookInstall()
{
    /* Nothing to do here as we have already installed all hooks in vpoxUsbMonHookCheckInit(). */
    return STATUS_SUCCESS;
}

static NTSTATUS vpoxUsbMonHookUninstall()
{
#ifdef VPOXUSBMON_DBG_NO_PNPHOOK
    return STATUS_SUCCESS;
#else
    NTSTATUS Status = STATUS_SUCCESS;
    for (int i = 0; i < VPOXUSBMON_MAXDRIVERS; i++)
    {
        if (g_VPoxUsbMonGlobals.pDrivers[i].DriverObject)
        {
            Assert(g_VPoxUsbMonGlobals.pDrivers[i].DriverObject == g_VPoxUsbMonGlobals.pDrivers[i].UsbHubPnPHook.Hook.pDrvObj);
            LOG(("Unhooking from %p...\n", g_VPoxUsbMonGlobals.pDrivers[i].DriverObject));
            Status = VPoxUsbHookUninstall(&g_VPoxUsbMonGlobals.pDrivers[i].UsbHubPnPHook.Hook);
            if (!NT_SUCCESS(Status))
            {
                /*
                 * We failed to uninstall the hook, so we keep the reference to the driver
                 * in order to prevent another driver re-using this slot because we are
                 * going to mark this hook as fUninitFailed.
                 */
                //AssertMsgFailed(("usbhub pnp unhook failed, setting the fUninitFailed flag, the current value of fUninitFailed (%d)", g_VPoxUsbMonGlobals.UsbHubPnPHook.fUninitFailed));
                LOG(("usbhub pnp unhook failed, setting the fUninitFailed flag, the current value of fUninitFailed (%d)", g_VPoxUsbMonGlobals.pDrivers[i].UsbHubPnPHook.fUninitFailed));
                g_VPoxUsbMonGlobals.pDrivers[i].UsbHubPnPHook.fUninitFailed = true;
            }
            else
            {
                /* The hook was removed successfully, now we can forget about this driver. */
                ObDereferenceObject(g_VPoxUsbMonGlobals.pDrivers[i].DriverObject);
                g_VPoxUsbMonGlobals.pDrivers[i].DriverObject = NULL;
            }
        }
    }
    return Status;
#endif
}


static NTSTATUS vpoxUsbMonCheckTermStuff()
{
    NTSTATUS Status = KeWaitForSingleObject(&g_VPoxUsbMonGlobals.OpenSynchEvent,
            Executive, KernelMode,
            FALSE, /* BOOLEAN Alertable */
            NULL /* IN PLARGE_INTEGER Timeout */
            );
    AssertRelease(Status == STATUS_SUCCESS);

    do
    {
        if (--g_VPoxUsbMonGlobals.cOpens)
            break;

        Status = vpoxUsbMonHookUninstall();

        NTSTATUS tmpStatus = VPoxUsbFltTerm();
        if (!NT_SUCCESS(tmpStatus))
        {
            /* this means a driver state is screwed up, KeBugCheckEx here ? */
            AssertReleaseFailed();
        }
    } while (0);

    KeSetEvent(&g_VPoxUsbMonGlobals.OpenSynchEvent, 0, FALSE);

    return Status;
}

static NTSTATUS vpoxUsbMonCheckInitStuff()
{
    NTSTATUS Status = KeWaitForSingleObject(&g_VPoxUsbMonGlobals.OpenSynchEvent,
            Executive, KernelMode,
            FALSE, /* BOOLEAN Alertable */
            NULL /* IN PLARGE_INTEGER Timeout */
        );
    if (Status == STATUS_SUCCESS)
    {
        do
        {
            if (g_VPoxUsbMonGlobals.cOpens++)
            {
                LOG(("opens: %d, success", g_VPoxUsbMonGlobals.cOpens));
                break;
            }

            Status = VPoxUsbFltInit();
            if (NT_SUCCESS(Status))
            {
                Status = vpoxUsbMonHookCheckInit();
                if (NT_SUCCESS(Status))
                {
                    Status = vpoxUsbMonHookInstall();
                    if (NT_SUCCESS(Status))
                    {
                        Status = STATUS_SUCCESS;
                        LOG(("succeded!!"));
                        break;
                    }
                    else
                    {
                        WARN(("vpoxUsbMonHookInstall failed, Status (0x%x)", Status));
                    }
                }
                else
                {
                    WARN(("vpoxUsbMonHookCheckInit failed, Status (0x%x)", Status));
                }
                VPoxUsbFltTerm();
            }
            else
            {
                WARN(("VPoxUsbFltInit failed, Status (0x%x)", Status));
            }

            --g_VPoxUsbMonGlobals.cOpens;
            Assert(!g_VPoxUsbMonGlobals.cOpens);
        } while (0);

        KeSetEvent(&g_VPoxUsbMonGlobals.OpenSynchEvent, 0, FALSE);
    }
    else
    {
        WARN(("KeWaitForSingleObject failed, Status (0x%x)", Status));
    }
    return Status;
}

static NTSTATUS vpoxUsbMonContextCreate(PVPOXUSBMONCTX *ppCtx)
{
    NTSTATUS Status;
    *ppCtx = NULL;
    PVPOXUSBMONCTX pFileCtx = (PVPOXUSBMONCTX)VPoxUsbMonMemAllocZ(sizeof (*pFileCtx));
    if (pFileCtx)
    {
        Status = vpoxUsbMonCheckInitStuff();
        if (Status == STATUS_SUCCESS)
        {
            Status = VPoxUsbFltCreate(&pFileCtx->FltCtx);
            if (Status == STATUS_SUCCESS)
            {
                *ppCtx = pFileCtx;
                LOG(("succeeded!!"));
                return STATUS_SUCCESS;
            }
            else
            {
                WARN(("VPoxUsbFltCreate failed"));
            }
            vpoxUsbMonCheckTermStuff();
        }
        else
        {
            WARN(("vpoxUsbMonCheckInitStuff failed"));
        }
        VPoxUsbMonMemFree(pFileCtx);
    }
    else
    {
        WARN(("VPoxUsbMonMemAllocZ failed"));
        Status = STATUS_NO_MEMORY;
    }

    return Status;
}

static NTSTATUS vpoxUsbMonContextClose(PVPOXUSBMONCTX pCtx)
{
    NTSTATUS Status = VPoxUsbFltClose(&pCtx->FltCtx);
    if (Status == STATUS_SUCCESS)
    {
        Status = vpoxUsbMonCheckTermStuff();
        Assert(Status == STATUS_SUCCESS);
        /* ignore the failure */
        VPoxUsbMonMemFree(pCtx);
    }

    return Status;
}

static NTSTATUS _stdcall VPoxUsbMonClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT pFileObj = pStack->FileObject;
    Assert(pFileObj->FsContext);
    PVPOXUSBMONCTX pCtx = (PVPOXUSBMONCTX)pFileObj->FsContext;

    LOG(("VPoxUsbMonClose"));

    NTSTATUS Status = vpoxUsbMonContextClose(pCtx);
    if (Status != STATUS_SUCCESS)
    {
        WARN(("vpoxUsbMonContextClose failed, Status (0x%x), prevent unload", Status));
        if (!InterlockedExchange(&g_VPoxUsbMonGlobals.ulPreventUnloadOn, 1))
        {
            LOGREL(("ulPreventUnloadOn not set, preventing unload"));
            UNICODE_STRING UniName;
            PDEVICE_OBJECT pTmpDevObj;
            RtlInitUnicodeString(&UniName, USBMON_DEVICE_NAME_NT);
            NTSTATUS tmpStatus = IoGetDeviceObjectPointer(&UniName, FILE_ALL_ACCESS, &g_VPoxUsbMonGlobals.pPreventUnloadFileObj, &pTmpDevObj);
            AssertRelease(NT_SUCCESS(tmpStatus));
            AssertRelease(pTmpDevObj == pDevObj);
        }
        else
        {
            WARN(("ulPreventUnloadOn already set"));
        }
        LOG(("success!!"));
        Status = STATUS_SUCCESS;
    }
    pFileObj->FsContext = NULL;
    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information  = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return Status;
}


static NTSTATUS _stdcall VPoxUsbMonCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    RT_NOREF1(pDevObj);
    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT pFileObj = pStack->FileObject;
    NTSTATUS Status;

    LOG(("VPoxUSBMonCreate"));

    if (pStack->Parameters.Create.Options & FILE_DIRECTORY_FILE)
    {
        WARN(("trying to open as a directory"));
        pIrp->IoStatus.Status = STATUS_NOT_A_DIRECTORY;
        pIrp->IoStatus.Information = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return STATUS_NOT_A_DIRECTORY;
    }

    pFileObj->FsContext = NULL;
    PVPOXUSBMONCTX pCtx = NULL;
    Status = vpoxUsbMonContextCreate(&pCtx);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pCtx);
        pFileObj->FsContext = pCtx;
    }
    else
    {
        WARN(("vpoxUsbMonContextCreate failed Status (0x%x)", Status));
    }

    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information  = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return Status;
}

static int VPoxUsbMonFltAdd(PVPOXUSBMONCTX pContext, PUSBFILTER pFilter, uintptr_t *pId)
{
#ifdef VPOXUSBMON_DBG_NO_FILTERS
    static uintptr_t idDummy = 1;
    *pId = idDummy;
    ++idDummy;
    return VINF_SUCCESS;
#else
    int rc = VPoxUsbFltAdd(&pContext->FltCtx, pFilter, pId);
    return rc;
#endif
}

static int VPoxUsbMonFltRemove(PVPOXUSBMONCTX pContext, uintptr_t uId)
{
#ifdef VPOXUSBMON_DBG_NO_FILTERS
    return VINF_SUCCESS;
#else
    int rc = VPoxUsbFltRemove(&pContext->FltCtx, uId);
    return rc;
#endif
}

static NTSTATUS VPoxUsbMonRunFilters(PVPOXUSBMONCTX pContext)
{
    NTSTATUS Status = VPoxUsbFltFilterCheck(&pContext->FltCtx);
    return Status;
}

static NTSTATUS VPoxUsbMonGetDevice(PVPOXUSBMONCTX pContext, HVPOXUSBDEVUSR hDevice, PUSBSUP_GETDEV_MON pInfo)
{
    NTSTATUS Status = VPoxUsbFltGetDevice(&pContext->FltCtx, hDevice, pInfo);
    return Status;
}

static NTSTATUS vpoxUsbMonIoctlDispatch(PVPOXUSBMONCTX pContext, ULONG Ctl, PVOID pvBuffer, ULONG cbInBuffer,
                                        ULONG cbOutBuffer, ULONG_PTR *pInfo)
{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG_PTR Info = 0;
    switch (Ctl)
    {
        case SUPUSBFLT_IOCTL_GET_VERSION:
        {
            PUSBSUP_VERSION pOut = (PUSBSUP_VERSION)pvBuffer;

            LOG(("SUPUSBFLT_IOCTL_GET_VERSION"));
            if (!pvBuffer || cbOutBuffer != sizeof(*pOut) || cbInBuffer != 0)
            {
                WARN(("SUPUSBFLT_IOCTL_GET_VERSION: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.",
                        cbInBuffer, 0, cbOutBuffer, sizeof (*pOut)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            pOut->u32Major = USBMON_MAJOR_VERSION;
            pOut->u32Minor = USBMON_MINOR_VERSION;
            Info = sizeof (*pOut);
            ASSERT_WARN(Status == STATUS_SUCCESS, ("unexpected status, 0x%x", Status));
            break;
        }

        case SUPUSBFLT_IOCTL_ADD_FILTER:
        {
            PUSBFILTER pFilter = (PUSBFILTER)pvBuffer;
            PUSBSUP_FLTADDOUT pOut = (PUSBSUP_FLTADDOUT)pvBuffer;
            uintptr_t uId = 0;
            int rc;
            if (RT_UNLIKELY(!pvBuffer || cbInBuffer != sizeof (*pFilter) || cbOutBuffer != sizeof (*pOut)))
            {
                WARN(("SUPUSBFLT_IOCTL_ADD_FILTER: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.",
                        cbInBuffer, sizeof (*pFilter), cbOutBuffer, sizeof (*pOut)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            rc = VPoxUsbMonFltAdd(pContext, pFilter, &uId);
            pOut->rc  = rc;
            pOut->uId = uId;
            Info = sizeof (*pOut);
            ASSERT_WARN(Status == STATUS_SUCCESS, ("unexpected status, 0x%x", Status));
            break;
        }

        case SUPUSBFLT_IOCTL_REMOVE_FILTER:
        {
            uintptr_t *pIn = (uintptr_t *)pvBuffer;
            int *pRc = (int *)pvBuffer;

            if (!pvBuffer || cbInBuffer != sizeof (*pIn) || (cbOutBuffer && cbOutBuffer != sizeof (*pRc)))
            {
                WARN(("SUPUSBFLT_IOCTL_REMOVE_FILTER: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.",
                        cbInBuffer, sizeof (*pIn), cbOutBuffer, 0));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            LOG(("SUPUSBFLT_IOCTL_REMOVE_FILTER %x", *pIn));
            int rc = VPoxUsbMonFltRemove(pContext, *pIn);
            if (cbOutBuffer)
            {
                /* we've validated that already */
                Assert(cbOutBuffer == (ULONG)*pRc);
                *pRc = rc;
                Info = sizeof (*pRc);
            }
            ASSERT_WARN(Status == STATUS_SUCCESS, ("unexpected status, 0x%x", Status));
            break;
        }

        case SUPUSBFLT_IOCTL_RUN_FILTERS:
        {
            if (pvBuffer || cbInBuffer || cbOutBuffer)
            {
                WARN(("SUPUSBFLT_IOCTL_RUN_FILTERS: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.",
                        cbInBuffer, 0, cbOutBuffer, 0));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            LOG(("SUPUSBFLT_IOCTL_RUN_FILTERS "));
            Status = VPoxUsbMonRunFilters(pContext);
            ASSERT_WARN(Status != STATUS_PENDING, ("status pending!"));
            break;
        }

        case SUPUSBFLT_IOCTL_GET_DEVICE:
        {
            HVPOXUSBDEVUSR hDevice;
            PUSBSUP_GETDEV_MON pOut = (PUSBSUP_GETDEV_MON)pvBuffer;
            if (!pvBuffer || cbInBuffer != sizeof (hDevice) || cbOutBuffer < sizeof (*pOut))
            {
                WARN(("SUPUSBFLT_IOCTL_GET_DEVICE: Invalid input/output sizes! cbIn=%d expected %d. cbOut=%d expected >= %d.",
                        cbInBuffer, sizeof (hDevice), cbOutBuffer, sizeof (*pOut)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            hDevice = *(HVPOXUSBDEVUSR*)pvBuffer;
            if (!hDevice)
            {
                WARN(("SUPUSBFLT_IOCTL_GET_DEVICE: hDevice is NULL!",
                        cbInBuffer, sizeof (hDevice), cbOutBuffer, sizeof (*pOut)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            Status = VPoxUsbMonGetDevice(pContext, hDevice, pOut);

            if (NT_SUCCESS(Status))
            {
                Info = sizeof (*pOut);
            }
            else
            {
                WARN(("VPoxUsbMonGetDevice fail 0x%x", Status));
            }
            break;
        }

        default:
            WARN(("Unknown code 0x%x", Ctl));
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    ASSERT_WARN(Status != STATUS_PENDING, ("Status pending!"));

    *pInfo = Info;
    return Status;
}

static NTSTATUS _stdcall VPoxUsbMonDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    ULONG_PTR Info = 0;
    NTSTATUS Status = IoAcquireRemoveLock(&g_VPoxUsbMonGlobals.RmLock, pDevObj);
    if (NT_SUCCESS(Status))
    {
        PIO_STACK_LOCATION pSl = IoGetCurrentIrpStackLocation(pIrp);
        PFILE_OBJECT pFileObj = pSl->FileObject;
        Assert(pFileObj);
        Assert(pFileObj->FsContext);
        PVPOXUSBMONCTX pCtx = (PVPOXUSBMONCTX)pFileObj->FsContext;
        Assert(pCtx);
        Status = vpoxUsbMonIoctlDispatch(pCtx,
                    pSl->Parameters.DeviceIoControl.IoControlCode,
                    pIrp->AssociatedIrp.SystemBuffer,
                    pSl->Parameters.DeviceIoControl.InputBufferLength,
                    pSl->Parameters.DeviceIoControl.OutputBufferLength,
                    &Info);
        ASSERT_WARN(Status != STATUS_PENDING, ("Status pending"));

        IoReleaseRemoveLock(&g_VPoxUsbMonGlobals.RmLock, pDevObj);
    }
    else
    {
        WARN(("IoAcquireRemoveLock failed Status (0x%x)", Status));
    }

    pIrp->IoStatus.Information = Info;
    pIrp->IoStatus.Status = Status;
    IoCompleteRequest (pIrp, IO_NO_INCREMENT);
    return Status;
}

static NTSTATUS vpoxUsbMonInternalIoctlDispatch(ULONG Ctl, PVOID pvBuffer,  ULONG_PTR *pInfo)
{
    NTSTATUS Status = STATUS_SUCCESS;
    *pInfo = 0;
    switch (Ctl)
    {
        case VPOXUSBIDC_INTERNAL_IOCTL_GET_VERSION:
        {
            PVPOXUSBIDC_VERSION pOut = (PVPOXUSBIDC_VERSION)pvBuffer;

            LOG(("VPOXUSBIDC_INTERNAL_IOCTL_GET_VERSION"));
            if (!pvBuffer)
            {
                WARN(("VPOXUSBIDC_INTERNAL_IOCTL_GET_VERSION: Buffer is NULL"));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            pOut->u32Major = VPOXUSBIDC_VERSION_MAJOR;
            pOut->u32Minor = VPOXUSBIDC_VERSION_MINOR;
            ASSERT_WARN(Status == STATUS_SUCCESS, ("unexpected status, 0x%x", Status));
            break;
        }

        case VPOXUSBIDC_INTERNAL_IOCTL_PROXY_STARTUP:
        {
            PVPOXUSBIDC_PROXY_STARTUP pOut = (PVPOXUSBIDC_PROXY_STARTUP)pvBuffer;

            LOG(("VPOXUSBIDC_INTERNAL_IOCTL_PROXY_STARTUP"));
            if (!pvBuffer)
            {
                WARN(("VPOXUSBIDC_INTERNAL_IOCTL_PROXY_STARTUP: Buffer is NULL"));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            PDEVICE_OBJECT pDevObj = pOut->u.pPDO;
            pOut->u.hDev = VPoxUsbFltProxyStarted(pDevObj);

            /* If we couldn't find the PDO in our list, that's a real problem and
             * the capturing will not really work. Log an error.
             */
            if (!pOut->u.hDev)
                vpoxUsbMonLogError(IO_ERR_DRIVER_ERROR, STATUS_SUCCESS, 2, sizeof("INTERNAL_IOCTL_PROXY_STARTUP"), "INTERNAL_IOCTL_PROXY_STARTUP");

            ASSERT_WARN(pOut->u.hDev, ("zero hDev"));
            ASSERT_WARN(Status == STATUS_SUCCESS, ("unexpected status, 0x%x", Status));
            break;
        }

        case VPOXUSBIDC_INTERNAL_IOCTL_PROXY_TEARDOWN:
        {
            PVPOXUSBIDC_PROXY_TEARDOWN pOut = (PVPOXUSBIDC_PROXY_TEARDOWN)pvBuffer;

            LOG(("VPOXUSBIDC_INTERNAL_IOCTL_PROXY_TEARDOWN"));
            if (!pvBuffer)
            {
                WARN(("VPOXUSBIDC_INTERNAL_IOCTL_PROXY_TEARDOWN: Buffer is NULL"));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            ASSERT_WARN(pOut->hDev, ("zero hDev"));
            VPoxUsbFltProxyStopped(pOut->hDev);
            ASSERT_WARN(Status == STATUS_SUCCESS, ("unexpected status, 0x%x", Status));
            break;
        }

        default:
        {
            WARN(("Unknown code 0x%x", Ctl));
            Status = STATUS_INVALID_PARAMETER;
            break;
        }
    }

    return Status;
}

static NTSTATUS _stdcall VPoxUsbMonInternalDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    ULONG_PTR Info = 0;
    NTSTATUS Status = IoAcquireRemoveLock(&g_VPoxUsbMonGlobals.RmLock, pDevObj);
    if (NT_SUCCESS(Status))
    {
        PIO_STACK_LOCATION pSl = IoGetCurrentIrpStackLocation(pIrp);
        Status = vpoxUsbMonInternalIoctlDispatch(pSl->Parameters.DeviceIoControl.IoControlCode,
                        pSl->Parameters.Others.Argument1,
                        &Info);
        Assert(Status != STATUS_PENDING);

        IoReleaseRemoveLock(&g_VPoxUsbMonGlobals.RmLock, pDevObj);
    }

    pIrp->IoStatus.Information = Info;
    pIrp->IoStatus.Status = Status;
    IoCompleteRequest (pIrp, IO_NO_INCREMENT);
    return Status;
}

/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
static void _stdcall VPoxUsbMonUnload(PDRIVER_OBJECT pDrvObj)
{
    RT_NOREF1(pDrvObj);
    LOG(("VPoxUSBMonUnload pDrvObj (0x%p)", pDrvObj));

    IoReleaseRemoveLockAndWait(&g_VPoxUsbMonGlobals.RmLock, &g_VPoxUsbMonGlobals);

    Assert(!g_VPoxUsbMonGlobals.cOpens);

    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, USBMON_DEVICE_NAME_DOS);
    IoDeleteSymbolicLink(&DosName);

    IoDeleteDevice(g_VPoxUsbMonGlobals.pDevObj);

    /* cleanup the logger */
    PRTLOGGER pLogger = RTLogRelSetDefaultInstance(NULL);
    if (pLogger)
        RTLogDestroy(pLogger);
    pLogger = RTLogSetDefaultInstance(NULL);
    if (pLogger)
        RTLogDestroy(pLogger);
}

RT_C_DECLS_BEGIN
NTSTATUS _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
RT_C_DECLS_END

/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
NTSTATUS _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    RT_NOREF1(pRegPath);
#ifdef VPOX_USB_WITH_VERBOSE_LOGGING
    RTLogGroupSettings(0, "+default.e.l.f.l2.l3");
    RTLogDestinations(0, "debugger");
#endif

    LOGREL(("Built %s %s", __DATE__, __TIME__));

    memset (&g_VPoxUsbMonGlobals, 0, sizeof (g_VPoxUsbMonGlobals));

    VPOX_PNPHOOKSTUB_INIT(0);
    VPOX_PNPHOOKSTUB_INIT(1);
    VPOX_PNPHOOKSTUB_INIT(2);
    VPOX_PNPHOOKSTUB_INIT(3);
    VPOX_PNPHOOKSTUB_INIT(4);
    AssertCompile(VPOXUSBMON_MAXDRIVERS == 5);

    KeInitializeEvent(&g_VPoxUsbMonGlobals.OpenSynchEvent, SynchronizationEvent, TRUE /* signaled */);
    IoInitializeRemoveLock(&g_VPoxUsbMonGlobals.RmLock, VPOXUSBMON_MEMTAG, 1, 100);
    UNICODE_STRING DevName;
    PDEVICE_OBJECT pDevObj;
    /* create the device */
    RtlInitUnicodeString(&DevName, USBMON_DEVICE_NAME_NT);
    NTSTATUS Status = IoAcquireRemoveLock(&g_VPoxUsbMonGlobals.RmLock, &g_VPoxUsbMonGlobals);
    if (NT_SUCCESS(Status))
    {
        Status = IoCreateDevice(pDrvObj, sizeof (VPOXUSBMONINS), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevObj);
        if (NT_SUCCESS(Status))
        {
            UNICODE_STRING DosName;
            RtlInitUnicodeString(&DosName, USBMON_DEVICE_NAME_DOS);
            Status = IoCreateSymbolicLink(&DosName, &DevName);
            if (NT_SUCCESS(Status))
            {
                PVPOXUSBMONINS pDevExt = (PVPOXUSBMONINS)pDevObj->DeviceExtension;
                memset(pDevExt, 0, sizeof(*pDevExt));

                pDrvObj->DriverUnload = VPoxUsbMonUnload;
                pDrvObj->MajorFunction[IRP_MJ_CREATE] = VPoxUsbMonCreate;
                pDrvObj->MajorFunction[IRP_MJ_CLOSE] = VPoxUsbMonClose;
                pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VPoxUsbMonDeviceControl;
                pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = VPoxUsbMonInternalDeviceControl;

                g_VPoxUsbMonGlobals.pDevObj = pDevObj;
                LOG(("VPoxUSBMon::DriverEntry returning STATUS_SUCCESS"));
                return STATUS_SUCCESS;
            }
            IoDeleteDevice(pDevObj);
        }
        IoReleaseRemoveLockAndWait(&g_VPoxUsbMonGlobals.RmLock, &g_VPoxUsbMonGlobals);
    }

    return Status;
}
