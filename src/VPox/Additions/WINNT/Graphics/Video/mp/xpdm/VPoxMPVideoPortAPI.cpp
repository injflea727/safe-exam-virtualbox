/* $Id: VPoxMPVideoPortAPI.cpp $ */
/** @file
 * VPox XPDM Miniport video port api
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
 */

#include "VPoxMPInternal.h"

/*Empty stubs*/
static VP_STATUS
vpoxWaitForSingleObjectVoid(IN PVOID  HwDeviceExtension, IN PVOID  Object, IN PLARGE_INTEGER  Timeout  OPTIONAL)
{
    RT_NOREF(HwDeviceExtension, Object, Timeout);
    WARN(("stub called"));
    return ERROR_INVALID_FUNCTION;
}

static LONG vpoxSetEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    RT_NOREF(HwDeviceExtension, pEvent);
    WARN(("stub called"));
    return 0;
}

static VOID vpoxClearEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    RT_NOREF(HwDeviceExtension, pEvent);
    WARN(("stub called"));
}

static VP_STATUS
vpoxCreateEventVoid(IN PVOID  HwDeviceExtension, IN ULONG  EventFlag, IN PVOID  Unused, OUT PEVENT  *ppEvent)
{
    RT_NOREF(HwDeviceExtension, EventFlag, Unused, ppEvent);
    WARN(("stub called"));
    return ERROR_INVALID_FUNCTION;
}

static VP_STATUS
vpoxDeleteEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    RT_NOREF(HwDeviceExtension, pEvent);
    WARN(("stub called"));
    return ERROR_INVALID_FUNCTION;
}

static PVOID
vpoxAllocatePoolVoid(IN PVOID  HwDeviceExtension, IN VPOXVP_POOL_TYPE  PoolType, IN size_t  NumberOfBytes, IN ULONG  Tag)
{
    RT_NOREF(HwDeviceExtension, PoolType, NumberOfBytes, Tag);
    WARN(("stub called"));
    return NULL;
}

static VOID vpoxFreePoolVoid(IN PVOID  HwDeviceExtension, IN PVOID  Ptr)
{
    RT_NOREF(HwDeviceExtension, Ptr);
    WARN(("stub called"));
}

static BOOLEAN
vpoxQueueDpcVoid(IN PVOID  HwDeviceExtension, IN PMINIPORT_DPC_ROUTINE  CallbackRoutine, IN PVOID  Context)
{
    RT_NOREF(HwDeviceExtension, CallbackRoutine, Context);
    WARN(("stub called"));
    return FALSE;
}

static VPOXVP_STATUS
vpoxCreateSecondaryDisplayVoid(IN PVOID HwDeviceExtension, IN OUT PVOID SecondaryDeviceExtension, IN ULONG fFlag)
{
    RT_NOREF(HwDeviceExtension, SecondaryDeviceExtension, fFlag);
    WARN(("stub called"));
    return ERROR_INVALID_FUNCTION;
}

#define VP_GETPROC(dst, type, name) \
{                                                                                   \
    pAPI->dst = (type)(pConfigInfo->VideoPortGetProcAddress)(pExt, (PUCHAR)(name)); \
}

/*Query video port for api functions or fill with stubs if those are not supported*/
void VPoxSetupVideoPortAPI(PVPOXMP_DEVEXT pExt, PVIDEO_PORT_CONFIG_INFO pConfigInfo)
{
    VPOXVIDEOPORTPROCS *pAPI = &pExt->u.primary.VideoPortProcs;
    VideoPortZeroMemory(pAPI, sizeof(VPOXVIDEOPORTPROCS));

    if (VPoxQueryWinVersion(NULL) <= WINVERSION_NT4)
    {
        /* VideoPortGetProcAddress is available for >= win2k */
        pAPI->pfnWaitForSingleObject = vpoxWaitForSingleObjectVoid;
        pAPI->pfnSetEvent = vpoxSetEventVoid;
        pAPI->pfnClearEvent = vpoxClearEventVoid;
        pAPI->pfnCreateEvent = vpoxCreateEventVoid;
        pAPI->pfnDeleteEvent = vpoxDeleteEventVoid;
        pAPI->pfnAllocatePool = vpoxAllocatePoolVoid;
        pAPI->pfnFreePool = vpoxFreePoolVoid;
        pAPI->pfnQueueDpc = vpoxQueueDpcVoid;
        pAPI->pfnCreateSecondaryDisplay = vpoxCreateSecondaryDisplayVoid;
        return;
    }

    VP_GETPROC(pfnWaitForSingleObject, PFNWAITFORSINGLEOBJECT, "VideoPortWaitForSingleObject");
    VP_GETPROC(pfnSetEvent, PFNSETEVENT, "VideoPortSetEvent");
    VP_GETPROC(pfnClearEvent, PFNCLEAREVENT, "VideoPortClearEvent");
    VP_GETPROC(pfnCreateEvent, PFNCREATEEVENT, "VideoPortCreateEvent");
    VP_GETPROC(pfnDeleteEvent, PFNDELETEEVENT, "VideoPortDeleteEvent");

    if(pAPI->pfnWaitForSingleObject
       && pAPI->pfnSetEvent
       && pAPI->pfnClearEvent
       && pAPI->pfnCreateEvent
       && pAPI->pfnDeleteEvent)
    {
        pAPI->fSupportedTypes |= VPOXVIDEOPORTPROCS_EVENT;
    }
    else
    {
        pAPI->pfnWaitForSingleObject = vpoxWaitForSingleObjectVoid;
        pAPI->pfnSetEvent = vpoxSetEventVoid;
        pAPI->pfnClearEvent = vpoxClearEventVoid;
        pAPI->pfnCreateEvent = vpoxCreateEventVoid;
        pAPI->pfnDeleteEvent = vpoxDeleteEventVoid;
    }

    VP_GETPROC(pfnAllocatePool, PFNALLOCATEPOOL, "VideoPortAllocatePool");
    VP_GETPROC(pfnFreePool, PFNFREEPOOL, "VideoPortFreePool");

    if(pAPI->pfnAllocatePool
       && pAPI->pfnFreePool)
    {
        pAPI->fSupportedTypes |= VPOXVIDEOPORTPROCS_POOL;
    }
    else
    {
        pAPI->pfnAllocatePool = vpoxAllocatePoolVoid;
        pAPI->pfnFreePool = vpoxFreePoolVoid;
    }

    VP_GETPROC(pfnQueueDpc, PFNQUEUEDPC, "VideoPortQueueDpc");

    if(pAPI->pfnQueueDpc)
    {
        pAPI->fSupportedTypes |= VPOXVIDEOPORTPROCS_DPC;
    }
    else
    {
        pAPI->pfnQueueDpc = vpoxQueueDpcVoid;
    }

    VP_GETPROC(pfnCreateSecondaryDisplay, PFNCREATESECONDARYDISPLAY, "VideoPortCreateSecondaryDisplay");

    if (pAPI->pfnCreateSecondaryDisplay)
    {
        pAPI->fSupportedTypes |= VPOXVIDEOPORTPROCS_CSD;
    }
    else
    {
        pAPI->pfnCreateSecondaryDisplay = vpoxCreateSecondaryDisplayVoid;
    }
}

#undef VP_GETPROC
