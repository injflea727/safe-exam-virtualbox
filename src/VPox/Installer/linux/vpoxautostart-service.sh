#!/bin/sh
# $Id: vpoxautostart-service.sh $
## @file
# VirtualPox autostart service init script.
#

#
# Copyright (C) 2012-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# chkconfig: 345 35 65
# description: VirtualPox autostart service
#
### BEGIN INIT INFO
# Provides:       vpoxautostart-service
# Required-Start: vpoxdrv
# Required-Stop:  vpoxdrv
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualPox autostart service
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
SCRIPTNAME=vpoxautostart-service.sh

[ -f /etc/debian_release -a -f /lib/lsb/init-functions ] || NOLSB=yes
[ -f /etc/vpox/vpox.cfg ] && . /etc/vpox/vpox.cfg

if [ -n "$INSTALL_DIR" ]; then
    binary="$INSTALL_DIR/VPoxAutostart"
else
    binary="/usr/lib/virtualpox/VPoxAutostart"
fi

# silently exit if the package was uninstalled but not purged,
# applies to Debian packages only (but shouldn't hurt elsewhere)
[ ! -f /etc/debian_release -o -x $binary ] || exit 0

[ -r /etc/default/virtualpox ] && . /etc/default/virtualpox

# Preamble for Gentoo
if [ "`which $0`" = "/sbin/rc" ]; then
    shift
fi

begin_msg()
{
    test -n "${2}" && echo "${SCRIPTNAME}: ${1}."
    logger -t "${SCRIPTNAME}" "${1}."
}

succ_msg()
{
    logger -t "${SCRIPTNAME}" "${1}."
}

fail_msg()
{
    echo "${SCRIPTNAME}: failed: ${1}." >&2
    logger -t "${SCRIPTNAME}" "failed: ${1}."
}

start_daemon() {
    usr="$1"
    shift
    su - $usr -c "$*"
}

if which start-stop-daemon >/dev/null 2>&1; then
    start_daemon() {
        usr="$1"
        shift
        bin="$1"
        shift
        start-stop-daemon --chuid $usr --start --exec $bin -- $@
    }
fi

vpoxdrvrunning() {
    lsmod | grep -q "vpoxdrv[^_-]"
}

valid_db_entry() {

    entry="$1"
    [ -z "$entry" ] && return 1

    user="$2"
    [ -z "$user" ] && return 1

    user_name=$(id -n -u "$user" 2>/dev/null)
    [ -z "$user_name" ] && return 1

    user_id=$(id -u "$user" 2>/dev/null)

    # Verify that @user identifies a user *by name* (i.e. not a numeric id).
    # Careful, all numeric user names are legal.
    if [ "$user_id" = "$user" ] && [ "$user_name" != "$user" ]; then
        return 1
    fi

    # Verify whether file name is the same as file owner name.
    [ -z "$(find "$entry" -user "$user" -type f 2>/dev/null)" ] && return 1

    return 0
}

start() {
    [ -z "$VPOXAUTOSTART_DB" ] && exit 0
    [ -z "$VPOXAUTOSTART_CONFIG" ] && exit 0
    begin_msg "Starting VirtualPox VMs configured for autostart" console;
    vpoxdrvrunning || {
        fail_msg "VirtualPox kernel module not loaded!"
        exit 0
    }
    PARAMS="--background --start --config $VPOXAUTOSTART_CONFIG"

    # prevent inheriting this setting to VPoxSVC
    unset VPOX_RELEASE_LOG_DEST

    for entry in "$VPOXAUTOSTART_DB"/*.start
    do
        user=$(basename "$entry" .start)
        [ "$user" = "*" ] && break
        valid_db_entry "$entry" "$user" || continue

        start_daemon "$user" "$binary" $PARAMS > /dev/null 2>&1
    done

    return $RETVAL
}

stop() {
    [ -z "$VPOXAUTOSTART_DB" ] && exit 0
    [ -z "$VPOXAUTOSTART_CONFIG" ] && exit 0

    PARAMS="--stop --config $VPOXAUTOSTART_CONFIG"

    # prevent inheriting this setting to VPoxSVC
    unset VPOX_RELEASE_LOG_DEST

    for entry in "$VPOXAUTOSTART_DB"/*.stop
    do
        user=$(basename "$entry" .stop)
        [ "$user" = "*" ] && break
        valid_db_entry "$entry" "$user" || continue

        start_daemon "$user" "$binary" $PARAMS > /dev/null 2>&1
    done

    return $RETVAL
}

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
*)
    echo "Usage: $0 {start|stop}"
    exit 1
esac

exit $RETVAL
