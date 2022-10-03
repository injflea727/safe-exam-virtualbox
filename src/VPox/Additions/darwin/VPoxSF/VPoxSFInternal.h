/* $Id: VPoxSFInternal.h $ */
/** @file
 * VPoxSF - Darwin Shared Folders, internal header.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_darwin_VPoxSF_VPoxSFInternal_h
#define GA_INCLUDED_SRC_darwin_VPoxSF_VPoxSFInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VPoxSFMount.h"

#include <libkern/libkern.h>
#include <iprt/types.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <mach/mach_port.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>
#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <vfs/vfs_support.h>
#undef PVM

#include <iprt/mem.h>
#include <VPox/VPoxGuest.h>
#include <VPox/VPoxGuestLibSharedFolders.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Private data we associate with a mount.
 */
typedef struct VPOXSFMNTDATA
{
    /** The shared folder mapping */
    VBGLSFMAP           hHostFolder;
    /** The root VNode. */
    vnode_t             pVnRoot;
    /** User that mounted shared folder (anyone but root?). */
    uid_t               uidMounter;
    /** The mount info from the mount() call. */
    VPOXSFDRWNMOUNTINFO MntInfo;
} VPOXSFMNTDATA;
/** Pointer to private mount data.  */
typedef VPOXSFMNTDATA *PVPOXSFMNTDATA;

/**
 * Private data we associate with a VNode.
 */
typedef struct VPOXSFDWNVNDATA
{
    /** The handle to the host object.  */
    SHFLHANDLE      hHandle;
    ///PSHFLSTRING     pPath;                  /** Path within shared folder */
    ///lck_attr_t     *pLockAttr;              /** BSD locking stuff */
    ///lck_rw_t       *pLock;                  /** BSD locking stuff */
} VPOXSFDWNVNDATA;
/** Pointer to private vnode data. */
typedef VPOXSFDWNVNDATA *PVPOXSFDWNVNDATA;



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern VBGLSFCLIENT         g_SfClientDarwin;
extern uint32_t volatile    g_cVPoxSfMounts;
extern struct vfsops        g_VPoxSfVfsOps;
extern struct vnodeopv_desc g_VPoxSfVnodeOpvDesc;
extern int (**g_papfnVPoxSfDwnVnDirOpsVector)(void *);



/*********************************************************************************************************************************
*   Functions                                                                                                                    *
*********************************************************************************************************************************/
bool    vpoxSfDwnConnect(void);
vnode_t vpoxSfDwnVnAlloc(mount_t pMount, enum vtype enmType, vnode_t pParent, uint64_t cbFile);


#endif /* !GA_INCLUDED_SRC_darwin_VPoxSF_VPoxSFInternal_h */

