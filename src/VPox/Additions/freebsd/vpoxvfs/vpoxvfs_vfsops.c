/* $Id: vpoxvfs_vfsops.c $ */
/** @file
 * Description.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vpoxvfs.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <iprt/mem.h>

#define VFSMP2SFGLOBINFO(mp) ((struct sf_glob_info *)mp->mnt_data)

static int vpoxvfs_version = VPOXVFS_VERSION;

SYSCTL_NODE(_vfs, OID_AUTO, vpoxvfs, CTLFLAG_RW, 0, "VirtualPox shared filesystem");
SYSCTL_INT(_vfs_vpoxvfs, OID_AUTO, version, CTLFLAG_RD, &vpoxvfs_version, 0, "");

/* global connection to the host service. */
static VBGLSFCLIENT g_vpoxSFClient;

static vfs_init_t       vpoxvfs_init;
static vfs_uninit_t     vpoxvfs_uninit;
static vfs_cmount_t     vpoxvfs_cmount;
static vfs_mount_t      vpoxvfs_mount;
static vfs_root_t       vpoxvfs_root;
static vfs_quotactl_t   vpoxvfs_quotactl;
static vfs_statfs_t     vpoxvfs_statfs;
static vfs_unmount_t    vpoxvfs_unmount;

static struct vfsops vpoxvfs_vfsops = {
    .vfs_init     =    vpoxvfs_init,
    .vfs_cmount   =    vpoxvfs_cmount,
    .vfs_mount    =    vpoxvfs_mount,
    .vfs_quotactl =    vpoxvfs_quotactl,
    .vfs_root     =    vpoxvfs_root,
    .vfs_statfs   =    vpoxvfs_statfs,
    .vfs_sync     =    vfs_stdsync,
    .vfs_uninit   =    vpoxvfs_uninit,
    .vfs_unmount  =    vpoxvfs_unmount,
};


VFS_SET(vpoxvfs_vfsops, vpoxvfs, VFCF_NETWORK);
MODULE_DEPEND(vpoxvfs, vpoxguest, 1, 1, 1);

static int vpoxvfs_cmount(struct mntarg *ma, void * data, int flags, struct thread *td)
{
    struct vpoxvfs_mount_info args;
    int rc = 0;

    printf("%s: Enter\n", __FUNCTION__);

    rc = copyin(data, &args, sizeof(struct vpoxvfs_mount_info));
    if (rc)
        return rc;

    ma = mount_argf(ma, "uid", "%d", args.uid);
    ma = mount_argf(ma, "gid", "%d", args.gid);
    ma = mount_arg(ma, "from", args.name, -1);

    rc = kernel_mount(ma, flags);

    printf("%s: Leave rc=%d\n", __FUNCTION__, rc);

    return rc;
}

static const char *vpoxvfs_opts[] = {
    "uid", "gid", "from", "fstype", "fspath", "errmsg", NULL
};

