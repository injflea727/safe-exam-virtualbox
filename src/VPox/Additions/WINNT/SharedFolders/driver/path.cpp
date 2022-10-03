/* $Id: path.cpp $ */
/** @file
 * VirtualPox Windows Guest Shared Folders - Path related routines.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "vbsf.h"
#include <iprt/err.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
static UNICODE_STRING g_UnicodeBackslash = { 2, 4, L"\\" };


/**
 * Handles failure scenarios where we may have to close the handle.
 */
DECL_NO_INLINE(static, NTSTATUS) vbsfNtCreateWorkerBail(NTSTATUS Status, VPOXSFCREATEREQ *pReq,
                                                        PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension)
{
    Log(("VPOXSF: vbsfNtCreateWorker: Returns %#x (Handle was %#RX64)\n", Status, pReq->CreateParms.Handle));
    if (pReq->CreateParms.Handle != SHFL_HANDLE_NIL)
    {
        AssertCompile(sizeof(VPOXSFCLOSEREQ) <= RT_UOFFSETOF(VPOXSFCREATEREQ, CreateParms));
        VbglR0SfHostReqClose(pNetRootExtension->map.root, (VPOXSFCLOSEREQ *)pReq, pReq->CreateParms.Handle);
    }
    return Status;
}


/**
 * Worker for VPoxMRxCreate that converts parameters and calls the host.
 *
 * The caller takes care of freeing the request buffer, so this function is free
 * to just return at will.
 */
