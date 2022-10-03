/* $Id: VPoxFsDxe.c $ */
/** @file
 * VPoxFsDxe.c - VirtualPox FS wrapper
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/PciIo.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <IndustryStandard/Pci22.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static EFI_STATUS EFIAPI
VPoxFsDB_Supported(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                   IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL);
static EFI_STATUS EFIAPI
VPoxFsDB_Start(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
               IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL);
static EFI_STATUS EFIAPI
VPoxFsDB_Stop(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
              IN UINTN NumberOfChildren, IN EFI_HANDLE *ChildHandleBuffer OPTIONAL);

static EFI_STATUS EFIAPI
VPoxFsCN_GetDriverName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                       IN CHAR8 *Language, OUT CHAR16 **DriverName);
static EFI_STATUS EFIAPI
VPoxFsCN_GetControllerName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                           IN EFI_HANDLE ControllerHandle,
                           IN EFI_HANDLE ChildHandle OPTIONAL,
                           IN CHAR8 *Language,  OUT CHAR16 **ControllerName);

static EFI_STATUS EFIAPI
VPoxFsCN2_GetDriverName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                        IN CHAR8 *Language, OUT CHAR16 **DriverName);
static EFI_STATUS EFIAPI
VPoxFsCN2_GetControllerName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                            IN EFI_HANDLE ControllerHandle,
                            IN EFI_HANDLE ChildHandle OPTIONAL,
                            IN CHAR8 *Language,  OUT CHAR16 **ControllerName);


/** EFI Driver Binding Protocol. */
static EFI_DRIVER_BINDING_PROTOCOL          g_VPoxFsDB =
{
    VPoxFsDB_Supported,
    VPoxFsDB_Start,
    VPoxFsDB_Stop,
    /* .Version             = */    1,
    /* .ImageHandle         = */ NULL,
    /* .DriverBindingHandle = */ NULL
};

/** EFI Component Name Protocol. */
static const EFI_COMPONENT_NAME_PROTOCOL    g_VPoxFsCN =
{
    VPoxFsCN_GetDriverName,
    VPoxFsCN_GetControllerName,
    "eng"
};

/** EFI Component Name 2 Protocol. */
static const EFI_COMPONENT_NAME2_PROTOCOL   g_VPoxFsCN2 =
{
    VPoxFsCN2_GetDriverName,
    VPoxFsCN2_GetControllerName,
    "en"
};

/** Driver name translation table. */
static CONST EFI_UNICODE_STRING_TABLE       g_aVPoxFsDriverLangAndNames[] =
{
    {   "eng;en",   L"VPox Universal FS Wrapper Driver" },
    {   NULL,       NULL }
};



/**
 * VPoxFsDxe entry point.
 *
 * @returns EFI status code.
 *
 * @param   ImageHandle     The image handle.
 * @param   SystemTable     The system table pointer.
 */
EFI_STATUS EFIAPI
DxeInitializeVPoxFs(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS  rc;
    DEBUG((DEBUG_INFO, "DxeInitializeVPoxFsDxe\n"));

    rc = EfiLibInstallDriverBindingComponentName2(ImageHandle, SystemTable,
                                                  &g_VPoxFsDB, ImageHandle,
                                                  &g_VPoxFsCN, &g_VPoxFsCN2);
    ASSERT_EFI_ERROR(rc);
    return rc;
}

EFI_STATUS EFIAPI
DxeUninitializeVPoxFs(IN EFI_HANDLE         ImageHandle)
{
    return EFI_SUCCESS;
}


/**
 * @copydoc EFI_DRIVER_BINDING_SUPPORTED
 */
static EFI_STATUS EFIAPI
VPoxFsDB_Supported(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
                   IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL)
{
    EFI_STATUS              rcRet = EFI_UNSUPPORTED;
    /* EFI_STATUS              rc; */

    return rcRet;
}


/**
 * @copydoc EFI_DRIVER_BINDING_START
 */
static EFI_STATUS EFIAPI
VPoxFsDB_Start(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
               IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL)
{
    /* EFI_STATUS              rc; */

    return  EFI_UNSUPPORTED;
}


/**
 * @copydoc EFI_DRIVER_BINDING_STOP
 */
static EFI_STATUS EFIAPI
VPoxFsDB_Stop(IN EFI_DRIVER_BINDING_PROTOCOL *This, IN EFI_HANDLE ControllerHandle,
              IN UINTN NumberOfChildren, IN EFI_HANDLE *ChildHandleBuffer OPTIONAL)
{
    /* EFI_STATUS                  rc; */

    return  EFI_UNSUPPORTED;
}


/** @copydoc EFI_COMPONENT_NAME_GET_DRIVER_NAME */
static EFI_STATUS EFIAPI
VPoxFsCN_GetDriverName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                       IN CHAR8 *Language, OUT CHAR16 **DriverName)
{
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                &g_aVPoxFsDriverLangAndNames[0],
                                DriverName,
                                TRUE);
}

/** @copydoc EFI_COMPONENT_NAME_GET_CONTROLLER_NAME */
static EFI_STATUS EFIAPI
VPoxFsCN_GetControllerName(IN EFI_COMPONENT_NAME_PROTOCOL *This,
                                    IN EFI_HANDLE ControllerHandle,
                                    IN EFI_HANDLE ChildHandle OPTIONAL,
                                    IN CHAR8 *Language, OUT CHAR16 **ControllerName)
{
    /** @todo try query the protocol from the controller and forward the query. */
    return EFI_UNSUPPORTED;
}

/** @copydoc EFI_COMPONENT_NAME2_GET_DRIVER_NAME */
static EFI_STATUS EFIAPI
VPoxFsCN2_GetDriverName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                        IN CHAR8 *Language, OUT CHAR16 **DriverName)
{
    return LookupUnicodeString2(Language,
                                This->SupportedLanguages,
                                &g_aVPoxFsDriverLangAndNames[0],
                                DriverName,
                                FALSE);
}

/** @copydoc EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME */
static EFI_STATUS EFIAPI
VPoxFsCN2_GetControllerName(IN EFI_COMPONENT_NAME2_PROTOCOL *This,
                            IN EFI_HANDLE ControllerHandle,
                            IN EFI_HANDLE ChildHandle OPTIONAL,
                            IN CHAR8 *Language, OUT CHAR16 **ControllerName)
{
    /** @todo try query the protocol from the controller and forward the query. */
    return EFI_UNSUPPORTED;
}
