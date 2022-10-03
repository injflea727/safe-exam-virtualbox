/** @file
 * VirtualPox - Extension Pack Interface.
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

#ifndef VPOX_INCLUDED_ExtPack_ExtPack_h
#define VPOX_INCLUDED_ExtPack_ExtPack_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VPox/types.h>

/** @def VPOXEXTPACK_IF_CS
 * Selects 'class' on 'struct' for interface references.
 * @param I         The interface name
 */
#if defined(__cplusplus) && !defined(RT_OS_WINDOWS)
# define VPOXEXTPACK_IF_CS(I)   class I
#else
# define VPOXEXTPACK_IF_CS(I)   struct I
#endif

VPOXEXTPACK_IF_CS(IUnknown);
VPOXEXTPACK_IF_CS(IConsole);
VPOXEXTPACK_IF_CS(IMachine);
VPOXEXTPACK_IF_CS(IVirtualPox);
VPOXEXTPACK_IF_CS(IProgress);

/**
 * Module kind for use with VPOXEXTPACKHLP::pfnFindModule.
 */
typedef enum VPOXEXTPACKMODKIND
{
    /** Zero is invalid as always. */
    VPOXEXTPACKMODKIND_INVALID = 0,
    /** Raw-mode context module. */
    VPOXEXTPACKMODKIND_RC,
    /** Ring-0 context module. */
    VPOXEXTPACKMODKIND_R0,
    /** Ring-3 context module. */
    VPOXEXTPACKMODKIND_R3,
    /** End of the valid values (exclusive). */
    VPOXEXTPACKMODKIND_END,
    /** The usual 32-bit type hack. */
    VPOXEXTPACKMODKIND_32BIT_HACK = 0x7fffffff
} VPOXEXTPACKMODKIND;

/**
 * Contexts returned by VPOXEXTPACKHLP::pfnGetContext.
 */
typedef enum VPOXEXTPACKCTX
{
    /** Zero is invalid as always. */
    VPOXEXTPACKCTX_INVALID = 0,
    /** The per-user daemon process (VPoxSVC). */
    VPOXEXTPACKCTX_PER_USER_DAEMON,
    /** A VM process. */
    VPOXEXTPACKCTX_VM_PROCESS,
    /** An API client process.
     * @remarks This will not be returned by VirtualPox yet. */
    VPOXEXTPACKCTX_CLIENT_PROCESS,
    /** End of the valid values (exclusive). */
    VPOXEXTPACKCTX_END,
    /** The usual 32-bit type hack. */
    VPOXEXTPACKCTX_32BIT_HACK = 0x7fffffff
} VPOXEXTPACKCTX;


/** Pointer to const helpers passed to the VPoxExtPackRegister() call. */
typedef const struct VPOXEXTPACKHLP *PCVPOXEXTPACKHLP;
/**
 * Extension pack helpers passed to VPoxExtPackRegister().
 *
 * This will be valid until the module is unloaded.
 */
