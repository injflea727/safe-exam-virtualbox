/* $Id: vbsf.h $ */
/** @file
 * VirtualPox Windows Guest Shared Folders - File System Driver header file
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

#ifndef GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsf_h
#define GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/*
 * This must be defined before including RX headers.
 */
#define MINIRDR__NAME               VPoxMRx
#define ___MINIRDR_IMPORTS_NAME     (VPoxMRxDeviceObject->RdbssExports)

/*
 * System and RX headers.
 */
#include <iprt/nt/nt.h> /* includes ntifs.h + wdm.h */
#include <iprt/win/windef.h>
#ifndef INVALID_HANDLE_VALUE
# define INVALID_HANDLE_VALUE RTNT_INVALID_HANDLE_VALUE /* (The rx.h definition causes warnings for amd64)  */
#endif
#include <iprt/nt/rx.h>

/*
 * VPox shared folders.
 */
#include "vbsfshared.h"
#include <VPox/log.h>
#include <VPox/VPoxGuestLibSharedFolders.h>
#ifdef __cplusplus /* not for Win2kWorkarounds.c */
# include <VPox/VPoxGuestLibSharedFoldersInline.h>
#endif


RT_C_DECLS_BEGIN

/*
 * Global data.
 */
extern PRDBSS_DEVICE_OBJECT VPoxMRxDeviceObject;
extern uint32_t             g_uSfLastFunction;
/** Pointer to the CcCoherencyFlushAndPurgeCache API (since win 7). */
typedef VOID (NTAPI *PFNCCCOHERENCYFLUSHANDPURGECACHE)(PSECTION_OBJECT_POINTERS, PLARGE_INTEGER, ULONG, PIO_STATUS_BLOCK,ULONG);
extern PFNCCCOHERENCYFLUSHANDPURGECACHE g_pfnCcCoherencyFlushAndPurgeCache;
#ifndef CC_FLUSH_AND_PURGE_NO_PURGE
# define CC_FLUSH_AND_PURGE_NO_PURGE 1
#endif


/**
 * Maximum drive letters (A - Z).
 */
#define _MRX_MAX_DRIVE_LETTERS 26

/**
 * The shared folders device extension.
 */
typedef struct _MRX_VPOX_DEVICE_EXTENSION
{
    /** The shared folders device object pointer. */
    PRDBSS_DEVICE_OBJECT pDeviceObject;

    /**
     * Keep a list of local connections used.
     * The size (_MRX_MAX_DRIVE_LETTERS = 26) of the array presents the available drive letters C: - Z: of Windows.
     */
    CHAR cLocalConnections[_MRX_MAX_DRIVE_LETTERS];
    PUNICODE_STRING wszLocalConnectionName[_MRX_MAX_DRIVE_LETTERS];
    FAST_MUTEX mtxLocalCon;

    /** Saved pointer to the original IRP_MJ_DEVICE_CONTROL handler. */
    NTSTATUS (* pfnRDBSSDeviceControl) (PDEVICE_OBJECT pDevObj, PIRP pIrp);
    /** Saved pointer to the original IRP_MJ_CREATE handler. */
    NTSTATUS (NTAPI * pfnRDBSSCreate)(PDEVICE_OBJECT pDevObj, PIRP pIrp);
    /** Saved pointer to the original IRP_MJ_SET_INFORMATION handler. */
    NTSTATUS (NTAPI * pfnRDBSSSetInformation)(PDEVICE_OBJECT pDevObj, PIRP pIrp);

} MRX_VPOX_DEVICE_EXTENSION, *PMRX_VPOX_DEVICE_EXTENSION;

/**
 * The shared folders NET_ROOT extension.
 */
typedef struct _MRX_VPOX_NETROOT_EXTENSION
{
    /** The shared folder map handle of this netroot. */
    VBGLSFMAP map;
    /** Simple initialized (mapped folder) indicator that works better with the
     *  zero filled defaults than SHFL_ROOT_NIL.  */
    bool        fInitialized;
} MRX_VPOX_NETROOT_EXTENSION, *PMRX_VPOX_NETROOT_EXTENSION;


/** Pointer to the VPox file object extension data. */
typedef struct MRX_VPOX_FOBX *PMRX_VPOX_FOBX;

/**
 * VPox extension data to the file control block (FCB).
 *
 * @note To unix people, think of the FCB as the inode structure.  This is our
 *       private addition to the inode info.
 */
typedef struct VBSFNTFCBEXT
{
    /** @name Pointers to file object extensions currently sitting on the given timestamps.
     *
     * The file object extensions pointed to have disabled implicit updating the
     * respective timestamp due to a FileBasicInformation set request.  Should these
     * timestamps be modified via any other file handle, these pointers will be
     * updated or set to NULL to reflect this.  So, when the cleaning up a file
     * object it can be more accurately determined whether to restore timestamps on
     * non-windows host systems or not.
     *
     * @{ */
    PMRX_VPOX_FOBX              pFobxLastAccessTime;
    PMRX_VPOX_FOBX              pFobxLastWriteTime;
    PMRX_VPOX_FOBX              pFobxChangeTime;
    /** @} */

    /** @name Cached volume info.
     * @{ */
    /** The RTTimeSystemNanoTS value when VolInfo was retrieved, 0 to force update. */
    uint64_t volatile           nsVolInfoUpToDate;
    /** Volume information. */
    SHFLVOLINFO volatile        VolInfo;
    /** @} */
} VBSFNTFCBEXT;
/** Pointer to the VPox FCB extension data. */
typedef VBSFNTFCBEXT *PVBSFNTFCBEXT;


/** @name  VPOX_FOBX_F_INFO_XXX
 * @{ */
#define VPOX_FOBX_F_INFO_LASTACCESS_TIME UINT8_C(0x01)
#define VPOX_FOBX_F_INFO_LASTWRITE_TIME  UINT8_C(0x02)
#define VPOX_FOBX_F_INFO_CHANGE_TIME     UINT8_C(0x04)
/** @} */

/**
 * The shared folders file extension.
 */
typedef struct MRX_VPOX_FOBX
{
    /** The host file handle. */
    SHFLHANDLE                  hFile;
    PMRX_SRV_CALL               pSrvCall;
    /** The RTTimeSystemNanoTS value when Info was retrieved, 0 to force update. */
    uint64_t                    nsUpToDate;
    /** Cached object info.
     * @todo Consider moving it to VBSFNTFCBEXT.  Better fit than on "handle". */
    SHFLFSOBJINFO               Info;

    /** VPOX_FOBX_F_INFO_XXX of timestamps which may need setting on close. */
    uint8_t                     fTimestampsSetByUser;
    /** VPOX_FOBX_F_INFO_XXX of timestamps which implicit updating is suppressed. */
    uint8_t                     fTimestampsUpdatingSuppressed;
    /** VPOX_FOBX_F_INFO_XXX of timestamps which may have implicitly update. */
    uint8_t                     fTimestampsImplicitlyUpdated;
} MRX_VPOX_FOBX;

#define VPoxMRxGetDeviceExtension(RxContext) \
        ((PMRX_VPOX_DEVICE_EXTENSION)((PBYTE)(RxContext)->RxDeviceObject + sizeof(RDBSS_DEVICE_OBJECT)))

#define VPoxMRxGetNetRootExtension(pNetRoot)    ((pNetRoot) != NULL ? (PMRX_VPOX_NETROOT_EXTENSION)(pNetRoot)->Context : NULL)

#define VPoxMRxGetFcbExtension(pFcb)            ((pFcb)     != NULL ?                   (PVBSFNTFCBEXT)(pFcb)->Context : NULL)

#define VPoxMRxGetSrvOpenExtension(pSrvOpen)    ((pSrvOpen) != NULL ?          (PMRX_VPOX_SRV_OPEN)(pSrvOpen)->Context : NULL)

#define VPoxMRxGetFileObjectExtension(pFobx)    ((pFobx)    != NULL ?                 (PMRX_VPOX_FOBX)(pFobx)->Context : NULL)

/** HACK ALERT: Special Create.ShareAccess indicating trailing slash for
 * non-directory IRP_MJ_CREATE request.
 * Set by VPoxHookMjCreate, used by VPoxMRxCreate. */
#define VPOX_MJ_CREATE_SLASH_HACK   UINT16_C(0x0400)

/** @name Prototypes for the dispatch table routines.
 * @{
 */
NTSTATUS VPoxMRxStart(IN OUT struct _RX_CONTEXT * RxContext,
                      IN OUT PRDBSS_DEVICE_OBJECT RxDeviceObject);
NTSTATUS VPoxMRxStop(IN OUT struct _RX_CONTEXT * RxContext,
                     IN OUT PRDBSS_DEVICE_OBJECT RxDeviceObject);

