/** @file
 * VPoxDisplay - private windows additions display header
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

#ifndef GA_INCLUDED_WINNT_VPoxDisplay_h
#define GA_INCLUDED_WINNT_VPoxDisplay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/assert.h>

#define VPOXESC_SETVISIBLEREGION            0xABCD9001
#define VPOXESC_ISVRDPACTIVE                0xABCD9002
#ifdef VPOX_WITH_WDDM
# define VPOXESC_REINITVIDEOMODES           0xABCD9003
# define VPOXESC_GETVPOXVIDEOCMCMD          0xABCD9004
# define VPOXESC_DBGPRINT                   0xABCD9005
# define VPOXESC_SCREENLAYOUT               0xABCD9006
// obsolete                                 0xABCD9007
// obsolete                                 0xABCD9008
// obsolete                                 0xABCD9009
// obsolete                                 0xABCD900A
// obsolete                                 0xABCD900B
// obsolete                                 0xABCD900C
# define VPOXESC_DBGDUMPBUF                 0xABCD900D
// obsolete                                 0xABCD900E
// obsolete                                 0xABCD900F
# define VPOXESC_REINITVIDEOMODESBYMASK     0xABCD9010
# define VPOXESC_ADJUSTVIDEOMODES           0xABCD9011
// obsolete                                 0xABCD9012
# define VPOXESC_CONFIGURETARGETS           0xABCD9013
# define VPOXESC_SETALLOCHOSTID             0xABCD9014
// obsolete                                 0xABCD9015
# define VPOXESC_UPDATEMODES                0xABCD9016
# define VPOXESC_GUEST_DISPLAYCHANGED       0xABCD9017
# define VPOXESC_TARGET_CONNECTIVITY        0xABCD9018
#endif /* #ifdef VPOX_WITH_WDDM */

# define VPOXESC_ISANYX                     0xABCD9200

typedef struct VPOXDISPIFESCAPE
{
    int32_t escapeCode;
    uint32_t u32CmdSpecific;
} VPOXDISPIFESCAPE, *PVPOXDISPIFESCAPE;

/* ensure command body is always 8-byte-aligned*/
AssertCompile((sizeof (VPOXDISPIFESCAPE) & 7) == 0);

#define VPOXDISPIFESCAPE_DATA_OFFSET() ((sizeof (VPOXDISPIFESCAPE) + 7) & ~7)
#define VPOXDISPIFESCAPE_DATA(_pHead, _t) ( (_t*)(((uint8_t*)(_pHead)) + VPOXDISPIFESCAPE_DATA_OFFSET()))
#define VPOXDISPIFESCAPE_DATA_SIZE(_s) ( (_s) < VPOXDISPIFESCAPE_DATA_OFFSET() ? 0 : (_s) - VPOXDISPIFESCAPE_DATA_OFFSET() )
#define VPOXDISPIFESCAPE_SIZE(_cbData) ((_cbData) ? VPOXDISPIFESCAPE_DATA_OFFSET() + (_cbData) : sizeof (VPOXDISPIFESCAPE))

#define IOCTL_VIDEO_VPOX_SETVISIBLEREGION \
    CTL_CODE(FILE_DEVICE_VIDEO, 0xA01, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_VPOX_ISANYX \
    CTL_CODE(FILE_DEVICE_VIDEO, 0xA02, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct VPOXDISPIFESCAPE_ISANYX
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32IsAnyX;
} VPOXDISPIFESCAPE_ISANYX, *PVPOXDISPIFESCAPE_ISANYX;

#ifdef VPOX_WITH_WDDM

/* Enables code which performs (un)plugging of virtual displays in VPOXESC_UPDATEMODES.
 * The code has been disabled as part of #8244.
 */
//#define VPOX_WDDM_REPLUG_ON_MODE_CHANGE

/* for VPOX_VIDEO_MAX_SCREENS definition */
#include <VPoxVideo.h>

typedef struct VPOXWDDM_RECOMMENDVIDPN_SOURCE
{
    RTRECTSIZE Size;
} VPOXWDDM_RECOMMENDVIDPN_SOURCE;

typedef struct VPOXWDDM_RECOMMENDVIDPN_TARGET
{
    int32_t iSource;
} VPOXWDDM_RECOMMENDVIDPN_TARGET;

typedef struct
{
    VPOXWDDM_RECOMMENDVIDPN_SOURCE aSources[VPOX_VIDEO_MAX_SCREENS];
    VPOXWDDM_RECOMMENDVIDPN_TARGET aTargets[VPOX_VIDEO_MAX_SCREENS];
} VPOXWDDM_RECOMMENDVIDPN, *PVPOXWDDM_RECOMMENDVIDPN;

#define VPOXWDDM_SCREENMASK_SIZE ((VPOX_VIDEO_MAX_SCREENS + 7) >> 3)

typedef struct VPOXDISPIFESCAPE_UPDATEMODES
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32TargetId;
    RTRECTSIZE Size;
} VPOXDISPIFESCAPE_UPDATEMODES;

typedef struct VPOXDISPIFESCAPE_TARGETCONNECTIVITY
{
    VPOXDISPIFESCAPE EscapeHdr;
    uint32_t u32TargetId;
    uint32_t fu32Connect;
} VPOXDISPIFESCAPE_TARGETCONNECTIVITY;

#endif /* VPOX_WITH_WDDM */

#endif /* !GA_INCLUDED_WINNT_VPoxDisplay_h */