typedef struct VPOXEXTPACKHLP
{
    /** Interface version.
     * This is set to VPOXEXTPACKHLP_VERSION. */
    uint32_t                    u32Version;

    /** The VirtualPox full version (see VPOX_FULL_VERSION).  */
    uint32_t                    uVPoxFullVersion;
    /** The VirtualPox subversion tree revision.  */
    uint32_t                    uVPoxInternalRevision;
    /** Explicit alignment padding, must be zero. */
    uint32_t                    u32Padding;
    /** Pointer to the version string (read-only). */
    const char                 *pszVPoxVersion;

    /**
     * Finds a module belonging to this extension pack.
     *
     * @returns VPox status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pszName         The module base name.
     * @param   pszExt          The extension. If NULL the default ring-3
     *                          library extension will be used.
     * @param   enmKind         The kind of module to locate.
     * @param   pszFound        Where to return the path to the module on
     *                          success.
     * @param   cbFound         The size of the buffer @a pszFound points to.
     * @param   pfNative        Where to return the native/agnostic indicator.
     */
    DECLR3CALLBACKMEMBER(int, pfnFindModule,(PCVPOXEXTPACKHLP pHlp, const char *pszName, const char *pszExt,
                                             VPOXEXTPACKMODKIND enmKind,
                                             char *pszFound, size_t cbFound, bool *pfNative));

    /**
     * Gets the path to a file belonging to this extension pack.
     *
     * @returns VPox status code.
     * @retval  VERR_INVALID_POINTER if any of the pointers are invalid.
     * @retval  VERR_BUFFER_OVERFLOW if the buffer is too small.  The buffer
     *          will contain nothing.
     *
     * @param   pHlp            Pointer to this helper structure.
     * @param   pszFilename     The filename.
     * @param   pszPath         Where to return the path to the file on
     *                          success.
     * @param   cbPath          The size of the buffer @a pszPath.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetFilePath,(PCVPOXEXTPACKHLP pHlp, const char *pszFilename, char *pszPath, size_t cbPath));

    /**
     * Gets the context the extension pack is operating in.
     *
     * @returns The context.
     * @retval  VPOXEXTPACKCTX_INVALID if @a pHlp is invalid.
     *
     * @param   pHlp            Pointer to this helper structure.
     */
    DECLR3CALLBACKMEMBER(VPOXEXTPACKCTX, pfnGetContext,(PCVPOXEXTPACKHLP pHlp));

    /**
     * Loads a HGCM service provided by an extension pack.
     *
     * @returns VPox status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pConsole        Pointer to the VM's console object.
     * @param   pszServiceLibrary Name of the library file containing the
     *                          service implementation, without extension.
     * @param   pszServiceName  Name of HGCM service.
     */
    DECLR3CALLBACKMEMBER(int, pfnLoadHGCMService,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IConsole) *pConsole,
                                                  const char *pszServiceLibrary, const char *pszServiceName));

    /**
     * Loads a VD plugin provided by an extension pack.
     *
     * This makes sense only in the context of the per-user service (VPoxSVC).
     *
     * @returns VPox status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pVirtualPox     Pointer to the VirtualPox object.
     * @param   pszPluginLibrary Name of the library file containing the plugin
     *                          implementation, without extension.
     */
    DECLR3CALLBACKMEMBER(int, pfnLoadVDPlugin,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox,
                                               const char *pszPluginLibrary));

    /**
     * Unloads a VD plugin provided by an extension pack.
     *
     * This makes sense only in the context of the per-user service (VPoxSVC).
     *
     * @returns VPox status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pVirtualPox     Pointer to the VirtualPox object.
     * @param   pszPluginLibrary Name of the library file containing the plugin
     *                          implementation, without extension.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnloadVDPlugin,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox,
                                                 const char *pszPluginLibrary));

    /**
     * Creates an IProgress object instance for a long running extension
     * pack provided API operation which is executed asynchronously.
     *
     * This implicitly creates a cancellable progress object, since anything
     * else is user unfriendly. You need to design your code to handle
     * cancellation with reasonable response time.
     *
     * @returns COM status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pInitiator      Pointer to the initiating object.
     * @param   pcszDescription Description of the overall task.
     * @param   cOperations     Number of operations for this task.
     * @param   uTotalOperationsWeight        Overall weight for the entire task.
     * @param   pcszFirstOperationDescription Description of the first operation.
     * @param   uFirstOperationWeight         Weight for the first operation.
     * @param   ppProgressOut   Output parameter for the IProgress object reference.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnCreateProgress,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IUnknown) *pInitiator,
                                                      const char *pcszDescription, uint32_t cOperations,
                                                      uint32_t uTotalOperationsWeight, const char *pcszFirstOperationDescription,
                                                      uint32_t uFirstOperationWeight, VPOXEXTPACK_IF_CS(IProgress) **ppProgressOut));

    /**
     * Checks if the Progress object is marked as canceled.
     *
     * @returns COM status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pProgress       Pointer to the IProgress object reference returned
     *                          by pfnCreateProgress.
     * @param   pfCanceled      @c true if canceled, @c false otherwise.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnGetCanceledProgress,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                           bool *pfCanceled));

    /**
     * Updates the percentage value of the current operation of the
     * Progress object.
     *
     * @returns COM status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pProgress       Pointer to the IProgress object reference returned
     *                          by pfnCreateProgress.
     * @param   uPercent        Result of the overall task.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnUpdateProgress,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                      uint32_t uPercent));

    /**
     * Signals that the current operation is successfully completed and
     * advances to the next operation. The operation percentage is reset
     * to 0.
     *
     * If the operation count is exceeded this returns an error.
     *
     * @returns COM status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pProgress       Pointer to the IProgress object reference returned
     *                          by pfnCreateProgress.
     * @param   pcszNextOperationDescription Description of the next operation.
     * @param   uNextOperationWeight         Weight for the next operation.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnNextOperationProgress,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                             const char *pcszNextOperationDescription,
                                                             uint32_t uNextOperationWeight));

    /**
     * Waits until the other task is completed (including all sub-operations)
     * and forward all changes from the other progress to this progress. This
     * means sub-operation number, description, percent and so on.
     *
     * The caller is responsible for having at least the same count of
     * sub-operations in this progress object as there are in the other
     * progress object.
     *
     * If the other progress object supports cancel and this object gets any
     * cancel request (when here enabled as well), it will be forwarded to
     * the other progress object.
     *
     * Error information is automatically preserved (by transferring it to
     * the current thread's error information). If the caller wants to set it
     * as the completion state of this progress it needs to be done separately.
     *
     * @returns COM status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pProgress       Pointer to the IProgress object reference returned
     *                          by pfnCreateProgress.
     * @param   pProgressOther  Pointer to an IProgress object reference, the one
     *                          to be waited for.
     * @param   cTimeoutMS      Timeout in milliseconds, 0 for indefinite wait.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnWaitOtherProgress,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                         VPOXEXTPACK_IF_CS(IProgress) *pProgressOther,
                                                         uint32_t cTimeoutMS));

    /**
     * Marks the whole task as complete and sets the result code.
     *
     * If the result code indicates a failure then this method will store
     * the currently set COM error info from the current thread in the
     * the errorInfo attribute of this Progress object instance. If there
     * is no error information available then an error is returned.
     *
     * If the result code indicates success then the task is terminated,
     * without paying attention to the current operation being the last.
     *
     * Note that this must be called only once for the given Progress
     * object. Subsequent calls will return errors.
     *
     * @returns COM status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pProgress       Pointer to the IProgress object reference returned
     *                          by pfnCreateProgress.
     * @param   uResultCode     Result of the overall task.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnCompleteProgress,(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                        uint32_t uResultCode));

    DECLR3CALLBACKMEMBER(int, pfnReserved1,(PCVPOXEXTPACKHLP pHlp)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved2,(PCVPOXEXTPACKHLP pHlp)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved3,(PCVPOXEXTPACKHLP pHlp)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved4,(PCVPOXEXTPACKHLP pHlp)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved5,(PCVPOXEXTPACKHLP pHlp)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved6,(PCVPOXEXTPACKHLP pHlp)); /**< Reserved for minor structure revisions. */

    /** Reserved for minor structure revisions. */
    uint32_t                    uReserved7;

    /** End of structure marker (VPOXEXTPACKHLP_VERSION). */
    uint32_t                    u32EndMarker;
} VPOXEXTPACKHLP;
/** Current version of the VPOXEXTPACKHLP structure.  */
#define VPOXEXTPACKHLP_VERSION          RT_MAKE_U32(0, 3)


