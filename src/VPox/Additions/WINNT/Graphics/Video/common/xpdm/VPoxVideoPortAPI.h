/* $Id: VPoxVideoPortAPI.h $ */
/** @file
 * VPox video port functions header
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_common_xpdm_VPoxVideoPortAPI_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_common_xpdm_VPoxVideoPortAPI_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* To maintain binary backward compatibility with older windows versions
 * we query at runtime for video port functions which are not present in NT 4.0
 * Those could used in the display driver also.
 */

/*Basic datatypes*/
typedef long VPOXVP_STATUS;
#ifndef VPOX_USING_W2K3DDK
typedef struct _ENG_EVENT *VPOXPEVENT;
#else
typedef struct _VIDEO_PORT_EVENT *VPOXPEVENT;
#endif
typedef struct _VIDEO_PORT_SPIN_LOCK *VPOXPSPIN_LOCK;
typedef union _LARGE_INTEGER *VPOXPLARGE_INTEGER;

typedef enum VPOXVP_POOL_TYPE
{
    VPoxVpNonPagedPool,
    VPoxVpPagedPool,
    VPoxVpNonPagedPoolCacheAligned = 4,
    VPoxVpPagedPoolCacheAligned
} VPOXVP_POOL_TYPE;

#define VPOXNOTIFICATION_EVENT 0x00000001UL
#define VPOXNO_ERROR           0x00000000UL

/*VideoPort API functions*/
typedef VPOXVP_STATUS (*PFNWAITFORSINGLEOBJECT) (void*  HwDeviceExtension, void*  Object, VPOXPLARGE_INTEGER  Timeout);
typedef long (*PFNSETEVENT) (void* HwDeviceExtension, VPOXPEVENT  pEvent);
typedef void (*PFNCLEAREVENT) (void*  HwDeviceExtension, VPOXPEVENT  pEvent);
typedef VPOXVP_STATUS (*PFNCREATEEVENT) (void*  HwDeviceExtension, unsigned long  EventFlag, void*  Unused, VPOXPEVENT  *ppEvent);
typedef VPOXVP_STATUS (*PFNDELETEEVENT) (void*  HwDeviceExtension, VPOXPEVENT  pEvent);
typedef void* (*PFNALLOCATEPOOL) (void*  HwDeviceExtension, VPOXVP_POOL_TYPE PoolType, size_t NumberOfBytes, unsigned long Tag);
typedef void (*PFNFREEPOOL) (void*  HwDeviceExtension, void*  Ptr);
typedef unsigned char (*PFNQUEUEDPC) (void* HwDeviceExtension, void (*CallbackRoutine)(void* HwDeviceExtension, void *Context), void *Context);
typedef VPOXVP_STATUS (*PFNCREATESECONDARYDISPLAY)(void* HwDeviceExtension, void* SecondaryDeviceExtension, unsigned long ulFlag);

/* pfn*Event and pfnWaitForSingleObject functions are available */
#define VPOXVIDEOPORTPROCS_EVENT    0x00000002
/* pfn*Pool functions are available */
#define VPOXVIDEOPORTPROCS_POOL     0x00000004
/* pfnQueueDpc function is available */
#define VPOXVIDEOPORTPROCS_DPC      0x00000008
/* pfnCreateSecondaryDisplay function is available */
#define VPOXVIDEOPORTPROCS_CSD      0x00000010

typedef struct VPOXVIDEOPORTPROCS
{
    /* ored VPOXVIDEOPORTPROCS_xxx constants describing the supported functionality */
    uint32_t fSupportedTypes;

    PFNWAITFORSINGLEOBJECT pfnWaitForSingleObject;

    PFNSETEVENT pfnSetEvent;
    PFNCLEAREVENT pfnClearEvent;
    PFNCREATEEVENT pfnCreateEvent;
    PFNDELETEEVENT pfnDeleteEvent;

    PFNALLOCATEPOOL pfnAllocatePool;
    PFNFREEPOOL pfnFreePool;

    PFNQUEUEDPC pfnQueueDpc;

    PFNCREATESECONDARYDISPLAY pfnCreateSecondaryDisplay;
} VPOXVIDEOPORTPROCS;

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_common_xpdm_VPoxVideoPortAPI_h */
