/* $Id: vpoxvfs_vnops.c $ */
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
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/limits.h>
#include <sys/lockf.h>
#include <sys/stat.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

/*
 * Prototypes for VPOXVFS vnode operations
 */
static vop_create_t     vpoxvfs_create;
static vop_mknod_t      vpoxvfs_mknod;
static vop_open_t       vpoxvfs_open;
static vop_close_t      vpoxvfs_close;
static vop_access_t     vpoxvfs_access;
static vop_getattr_t    vpoxvfs_getattr;
static vop_setattr_t    vpoxvfs_setattr;
static vop_read_t       vpoxvfs_read;
static vop_write_t      vpoxvfs_write;
static vop_fsync_t      vpoxvfs_fsync;
static vop_remove_t     vpoxvfs_remove;
static vop_link_t       vpoxvfs_link;
static vop_lookup_t     vpoxvfs_lookup;
static vop_rename_t     vpoxvfs_rename;
static vop_mkdir_t      vpoxvfs_mkdir;
static vop_rmdir_t      vpoxvfs_rmdir;
static vop_symlink_t    vpoxvfs_symlink;
static vop_readdir_t    vpoxvfs_readdir;
static vop_strategy_t   vpoxvfs_strategy;
static vop_print_t      vpoxvfs_print;
static vop_pathconf_t   vpoxvfs_pathconf;
static vop_advlock_t    vpoxvfs_advlock;
static vop_getextattr_t vpoxvfs_getextattr;
static vop_ioctl_t      vpoxvfs_ioctl;
static vop_getpages_t   vpoxvfs_getpages;
static vop_inactive_t   vpoxvfs_inactive;
static vop_putpages_t   vpoxvfs_putpages;
static vop_reclaim_t    vpoxvfs_reclaim;

struct vop_vector vpoxvfs_vnodeops = {
    .vop_default    =   &default_vnodeops,

    .vop_access     =   vpoxvfs_access,
    .vop_advlock    =   vpoxvfs_advlock,
    .vop_close      =   vpoxvfs_close,
    .vop_create     =   vpoxvfs_create,
    .vop_fsync      =   vpoxvfs_fsync,
    .vop_getattr    =   vpoxvfs_getattr,
    .vop_getextattr =   vpoxvfs_getextattr,
    .vop_getpages   =   vpoxvfs_getpages,
    .vop_inactive   =   vpoxvfs_inactive,
    .vop_ioctl      =   vpoxvfs_ioctl,
    .vop_link       =   vpoxvfs_link,
    .vop_lookup     =   vpoxvfs_lookup,
    .vop_mkdir      =   vpoxvfs_mkdir,
    .vop_mknod      =   vpoxvfs_mknod,
    .vop_open       =   vpoxvfs_open,
    .vop_pathconf   =   vpoxvfs_pathconf,
    .vop_print      =   vpoxvfs_print,
    .vop_putpages   =   vpoxvfs_putpages,
    .vop_read       =   vpoxvfs_read,
    .vop_readdir    =   vpoxvfs_readdir,
    .vop_reclaim    =   vpoxvfs_reclaim,
    .vop_remove     =   vpoxvfs_remove,
    .vop_rename     =   vpoxvfs_rename,
    .vop_rmdir      =   vpoxvfs_rmdir,
    .vop_setattr    =   vpoxvfs_setattr,
    .vop_strategy   =   vpoxvfs_strategy,
    .vop_symlink    =   vpoxvfs_symlink,
    .vop_write      =   vpoxvfs_write,
};

static int vpoxvfs_access(struct vop_access_args *ap)
{
    return 0;
}

static int vpoxvfs_open(struct vop_open_args *ap)
{
    return 0;
}

static int vpoxvfs_close(struct vop_close_args *ap)
{
    return 0;
}

static int vpoxvfs_getattr(struct vop_getattr_args *ap)
{
    return 0;
}

static int vpoxvfs_setattr(struct vop_setattr_args *ap)
{
    return 0;
}

static int vpoxvfs_read(struct vop_read_args *ap)
{
    return 0;
}

static int vpoxvfs_write(struct vop_write_args *ap)
{
    return 0;
}

static int vpoxvfs_create(struct vop_create_args *ap)
{
    return 0;
}

static int vpoxvfs_remove(struct vop_remove_args *ap)
{
    return 0;
}

static int vpoxvfs_rename(struct vop_rename_args *ap)
{
    return 0;
}

static int vpoxvfs_link(struct vop_link_args *ap)
{
    return EOPNOTSUPP;
}

static int vpoxvfs_symlink(struct vop_symlink_args *ap)
{
    return EOPNOTSUPP;
}

static int vpoxvfs_mknod(struct vop_mknod_args *ap)
{
    return EOPNOTSUPP;
}

static int vpoxvfs_mkdir(struct vop_mkdir_args *ap)
{
    return 0;
}

static int vpoxvfs_rmdir(struct vop_rmdir_args *ap)
{
    return 0;
}

static int vpoxvfs_readdir(struct vop_readdir_args *ap)
{
    return 0;
}

static int vpoxvfs_fsync(struct vop_fsync_args *ap)
{
    return 0;
}

static int vpoxvfs_print (struct vop_print_args *ap)
{
    return 0;
}

static int vpoxvfs_pathconf (struct vop_pathconf_args *ap)
{
    return 0;
}

static int vpoxvfs_strategy (struct vop_strategy_args *ap)
{
    return 0;
}

static int vpoxvfs_ioctl(struct vop_ioctl_args *ap)
{
    return ENOTTY;
}

static int vpoxvfs_getextattr(struct vop_getextattr_args *ap)
{
    return 0;
}

static int vpoxvfs_advlock(struct vop_advlock_args *ap)
{
    return 0;
}

static int vpoxvfs_lookup(struct vop_lookup_args *ap)
{
    return 0;
}

static int vpoxvfs_inactive(struct vop_inactive_args *ap)
{
    return 0;
}

static int vpoxvfs_reclaim(struct vop_reclaim_args *ap)
{
    return 0;
}

static int vpoxvfs_getpages(struct vop_getpages_args *ap)
{
    return 0;
}

static int vpoxvfs_putpages(struct vop_putpages_args *ap)
{
    return 0;
}

