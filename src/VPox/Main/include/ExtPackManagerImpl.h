/* $Id: ExtPackManagerImpl.h $ */
/** @file
 * VirtualPox Main - interface for Extension Packs, VPoxSVC & VPoxC.
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

#ifndef MAIN_INCLUDED_ExtPackManagerImpl_h
#define MAIN_INCLUDED_ExtPackManagerImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VirtualPoxBase.h"
#include <VPox/ExtPack/ExtPack.h>
#include "ExtPackWrap.h"
#include "ExtPackFileWrap.h"
#include "ExtPackManagerWrap.h"
#include <iprt/fs.h>


/** The name of the oracle extension back. */
#define ORACLE_PUEL_EXTPACK_NAME "Oracle VM VirtualPox Extension Pack"


#ifndef VPOX_COM_INPROC
/**
 * An extension pack file.
 */
class ATL_NO_VTABLE ExtPackFile :
    public ExtPackFileWrap
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(ExtPackFile)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     initWithFile(const char *a_pszFile, const char *a_pszDigest, class ExtPackManager *a_pExtPackMgr, VirtualPox *a_pVirtualPox);
    void        uninit();
    /** @}  */

private:
    /** @name Misc init helpers
     * @{ */
    HRESULT     initFailed(const char *a_pszWhyFmt, ...);
    /** @} */

private:

    // wrapped IExtPackFile properties
    HRESULT getName(com::Utf8Str &aName);
    HRESULT getDescription(com::Utf8Str &aDescription);
    HRESULT getVersion(com::Utf8Str &aVersion);
    HRESULT getRevision(ULONG *aRevision);
    HRESULT getEdition(com::Utf8Str &aEdition);
    HRESULT getVRDEModule(com::Utf8Str &aVRDEModule);
    HRESULT getPlugIns(std::vector<ComPtr<IExtPackPlugIn> > &aPlugIns);
    HRESULT getUsable(BOOL *aUsable);
    HRESULT getWhyUnusable(com::Utf8Str &aWhyUnusable);
    HRESULT getShowLicense(BOOL *aShowLicense);
    HRESULT getLicense(com::Utf8Str &aLicense);
    HRESULT getFilePath(com::Utf8Str &aFilePath);

    // wrapped IExtPackFile methods
    HRESULT queryLicense(const com::Utf8Str &aPreferredLocale,
                         const com::Utf8Str &aPreferredLanguage,
                         const com::Utf8Str &aFormat,
                         com::Utf8Str &aLicenseText);
    HRESULT install(BOOL aReplace,
                    const com::Utf8Str &aDisplayInfo,
                    ComPtr<IProgress> &aProgess);

    struct Data;
    /** Pointer to the private instance. */
    Data *m;

    friend class ExtPackManager;
    friend class ExtPackInstallTask;
};
#endif /* !VPOX_COM_INPROC */


/**
 * An installed extension pack.
 */
class ATL_NO_VTABLE ExtPack :
    public ExtPackWrap
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(ExtPack)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     initWithDir(VirtualPox *a_pVirtualPox, VPOXEXTPACKCTX a_enmContext, const char *a_pszName, const char *a_pszDir);
    void        uninit();
    RTMEMEF_NEW_AND_DELETE_OPERATORS();
    /** @}  */

    /** @name Internal interfaces used by ExtPackManager.
     * @{ */
#ifndef VPOX_COM_INPROC
    bool        i_callInstalledHook(IVirtualPox *a_pVirtualPox, AutoWriteLock *a_pLock, PRTERRINFO pErrInfo);
    HRESULT     i_callUninstallHookAndClose(IVirtualPox *a_pVirtualPox, bool a_fForcedRemoval);
    bool        i_callVirtualPoxReadyHook(IVirtualPox *a_pVirtualPox, AutoWriteLock *a_pLock);
#endif
#ifdef VPOX_COM_INPROC
    bool        i_callConsoleReadyHook(IConsole *a_pConsole, AutoWriteLock *a_pLock);
#endif
#ifndef VPOX_COM_INPROC
    bool        i_callVmCreatedHook(IVirtualPox *a_pVirtualPox, IMachine *a_pMachine, AutoWriteLock *a_pLock);
#endif
#ifdef VPOX_COM_INPROC
    bool        i_callVmConfigureVmmHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock, int *a_pvrc);
    bool        i_callVmPowerOnHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock, int *a_pvrc);
    bool        i_callVmPowerOffHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock);
