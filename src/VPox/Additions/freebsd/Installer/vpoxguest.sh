#!/bin/bash
# $Id: vpoxguest.sh $
## @file
# VirtualPox Guest Additions kernel module control script for FreeBSD.
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

VPOXGUESTFILE=""
SILENTUNLOAD=""

abort()
{
    echo 1>&2 "$1"
    exit 1
}

info()
{
    echo 1>&2 "$1"
}

get_module_path()
{
    moduledir="/boot/kernel";
    modulepath=$moduledir/vpoxguest.ko
    if test -f "$modulepath"; then
        VPOXGUESTFILE="$modulepath"
    else
        VPOXGUESTFILE=""
    fi
}

check_if_installed()
{
    if test "$VPOXGUESTFILE" -a -f "$VPOXGUESTFILE"; then
        return 0
    fi
    abort "VirtualPox kernel module (vpoxguest) not installed."
}

module_loaded()
{
    loadentry=`kldstat | grep vpoxguest`
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

check_root()
{
    if test `id -u` -ne 0; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

start()
{
    if module_loaded; then
        info "vpoxguest already loaded..."
    else
        /sbin/kldload vpoxguest.ko
        if ! module_loaded; then
            abort "Failed to load vpoxguest."
        elif test -c "/dev/vpoxguest"; then
            info "Loaded vpoxguest."
        else
            stop
            abort "Aborting due to attach failure."
        fi
    fi
}

stop()
{
    if module_loaded; then
        /sbin/kldunload vpoxguest.ko
        info "Unloaded vpoxguest."
    elif test -z "$SILENTUNLOAD"; then
        info "vpoxguest not loaded."
    fi
}

restart()
{
    stop
    sync
    start
    return 0
}

status()
{
    if module_loaded; then
        info "vpoxguest running."
    else
        info "vpoxguest stopped."
    fi
}

check_root
get_module_path
check_if_installed

if test "$2" = "silentunload"; then
    SILENTUNLOAD="$2"
fi

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
restart)
    restart
    ;;
status)
    status
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit

