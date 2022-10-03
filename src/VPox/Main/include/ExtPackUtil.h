/* $Id: ExtPackUtil.h $ */
/** @file
 * VirtualPox Main - Extension Pack Utilities and definitions, VPoxC, VPoxSVC, ++.
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

#ifndef MAIN_INCLUDED_ExtPackUtil_h
#define MAIN_INCLUDED_ExtPackUtil_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef __cplusplus
# include <iprt/cpp/ministring.h>
#endif
#include <iprt/fs.h>
#include <iprt/vfs.h>


/** @name VPOX_EXTPACK_DESCRIPTION_NAME
 * The name of the description file in an extension pack.  */
#define VPOX_EXTPACK_DESCRIPTION_NAME   "ExtPack.xml"
/** @name VPOX_EXTPACK_DESCRIPTION_NAME
 * The name of the manifest file in an extension pack.  */
#define VPOX_EXTPACK_MANIFEST_NAME      "ExtPack.manifest"
/** @name VPOX_EXTPACK_SIGNATURE_NAME
 * The name of the signature file in an extension pack.  */
#define VPOX_EXTPACK_SIGNATURE_NAME     "ExtPack.signature"
/** @name VPOX_EXTPACK_LICENSE_NAME_PREFIX
 * The name prefix of a license file in an extension pack. There can be
 * several license files in a pack, the variations being on locale, language
 * and format (HTML, RTF, plain text). All extension packages shall include
 * a  */
#define VPOX_EXTPACK_LICENSE_NAME_PREFIX "ExtPack-license"
/** @name VPOX_EXTPACK_SUFFIX
 * The suffix of a extension pack tarball. */
#define VPOX_EXTPACK_SUFFIX             ".vpox-extpack"

/** The minimum length (strlen) of a extension pack name. */
#define VPOX_EXTPACK_NAME_MIN_LEN       3
/** The max length (strlen) of a extension pack name. */
#define VPOX_EXTPACK_NAME_MAX_LEN       64

/** The architecture-dependent application data subdirectory where the
 * extension packs are installed.  Relative to RTPathAppPrivateArch. */
#define VPOX_EXTPACK_INSTALL_DIR        "ExtensionPacks"
/** The architecture-independent application data subdirectory where the
 * certificates are installed.  Relative to RTPathAppPrivateNoArch. */
#define VPOX_EXTPACK_CERT_DIR           "ExtPackCertificates"

/** The maximum entry name length.
 * Play short and safe. */
#define VPOX_EXTPACK_MAX_MEMBER_NAME_LENGTH 128


#ifdef __cplusplus

/**
 * Plug-in descriptor.
 */
typedef struct VPOXEXTPACKPLUGINDESC
{
    /** The name. */
    RTCString        strName;
    /** The module name. */
    RTCString        strModule;
    /** The description. */
    RTCString        strDescription;
    /** The frontend or component which it plugs into. */
    RTCString        strFrontend;
} VPOXEXTPACKPLUGINDESC;
/** Pointer to a plug-in descriptor. */
typedef VPOXEXTPACKPLUGINDESC *PVPOXEXTPACKPLUGINDESC;

/**
 * Extension pack descriptor
 *
 * This is the internal representation of the ExtPack.xml.
 */
typedef struct VPOXEXTPACKDESC
{
    /** The name. */
    RTCString               strName;
    /** The description. */
    RTCString               strDescription;
    /** The version string. */
    RTCString               strVersion;
    /** The edition string. */
    RTCString               strEdition;
    /** The internal revision number. */
    uint32_t                uRevision;
    /** The name of the main module. */
    RTCString               strMainModule;
    /** The name of the main VM module, empty if none. */
    RTCString               strMainVMModule;
    /** The name of the VRDE module, empty if none. */
    RTCString               strVrdeModule;
    /** The number of plug-in descriptors. */
    uint32_t                cPlugIns;
    /** Pointer to an array of plug-in descriptors. */
    PVPOXEXTPACKPLUGINDESC  paPlugIns;
    /** Whether to show the license prior to installation. */
    bool                    fShowLicense;
} VPOXEXTPACKDESC;

/** Pointer to a extension pack descriptor. */
typedef VPOXEXTPACKDESC *PVPOXEXTPACKDESC;
/** Pointer to a const extension pack descriptor. */
typedef VPOXEXTPACKDESC const *PCVPOXEXTPACKDESC;


void                VPoxExtPackInitDesc(PVPOXEXTPACKDESC a_pExtPackDesc);
RTCString          *VPoxExtPackLoadDesc(const char *a_pszDir, PVPOXEXTPACKDESC a_pExtPackDesc, PRTFSOBJINFO a_pObjInfo);
RTCString          *VPoxExtPackLoadDescFromVfsFile(RTVFSFILE hVfsFile, PVPOXEXTPACKDESC a_pExtPackDesc, PRTFSOBJINFO a_pObjInfo);
RTCString          *VPoxExtPackExtractNameFromTarballPath(const char *pszTarball);
void                VPoxExtPackFreeDesc(PVPOXEXTPACKDESC a_pExtPackDesc);
bool                VPoxExtPackIsValidName(const char *pszName);
bool                VPoxExtPackIsValidMangledName(const char *pszMangledName, size_t cchMax = RTSTR_MAX);
RTCString          *VPoxExtPackMangleName(const char *pszName);
RTCString          *VPoxExtPackUnmangleName(const char *pszMangledName, size_t cbMax);
int                 VPoxExtPackCalcDir(char *pszExtPackDir, size_t cbExtPackDir, const char *pszParentDir, const char *pszName);
bool                VPoxExtPackIsValidVersionString(const char *pszVersion);
bool                VPoxExtPackIsValidEditionString(const char *pszEdition);
bool                VPoxExtPackIsValidModuleString(const char *pszModule);

int                 VPoxExtPackValidateMember(const char *pszName, RTVFSOBJTYPE enmType, RTVFSOBJ hVfsObj, char *pszError, size_t cbError);
int                 VPoxExtPackOpenTarFss(RTFILE hTarballFile, char *pszError, size_t cbError, PRTVFSFSSTREAM phTarFss, PRTMANIFEST phFileManifest);
int                 VPoxExtPackValidateTarball(RTFILE hTarballFile, const char *pszExtPackName,
                                               const char *pszTarball, const char *pszTarballDigest,
                                               char *pszError, size_t cbError,
                                               PRTMANIFEST phValidManifest, PRTVFSFILE phXmlFile, RTCString *pStrDigest);
#endif /* __cplusplus */

#endif /* !MAIN_INCLUDED_ExtPackUtil_h */