#endif
    HRESULT     i_checkVrde(void);
    HRESULT     i_getVrdpLibraryName(Utf8Str *a_pstrVrdeLibrary);
    HRESULT     i_getLibraryName(const char *a_pszModuleName, Utf8Str *a_pstrLibrary);
    bool        i_wantsToBeDefaultVrde(void) const;
    HRESULT     i_refresh(bool *pfCanDelete);
#ifndef VPOX_COM_INPROC
    bool        i_areThereCloudProviderUninstallVetos();
    void        i_notifyCloudProviderManager();
#endif
    /** @}  */

protected:
    /** @name Internal helper methods.
     * @{ */
    void        i_probeAndLoad(void);
    bool        i_findModule(const char *a_pszName, const char *a_pszExt, VPOXEXTPACKMODKIND a_enmKind,
                             Utf8Str *a_ppStrFound, bool *a_pfNative, PRTFSOBJINFO a_pObjInfo) const;
    static bool i_objinfoIsEqual(PCRTFSOBJINFO pObjInfo1, PCRTFSOBJINFO pObjInfo2);
    /** @}  */

    /** @name Extension Pack Helpers
     * @{ */
    static DECLCALLBACK(int)    i_hlpFindModule(PCVPOXEXTPACKHLP pHlp, const char *pszName, const char *pszExt,
                                                VPOXEXTPACKMODKIND enmKind, char *pszFound, size_t cbFound, bool *pfNative);
    static DECLCALLBACK(int)    i_hlpGetFilePath(PCVPOXEXTPACKHLP pHlp, const char *pszFilename, char *pszPath, size_t cbPath);
    static DECLCALLBACK(VPOXEXTPACKCTX) i_hlpGetContext(PCVPOXEXTPACKHLP pHlp);
    static DECLCALLBACK(int)    i_hlpLoadHGCMService(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IConsole) *pConsole, const char *pszServiceLibrary, const char *pszServiceName);
    static DECLCALLBACK(int)    i_hlpLoadVDPlugin(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox, const char *pszPluginLibrary);
    static DECLCALLBACK(int)    i_hlpUnloadVDPlugin(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IVirtualPox) *pVirtualPox, const char *pszPluginLibrary);
    static DECLCALLBACK(uint32_t) i_hlpCreateProgress(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IUnknown) *pInitiator,
                                                      const char *pcszDescription, uint32_t cOperations,
                                                      uint32_t uTotalOperationsWeight, const char *pcszFirstOperationDescription,
                                                      uint32_t uFirstOperationWeight, VPOXEXTPACK_IF_CS(IProgress) **ppProgressOut);
    static DECLCALLBACK(uint32_t) i_hlpGetCanceledProgress(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                           bool *pfCanceled);
    static DECLCALLBACK(uint32_t) i_hlpUpdateProgress(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                      uint32_t uPercent);
    static DECLCALLBACK(uint32_t) i_hlpNextOperationProgress(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                             const char *pcszNextOperationDescription,
                                                             uint32_t uNextOperationWeight);
    static DECLCALLBACK(uint32_t) i_hlpWaitOtherProgress(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                         VPOXEXTPACK_IF_CS(IProgress) *pProgressOther,
                                                         uint32_t cTimeoutMS);
    static DECLCALLBACK(uint32_t) i_hlpCompleteProgress(PCVPOXEXTPACKHLP pHlp, VPOXEXTPACK_IF_CS(IProgress) *pProgress,
                                                        uint32_t uResultCode);
    static DECLCALLBACK(int)    i_hlpReservedN(PCVPOXEXTPACKHLP pHlp);
    /** @}  */

private:

    // wrapped IExtPack properties
    HRESULT getName(com::Utf8Str &aName);
    HRESULT getDescription(com::Utf8Str &aDescription);
    HRESULT getVersion(com::Utf8Str &aVersion);
    HRESULT getRevision(ULONG *aRevision);
    HRESULT getEdition(com::Utf8Str &aEdition);
    HRESULT getVRDEModule(com::Utf8Str &aVRDEModule);
    HRESULT getPlugIns(std::vector<ComPtr<IExtPackPlugIn> > &aPlugIns);
    HRESULT getUsable(BOOL *aUsable);
    HRESULT getWhyUnusable(com::Utf8Str &aWhyUnusable);
    HRESULT getShowLicense(BOOL *aShowLicense);
    HRESULT getLicense(com::Utf8Str &aLicense);

    // wrapped IExtPack methods
    HRESULT queryLicense(const com::Utf8Str &aPreferredLocale,
                         const com::Utf8Str &aPreferredLanguage,
                         const com::Utf8Str &aFormat,
                         com::Utf8Str &aLicenseText);
    HRESULT queryObject(const com::Utf8Str &aObjUuid,
                        ComPtr<IUnknown> &aReturnInterface);


    struct Data;
    /** Pointer to the private instance. */
    Data *m;

    friend class ExtPackManager;
};


