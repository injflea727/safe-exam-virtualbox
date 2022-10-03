/* $Id: vpoxfs.h $ */
/** @file
 * VirtualPox File System Driver for Solaris Guests, Internal Header.
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

#ifndef GA_INCLUDED_SRC_solaris_SharedFolders_vpoxfs_h
#define GA_INCLUDED_SRC_solaris_SharedFolders_vpoxfs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HOST_NAME   256
#define MAX_NLS_NAME    32
/** Default stat cache ttl (in ms) */
#define DEF_STAT_TTL_MS 200

/** The module name. */
#define DEVICE_NAME              "vpoxfs"

#ifdef _KERNEL

#include <VPox/VPoxGuestLibSharedFolders.h>
#include <sys/vfs.h>

/** VNode for VPoxVFS */
typedef struct vpoxvfs_vnode
{
    vnode_t     *pVNode;
    vattr_t     Attr;
    SHFLSTRING  *pPath;
    kmutex_t    MtxContents;
} vpoxvfs_vnode_t;


/** Per-file system mount instance data. */
typedef struct vpoxvfs_globinfo
{
    VBGLSFMAP       Map;
    int             Ttl;
    int             Uid;
    int             Gid;
    vfs_t           *pVFS;
    vpoxvfs_vnode_t *pVNodeRoot;
    kmutex_t        MtxFS;
} vpoxvfs_globinfo_t;

extern struct vnodeops *g_pVPoxVFS_vnodeops;
extern const fs_operation_def_t g_VPoxVFS_vnodeops_template[];
extern VBGLSFCLIENT g_VPoxVFSClient;

/** Helper functions */
extern int vpoxvfs_Stat(const char *pszCaller, vpoxvfs_globinfo_t *pVPoxVFSGlobalInfo, SHFLSTRING *pPath,
                        PSHFLFSOBJINFO pResult, boolean_t fAllowFailure);
extern void vpoxvfs_InitVNode(vpoxvfs_globinfo_t *pVPoxVFSGlobalInfo, vpoxvfs_vnode_t *pVPoxVNode,
                              PSHFLFSOBJINFO pFSInfo);


/** Helper macros */
#define VFS_TO_VPOXVFS(vfs)      ((vpoxvfs_globinfo_t *)((vfs)->vfs_data))
#define VPOXVFS_TO_VFS(vpoxvfs)  ((vpoxvfs)->pVFS)
#define VN_TO_VPOXVN(vnode)      ((vpoxvfs_vnode_t *)((vnode)->v_data))
#define VPOXVN_TO_VN(vpoxvnode)  ((vpoxvnode)->pVNode)

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* !GA_INCLUDED_SRC_solaris_SharedFolders_vpoxfs_h */

