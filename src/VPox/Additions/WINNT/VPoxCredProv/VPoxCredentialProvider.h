/* $Id: VPoxCredentialProvider.h $ */
/** @file
 * VPoxCredentialProvider - Main file of the VirtualPox Credential Provider.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VPoxCredProv_VPoxCredentialProvider_h
#define GA_INCLUDED_SRC_WINNT_VPoxCredProv_VPoxCredentialProvider_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>
#include <credentialprovider.h>
#include <Shlguid.h>

#include "VPoxCredProvUtils.h"

/** The VirtualPox credential provider class ID -- must not be changed. */
DEFINE_GUID(CLSID_VPoxCredProvider, 0x275d3bcc, 0x22bb, 0x4948, 0xa7, 0xf6, 0x3a, 0x30, 0x54, 0xeb, 0xa9, 0x2b);

/**
 * The credential provider's UI field IDs, used for
 * handling / identifying them.
 */
enum VPOXCREDPROV_FIELDID
{
    VPOXCREDPROV_FIELDID_TILEIMAGE       = 0,
    VPOXCREDPROV_FIELDID_USERNAME        = 1,
    VPOXCREDPROV_FIELDID_PASSWORD        = 2,
    VPOXCREDPROV_FIELDID_DOMAINNAME      = 3,
    VPOXCREDPROV_FIELDID_SUBMIT_BUTTON   = 4,
    VPOXCREDPROV_FIELDID_PROVIDER_LOGO   = 5,
    VPOXCREDPROV_FIELDID_PROVIDER_LABEL  = 6
};

/* Note: If new fields are added to VPOXCREDPROV_FIELDID and s_VPoxCredProvFields,
         don't forget to increase this define! */
#define VPOXCREDPROV_NUM_FIELDS            7

/** Maximum credential provider field length (in characters). */
#define VPOXCREDPROV_MAX_FIELD_LEN         255

struct VPOXCREDPROV_FIELD
{
    /** The actual description of this field: It's label,
     *  official field type ID, ... */
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR desc;
    /** The field's display state within the. */
    CREDENTIAL_PROVIDER_FIELD_STATE state;
    /** The interactive state: Used when this field gets shown to determine
     *  its state -- currently, only focussing is implemented. */
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE stateInteractive;
};

#ifndef PCREDENTIAL_PROVIDER_FIELD_DESCRIPTOR
 #define PCREDENTIAL_PROVIDER_FIELD_DESCRIPTOR CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*
#endif

#ifndef CPFG_CREDENTIAL_PROVIDER_LOGO
 /* 2d837775-f6cd-464e-a745-482fd0b47493 */
 DEFINE_GUID(CPFG_CREDENTIAL_PROVIDER_LOGO, 0x2d837775, 0xf6cd, 0x464e, 0xa7, 0x45, 0x48, 0x2f, 0xd0, 0xb4, 0x74, 0x93);
#endif

#ifndef CPFG_CREDENTIAL_PROVIDER_LABEL
 /* 286BBFF3-BAD4-438F-B007-79B7267C3D48 */
 DEFINE_GUID(CPFG_CREDENTIAL_PROVIDER_LABEL, 0x286BBFF3, 0xBAD4, 0x438F, 0xB0 ,0x07, 0x79, 0xB7, 0x26, 0x7C, 0x3D, 0x48);
#endif

/** The credential provider's default fields. */
static const VPOXCREDPROV_FIELD s_VPoxCredProvDefaultFields[] =
{
    /** The user's profile image (tile). */
    { { VPOXCREDPROV_FIELDID_TILEIMAGE,      CPFT_TILE_IMAGE,    L"Tile Image",     0,                             }, CPFS_DISPLAY_IN_BOTH,            CPFIS_NONE    },
    { { VPOXCREDPROV_FIELDID_USERNAME,       CPFT_LARGE_TEXT,    L"Username",       CPFG_LOGON_USERNAME            }, CPFS_DISPLAY_IN_BOTH,            CPFIS_NONE    },
    { { VPOXCREDPROV_FIELDID_PASSWORD,       CPFT_PASSWORD_TEXT, L"Password",       CPFG_LOGON_PASSWORD            }, CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_FOCUSED },
    { { VPOXCREDPROV_FIELDID_DOMAINNAME,     CPFT_LARGE_TEXT,    L"",               0                              }, CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_FOCUSED },
    { { VPOXCREDPROV_FIELDID_SUBMIT_BUTTON,  CPFT_SUBMIT_BUTTON, L"Submit",         0                              }, CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_FOCUSED },
    /** New since Windows 8: The image used to represent a credential provider on the logon page. */
    { { VPOXCREDPROV_FIELDID_PROVIDER_LOGO,  CPFT_TILE_IMAGE,    L"Provider Logo",  CPFG_CREDENTIAL_PROVIDER_LOGO  }, CPFS_HIDDEN /* Not used yet. */, CPFIS_NONE },
    /** New since Windows 8: The label associated with a credential provider on the logon page. */
    { { VPOXCREDPROV_FIELDID_PROVIDER_LABEL, CPFT_SMALL_TEXT,    L"Provider Label", CPFG_CREDENTIAL_PROVIDER_LABEL }, CPFS_HIDDEN /* Not used yet. */, CPFIS_NONE }
};

/** Prototypes. */
void VPoxCredentialProviderAcquire(void);
void VPoxCredentialProviderRelease(void);
LONG VPoxCredentialProviderRefCount(void);

HRESULT VPoxCredentialProviderCreate(REFCLSID classID,
                                     REFIID interfaceID, void **ppvInterface);

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxCredProv_VPoxCredentialProvider_h */