static NTSTATUS vbsfNtCreateWorker(PRX_CONTEXT RxContext, VPOXSFCREATEREQ *pReq, ULONG *pulCreateAction,
                                   PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension, PMRX_FCB pFcb)
{
    /*
     * Check out the options.
     */
    ULONG const fOptions            = RxContext->Create.NtCreateParameters.CreateOptions & FILE_VALID_OPTION_FLAGS;
    ULONG const CreateDisposition   = RxContext->Create.NtCreateParameters.Disposition;
    bool const  fCreateDir          = (fOptions & FILE_DIRECTORY_FILE)
                                   && (CreateDisposition == FILE_CREATE || CreateDisposition == FILE_OPEN_IF);
    bool const  fTemporaryFile      = (RxContext->Create.NtCreateParameters.FileAttributes & FILE_ATTRIBUTE_TEMPORARY)
                                   || (pFcb->FcbState & FCB_STATE_TEMPORARY);

    Log(("VPOXSF: vbsfNtCreateWorker: fTemporaryFile %d, fCreateDir %d%s%s%s\n", fTemporaryFile, fCreateDir,
         fOptions & FILE_DIRECTORY_FILE ? ", FILE_DIRECTORY_FILE" : "",
         fOptions & FILE_NON_DIRECTORY_FILE ? ", FILE_NON_DIRECTORY_FILE" : "",
         fOptions & FILE_DELETE_ON_CLOSE ? ", FILE_DELETE_ON_CLOSE" : ""));

    /* Check consistency in specified flags. */
    if (fTemporaryFile && fCreateDir) /* Directories with temporary flag set are not allowed! */
    {
        Log(("VPOXSF: vbsfNtCreateWorker: Not allowed: Temporary directories!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    if ((fOptions & (FILE_DIRECTORY_FILE | FILE_NON_DIRECTORY_FILE)) == (FILE_DIRECTORY_FILE | FILE_NON_DIRECTORY_FILE))
    {
        /** @todo r=bird: Check if FILE_DIRECTORY_FILE+FILE_NON_DIRECTORY_FILE really is illegal in all combinations... */
        Log(("VPOXSF: vbsfNtCreateWorker: Unsupported combination: dir && !dir\n"));
        return STATUS_INVALID_PARAMETER;
    }

    /*
     * Initialize create parameters.
     */
    RT_ZERO(pReq->CreateParms);
    pReq->CreateParms.Handle = SHFL_HANDLE_NIL;
    pReq->CreateParms.Result = SHFL_NO_RESULT;

    /*
     * Directory.
     */
    if (fOptions & FILE_DIRECTORY_FILE)
    {
        if (CreateDisposition != FILE_CREATE && CreateDisposition != FILE_OPEN && CreateDisposition != FILE_OPEN_IF)
        {
            Log(("VPOXSF: vbsfNtCreateWorker: Invalid disposition 0x%08X for directory!\n",
                 CreateDisposition));
            return STATUS_INVALID_PARAMETER;
        }

        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_DIRECTORY\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_DIRECTORY;
    }

    /*
     * Disposition.
     */
    switch (CreateDisposition)
    {
        case FILE_SUPERSEDE:
            pReq->CreateParms.CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        case FILE_OPEN:
            pReq->CreateParms.CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW\n"));
            break;

        case FILE_CREATE:
            pReq->CreateParms.CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        case FILE_OPEN_IF:
            pReq->CreateParms.CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        case FILE_OVERWRITE:
            pReq->CreateParms.CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW\n"));
            break;

        case FILE_OVERWRITE_IF:
            pReq->CreateParms.CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW\n"));
            break;

        default:
            Log(("VPOXSF: vbsfNtCreateWorker: Unexpected create disposition: 0x%08X\n", CreateDisposition));
            return STATUS_INVALID_PARAMETER;
    }

    /*
     * Access mode.
     */
    ACCESS_MASK const DesiredAccess = RxContext->Create.NtCreateParameters.DesiredAccess;
    if (DesiredAccess & FILE_READ_DATA)
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_READ\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_READ;
    }

    /* FILE_WRITE_DATA means write access regardless of FILE_APPEND_DATA bit.
       FILE_APPEND_DATA without FILE_WRITE_DATA means append only mode. */
    if (DesiredAccess & FILE_WRITE_DATA)
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_WRITE\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_WRITE;
    }
    else if (DesiredAccess & FILE_APPEND_DATA)
    {
        /* Both write and append access flags are required for shared folders,
         * as on Windows FILE_APPEND_DATA implies write access. */
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_WRITE | SHFL_CF_ACCESS_APPEND\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_WRITE | SHFL_CF_ACCESS_APPEND;
    }

    if (DesiredAccess & FILE_READ_ATTRIBUTES)
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_ATTR_READ\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_ATTR_READ;
    }
    if (DesiredAccess & FILE_WRITE_ATTRIBUTES)
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_ATTR_WRITE\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_ATTR_WRITE;
    }

    /*
     * Sharing.
     */
    ULONG const ShareAccess = RxContext->Create.NtCreateParameters.ShareAccess;
    if (ShareAccess & (FILE_SHARE_READ | FILE_SHARE_WRITE))
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_DENYNONE\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_DENYNONE;
    }
    else if (ShareAccess & FILE_SHARE_READ)
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_DENYWRITE\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_DENYWRITE;
    }
    else if (ShareAccess & FILE_SHARE_WRITE)
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_DENYREAD\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_DENYREAD;
    }
    else
    {
        Log(("VPOXSF: vbsfNtCreateWorker: CreateFlags |= SHFL_CF_ACCESS_DENYALL\n"));
        pReq->CreateParms.CreateFlags |= SHFL_CF_ACCESS_DENYALL;
    }

    /*
     * Set initial allocation size and attributes.
     * There aren't too many attributes that need to be passed over.
     */
    pReq->CreateParms.Info.cbObject   = RxContext->Create.NtCreateParameters.AllocationSize.QuadPart;
    pReq->CreateParms.Info.Attr.fMode = NTToVPoxFileAttributes(  RxContext->Create.NtCreateParameters.FileAttributes
                                                               & (  FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN
                                                                  | FILE_ATTRIBUTE_SYSTEM   | FILE_ATTRIBUTE_ARCHIVE));

    /*
     * Call the host.
     */
    Log(("VPOXSF: vbsfNtCreateWorker: Calling VbglR0SfHostReqCreate(fCreate=%#RX32)...\n", pReq->CreateParms.CreateFlags));
    int vrc = VbglR0SfHostReqCreate(pNetRootExtension->map.root, pReq);
    Log(("VPOXSF: vbsfNtCreateWorker: VbglR0SfHostReqCreate returns vrc = %Rrc, Result = 0x%x, Handle = %#RX64\n",
         vrc, pReq->CreateParms.Result, pReq->CreateParms.Handle));

    if (RT_SUCCESS(vrc))
    {
        /*
         * The request succeeded. Analyze host response,
         */
        switch (pReq->CreateParms.Result)
        {
            case SHFL_PATH_NOT_FOUND:
                /* Path to the object does not exist. */
                Log(("VPOXSF: vbsfNtCreateWorker: Path not found -> STATUS_OBJECT_PATH_NOT_FOUND + FILE_DOES_NOT_EXIST\n"));
                *pulCreateAction = FILE_DOES_NOT_EXIST;
                return STATUS_OBJECT_PATH_NOT_FOUND;

            case SHFL_FILE_NOT_FOUND:
                *pulCreateAction = FILE_DOES_NOT_EXIST;
                if (pReq->CreateParms.Handle == SHFL_HANDLE_NIL)
                {
                    Log(("VPOXSF: vbsfNtCreateWorker: File not found -> STATUS_OBJECT_NAME_NOT_FOUND + FILE_DOES_NOT_EXIST\n"));
                    return STATUS_OBJECT_NAME_NOT_FOUND;
                }
                AssertMsgFailed(("VPOXSF: vbsfNtCreateWorker: WTF? File not found but have a handle!\n"));
                return vbsfNtCreateWorkerBail(STATUS_UNSUCCESSFUL, pReq, pNetRootExtension);

            case SHFL_FILE_EXISTS:
                Log(("VPOXSF: vbsfNtCreateWorker: File exists, Handle = %#RX64\n", pReq->CreateParms.Handle));
                if (pReq->CreateParms.Handle == SHFL_HANDLE_NIL)
                {
                    *pulCreateAction = FILE_EXISTS;
                    if (CreateDisposition == FILE_CREATE)
                    {
                        /* File was not opened because we requested a create. */
                        Log(("VPOXSF: vbsfNtCreateWorker: File exists already, create failed -> STATUS_OBJECT_NAME_COLLISION\n"));
                        return STATUS_OBJECT_NAME_COLLISION;
                    }

                    /* Actually we should not go here, unless we have no rights to open the object. */
                    Log(("VPOXSF: vbsfNtCreateWorker: Existing file was not opened! -> STATUS_ACCESS_DENIED\n"));
                    return STATUS_ACCESS_DENIED;
                }

                /* An existing file was opened. */
                *pulCreateAction = FILE_OPENED;
                break;

            case SHFL_FILE_CREATED:
                Log(("VPOXSF: vbsfNtCreateWorker: File created (Handle=%#RX64) / FILE_CREATED\n", pReq->CreateParms.Handle));
                /* A new file was created. */
                Assert(pReq->CreateParms.Handle != SHFL_HANDLE_NIL);
                *pulCreateAction = FILE_CREATED;
                break;

            case SHFL_FILE_REPLACED:
                /* An existing file was replaced or overwritten. */
                Assert(pReq->CreateParms.Handle != SHFL_HANDLE_NIL);
                if (CreateDisposition == FILE_SUPERSEDE)
                {
                    Log(("VPOXSF: vbsfNtCreateWorker: File replaced (Handle=%#RX64) / FILE_SUPERSEDED\n", pReq->CreateParms.Handle));
                    *pulCreateAction = FILE_SUPERSEDED;
                }
                else
                {
                    Log(("VPOXSF: vbsfNtCreateWorker: File replaced (Handle=%#RX64) / FILE_OVERWRITTEN\n", pReq->CreateParms.Handle));
                    *pulCreateAction = FILE_OVERWRITTEN;
                }
                break;

            default:
                Log(("VPOXSF: vbsfNtCreateWorker: Invalid CreateResult from host (0x%08X)\n", pReq->CreateParms.Result));
                *pulCreateAction = FILE_DOES_NOT_EXIST;
                return vbsfNtCreateWorkerBail(STATUS_OBJECT_PATH_NOT_FOUND, pReq, pNetRootExtension);
        }

        /*
         * Check flags.
         */
        if (!(fOptions & FILE_NON_DIRECTORY_FILE) || !FlagOn(pReq->CreateParms.Info.Attr.fMode, RTFS_DOS_DIRECTORY))
        { /* likely */ }
        else
        {
            /* Caller wanted only a file, but the object is a directory. */
            Log(("VPOXSF: vbsfNtCreateWorker: -> STATUS_FILE_IS_A_DIRECTORY!\n"));
            return vbsfNtCreateWorkerBail(STATUS_FILE_IS_A_DIRECTORY, pReq, pNetRootExtension);
        }

        if (!(fOptions & FILE_DIRECTORY_FILE) || FlagOn(pReq->CreateParms.Info.Attr.fMode, RTFS_DOS_DIRECTORY))
        { /* likely */ }
        else
        {
            /* Caller wanted only a directory, but the object is not a directory. */
            Log(("VPOXSF: vbsfNtCreateWorker: -> STATUS_NOT_A_DIRECTORY!\n"));
            return vbsfNtCreateWorkerBail(STATUS_NOT_A_DIRECTORY, pReq, pNetRootExtension);
        }

        return STATUS_SUCCESS;
    }

    /*
     * Failed. Map some VPoxRC to STATUS codes expected by the system.
     */
    switch (vrc)
    {
        case VERR_ALREADY_EXISTS:
            Log(("VPOXSF: vbsfNtCreateWorker: VERR_ALREADY_EXISTS -> STATUS_OBJECT_NAME_COLLISION + FILE_EXISTS\n"));
            *pulCreateAction = FILE_EXISTS;
            return STATUS_OBJECT_NAME_COLLISION;

        /* On POSIX systems, the "mkdir" command returns VERR_FILE_NOT_FOUND when
           doing a recursive directory create. Handle this case.

           bird: We end up here on windows systems too if opening a dir that doesn't
                 exists.  Thus, I've changed the SHFL_PATH_NOT_FOUND to SHFL_FILE_NOT_FOUND
                 so that FsPerf is happy. */
        case VERR_FILE_NOT_FOUND: /** @todo r=bird: this is a host bug, isn't it? */
            pReq->CreateParms.Result = SHFL_FILE_NOT_FOUND;
            pReq->CreateParms.Handle = SHFL_HANDLE_NIL;
            *pulCreateAction = FILE_DOES_NOT_EXIST;
            Log(("VPOXSF: vbsfNtCreateWorker: VERR_FILE_NOT_FOUND -> STATUS_OBJECT_NAME_NOT_FOUND + FILE_DOES_NOT_EXIST\n"));
            return STATUS_OBJECT_NAME_NOT_FOUND;

        default:
        {
            *pulCreateAction = FILE_DOES_NOT_EXIST;
            NTSTATUS Status = vbsfNtVPoxStatusToNt(vrc);
            Log(("VPOXSF: vbsfNtCreateWorker: %Rrc -> %#010x + FILE_DOES_NOT_EXIST\n", vrc, Status));
            return Status;
        }
    }
}

/**
 * Create/open a file, directory, ++.
 *
 * The RDBSS library will do a table lookup on the path passed in by the user
 * and therefore share FCBs for objects with the same path.
 *
 * The FCB needs to be locked exclusively upon successful return, however it
 * seems like it's not always locked when we get here (only older RDBSS library
 * versions?), so we have to check this before returning.
 *
 */
NTSTATUS VPoxMRxCreate(IN OUT PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    PMRX_NET_ROOT               pNetRoot          = capFcb->pNetRoot;
    PMRX_SRV_OPEN               pSrvOpen          = RxContext->pRelevantSrvOpen;
    PUNICODE_STRING             RemainingName     = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension = VPoxMRxGetNetRootExtension(capFcb->pNetRoot);


    /*
     * Log stuff and make some small adjustments to empty paths and caching flags.
     */
    Log(("VPOXSF: VPoxMRxCreate:  CreateOptions = %#010x\n", RxContext->Create.NtCreateParameters.CreateOptions));
    Log(("VPOXSF: VPoxMRxCreate:    Disposition = %#010x\n", RxContext->Create.NtCreateParameters.Disposition));
    Log(("VPOXSF: VPoxMRxCreate:  DesiredAccess = %#010x\n", RxContext->Create.NtCreateParameters.DesiredAccess));
    Log(("VPOXSF: VPoxMRxCreate:    ShareAccess = %#010x\n", RxContext->Create.NtCreateParameters.ShareAccess));
    Log(("VPOXSF: VPoxMRxCreate: FileAttributes = %#010x\n", RxContext->Create.NtCreateParameters.FileAttributes));
    Log(("VPOXSF: VPoxMRxCreate: AllocationSize = %#RX64\n", RxContext->Create.NtCreateParameters.AllocationSize.QuadPart));
    Log(("VPOXSF: VPoxMRxCreate: name ptr %p length=%d, SrvOpen->Flags %#010x\n",
         RemainingName, RemainingName->Length, pSrvOpen->Flags));

    /* Disable FastIO. It causes a verifier bugcheck. */
#ifdef SRVOPEN_FLAG_DONTUSE_READ_CACHING
    SetFlag(pSrvOpen->Flags, SRVOPEN_FLAG_DONTUSE_READ_CACHING | SRVOPEN_FLAG_DONTUSE_WRITE_CACHING);
#else
    SetFlag(pSrvOpen->Flags, SRVOPEN_FLAG_DONTUSE_READ_CACHEING | SRVOPEN_FLAG_DONTUSE_WRITE_CACHEING);
#endif

    if (RemainingName->Length)
        Log(("VPOXSF: VPoxMRxCreate: Attempt to open %.*ls\n",
             RemainingName->Length/sizeof(WCHAR), RemainingName->Buffer));
    else if (FlagOn(RxContext->Create.Flags, RX_CONTEXT_CREATE_FLAG_STRIPPED_TRAILING_BACKSLASH))
    {
        Log(("VPOXSF: VPoxMRxCreate: Empty name -> Only backslash used\n"));
        RemainingName = &g_UnicodeBackslash;
    }

    /*
     * Fend off unsupported and invalid requests before we start allocating memory.
     */
    if (   pNetRoot->Type != NET_ROOT_WILD
        && pNetRoot->Type != NET_ROOT_DISK)
    {
        Log(("VPOXSF: VPoxMRxCreate: netroot type %d not supported\n",
             pNetRoot->Type));
        return STATUS_NOT_IMPLEMENTED;
    }

    if (RxContext->Create.EaLength == 0)
    { /* likely */ }
    else
    {
        Log(("VPOXSF: VPoxMRxCreate: Unsupported: extended attributes!\n"));
        return STATUS_EAS_NOT_SUPPORTED;
    }

    if (!(capFcb->FcbState & FCB_STATE_PAGING_FILE))
    { /* likely */ }
    else
    {
        Log(("VPOXSF: VPoxMRxCreate: Unsupported: paging file!\n"));
        return STATUS_NOT_IMPLEMENTED;
    }

    if (!(RxContext->Create.NtCreateParameters.CreateOptions & FILE_OPEN_BY_FILE_ID))
    { /* likely */ }
    else
    {
        Log(("VPOXSF: VPoxMRxCreate: Unsupported: file open by id!\n"));
        return STATUS_NOT_IMPLEMENTED;
    }

    /*
     * Allocate memory for the request.
     */
    bool const     fSlashHack = RxContext->CurrentIrpSp
                             && (RxContext->CurrentIrpSp->Parameters.Create.ShareAccess & VPOX_MJ_CREATE_SLASH_HACK);
    uint16_t const  cbPath    = RemainingName->Length;
    uint32_t const  cbPathAll = cbPath + fSlashHack * sizeof(RTUTF16) + sizeof(RTUTF16);
    AssertReturn(cbPathAll < _64K, STATUS_NAME_TOO_LONG);

    uint32_t const  cbReq     = RT_UOFFSETOF(VPOXSFCREATEREQ, StrPath.String) + cbPathAll;
    VPOXSFCREATEREQ *pReq     = (VPOXSFCREATEREQ *)VbglR0PhysHeapAlloc(cbReq);
    if (pReq)
    { }
    else
        return STATUS_INSUFFICIENT_RESOURCES;

    /*
     * Copy out the path string.
     */
    pReq->StrPath.u16Size = (uint16_t)cbPathAll;
    if (!fSlashHack)
    {
        pReq->StrPath.u16Length = cbPath;
        memcpy(&pReq->StrPath.String, RemainingName->Buffer, cbPath);
        pReq->StrPath.String.utf16[cbPath / sizeof(RTUTF16)] = '\0';
    }
    else
    {
        /* HACK ALERT! Here we add back the lsash we had to hide from RDBSS. */
        pReq->StrPath.u16Length = cbPath + sizeof(RTUTF16);
        memcpy(&pReq->StrPath.String, RemainingName->Buffer, cbPath);
        pReq->StrPath.String.utf16[cbPath / sizeof(RTUTF16)] = '\\';
        pReq->StrPath.String.utf16[cbPath / sizeof(RTUTF16) + 1] = '\0';
    }
    Log(("VPOXSF: VPoxMRxCreate: %.*ls\n", pReq->StrPath.u16Length / sizeof(RTUTF16), pReq->StrPath.String.utf16));

    /*
     * Hand the bulk work off to a worker function to simplify bailout and cleanup.
     */
    ULONG       CreateAction = FILE_CREATED;
    NTSTATUS    Status = vbsfNtCreateWorker(RxContext, pReq, &CreateAction, pNetRootExtension, capFcb);
    if (Status == STATUS_SUCCESS)
    {
        Log(("VPOXSF: VPoxMRxCreate: EOF is 0x%RX64 AllocSize is 0x%RX64\n",
             pReq->CreateParms.Info.cbObject, pReq->CreateParms.Info.cbAllocated));
        Log(("VPOXSF: VPoxMRxCreate: CreateAction = %#010x\n", CreateAction));

        /*
         * Create the file object extension.
         * After this we're out of the woods and nothing more can go wrong.
         */
        PMRX_FOBX pFobx;
        RxContext->pFobx = pFobx = RxCreateNetFobx(RxContext, pSrvOpen);
        PMRX_VPOX_FOBX pVPoxFobx = pFobx ? VPoxMRxGetFileObjectExtension(pFobx) : NULL;
        if (pFobx && pVPoxFobx)
        {
            /*
             * Make sure we've got the FCB locked exclusivly before updating it and returning.
             * (bird: not entirely sure if this is needed for the W10 RDBSS, but cannot hurt.)
             */
            if (!RxIsFcbAcquiredExclusive(capFcb))
                RxAcquireExclusiveFcbResourceInMRx(capFcb);

            /*
             * Initialize our file object extension data.
             */
            pVPoxFobx->Info         = pReq->CreateParms.Info;
            pVPoxFobx->nsUpToDate   = RTTimeSystemNanoTS();
            pVPoxFobx->hFile        = pReq->CreateParms.Handle;
            pVPoxFobx->pSrvCall     = RxContext->Create.pSrvCall;

            /* bird: Dunno what this really's about. */
            pFobx->OffsetOfNextEaToReturn = 1;

            /*
             * Initialize the FCB if this is the first open.
             *
             * Note! The RxFinishFcbInitialization call expects node types as the 2nd parameter,
             *       but is for  some reason given enum RX_FILE_TYPE as type.
             */
            if (capFcb->OpenCount == 0)
            {
                Log(("VPOXSF: VPoxMRxCreate: Initializing the FCB.\n"));
                FCB_INIT_PACKET               InitPacket;
                FILE_NETWORK_OPEN_INFORMATION Data;
                ULONG                         NumberOfLinks = 0; /** @todo ?? */
                Data.CreationTime.QuadPart   = RTTimeSpecGetNtTime(&pReq->CreateParms.Info.BirthTime);
                Data.LastAccessTime.QuadPart = RTTimeSpecGetNtTime(&pReq->CreateParms.Info.AccessTime);
                Data.LastWriteTime.QuadPart  = RTTimeSpecGetNtTime(&pReq->CreateParms.Info.ModificationTime);
                Data.ChangeTime.QuadPart     = RTTimeSpecGetNtTime(&pReq->CreateParms.Info.ChangeTime);
                /** @todo test sparse files.  CcSetFileSizes is documented to not want allocation size smaller than EOF offset. */
                Data.AllocationSize.QuadPart = pReq->CreateParms.Info.cbAllocated;
                Data.EndOfFile.QuadPart      = pReq->CreateParms.Info.cbObject;
                Data.FileAttributes          = VPoxToNTFileAttributes(pReq->CreateParms.Info.Attr.fMode);
                RxFormInitPacket(InitPacket,
                                 &Data.FileAttributes,
                                 &NumberOfLinks,
                                 &Data.CreationTime,
                                 &Data.LastAccessTime,
                                 &Data.LastWriteTime,
                                 &Data.ChangeTime,
                                 &Data.AllocationSize,
                                 &Data.EndOfFile,
                                 &Data.EndOfFile);
                if (pReq->CreateParms.Info.Attr.fMode & RTFS_DOS_DIRECTORY)
                    RxFinishFcbInitialization(capFcb, (RX_FILE_TYPE)RDBSS_NTC_STORAGE_TYPE_DIRECTORY, &InitPacket);
                else
                    RxFinishFcbInitialization(capFcb, (RX_FILE_TYPE)RDBSS_NTC_STORAGE_TYPE_FILE, &InitPacket);
            }


            /*
             * See if the size has changed and update the FCB if it has.
             */
            if (   capFcb->OpenCount > 0
                && capFcb->Header.FileSize.QuadPart != pReq->CreateParms.Info.cbObject)
            {
                PFILE_OBJECT pFileObj = RxContext->CurrentIrpSp->FileObject;
                Assert(pFileObj);
                if (pFileObj)
                    vbsfNtUpdateFcbSize(pFileObj, capFcb, pVPoxFobx, pReq->CreateParms.Info.cbObject,
                                        capFcb->Header.FileSize.QuadPart, pReq->CreateParms.Info.cbAllocated);
            }

            /*
             * Set various return values.
             */

            /* This is "our" contribution to the buffering flags (no buffering, please). */
            pSrvOpen->BufferingFlags = 0;

            /* This is the IO_STATUS_BLOCK::Information value, I think. */
            RxContext->Create.ReturnedCreateInformation = CreateAction;

            /*
             * Do logging.
             */
            Log(("VPOXSF: VPoxMRxCreate: Info: BirthTime        %RI64\n", RTTimeSpecGetNano(&pVPoxFobx->Info.BirthTime)));
            Log(("VPOXSF: VPoxMRxCreate: Info: ChangeTime       %RI64\n", RTTimeSpecGetNano(&pVPoxFobx->Info.ChangeTime)));
            Log(("VPOXSF: VPoxMRxCreate: Info: ModificationTime %RI64\n", RTTimeSpecGetNano(&pVPoxFobx->Info.ModificationTime)));
            Log(("VPOXSF: VPoxMRxCreate: Info: AccessTime       %RI64\n", RTTimeSpecGetNano(&pVPoxFobx->Info.AccessTime)));
            Log(("VPOXSF: VPoxMRxCreate: Info: fMode            %#RX32\n", pVPoxFobx->Info.Attr.fMode));
            if (!(pVPoxFobx->Info.Attr.fMode & RTFS_DOS_DIRECTORY))
            {
                Log(("VPOXSF: VPoxMRxCreate: Info: cbObject         %#RX64\n", pVPoxFobx->Info.cbObject));
                Log(("VPOXSF: VPoxMRxCreate: Info: cbAllocated      %#RX64\n", pVPoxFobx->Info.cbAllocated));
            }
            Log(("VPOXSF: VPoxMRxCreate: NetRoot is %p, Fcb is %p, pSrvOpen is %p, Fobx is %p\n",
                 pNetRoot, capFcb, pSrvOpen, RxContext->pFobx));
            Log(("VPOXSF: VPoxMRxCreate: returns STATUS_SUCCESS\n"));
        }
        else
        {
            Log(("VPOXSF: VPoxMRxCreate: RxCreateNetFobx failed (pFobx=%p)\n", pFobx));
            Assert(!pFobx);
            AssertCompile(sizeof(VPOXSFCLOSEREQ) <= RT_UOFFSETOF(VPOXSFCREATEREQ, CreateParms));
            VbglR0SfHostReqClose(pNetRootExtension->map.root, (VPOXSFCLOSEREQ *)pReq, pReq->CreateParms.Handle);
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
        Log(("VPOXSF: VPoxMRxCreate: vbsfNtCreateWorker failed %#010x\n", Status));
    VbglR0PhysHeapFree(pReq);
    return Status;
}

NTSTATUS VPoxMRxComputeNewBufferingState(IN OUT PMRX_SRV_OPEN pMRxSrvOpen, IN PVOID pMRxContext, OUT PULONG pNewBufferingState)
{
    RT_NOREF(pMRxSrvOpen, pMRxContext, pNewBufferingState);
    Log(("VPOXSF: MRxComputeNewBufferingState\n"));
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS VPoxMRxDeallocateForFcb(IN OUT PMRX_FCB pFcb)
{
    RT_NOREF(pFcb);
    Log(("VPOXSF: MRxDeallocateForFcb\n"));
    return STATUS_SUCCESS;
}

NTSTATUS VPoxMRxDeallocateForFobx(IN OUT PMRX_FOBX pFobx)
{
    RT_NOREF(pFobx);
    Log(("VPOXSF: MRxDeallocateForFobx\n"));
    return STATUS_SUCCESS;
}

NTSTATUS VPoxMRxTruncate(IN PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VPOXSF: MRxTruncate\n"));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS VPoxMRxCleanupFobx(IN PRX_CONTEXT RxContext)
{
    PMRX_VPOX_FOBX pVPoxFobx = VPoxMRxGetFileObjectExtension(RxContext->pFobx);

    Log(("VPOXSF: MRxCleanupFobx: pVPoxFobx = %p, Handle = 0x%RX64\n", pVPoxFobx, pVPoxFobx? pVPoxFobx->hFile: 0));

    if (!pVPoxFobx)
        return STATUS_INVALID_PARAMETER;

    return STATUS_SUCCESS;
}

NTSTATUS VPoxMRxForceClosed(IN PMRX_SRV_OPEN pSrvOpen)
{
    RT_NOREF(pSrvOpen);
    Log(("VPOXSF: MRxForceClosed\n"));
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Ensures the FCBx doesn't have dangling pointers to @a pVPoxFobx.
 *
 * This isn't strictly speaking needed, as nobody currently dereference these
 * pointers, however better keeping things neath and tidy.
 */
DECLINLINE(void) vbsfNtCleanupFcbxTimestampRefsOnClose(PMRX_VPOX_FOBX pVPoxFobx, PVBSFNTFCBEXT pVPoxFcbx)
{
    pVPoxFobx->fTimestampsSetByUser          = 0;
    pVPoxFobx->fTimestampsUpdatingSuppressed = 0;
    pVPoxFobx->fTimestampsImplicitlyUpdated  = 0;
    if (pVPoxFcbx->pFobxLastAccessTime == pVPoxFobx)
        pVPoxFcbx->pFobxLastAccessTime = NULL;
    if (pVPoxFcbx->pFobxLastWriteTime  == pVPoxFobx)
        pVPoxFcbx->pFobxLastWriteTime  = NULL;
    if (pVPoxFcbx->pFobxChangeTime     == pVPoxFobx)
        pVPoxFcbx->pFobxChangeTime     = NULL;
}

/**
 * Closes an opened file handle of a MRX_VPOX_FOBX.
 *
 * Updates file attributes if necessary.
 *
 * Used by VPoxMRxCloseSrvOpen and vbsfNtRename.
 */
NTSTATUS vbsfNtCloseFileHandle(PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension,
                               PMRX_VPOX_FOBX pVPoxFobx,
                               PVBSFNTFCBEXT pVPoxFcbx)
{
    if (pVPoxFobx->hFile == SHFL_HANDLE_NIL)
    {
        Log(("VPOXSF: vbsfCloseFileHandle: SHFL_HANDLE_NIL\n"));
        return STATUS_SUCCESS;
    }

    Log(("VPOXSF: vbsfCloseFileHandle: 0x%RX64, fTimestampsUpdatingSuppressed = %#x, fTimestampsImplicitlyUpdated = %#x\n",
         pVPoxFobx->hFile, pVPoxFobx->fTimestampsUpdatingSuppressed, pVPoxFobx->fTimestampsImplicitlyUpdated));

    /*
     * We allocate a single request buffer for the timestamp updating and the closing
     * to save time (at the risk of running out of heap, but whatever).
     */
    union MyCloseAndInfoReq
    {
        VPOXSFCLOSEREQ   Close;
        VPOXSFOBJINFOREQ Info;
    } *pReq = (union MyCloseAndInfoReq *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
        RT_ZERO(*pReq);
    else
        return STATUS_INSUFF_SERVER_RESOURCES;

    /*
     * Restore timestamp that we may implicitly been updated via this handle
     * after the user explicitly set them or turn off implict updating (the -1 value).
     *
     * Note! We ignore the status of this operation.
     */
    Assert(pVPoxFcbx);
    uint8_t fUpdateTs = pVPoxFobx->fTimestampsUpdatingSuppressed & pVPoxFobx->fTimestampsImplicitlyUpdated;
    if (fUpdateTs)
    {
        /** @todo skip this if the host is windows and fTimestampsUpdatingSuppressed == fTimestampsSetByUser */
        /** @todo pass -1 timestamps thru so we can always skip this on windows hosts! */
        if (   (fUpdateTs & VPOX_FOBX_F_INFO_LASTACCESS_TIME)
            && pVPoxFcbx->pFobxLastAccessTime == pVPoxFobx)
            pReq->Info.ObjInfo.AccessTime        = pVPoxFobx->Info.AccessTime;
        else
            fUpdateTs &= ~VPOX_FOBX_F_INFO_LASTACCESS_TIME;

        if (   (fUpdateTs & VPOX_FOBX_F_INFO_LASTWRITE_TIME)
            && pVPoxFcbx->pFobxLastWriteTime  == pVPoxFobx)
            pReq->Info.ObjInfo.ModificationTime  = pVPoxFobx->Info.ModificationTime;
        else
            fUpdateTs &= ~VPOX_FOBX_F_INFO_LASTWRITE_TIME;

        if (   (fUpdateTs & VPOX_FOBX_F_INFO_CHANGE_TIME)
            && pVPoxFcbx->pFobxChangeTime     == pVPoxFobx)
            pReq->Info.ObjInfo.ChangeTime        = pVPoxFobx->Info.ChangeTime;
        else
            fUpdateTs &= ~VPOX_FOBX_F_INFO_CHANGE_TIME;
        if (fUpdateTs)
        {
            Log(("VPOXSF: vbsfCloseFileHandle: Updating timestamp: %#x\n", fUpdateTs));
            int vrc = VbglR0SfHostReqSetObjInfo(pNetRootExtension->map.root, &pReq->Info, pVPoxFobx->hFile);
            if (RT_FAILURE(vrc))
                Log(("VPOXSF: vbsfCloseFileHandle: VbglR0SfHostReqSetObjInfo failed for fUpdateTs=%#x: %Rrc\n", fUpdateTs, vrc));
            RT_NOREF(vrc);
        }
        else
            Log(("VPOXSF: vbsfCloseFileHandle: no timestamp needing updating\n"));
    }

    vbsfNtCleanupFcbxTimestampRefsOnClose(pVPoxFobx, pVPoxFcbx);

    /*
     * Now close the handle.
     */
    int vrc = VbglR0SfHostReqClose(pNetRootExtension->map.root, &pReq->Close, pVPoxFobx->hFile);

    pVPoxFobx->hFile = SHFL_HANDLE_NIL;

    VbglR0PhysHeapFree(pReq);

    NTSTATUS const Status = RT_SUCCESS(vrc) ? STATUS_SUCCESS : vbsfNtVPoxStatusToNt(vrc);
    Log(("VPOXSF: vbsfCloseFileHandle: Returned 0x%08X (vrc=%Rrc)\n", Status, vrc));
    return Status;
}

/**
 * @note We don't collapse opens, this is called whenever a handle is closed.
 */
NTSTATUS VPoxMRxCloseSrvOpen(IN PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;

    PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension = VPoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VPOX_FOBX pVPoxFobx = VPoxMRxGetFileObjectExtension(capFobx);
    PMRX_SRV_OPEN pSrvOpen = capFobx->pSrvOpen;


    Log(("VPOXSF: MRxCloseSrvOpen: capFcb = %p, capFobx = %p, pVPoxFobx = %p, pSrvOpen = %p\n",
          capFcb, capFobx, pVPoxFobx, pSrvOpen));

#ifdef LOG_ENABLED
    PUNICODE_STRING pRemainingName = pSrvOpen->pAlreadyPrefixedName;
    Log(("VPOXSF: MRxCloseSrvOpen: Remaining name = %.*ls, Len = %d\n",
         pRemainingName->Length / sizeof(WCHAR), pRemainingName->Buffer, pRemainingName->Length));
#endif

    if (!pVPoxFobx)
        return STATUS_INVALID_PARAMETER;

    if (FlagOn(pSrvOpen->Flags, (SRVOPEN_FLAG_FILE_RENAMED | SRVOPEN_FLAG_FILE_DELETED)))
    {
        /* If we renamed or delete the file/dir, then it's already closed */
        Assert(pVPoxFobx->hFile == SHFL_HANDLE_NIL);
        Log(("VPOXSF: MRxCloseSrvOpen: File was renamed, handle 0x%RX64 ignore close.\n",
             pVPoxFobx->hFile));
        return STATUS_SUCCESS;
    }

    /*
     * Remove file or directory if delete action is pending and the this is the last open handle.
     */
    NTSTATUS Status = STATUS_SUCCESS;
    if (capFcb->FcbState & FCB_STATE_DELETE_ON_CLOSE)
    {
        Log(("VPOXSF: MRxCloseSrvOpen: Delete on close. Open count = %d\n",
             capFcb->OpenCount));

        if (capFcb->OpenCount == 0)
            Status = vbsfNtRemove(RxContext);
    }

    /*
     * Close file if we still have a handle to it.
     */
    if (pVPoxFobx->hFile != SHFL_HANDLE_NIL)
        vbsfNtCloseFileHandle(pNetRootExtension, pVPoxFobx, VPoxMRxGetFcbExtension(capFcb));

    return Status;
}

/**
 * Worker for vbsfNtSetBasicInfo and VPoxMRxCloseSrvOpen.
 *
 * Only called by vbsfNtSetBasicInfo if there is exactly one open handle.  And
 * VPoxMRxCloseSrvOpen calls it when the last handle is being closed.
 */
NTSTATUS vbsfNtRemove(IN PRX_CONTEXT RxContext)
{
    RxCaptureFcb;
    RxCaptureFobx;
    PMRX_VPOX_NETROOT_EXTENSION pNetRootExtension = VPoxMRxGetNetRootExtension(capFcb->pNetRoot);
    PMRX_VPOX_FOBX              pVPoxFobx         = VPoxMRxGetFileObjectExtension(capFobx);
    PUNICODE_STRING             pRemainingName    = GET_ALREADY_PREFIXED_NAME_FROM_CONTEXT(RxContext);
    uint16_t const              cwcRemainingName  = pRemainingName->Length / sizeof(WCHAR);

    Log(("VPOXSF: vbsfNtRemove: Delete %.*ls. open count = %d\n",
         cwcRemainingName, pRemainingName->Buffer, capFcb->OpenCount));
    Assert(RxIsFcbAcquiredExclusive(capFcb));

    /*
     * We've got function that does both deletion and handle closing starting with 6.0.8,
     * this saves us a host call when just deleting the file/dir.
     */
    uint32_t const  fRemove = pVPoxFobx->Info.Attr.fMode & RTFS_DOS_DIRECTORY ? SHFL_REMOVE_DIR : SHFL_REMOVE_FILE;
    NTSTATUS        Status;
    int             vrc;
    if (g_uSfLastFunction >= SHFL_FN_CLOSE_AND_REMOVE)
    {
        size_t const cbReq = RT_UOFFSETOF(VPOXSFCLOSEANDREMOVEREQ, StrPath.String) + (cwcRemainingName + 1) * sizeof(RTUTF16);
        VPOXSFCLOSEANDREMOVEREQ *pReq = (VPOXSFCLOSEANDREMOVEREQ *)VbglR0PhysHeapAlloc((uint32_t)cbReq);
        if (pReq)
            RT_ZERO(*pReq);
        else
            return STATUS_INSUFFICIENT_RESOURCES;

        memcpy(&pReq->StrPath.String, pRemainingName->Buffer, cwcRemainingName * sizeof(RTUTF16));
        pReq->StrPath.String.utf16[cwcRemainingName] = '\0';
        pReq->StrPath.u16Length = cwcRemainingName * 2;
        pReq->StrPath.u16Size   = cwcRemainingName * 2 + (uint16_t)sizeof(RTUTF16);
        vrc = VbglR0SfHostReqCloseAndRemove(pNetRootExtension->map.root, pReq, fRemove, pVPoxFobx->hFile);
        pVPoxFobx->hFile = SHFL_HANDLE_NIL;

        VbglR0PhysHeapFree(pReq);
    }
    else
    {
        /*
         * We allocate a single request buffer for the closing and deletion to save time.
         */
        AssertCompile(sizeof(VPOXSFCLOSEREQ) <= sizeof(VPOXSFREMOVEREQ));
        AssertReturn((cwcRemainingName + 1) * sizeof(RTUTF16) < _64K, STATUS_NAME_TOO_LONG);
        size_t cbReq = RT_UOFFSETOF(VPOXSFREMOVEREQ, StrPath.String) + (cwcRemainingName + 1) * sizeof(RTUTF16);
        union MyCloseAndRemoveReq
        {
            VPOXSFCLOSEREQ  Close;
            VPOXSFREMOVEREQ Remove;
        } *pReq = (union MyCloseAndRemoveReq *)VbglR0PhysHeapAlloc((uint32_t)cbReq);
        if (pReq)
            RT_ZERO(*pReq);
        else
            return STATUS_INSUFFICIENT_RESOURCES;

        /*
         * Close file first if not already done.  We dont use vbsfNtCloseFileHandle here
         * as we got our own request buffer and have no need to update any file info.
         */
        if (pVPoxFobx->hFile != SHFL_HANDLE_NIL)
        {
            int vrcClose = VbglR0SfHostReqClose(pNetRootExtension->map.root, &pReq->Close, pVPoxFobx->hFile);
            pVPoxFobx->hFile = SHFL_HANDLE_NIL;
            if (RT_FAILURE(vrcClose))
                Log(("VPOXSF: vbsfNtRemove: Closing the handle failed! vrcClose %Rrc, hFile %#RX64 (probably)\n",
                     vrcClose, pReq->Close.Parms.u64Handle.u.value64));
        }

        /*
         * Try remove the file.
         */
        uint16_t const cwcToCopy = pRemainingName->Length / sizeof(WCHAR);
        AssertMsgReturnStmt(cwcToCopy == cwcRemainingName,
                            ("%#x, was %#x; FCB exclusivity: %d\n", cwcToCopy, cwcRemainingName, RxIsFcbAcquiredExclusive(capFcb)),
                            VbglR0PhysHeapFree(pReq), STATUS_INTERNAL_ERROR);
        memcpy(&pReq->Remove.StrPath.String, pRemainingName->Buffer, cwcToCopy * sizeof(RTUTF16));
        pReq->Remove.StrPath.String.utf16[cwcToCopy] = '\0';
        pReq->Remove.StrPath.u16Length = cwcToCopy * 2;
        pReq->Remove.StrPath.u16Size   = cwcToCopy * 2 + (uint16_t)sizeof(RTUTF16);
        vrc = VbglR0SfHostReqRemove(pNetRootExtension->map.root, &pReq->Remove, fRemove);

        VbglR0PhysHeapFree(pReq);
    }

    if (RT_SUCCESS(vrc))
    {
        SetFlag(capFobx->pSrvOpen->Flags, SRVOPEN_FLAG_FILE_DELETED);
        vbsfNtCleanupFcbxTimestampRefsOnClose(pVPoxFobx, VPoxMRxGetFcbExtension(capFcb));
        Status = STATUS_SUCCESS;
    }
    else
    {
        Log(("VPOXSF: vbsfNtRemove: %s failed with %Rrc\n",
             g_uSfLastFunction >= SHFL_FN_CLOSE_AND_REMOVE ? "VbglR0SfHostReqCloseAndRemove" : "VbglR0SfHostReqRemove", vrc));
        Status = vbsfNtVPoxStatusToNt(vrc);
    }

    Log(("VPOXSF: vbsfNtRemove: Returned %#010X (%Rrc)\n", Status, vrc));
    return Status;
}

NTSTATUS VPoxMRxShouldTryToCollapseThisOpen(IN PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VPOXSF: MRxShouldTryToCollapseThisOpen\n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS VPoxMRxCollapseOpen(IN OUT PRX_CONTEXT RxContext)
{
    RT_NOREF(RxContext);
    Log(("VPOXSF: MRxCollapseOpen\n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}
