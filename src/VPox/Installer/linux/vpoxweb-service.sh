#!/bin/sh
# $Id: vpoxweb-service.sh $
## @file
# VirtualPox web service API daemon init script.
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
# description: VirtualPox web service API
#
### BEGIN INIT INFO
# Provides:       vpoxweb-service
# Required-Start: vpoxdrv
# Required-Stop:  vpoxdrv
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualPox web service API
# X-Required-Target-Start: network-online
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
SCRIPTNAME=vpoxweb-service.sh

[ -f /etc/vpox/vpox.cfg ] && . /etc/vpox/vpox.cfg

if [ -n "$INSTALL_DIR" ]; then
    binary="$INSTALL_DIR/vpoxwebsrv"
    vpoxmanage="$INSTALL_DIR/VPoxManage"
else
    binary="/usr/lib/virtualpox/vpoxwebsrv"
    vpoxmanage="/usr/lib/virtualpox/VPoxManage"
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
        fail_msg "VPOXWEB_USER must not contain multiple users!"
        exit 1
    fi
}

start() {
    if ! test -f $PIDFILE; then
        [ -z "$VPOXWEB_USER" ] && exit 0
        begin_msg "Starting VirtualPox web service" console;
        check_single_user $VPOXWEB_USER
        vpoxdrvrunning || {
            fail_msg "VirtualPox kernel module not loaded!"
            exit 0
        }
        PARAMS="--background"
        [ -n "$VPOXWEB_HOST" ]           && PARAMS="$PARAMS -H $VPOXWEB_HOST"
        [ -n "$VPOXWEB_PORT" ]           && PARAMS="$PARAMS -p $VPOXWEB_PORT"
        [ -n "$VPOXWEB_SSL_KEYFILE" ]    && PARAMS="$PARAMS -s -K $VPOXWEB_SSL_KEYFILE"
        [ -n "$VPOXWEB_SSL_PASSWORDFILE" ] && PARAMS="$PARAMS -a $VPOXWEB_SSL_PASSWORDFILE"
        [ -n "$VPOXWEB_SSL_CACERT" ]     && PARAMS="$PARAMS -c $VPOXWEB_SSL_CACERT"
        [ -n "$VPOXWEB_SSL_CAPATH" ]     && PARAMS="$PARAMS -C $VPOXWEB_SSL_CAPATH"
        [ -n "$VPOXWEB_SSL_DHFILE" ]     && PARAMS="$PARAMS -D $VPOXWEB_SSL_DHFILE"
        [ -n "$VPOXWEB_SSL_RANDFILE" ]   && PARAMS="$PARAMS -r $VPOXWEB_SSL_RANDFILE"
        [ -n "$VPOXWEB_TIMEOUT" ]        && PARAMS="$PARAMS -t $VPOXWEB_TIMEOUT"
        [ -n "$VPOXWEB_CHECK_INTERVAL" ] && PARAMS="$PARAMS -i $VPOXWEB_CHECK_INTERVAL"
        [ -n "$VPOXWEB_THREADS" ]        && PARAMS="$PARAMS -T $VPOXWEB_THREADS"
        [ -n "$VPOXWEB_KEEPALIVE" ]      && PARAMS="$PARAMS -k $VPOXWEB_KEEPALIVE"
        [ -n "$VPOXWEB_AUTHENTICATION" ] && PARAMS="$PARAMS -A $VPOXWEB_AUTHENTICATION"
        [ -n "$VPOXWEB_LOGFILE" ]        && PARAMS="$PARAMS -F $VPOXWEB_LOGFILE"
        [ -n "$VPOXWEB_ROTATE" ]         && PARAMS="$PARAMS -R $VPOXWEB_ROTATE"
        [ -n "$VPOXWEB_LOGSIZE" ]        && PARAMS="$PARAMS -S $VPOXWEB_LOGSIZE"
        [ -n "$VPOXWEB_LOGINTERVAL" ]    && PARAMS="$PARAMS -I $VPOXWEB_LOGINTERVAL"
        # set authentication method + password hash
        if [ -n "$VPOXWEB_AUTH_LIBRARY" ]; then
            su - "$VPOXWEB_USER" -c "$vpoxmanage setproperty websrvauthlibrary \"$VPOXWEB_AUTH_LIBRARY\""
            if [ $? -ne 0 ]; then
                fail_msg "Error $? setting webservice authentication library to $VPOXWEB_AUTH_LIBRARY"
            fi
        fi
        if [ -n "$VPOXWEB_AUTH_PWHASH" ]; then
            su - "$VPOXWEB_USER" -c "$vpoxmanage setextradata global \"VPoxAuthSimple/users/$VPOXWEB_USER\" \"$VPOXWEB_AUTH_PWHASH\""
            if [ $? -ne 0 ]; then
                fail_msg "Error $? setting webservice password hash"
            fi
        fi
        # prevent inheriting this setting to VPoxSVC
        unset VPOX_RELEASE_LOG_DEST
        start_daemon $VPOXWEB_USER $binary $PARAMS > /dev/null 2>&1
        # ugly: wait until the final process has forked
        sleep .1
        PID=`pidof $binary 2>/dev/null`
        if [ -n "$PID" ]; then
            echo "$PID" > $PIDFILE
            RETVAL=0
            succ_msg "VirtualPox web service started"
        else
            RETVAL=1
            fail_msg "VirtualPox web service failed to start"
        fi
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin_msg "Stopping VirtualPox web service" console;
        killproc $binary
        RETVAL=$?
        # Be careful: wait 1 second, making sure that everything is cleaned up.
        sleep 1
        if ! pidof $binary > /dev/null 2>&1; then
            rm -f $PIDFILE
            succ_msg "VirtualPox web service stopped"
        else
            fail_msg "VirtualPox web service failed to stop"
        fi
    fi
    return $RETVAL
}

restart() {
    stop && start
}

status() {
    echo -n "Checking for VPox Web Service"
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