/**
 * Extension pack manager.
 */
class ATL_NO_VTABLE ExtPackManager :
    public ExtPackManagerWrap
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(ExtPackManager)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     initExtPackManager(VirtualPox *a_pVirtualPox, VPOXEXTPACKCTX a_enmContext);
    void        uninit();
    /** @}  */

    /** @name Internal interfaces used by other Main classes.
     * @{ */
#ifndef VPOX_COM_INPROC
    HRESULT     i_doInstall(ExtPackFile *a_pExtPackFile, bool a_fReplace, Utf8Str const *a_pstrDisplayInfo);
    HRESULT     i_doUninstall(const Utf8Str *a_pstrName, bool a_fForcedRemoval, const Utf8Str *a_pstrDisplayInfo);
    void        i_callAllVirtualPoxReadyHooks(void);
    HRESULT     i_queryObjects(const com::Utf8Str &aObjUuid, std::vector<ComPtr<IUnknown> > &aObjects, std::vector<com::Utf8Str> *a_pstrExtPackNames);
#endif
#ifdef VPOX_COM_INPROC
    void        i_callAllConsoleReadyHooks(IConsole *a_pConsole);
#endif
#ifndef VPOX_COM_INPROC
    void        i_callAllVmCreatedHooks(IMachine *a_pMachine);
#endif
#ifdef VPOX_COM_INPROC
    int         i_callAllVmConfigureVmmHooks(IConsole *a_pConsole, PVM a_pVM);
    int         i_callAllVmPowerOnHooks(IConsole *a_pConsole, PVM a_pVM);
    void        i_callAllVmPowerOffHooks(IConsole *a_pConsole, PVM a_pVM);
#endif
    HRESULT     i_checkVrdeExtPack(Utf8Str const *a_pstrExtPack);
    int         i_getVrdeLibraryPathForExtPack(Utf8Str const *a_pstrExtPack, Utf8Str *a_pstrVrdeLibrary);
    HRESULT     i_getLibraryPathForExtPack(const char *a_pszModuleName, const char *a_pszExtPack, Utf8Str *a_pstrLibrary);
    HRESULT     i_getDefaultVrdeExtPack(Utf8Str *a_pstrExtPack);
    bool        i_isExtPackUsable(const char *a_pszExtPack);
    void        i_dumpAllToReleaseLog(void);
    uint64_t    i_getUpdateCounter(void);
    /** @}  */

private:
    // wrapped IExtPackManager properties
    HRESULT getInstalledExtPacks(std::vector<ComPtr<IExtPack> > &aInstalledExtPacks);

   // wrapped IExtPackManager methods
    HRESULT find(const com::Utf8Str &aName,
                 ComPtr<IExtPack> &aReturnData);
    HRESULT openExtPackFile(const com::Utf8Str &aPath,
                                  ComPtr<IExtPackFile> &aFile);
    HRESULT uninstall(const com::Utf8Str &aName,
                      BOOL aForcedRemoval,
                      const com::Utf8Str &aDisplayInfo,
                      ComPtr<IProgress> &aProgess);
    HRESULT cleanup();
    HRESULT queryAllPlugInsForFrontend(const com::Utf8Str &aFrontendName,
                                       std::vector<com::Utf8Str> &aPlugInModules);
    HRESULT isExtPackUsable(const com::Utf8Str &aName,
                            BOOL *aUsable);

    bool        i_areThereAnyRunningVMs(void) const;
    HRESULT     i_runSetUidToRootHelper(Utf8Str const *a_pstrDisplayInfo, const char *a_pszCommand, ...);
    ExtPack    *i_findExtPack(const char *a_pszName);
    void        i_removeExtPack(const char *a_pszName);
    HRESULT     i_refreshExtPack(const char *a_pszName, bool a_fUnsuableIsError, ExtPack **a_ppExtPack);

private:
    struct Data;
    /** Pointer to the private instance. */
    Data *m;

    friend class ExtPackUninstallTask;
};

#endif /* !MAIN_INCLUDED_ExtPackManagerImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
