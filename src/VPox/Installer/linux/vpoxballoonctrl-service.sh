#!/bin/sh
# $Id: vpoxballoonctrl-service.sh $
## @file
# VirtualPox watchdog daemon init script.
#

#
# Copyright (C) 2006-2020 Oracle Corporation
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
# description: VirtualPox watchdog daemon
#
### BEGIN INIT INFO
# Provides:       vpoxballoonctrl-service
# Required-Start: vpoxdrv
# Required-Stop:  vpoxdrv
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualPox watchdog daemon
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
SCRIPTNAME=vpoxballoonctrl-service.sh

[ -f /etc/vpox/vpox.cfg ] && . /etc/vpox/vpox.cfg

if [ -n "$INSTALL_DIR" ]; then
    binary="$INSTALL_DIR/VPoxBalloonCtrl"
else
    binary="/usr/lib/virtualpox/VPoxBalloonCtrl"
fi

# silently exit if the package was uninstalled but not purged,
# applies to Debian packages only (but shouldn't hurt elsewhere)
[ ! -f /etc/debian_release -o -x $binary ] || exit 0

[ -r /etc/default/virtualpox ] && . /etc/default/virtualpox

PIDFILE="/var/run/${SCRIPTNAME}"

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

killproc() {
    killall $1
    rm -f $PIDFILE
}

if which start-stop-daemon >/dev/null 2>&1; then
    start_daemon() {
        usr="$1"
        shift
        bin="$1"
        shift
        start-stop-daemon --background --chuid $usr --start --exec $bin -- $@
    }

    killproc() {
        start-stop-daemon --stop --exec $@
    }
fi

vpoxdrvrunning() {
    lsmod | grep -q "vpoxdrv[^_-]"
}

check_single_user() {
    if [ -n "$2" ]; then
        fail_msg "VPOXWATCHDOG_USER must not contain multiple users!"
        exit 1
    fi
}