NTSTATUS VPoxMRxCreate(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxCollapseOpen(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxShouldTryToCollapseThisOpen(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxFlush(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxTruncate(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxCleanupFobx(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxCloseSrvOpen(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxDeallocateForFcb(IN OUT PMRX_FCB pFcb);
NTSTATUS VPoxMRxDeallocateForFobx(IN OUT PMRX_FOBX pFobx);
NTSTATUS VPoxMRxForceClosed(IN OUT PMRX_SRV_OPEN SrvOpen);

NTSTATUS VPoxMRxQueryDirectory(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxQueryFileInfo(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxSetFileInfo(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxSetFileInfoAtCleanup(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxQueryEaInfo(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxSetEaInfo(IN OUT struct _RX_CONTEXT * RxContext);
NTSTATUS VPoxMRxQuerySdInfo(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxSetSdInfo(IN OUT struct _RX_CONTEXT * RxContext);
NTSTATUS VPoxMRxQueryVolumeInfo(IN OUT PRX_CONTEXT RxContext);

NTSTATUS VPoxMRxComputeNewBufferingState(IN OUT PMRX_SRV_OPEN pSrvOpen,
                                         IN PVOID pMRxContext,
                                         OUT ULONG *pNewBufferingState);

NTSTATUS VPoxMRxRead(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxWrite(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxLocks(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxFsCtl(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxIoCtl(IN OUT PRX_CONTEXT RxContext);
NTSTATUS VPoxMRxNotifyChangeDirectory(IN OUT PRX_CONTEXT RxContext);

ULONG NTAPI VPoxMRxExtendStub(IN OUT struct _RX_CONTEXT * RxContext,
                              IN OUT PLARGE_INTEGER pNewFileSize,
                              OUT PLARGE_INTEGER pNewAllocationSize);
NTSTATUS VPoxMRxCompleteBufferingStateChangeRequest(IN OUT PRX_CONTEXT RxContext,
                                                    IN OUT PMRX_SRV_OPEN SrvOpen,
                                                    IN PVOID pContext);

NTSTATUS VPoxMRxCreateVNetRoot(IN OUT PMRX_CREATENETROOT_CONTEXT pContext);
NTSTATUS VPoxMRxFinalizeVNetRoot(IN OUT PMRX_V_NET_ROOT pVirtualNetRoot,
                                 IN PBOOLEAN ForceDisconnect);
NTSTATUS VPoxMRxFinalizeNetRoot(IN OUT PMRX_NET_ROOT pNetRoot,
                                IN PBOOLEAN ForceDisconnect);
NTSTATUS VPoxMRxUpdateNetRootState(IN PMRX_NET_ROOT pNetRoot);
VOID     VPoxMRxExtractNetRootName(IN PUNICODE_STRING FilePathName,
                                   IN PMRX_SRV_CALL SrvCall,
                                   OUT PUNICODE_STRING NetRootName,
                                   OUT PUNICODE_STRING RestOfName OPTIONAL);

NTSTATUS VPoxMRxCreateSrvCall(PMRX_SRV_CALL pSrvCall,
                              PMRX_SRVCALL_CALLBACK_CONTEXT pCallbackContext);
NTSTATUS VPoxMRxSrvCallWinnerNotify(IN OUT PMRX_SRV_CALL pSrvCall,
                                    IN BOOLEAN ThisMinirdrIsTheWinner,
                                    IN OUT PVOID pSrvCallContext);
NTSTATUS VPoxMRxFinalizeSrvCall(PMRX_SRV_CALL pSrvCall,
                                BOOLEAN Force);

NTSTATUS VPoxMRxDevFcbXXXControlFile(IN OUT PRX_CONTEXT RxContext);
/** @} */

/** @name Support functions and helpers
 * @{
 */
NTSTATUS vbsfNtDeleteConnection(IN PRX_CONTEXT RxContext,
                                OUT PBOOLEAN PostToFsp);
NTSTATUS vbsfNtCreateConnection(IN PRX_CONTEXT RxContext,
                                OUT PBOOLEAN PostToFsp);
NTSTATUS vbsfNtCloseFileHandle(PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension,
                               PMRX_VPOX_FOBX pVPoxFobx,
                               PVBSFNTFCBEXT pVPoxFcbx);
NTSTATUS vbsfNtRemove(IN PRX_CONTEXT RxContext);
NTSTATUS vbsfNtVPoxStatusToNt(int vrc);
PVOID    vbsfNtAllocNonPagedMem(ULONG ulSize);
void     vbsfNtFreeNonPagedMem(PVOID lpMem);
NTSTATUS vbsfNtShflStringFromUnicodeAlloc(PSHFLSTRING *ppShflString, const WCHAR *pwc, uint16_t cb);
#if defined(DEBUG) || defined(LOG_ENABLED)
const char *vbsfNtMajorFunctionName(UCHAR MajorFunction, LONG MinorFunction);
#endif

void     vbsfNtUpdateFcbSize(PFILE_OBJECT pFileObj, PMRX_FCB pFcb, PMRX_VPOX_FOBX pVPoxFobX,
                             LONGLONG cbFileNew, LONGLONG cbFileOld, LONGLONG cbAllocated);
int      vbsfNtQueryAndUpdateFcbSize(PMRX_VPOX_NETROOT_EXTENSION pNetRootX, PFILE_OBJECT pFileObj,
                                     PMRX_VPOX_FOBX pVPoxFobX, PMRX_FCB pFcb, PVBSFNTFCBEXT pVPoxFcbX);

/**
 * Converts VPox (IPRT) file mode to NT file attributes.
 *
 * @returns NT file attributes
 * @param   fIprtMode   IPRT file mode.
 *
 */
DECLINLINE(uint32_t) VPoxToNTFileAttributes(uint32_t fIprtMode)
{
    AssertCompile((RTFS_DOS_READONLY               >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_READONLY);
    AssertCompile((RTFS_DOS_HIDDEN                 >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_HIDDEN);
    AssertCompile((RTFS_DOS_SYSTEM                 >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_SYSTEM);
    AssertCompile((RTFS_DOS_DIRECTORY              >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_DIRECTORY);
    AssertCompile((RTFS_DOS_ARCHIVED               >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_ARCHIVE);
    AssertCompile((RTFS_DOS_NT_DEVICE              >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_DEVICE);
    AssertCompile((RTFS_DOS_NT_NORMAL              >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_NORMAL);
    AssertCompile((RTFS_DOS_NT_TEMPORARY           >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_TEMPORARY);
    AssertCompile((RTFS_DOS_NT_SPARSE_FILE         >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_SPARSE_FILE);
    AssertCompile((RTFS_DOS_NT_REPARSE_POINT       >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_REPARSE_POINT);
    AssertCompile((RTFS_DOS_NT_COMPRESSED          >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_COMPRESSED);
    AssertCompile((RTFS_DOS_NT_OFFLINE             >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_OFFLINE);
    AssertCompile((RTFS_DOS_NT_NOT_CONTENT_INDEXED >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
    AssertCompile((RTFS_DOS_NT_ENCRYPTED           >> RTFS_DOS_SHIFT) == FILE_ATTRIBUTE_ENCRYPTED);

    uint32_t fNtAttribs = (fIprtMode & (RTFS_DOS_MASK_NT & ~(RTFS_DOS_NT_OFFLINE | RTFS_DOS_NT_DEVICE | RTFS_DOS_NT_REPARSE_POINT)))
                       >> RTFS_DOS_SHIFT;
    return fNtAttribs ? fNtAttribs : FILE_ATTRIBUTE_NORMAL;
}

/**
 * Converts NT file attributes to VPox (IPRT) ones.
 *
 * @returns IPRT file mode
 * @param   fNtAttribs      NT file attributes
 */
DECLINLINE(uint32_t) NTToVPoxFileAttributes(uint32_t fNtAttribs)
{
    uint32_t fIprtMode = (fNtAttribs << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_NT;
    fIprtMode &= ~(RTFS_DOS_NT_OFFLINE | RTFS_DOS_NT_DEVICE | RTFS_DOS_NT_REPARSE_POINT);
    return fIprtMode ? fIprtMode : RTFS_DOS_NT_NORMAL;
}

/**
 * Helper for converting VPox object info to NT basic file info.
 */
DECLINLINE(void) vbsfNtBasicInfoFromVPoxObjInfo(FILE_BASIC_INFORMATION *pNtBasicInfo, PCSHFLFSOBJINFO pVPoxInfo)
{
    pNtBasicInfo->CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pVPoxInfo->BirthTime);
    pNtBasicInfo->LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pVPoxInfo->AccessTime);
    pNtBasicInfo->LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pVPoxInfo->ModificationTime);
    pNtBasicInfo->ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pVPoxInfo->ChangeTime);
    pNtBasicInfo->FileAttributes          = VPoxToNTFileAttributes(pVPoxInfo->Attr.fMode);
}


/** @} */

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_WINNT_SharedFolders_driver_vbsf_h */