/** Pointer to the extension pack callback table. */
typedef struct VPOXEXTPACKREG const *PCVPOXEXTPACKREG;
/**
 * Callback table returned by VPoxExtPackRegister.
 *
 * All the callbacks are called the context of the per-user service (VPoxSVC).
 *
 * This must be valid until the extension pack main module is unloaded.
 */
typedef struct VPOXEXTPACKREG
{
    /** Interface version.
     * This is set to VPOXEXTPACKREG_VERSION. */
    uint32_t                    u32Version;
    /** The VirtualPox version this extension pack was built against.  */
    uint32_t                    uVPoxVersion;

    /**
     * Hook for doing setups after the extension pack was installed.
     *
     * @returns VPox status code.
     * @retval  VERR_EXTPACK_UNSUPPORTED_HOST_UNINSTALL if the extension pack
     *          requires some different host version or a prerequisite is
     *          missing from the host.  Automatic uninstall will be attempted.
     *          Must set error info.
     *
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualPox The VirtualPox interface.
     * @param   pErrInfo    Where to return extended error information.
     */
    DECLCALLBACKMEMBER(int, pfnInstalled)(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox,
                                          PRTERRINFO pErrInfo);

    /**
     * Hook for cleaning up before the extension pack is uninstalled.
     *
     * @returns VPox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualPox The VirtualPox interface.
     *
     * @todo    This is currently called holding locks making pVirtualPox
     *          relatively unusable.
     */
    DECLCALLBACKMEMBER(int, pfnUninstall)(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox);

    /**
     * Hook for doing work after the VirtualPox object is ready.
     *
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualPox The VirtualPox interface.
     */
    DECLCALLBACKMEMBER(void, pfnVirtualPoxReady)(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox);

    /**
     * Hook for doing work before unloading.
     *
     * @param   pThis       Pointer to this structure.
     *
     * @remarks The helpers are not available at this point in time.
     * @remarks This is not called on uninstall, then pfnUninstall will be the
     *          last callback.
     */
    DECLCALLBACKMEMBER(void, pfnUnload)(PCVPOXEXTPACKREG pThis);

    /**
     * Hook for changing the default VM configuration upon creation.
     *
     * @returns VPox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualPox The VirtualPox interface.
     * @param   pMachine    The machine interface.
     */
    DECLCALLBACKMEMBER(int, pfnVMCreated)(PCVPOXEXTPACKREG pThis, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox,
                                          VPOXEXTPACK_IF_CS(IMachine) *pMachine);

    /**
     * Query the IUnknown interface to an object in the main module.
     *
     * @returns IUnknown pointer (referenced) on success, NULL on failure.
     * @param   pThis       Pointer to this structure.
     * @param   pObjectId   Pointer to the object ID (UUID).
     */
    DECLCALLBACKMEMBER(void *, pfnQueryObject)(PCVPOXEXTPACKREG pThis, PCRTUUID pObjectId);

    DECLR3CALLBACKMEMBER(int, pfnReserved1,(PCVPOXEXTPACKREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved2,(PCVPOXEXTPACKREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved3,(PCVPOXEXTPACKREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved4,(PCVPOXEXTPACKREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved5,(PCVPOXEXTPACKREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved6,(PCVPOXEXTPACKREG pThis)); /**< Reserved for minor structure revisions. */

    /** Reserved for minor structure revisions. */
    uint32_t                    uReserved7;

    /** End of structure marker (VPOXEXTPACKREG_VERSION). */
    uint32_t                    u32EndMarker;
} VPOXEXTPACKREG;
/** Current version of the VPOXEXTPACKREG structure.  */
#define VPOXEXTPACKREG_VERSION        RT_MAKE_U32(0, 2)


/**
 * The VPoxExtPackRegister callback function.
 *
 * The Main API (as in VPoxSVC) will invoke this function after loading an
 * extension pack Main module. Its job is to do version compatibility checking
 * and returning the extension pack registration structure.
 *
 * @returns VPox status code.
 * @param   pHlp            Pointer to the extension pack helper function
 *                          table.  This is valid until the module is unloaded.
 * @param   ppReg           Where to return the pointer to the registration
 *                          structure containing all the hooks.  This structure
 *                          be valid and unchanged until the module is unloaded
 *                          (i.e. use some static const data for it).
 * @param   pErrInfo        Where to return extended error information.
 */
typedef DECLCALLBACK(int) FNVPOXEXTPACKREGISTER(PCVPOXEXTPACKHLP pHlp, PCVPOXEXTPACKREG *ppReg, PRTERRINFO pErrInfo);
/** Pointer to a FNVPOXEXTPACKREGISTER. */
typedef FNVPOXEXTPACKREGISTER *PFNVPOXEXTPACKREGISTER;

/** The name of the main module entry point. */
#define VPOX_EXTPACK_MAIN_MOD_ENTRY_POINT   "VPoxExtPackRegister"


/** Pointer to the extension pack VM callback table. */
typedef struct VPOXEXTPACKVMREG const *PCVPOXEXTPACKVMREG;
/**
 * Callback table returned by VPoxExtPackVMRegister.
 *
 * All the callbacks are called the context of a VM process.
 *
 * This must be valid until the extension pack main VM module is unloaded.
 */
typedef struct VPOXEXTPACKVMREG
{
    /** Interface version.
     * This is set to VPOXEXTPACKVMREG_VERSION. */
    uint32_t                    u32Version;
    /** The VirtualPox version this extension pack was built against.  */
    uint32_t                    uVPoxVersion;

    /**
     * Hook for doing work after the Console object is ready.
     *
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The Console interface.
     */
    DECLCALLBACKMEMBER(void, pfnConsoleReady)(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole);

    /**
     * Hook for doing work before unloading.
     *
     * @param   pThis       Pointer to this structure.
     *
     * @remarks The helpers are not available at this point in time.
     */
    DECLCALLBACKMEMBER(void, pfnUnload)(PCVPOXEXTPACKVMREG pThis);

    /**
     * Hook for configuring the VMM for a VM.
     *
     * @returns VPox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The console interface.
     * @param   pVM         The cross context VM structure.
     */
    DECLCALLBACKMEMBER(int, pfnVMConfigureVMM)(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);

    /**
     * Hook for doing work right before powering on the VM.
     *
     * @returns VPox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The console interface.
     * @param   pVM         The cross context VM structure.
     */
    DECLCALLBACKMEMBER(int, pfnVMPowerOn)(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);

    /**
     * Hook for doing work after powering off the VM.
     *
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The console interface.
     * @param   pVM         The cross context VM structure. Can be NULL.
     */
    DECLCALLBACKMEMBER(void, pfnVMPowerOff)(PCVPOXEXTPACKVMREG pThis, VPOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);

    /**
     * Query the IUnknown interface to an object in the main VM module.
     *
     * @returns IUnknown pointer (referenced) on success, NULL on failure.
     * @param   pThis       Pointer to this structure.
     * @param   pObjectId   Pointer to the object ID (UUID).
     */
    DECLCALLBACKMEMBER(void *, pfnQueryObject)(PCVPOXEXTPACKVMREG pThis, PCRTUUID pObjectId);

    DECLR3CALLBACKMEMBER(int, pfnReserved1,(PCVPOXEXTPACKVMREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved2,(PCVPOXEXTPACKVMREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved3,(PCVPOXEXTPACKVMREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved4,(PCVPOXEXTPACKVMREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved5,(PCVPOXEXTPACKVMREG pThis)); /**< Reserved for minor structure revisions. */
    DECLR3CALLBACKMEMBER(int, pfnReserved6,(PCVPOXEXTPACKVMREG pThis)); /**< Reserved for minor structure revisions. */

    /** Reserved for minor structure revisions. */
    uint32_t                    uReserved7;

    /** End of structure marker (VPOXEXTPACKVMREG_VERSION). */
    uint32_t                    u32EndMarker;
} VPOXEXTPACKVMREG;
/** Current version of the VPOXEXTPACKVMREG structure.  */
#define VPOXEXTPACKVMREG_VERSION      RT_MAKE_U32(0, 2)


/**
 * The VPoxExtPackVMRegister callback function.
 *
 * The Main API (in a VM process) will invoke this function after loading an
 * extension pack VM module. Its job is to do version compatibility checking
 * and returning the extension pack registration structure for a VM.
 *
 * @returns VPox status code.
 * @param   pHlp            Pointer to the extension pack helper function
 *                          table.  This is valid until the module is unloaded.
 * @param   ppReg           Where to return the pointer to the registration
 *                          structure containing all the hooks.  This structure
 *                          be valid and unchanged until the module is unloaded
 *                          (i.e. use some static const data for it).
 * @param   pErrInfo        Where to return extended error information.
 */
typedef DECLCALLBACK(int) FNVPOXEXTPACKVMREGISTER(PCVPOXEXTPACKHLP pHlp, PCVPOXEXTPACKVMREG *ppReg, PRTERRINFO pErrInfo);
/** Pointer to a FNVPOXEXTPACKVMREGISTER. */
typedef FNVPOXEXTPACKVMREGISTER *PFNVPOXEXTPACKVMREGISTER;

/** The name of the main VM module entry point. */
#define VPOX_EXTPACK_MAIN_VM_MOD_ENTRY_POINT   "VPoxExtPackVMRegister"


/**
 * Checks if extension pack interface version is compatible.
 *
 * @returns true if the do, false if they don't.
 * @param   u32Provider     The provider version.
 * @param   u32User         The user version.
 */
#define VPOXEXTPACK_IS_VER_COMPAT(u32Provider, u32User) \
    (    VPOXEXTPACK_IS_MAJOR_VER_EQUAL(u32Provider, u32User) \
      && (int32_t)RT_LOWORD(u32Provider) >= (int32_t)RT_LOWORD(u32User) ) /* stupid casts to shut up gcc */

/**
 * Check if two extension pack interface versions has the same major version.
 *
 * @returns true if the do, false if they don't.
 * @param   u32Ver1         The first version number.
 * @param   u32Ver2         The second version number.
 */
#define VPOXEXTPACK_IS_MAJOR_VER_EQUAL(u32Ver1, u32Ver2)  (RT_HIWORD(u32Ver1) == RT_HIWORD(u32Ver2))

#endif /* !VPOX_INCLUDED_ExtPack_ExtPack_h */

