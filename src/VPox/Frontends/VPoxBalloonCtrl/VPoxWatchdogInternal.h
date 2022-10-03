/* $Id: VPoxWatchdogInternal.h $ */
/** @file
 * VPoxWatchdog - VirtualPox Watchdog Service.
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

#ifndef VPOX_INCLUDED_SRC_VPoxBalloonCtrl_VPoxWatchdogInternal_h
#define VPOX_INCLUDED_SRC_VPoxBalloonCtrl_VPoxWatchdogInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifndef VPOX_ONLY_DOCS
# include <iprt/getopt.h>
# include <iprt/time.h>

# include <VPox/err.h>
# include <VPox/com/com.h>
# include <VPox/com/string.h>
# include <VPox/com/Guid.h>
# include <VPox/com/array.h>
# include <VPox/com/ErrorInfo.h>
# include <VPox/com/VirtualPox.h>
#endif /* !VPOX_ONLY_DOCS */

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace com;

////////////////////////////////////////////////////////////////////////////////
//
// definitions
//
////////////////////////////////////////////////////////////////////////////////

/** Command handler argument. */
struct HandlerArg
{
    int argc;
    char **argv;
};

/**
 * A module's payload for a machine entry.
 * The payload data is not (yet) thread safe -- so only
 * use this in one module at a time only!
 */
typedef struct VPOXWATCHDOG_MODULE_PAYLOAD
{
    /** Pointer to allocated payload. Can be NULL if
     * a module doesn't have an own payload. */
    void *pvData;
    /** Size of payload (in bytes). */
    size_t cbData;
    /** @todo Add mutex for locking + getPayloadLocked(). */
} VPOXWATCHDOG_MODULE_PAYLOAD, *PVPOXWATCHDOG_MODULE_PAYLOAD;

/**
 * Map containing a module's individual payload -- the module itself
 * is responsible for allocating/handling/destroying this payload.
 * Primary key is the module name.
 */
typedef std::map<const char*, VPOXWATCHDOG_MODULE_PAYLOAD> mapPayload;
typedef std::map<const char*, VPOXWATCHDOG_MODULE_PAYLOAD>::iterator mapPayloadIter;
typedef std::map<const char*, VPOXWATCHDOG_MODULE_PAYLOAD>::const_iterator mapPayloadIterConst;

/** Group list (plus additional per-group flags, not used yet) for one VM.
 *  Primary key is the group name, secondary specify flags (if any). */
typedef std::map<Utf8Str, uint32_t> mapGroups;
typedef std::map<Utf8Str, uint32_t>::iterator mapGroupsIter;
typedef std::map<Utf8Str, uint32_t>::const_iterator mapGroupsIterConst;

/** A machine's internal entry.
 *  Primary key is the machine's UUID. */
typedef struct VPOXWATCHDOG_MACHINE
{
    ComPtr<IMachine> machine;
    /** The machine's name. For logging. */
    Bstr strName;
#ifndef VPOX_WATCHDOG_GLOBAL_PERFCOL
    ComPtr<IPerformanceCollector> collector;
#endif
    /** The group(s) this machine belongs to. */
    mapGroups groups;
    /** Map containing the individual module payloads. */
    mapPayload payload;
} VPOXWATCHDOG_MACHINE, *PVPOXWATCHDOG_MACHINE;
typedef std::map<Bstr, VPOXWATCHDOG_MACHINE> mapVM;
typedef std::map<Bstr, VPOXWATCHDOG_MACHINE>::iterator mapVMIter;
typedef std::map<Bstr, VPOXWATCHDOG_MACHINE>::const_iterator mapVMIterConst;

/** Members of a VM group; currently only represented by the machine's UUID.
 *  Primary key is the machine's UUID. */
typedef std::vector<Bstr> vecGroupMembers;
typedef std::vector<Bstr>::iterator vecGroupMembersIter;
typedef std::vector<Bstr>::const_iterator vecGroupMembersIterConst;

/** A VM group. Can contain none, one or more group members.
 *  Primary key is the group's name. */
typedef std::map<Utf8Str, vecGroupMembers> mapGroup;
typedef std::map<Utf8Str, vecGroupMembers>::iterator mapGroupIter;
typedef std::map<Utf8Str, vecGroupMembers>::const_iterator mapGroupIterConst;

/**
 * A module descriptor.
 */
