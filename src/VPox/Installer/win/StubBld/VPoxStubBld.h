/* $Id: VPoxStubBld.h $ */
/** @file
 * VPoxStubBld - VirtualPox's Windows installer stub builder.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VPOX_INCLUDED_SRC_StubBld_VPoxStubBld_h
#define VPOX_INCLUDED_SRC_StubBld_VPoxStubBld_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VPOXSTUB_MAX_PACKAGES 128

typedef struct VPOXSTUBPKGHEADER
{
    /** Some magic string not defined by this header? Turns out it's a write only
     *  field... */
    char    szMagic[9];
    /* Inbetween szMagic and dwVersion there are 3 bytes of implicit padding. */
    /** Some version number not defined by this header? Also write only field.
     *  Should be a uint32_t, not DWORD. */
    DWORD   dwVersion;
    /** Number of packages following the header. byte is prefixed 'b', not 'by'!
     *  Use uint8_t instead of BYTE. */
    BYTE    byCntPkgs;
    /* There are 3 bytes of implicit padding here. */
} VPOXSTUBPKGHEADER;
typedef VPOXSTUBPKGHEADER *PVPOXSTUBPKGHEADER;

typedef enum VPOXSTUBPKGARCH
{
    VPOXSTUBPKGARCH_ALL = 0,
    VPOXSTUBPKGARCH_X86,
    VPOXSTUBPKGARCH_AMD64
} VPOXSTUBPKGARCH;

typedef struct VPOXSTUBPKG
{
    BYTE byArch;
    /** Probably the name of the PE resource or something, read the source to
     *  find out for sure.  Don't use _MAX_PATH, define your own max lengths! */
    char szResourceName[_MAX_PATH];
    char szFileName[_MAX_PATH];
} VPOXSTUBPKG;
typedef VPOXSTUBPKG *PVPOXSTUBPKG;

/* Only for construction. */
/* Since it's only used by VPoxStubBld.cpp, why not just keep it there? */

typedef struct VPOXSTUBBUILDPKG
{
    char szSourcePath[_MAX_PATH];
    BYTE byArch;
} VPOXSTUBBUILDPKG;
typedef VPOXSTUBBUILDPKG *PVPOXSTUBBUILDPKG;

#endif /* !VPOX_INCLUDED_SRC_StubBld_VPoxStubBld_h */
