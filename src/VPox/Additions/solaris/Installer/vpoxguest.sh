#!/bin/sh
# $Id: vpoxguest.sh $
## @file
# VirtualPox Guest Additions kernel module control script for Solaris.
#

#
# Copyright (C) 2008-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualPox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

LC_ALL=C
export LC_ALL

LANG=C
export LANG

SILENTUNLOAD=""
MODNAME="vpoxguest"
VFSMODNAME="vpoxfs"
VMSMODNAME="vpoxms"
MODDIR32="/usr/kernel/drv"
MODDIR64="/usr/kernel/drv/amd64"
VFSDIR32="/usr/kernel/fs"
VFSDIR64="/usr/kernel/fs/amd64"

abort()
{
    echo 1>&2 "## $1"
    exit 1
}

info()
{
    echo 1>&2 "$1"
}

check_if_installed()
{
    cputype=`isainfo -k`
    modulepath="$MODDIR32/$MODNAME"
    if test "$cputype" = "amd64"; then
        modulepath="$MODDIR64/$MODNAME"
    fi
    if test -f "$modulepath"; then
        return 0
    fi
    abort "VirtualPox kernel module ($MODNAME) NOT installed."
}

module_loaded()
{
    if test -z "$1"; then
        abort "missing argument to module_loaded()"
    fi

    modname=$1
    # modinfo should now work properly since we prevent module autounloading.
    loadentry=`/usr/sbin/modinfo | grep "$modname "`
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

vpoxguest_loaded()
{
    module_loaded $MODNAME
    return $?
}

vpoxfs_loaded()
{
    module_loaded $VFSMODNAME
    return $?
}

vpoxms_loaded()
{
    module_loaded $VMSMODNAME
    return $?
}

check_root()
{
    # the reason we don't use "-u" is that some versions of id are old and do not
    # support this option (eg. Solaris 10) and do not have a "--version" to check it either
    # so go with the uglier but more generic approach
    idbin=`which id`
    isroot=`$idbin | grep "uid=0"`
    if test -z "$isroot"; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

start_module()
{
    /usr/sbin/add_drv -i'pci90ee,cafe' -m'* 0666 root sys' $MODNAME
    if test ! vpoxguest_loaded; then
        abort "Failed to load VirtualPox guest kernel module."
    elif test -c "/devices/pci@0,0/pci90ee,cafe@4:$MODNAME"; then
        info "VirtualPox guest kernel module loaded."
    else
        info "VirtualPox guest kernel module failed to attach."
    fi
}

stop_module()
{
    if vpoxguest_loaded; then
        /usr/sbin/rem_drv $MODNAME || abort "Failed to unload VirtualPox guest kernel module."
        info "VirtualPox guest kernel module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualPox guest kernel module not loaded."
    fi
}

start_vpoxfs()
{
    if vpoxfs_loaded; then
        info "VirtualPox FileSystem kernel module already loaded."
    else
        /usr/sbin/modload -p fs/$VFSMODNAME || abort "Failed to load VirtualPox FileSystem kernel module."
        if test ! vpoxfs_loaded; then
            info "Failed to load VirtualPox FileSystem kernel module."
        else
            info "VirtualPox FileSystem kernel module loaded."
        fi
    fi
}

stop_vpoxfs()
{
    if vpoxfs_loaded; then
        vpoxfs_mod_id=`/usr/sbin/modinfo | grep $VFSMODNAME | cut -f 1 -d ' ' `
        if test -n "$vpoxfs_mod_id"; then
            /usr/sbin/modunload -i $vpoxfs_mod_id || abort "Failed to unload VirtualPox FileSystem module."
            info "VirtualPox FileSystem kernel module unloaded."
        fi
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualPox FileSystem kernel module not loaded."
    fi
}

start_vpoxms()
{
    /usr/sbin/add_drv -m'* 0666 root sys' $VMSMODNAME
    if test ! vpoxms_loaded; then
        abort "Failed to load VirtualPox pointer integration module."
    elif test -c "/devices/pseudo/$VMSMODNAME@0:$VMSMODNAME"; then
        info "VirtualPox pointer integration module loaded."
    else
        info "VirtualPox pointer integration module failed to attach."
    fi
}

stop_vpoxms()
{
    if vpoxms_loaded; then
        /usr/sbin/rem_drv $VMSMODNAME || abort "Failed to unload VirtualPox pointer integration module."
        info "VirtualPox pointer integration module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualPox pointer integration module not loaded."
    fi
}

status_module()
{
    if vpoxguest_loaded; then
        info "Running."
    else
        info "Stopped."
    fi
}

stop_all()
{
    stop_vpoxms
    stop_vpoxfs
    stop_module
    return 0
}

restart_all()
{
    stop_all
    start_module
    start_vpoxfs
    start_vpoxms
    return 0
}

check_root
check_if_installed

if test "$2" = "silentunload"; then
    SILENTUNLOAD="$2"
fi

case "$1" in
stopall)
    stop_all
    ;;
restartall)
    restart_all
    ;;
start)
    start_module
    start_vpoxms
    ;;
stop)
    stop_vpoxms
    stop_module
    ;;
status)
    status_module
    ;;
vfsstart)
    start_vpoxfs
    ;;
vfsstop)
    stop_vpoxfs
    ;;
vmsstart)
    start_vpoxms
    ;;
vmsstop)
    stop_vpoxms
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit 0