typedef struct
{
    /** The short module name. */
    const char *pszName;
    /** The longer module name. */
    const char *pszDescription;
    /** A comma-separated list of modules this module
     *  depends on. Might be NULL if no dependencies. */
    const char *pszDepends;
    /** Priority (lower is higher, 0 is invalid) of
     *  module execution. */
    uint32_t    uPriority;
    /** The usage options stuff for the --help screen. */
    const char *pszUsage;
    /** The option descriptions for the --help screen. */
    const char *pszOptions;

    /**
     * Called before parsing arguments.
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnPreInit)(void);

    /**
     * Tries to parse the given command line options.
     *
     * @returns 0 if we parsed, -1 if it didn't and anything else means exit.
     * @param   argc        Argument count.
     * @param   argv        Arguments.
     * @param   piConsumed  How many parameters this callback consumed from the
     *                      remaining arguments passed in.
     */
    DECLCALLBACKMEMBER(int, pfnOption)(int argc, char *argv[], int *piConsumed);

    /**
     * Called before parsing arguments.
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit)(void);

    /** Called from the watchdog's main function. Non-blocking.
     *
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnMain)(void);

    /**
     * Stop the module.
     */
    DECLCALLBACKMEMBER(int, pfnStop)(void);

    /**
     * Does termination cleanups.
     *
     * @remarks This may be called even if pfnInit hasn't been called!
     */
    DECLCALLBACKMEMBER(void, pfnTerm)(void);

    /** @name  Callbacks.
     * @{
     */

    /**
     *
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnOnMachineRegistered)(const Bstr &strUuid);

    /**
     *
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnOnMachineUnregistered)(const Bstr &strUuid);

    /**
     *
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnOnMachineStateChanged)(const Bstr &strUuid, MachineState_T enmState);

    /**
     *
     * @returns VPox status code.
     */
    DECLCALLBACKMEMBER(int, pfnOnServiceStateChanged)(bool fAvailable);

    /** @} */
} VPOXMODULE;
/** Pointer to a VPOXMODULE. */
typedef VPOXMODULE *PVPOXMODULE;
/** Pointer to a const VPOXMODULE. */
typedef VPOXMODULE const *PCVPOXMODULE;

RT_C_DECLS_BEGIN

extern bool                             g_fDryrun;
extern bool                             g_fVerbose;
extern ComPtr<IVirtualPox>              g_pVirtualPox;
extern ComPtr<ISession>                 g_pSession;
extern mapVM                            g_mapVM;
extern mapGroup                         g_mapGroup;
# ifdef VPOX_WATCHDOG_GLOBAL_PERFCOL
extern ComPtr<IPerformanceCollector>    g_pPerfCollector;
# endif

extern VPOXMODULE g_ModBallooning;
extern VPOXMODULE g_ModAPIMonitor;

extern void serviceLog(const char *pszFormat, ...);
#define serviceLogVerbose(a) if (g_fVerbose) { serviceLog a; }

int groupAdd(mapGroups &groups, const char *pszGroupsToAdd, uint32_t fFlags);

extern int getMetric(PVPOXWATCHDOG_MACHINE pMachine, const Bstr& strName, LONG *pulData);
void* payloadFrom(PVPOXWATCHDOG_MACHINE pMachine, const char *pszModule);
int payloadAlloc(PVPOXWATCHDOG_MACHINE pMachine, const char *pszModule, size_t cbSize, void **ppszPayload);
void payloadFree(PVPOXWATCHDOG_MACHINE pMachine, const char *pszModule);

PVPOXWATCHDOG_MACHINE getMachine(const Bstr& strUuid);
MachineState_T getMachineState(const PVPOXWATCHDOG_MACHINE pMachine);

int cfgGetValueStr(const ComPtr<IVirtualPox> &rptrVPox, const ComPtr<IMachine> &rptrMachine,
                   const char *pszGlobal, const char *pszVM, Utf8Str &strValue, Utf8Str strDefault);
int cfgGetValueU32(const ComPtr<IVirtualPox> &rptrVPox, const ComPtr<IMachine> &rptrMachine,
                   const char *pszGlobal, const char *pszVM, uint32_t *puValue, uint32_t uDefault);
RT_C_DECLS_END

#endif /* !VPOX_INCLUDED_SRC_VPoxBalloonCtrl_VPoxWatchdogInternal_h */