static int vpoxvfs_mount(struct mount *mp, struct thread *td)
{
    int rc;
    char *pszShare;
    int  cbShare, cbOption;
    int uid = 0, gid = 0;
    struct sf_glob_info *pShFlGlobalInfo;
    SHFLSTRING *pShFlShareName = NULL;
    int cbShFlShareName;

    printf("%s: Enter\n", __FUNCTION__);

    if (mp->mnt_flag & (MNT_UPDATE | MNT_ROOTFS))
        return EOPNOTSUPP;

    if (vfs_filteropt(mp->mnt_optnew, vpoxvfs_opts))
    {
        vfs_mount_error(mp, "%s", "Invalid option");
        return EINVAL;
    }

    rc = vfs_getopt(mp->mnt_optnew, "from", (void **)&pszShare, &cbShare);
    if (rc || pszShare[cbShare-1] != '\0' || cbShare > 0xfffe)
        return EINVAL;

    rc = vfs_getopt(mp->mnt_optnew, "gid", (void **)&gid, &cbOption);
    if ((rc != ENOENT) && (rc || cbOption != sizeof(gid)))
        return EINVAL;

    rc = vfs_getopt(mp->mnt_optnew, "uid", (void **)&uid, &cbOption);
    if ((rc != ENOENT) && (rc || cbOption != sizeof(uid)))
        return EINVAL;

    pShFlGlobalInfo = RTMemAllocZ(sizeof(struct sf_glob_info));
    if (!pShFlGlobalInfo)
        return ENOMEM;

    cbShFlShareName = offsetof (SHFLSTRING, String.utf8) + cbShare + 1;
    pShFlShareName  = RTMemAllocZ(cbShFlShareName);
    if (!pShFlShareName)
        return VERR_NO_MEMORY;

    pShFlShareName->u16Length = cbShare;
    pShFlShareName->u16Size   = cbShare + 1;
    memcpy (pShFlShareName->String.utf8, pszShare, cbShare + 1);

    rc = VbglR0SfMapFolder (&g_vpoxSFClient, pShFlShareName, &pShFlGlobalInfo->map);
    RTMemFree(pShFlShareName);

    if (RT_FAILURE (rc))
    {
        RTMemFree(pShFlGlobalInfo);
        printf("VbglR0SfMapFolder failed rc=%d\n", rc);
        return EPROTO;
    }

    pShFlGlobalInfo->uid = uid;
    pShFlGlobalInfo->gid = gid;

    mp->mnt_data = pShFlGlobalInfo;

    /** @todo root vnode. */

    vfs_getnewfsid(mp);
    vfs_mountedfrom(mp, pszShare);

    printf("%s: Leave rc=0\n", __FUNCTION__);

    return 0;
}

static int vpoxvfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
    struct sf_glob_info *pShFlGlobalInfo = VFSMP2SFGLOBINFO(mp);
    int rc;
    int flags = 0;

    rc = VbglR0SfUnmapFolder(&g_vpoxSFClient, &pShFlGlobalInfo->map);
    if (RT_FAILURE(rc))
        printf("Failed to unmap shared folder\n");

    if (mntflags & MNT_FORCE)
        flags |= FORCECLOSE;

    /* There is 1 extra root vnode reference (vnode_root). */
    rc = vflush(mp, 1, flags, td);
    if (rc)
        return rc;


    RTMemFree(pShFlGlobalInfo);
    mp->mnt_data = NULL;

    return 0;
}

static int vpoxvfs_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
{
    int rc = 0;
    struct sf_glob_info *pShFlGlobalInfo = VFSMP2SFGLOBINFO(mp);
    struct vnode *vp;

    printf("%s: Enter\n", __FUNCTION__);

    vp = pShFlGlobalInfo->vnode_root;
    VREF(vp);

    vn_lock(vp, flags | LK_RETRY, td);
    *vpp = vp;

    printf("%s: Leave\n", __FUNCTION__);

    return rc;
}

static int vpoxvfs_quotactl(struct mount *mp, int cmd, uid_t uid, void *arg, struct thread *td)
{
    return EOPNOTSUPP;
}

int vpoxvfs_init(struct vfsconf *vfsp)
{
    int rc;

    /* Initialize the R0 guest library. */
    rc = VbglR0SfInit();
    if (RT_FAILURE(rc))
        return ENXIO;

    /* Connect to the host service. */
    rc = VbglR0SfConnect(&g_vpoxSFClient);
    if (RT_FAILURE(rc))
    {
        printf("Failed to get connection to host! rc=%d\n", rc);
        VbglR0SfTerm();
        return ENXIO;
    }

    rc = VbglR0SfSetUtf8(&g_vpoxSFClient);
    if (RT_FAILURE (rc))
    {
        printf("VbglR0SfSetUtf8 failed, rc=%d\n", rc);
        VbglR0SfDisconnect(&g_vpoxSFClient);
        VbglR0SfTerm();
        return EPROTO;
    }

    printf("Successfully loaded shared folder module\n");

    return 0;
}

int vpoxvfs_uninit(struct vfsconf *vfsp)
{
    VbglR0SfDisconnect(&g_vpoxSFClient);
    VbglR0SfTerm();

    return 0;
}

int vpoxvfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
    return 0;
}