start() {
    if ! test -f $PIDFILE; then
        [ -z "$VPOXWATCHDOG_USER" -a -z "$VPOXBALLOONCTRL_USER" ] && exit 0
        [ -z "$VPOXWATCHDOG_USER" ] && VPOXWATCHDOG_USER="$VPOXBALLOONCTRL_USER"
        begin_msg "Starting VirtualPox watchdog service" console;
        check_single_user $VPOXWATCHDOG_USER
        vpoxdrvrunning || {
            fail_msg "VirtualPox kernel module not loaded!"
            exit 0
        }
        # Handle legacy parameters, do not add any further ones unless absolutely necessary.
        [ -z "$VPOXWATCHDOG_BALLOON_INTERVAL" -a -n "$VPOXBALLOONCTRL_INTERVAL" ]           && VPOXWATCHDOG_BALLOON_INTERVAL="$VPOXBALLOONCTRL_INTERVAL"
        [ -z "$VPOXWATCHDOG_BALLOON_INCREMENT" -a -n "$VPOXBALLOONCTRL_INCREMENT" ]         && VPOXWATCHDOG_BALLOON_INCREMENT="$VPOXBALLOONCTRL_INCREMENT"
        [ -z "$VPOXWATCHDOG_BALLOON_DECREMENT" -a -n "$VPOXBALLOONCTRL_DECREMENT" ]         && VPOXWATCHDOG_BALLOON_DECREMENT="$VPOXBALLOONCTRL_DECREMENT"
        [ -z "$VPOXWATCHDOG_BALLOON_LOWERLIMIT" -a -n "$VPOXBALLOONCTRL_LOWERLIMIT" ]       && VPOXWATCHDOG_BALLOON_LOWERLIMIT="$VPOXBALLOONCTRL_LOWERLIMIT"
        [ -z "$VPOXWATCHDOG_BALLOON_SAFETYMARGIN" -a -n "$VPOXBALLOONCTRL_SAFETYMARGIN" ]   && VPOXWATCHDOG_BALLOON_SAFETYMARGIN="$VPOXBALLOONCTRL_SAFETYMARGIN"
        [ -z "$VPOXWATCHDOG_ROTATE" -a -n "$VPOXBALLOONCTRL_ROTATE" ]           && VPOXWATCHDOG_ROTATE="$VPOXBALLOONCTRL_ROTATE"
        [ -z "$VPOXWATCHDOG_LOGSIZE" -a -n "$VPOXBALLOONCTRL_LOGSIZE" ]         && VPOXWATCHDOG_LOGSIZE="$VPOXBALLOONCTRL_LOGSIZE"
        [ -z "$VPOXWATCHDOG_LOGINTERVAL" -a -n "$VPOXBALLOONCTRL_LOGINTERVAL" ] && VPOXWATCHDOG_LOGINTERVAL="$VPOXBALLOONCTRL_LOGINTERVAL"

        PARAMS="--background"
        [ -n "$VPOXWATCHDOG_BALLOON_INTERVAL" ]     && PARAMS="$PARAMS --balloon-interval \"$VPOXWATCHDOG_BALLOON_INTERVAL\""
        [ -n "$VPOXWATCHDOG_BALLOON_INCREMENT" ]    && PARAMS="$PARAMS --balloon-inc \"$VPOXWATCHDOG_BALLOON_INCREMENT\""
        [ -n "$VPOXWATCHDOG_BALLOON_DECREMENT" ]    && PARAMS="$PARAMS --balloon-dec \"$VPOXWATCHDOG_BALLOON_DECREMENT\""
        [ -n "$VPOXWATCHDOG_BALLOON_LOWERLIMIT" ]   && PARAMS="$PARAMS --balloon-lower-limit \"$VPOXWATCHDOG_BALLOON_LOWERLIMIT\""
        [ -n "$VPOXWATCHDOG_BALLOON_SAFETYMARGIN" ] && PARAMS="$PARAMS --balloon-safety-margin \"$VPOXWATCHDOG_BALLOON_SAFETYMARGIN\""
        [ -n "$VPOXWATCHDOG_ROTATE" ]       && PARAMS="$PARAMS -R \"$VPOXWATCHDOG_ROTATE\""
        [ -n "$VPOXWATCHDOG_LOGSIZE" ]      && PARAMS="$PARAMS -S \"$VPOXWATCHDOG_LOGSIZE\""
        [ -n "$VPOXWATCHDOG_LOGINTERVAL" ]  && PARAMS="$PARAMS -I \"$VPOXWATCHDOG_LOGINTERVAL\""
        # prevent inheriting this setting to VPoxSVC
        unset VPOX_RELEASE_LOG_DEST
        start_daemon $VPOXWATCHDOG_USER $binary $PARAMS > /dev/null 2>&1
        # ugly: wait until the final process has forked
        sleep .1
        PID=`pidof $binary 2>/dev/null`
        if [ -n "$PID" ]; then
            echo "$PID" > $PIDFILE
            RETVAL=0
            succ_msg "VirtualPox watchdog service started"
        else
            RETVAL=1
            fail_msg "VirtualPox watchdog service failed to start"
        fi
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin_msg "Stopping VirtualPox watchdog service" console;
        killproc $binary
        RETVAL=$?
        if ! pidof $binary > /dev/null 2>&1; then
            rm -f $PIDFILE
            succ_msg "VirtualPox watchdog service stopped"
        else
            fail_msg "VirtualPox watchdog service failed to stop"
        fi
    fi
    return $RETVAL
}

restart() {
    stop && start
}

status() {
    echo -n "Checking for VPox watchdog service"
    if [ -f $PIDFILE ]; then
        echo " ...running"
    else
        echo " ...not running"
    fi
}

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
force-reload)
    restart
    ;;
status)
    status
    ;;
setup)
    ;;
cleanup)
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit $RETVAL
