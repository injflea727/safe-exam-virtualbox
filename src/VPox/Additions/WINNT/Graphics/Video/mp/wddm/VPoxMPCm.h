/* $Id: VPoxMPCm.h $ */
/** @file
 * VPox WDDM Miniport driver
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPCm_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPCm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

typedef struct VPOXVIDEOCM_MGR
{
    KSPIN_LOCK SynchLock;
    /* session list */
    LIST_ENTRY SessionList;
} VPOXVIDEOCM_MGR, *PVPOXVIDEOCM_MGR;

typedef struct VPOXVIDEOCM_CTX
{
    LIST_ENTRY SessionEntry;
    struct VPOXVIDEOCM_SESSION *pSession;
    uint64_t u64UmData;
    VPOXWDDM_HTABLE AllocTable;
} VPOXVIDEOCM_CTX, *PVPOXVIDEOCM_CTX;

void vpoxVideoCmCtxInitEmpty(PVPOXVIDEOCM_CTX pContext);

NTSTATUS vpoxVideoCmCtxAdd(PVPOXVIDEOCM_MGR pMgr, PVPOXVIDEOCM_CTX pContext, HANDLE hUmEvent, uint64_t u64UmData);
NTSTATUS vpoxVideoCmCtxRemove(PVPOXVIDEOCM_MGR pMgr, PVPOXVIDEOCM_CTX pContext);
NTSTATUS vpoxVideoCmInit(PVPOXVIDEOCM_MGR pMgr);
NTSTATUS vpoxVideoCmTerm(PVPOXVIDEOCM_MGR pMgr);
NTSTATUS vpoxVideoCmSignalEvents(PVPOXVIDEOCM_MGR pMgr);

NTSTATUS vpoxVideoCmCmdSubmitCompleteEvent(PVPOXVIDEOCM_CTX pContext, PKEVENT pEvent);
void* vpoxVideoCmCmdCreate(PVPOXVIDEOCM_CTX pContext, uint32_t cbSize);
void* vpoxVideoCmCmdReinitForContext(void *pvCmd, PVPOXVIDEOCM_CTX pContext);
void vpoxVideoCmCmdRetain(void *pvCmd);
void vpoxVideoCmCmdRelease(void *pvCmd);
#define VPOXVIDEOCM_SUBMITSIZE_DEFAULT (~0UL)
void vpoxVideoCmCmdSubmit(void *pvCmd, uint32_t cbSize);

#define VPOXVIDEOCMCMDVISITOR_RETURN_BREAK    0x00000001
#define VPOXVIDEOCMCMDVISITOR_RETURN_RMCMD    0x00000002
typedef DECLCALLBACK(UINT) FNVPOXVIDEOCMCMDVISITOR(PVPOXVIDEOCM_CTX pContext, PVOID pvCmd, uint32_t cbCmd, PVOID pvVisitor);
typedef FNVPOXVIDEOCMCMDVISITOR *PFNVPOXVIDEOCMCMDVISITOR;
NTSTATUS vpoxVideoCmCmdVisit(PVPOXVIDEOCM_CTX pContext, BOOLEAN bEntireSession, PFNVPOXVIDEOCMCMDVISITOR pfnVisitor, PVOID pvVisitor);

NTSTATUS vpoxVideoCmEscape(PVPOXVIDEOCM_CTX pContext, PVPOXDISPIFESCAPE_GETVPOXVIDEOCMCMD pCmd, uint32_t cbCmd);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VPoxMPCm_h */
