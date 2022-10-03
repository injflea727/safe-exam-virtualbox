#!/bin/sh
## @file
# Oracle VM VirtualPox startup script, Linux hosts.
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

PATH="/usr/bin:/bin:/usr/sbin:/sbin"

# The below is GNU-specific.  See slightly further down for a version which
# works on Solaris and OS X.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_DIR="${TARGET%/[!/]*}"

# (
#     path="${0}"
#     while test -n "${path}"; do
#         # Make sure we have at least one slash and no leading dash.
#         expr "${path}" : / > /dev/null || path="./${path}"
#         # Filter out bad characters in the path name.
#         expr "${path}" : ".*[*?<>\\]" > /dev/null && exit 1
#         # Catch embedded new-lines and non-existing (or path-relative) files.
#         # $0 should always be absolute when scripts are invoked through "#!".
#         test "`ls -l -d "${path}" 2> /dev/null | wc -l`" -eq 1 || exit 1
#         # Change to the folder containing the file to resolve relative links.
#         folder=`expr "${path}" : "\(.*/\)[^/][^/]*/*$"` || exit 1
#         path=`expr "x\`ls -l -d "${path}"\`" : "[^>]* -> \(.*\)"`
#         cd "${folder}"
#         # If the last path was not a link then we are in the target folder.
#         test -n "${path}" || pwd
#     done
# )

if test -f /usr/lib/virtualpox/VirtualPox &&
    test -x /usr/lib/virtualpox/VirtualPox; then
    INSTALL_DIR=/usr/lib/virtualpox
elif test -f "${MY_DIR}/VirtualPox" && test -x "${MY_DIR}/VirtualPox"; then
    INSTALL_DIR="${MY_DIR}"
else
    echo "Could not find VirtualPox installation. Please reinstall."
    exit 1
fi

# Note: This script must not fail if the module was not successfully installed
#       because the user might not want to run a VM but only change VM params!

if [ "$1" = "shutdown" ]; then
    SHUTDOWN="true"
elif ! lsmod|grep -q vpoxdrv; then
    cat << EOF
WARNING: The vpoxdrv kernel module is not loaded. Either there is no module
         available for the current kernel (`uname -r`) or it failed to
         load. Please recompile the kernel module and install it by

           sudo /sbin/vpoxconfig

         You will not be able to start VMs until this problem is fixed.
EOF
elif [ ! -c /dev/vpoxdrv ]; then
    cat << EOF
WARNING: The character device /dev/vpoxdrv does not exist. Try

           sudo /sbin/vpoxconfig

         and if that is not successful, try to re-install the package.

         You will not be able to start VMs until this problem is fixed.
EOF
fi

if [ -f /etc/vpox/module_not_compiled ]; then
    cat << EOF
WARNING: The compilation of the vpoxdrv.ko kernel module failed during the
         installation for some reason. Starting a VM will not be possible.
         Please consult the User Manual for build instructions.
EOF
fi

SERVER_PID=`ps -U \`whoami\` | grep VPoxSVC | awk '{ print $1 }'`
if [ -z "$SERVER_PID" ]; then
    # Server not running yet/anymore, cleanup socket path.
    # See IPC_GetDefaultSocketPath()!
    if [ -n "$LOGNAME" ]; then
        rm -rf /tmp/.vpox-$LOGNAME-ipc > /dev/null 2>&1
    else
        rm -rf /tmp/.vpox-$USER-ipc > /dev/null 2>&1
    fi
fi

if [ "$SHUTDOWN" = "true" ]; then
    if [ -n "$SERVER_PID" ]; then
        kill -TERM $SERVER_PID
        sleep 2
    fi
    exit 0
fi

APP=`basename $0`
case "$APP" in
    VirtualPox|virtualpox)
        exec "$INSTALL_DIR/VirtualPox" "$@"
        ;;
    VirtualPoxVM|virtualpoxvm)
        exec "$INSTALL_DIR/VirtualPoxVM" "$@"
        ;;
    VPoxManage|vpoxmanage)
        exec "$INSTALL_DIR/VPoxManage" "$@"
        ;;
    VPoxSDL|vpoxsdl)
        exec "$INSTALL_DIR/VPoxSDL" "$@"
        ;;
    VPoxVRDP|VPoxHeadless|vpoxheadless)
        exec "$INSTALL_DIR/VPoxHeadless" "$@"
        ;;
    VPoxAutostart|vpoxautostart)
        exec "$INSTALL_DIR/VPoxAutostart" "$@"
        ;;
    VPoxBalloonCtrl|vpoxballoonctrl)
        exec "$INSTALL_DIR/VPoxBalloonCtrl" "$@"
        ;;
    VPoxBugReport|vpoxbugreport)
        exec "$INSTALL_DIR/VPoxBugReport" "$@"
        ;;
    VPoxDTrace|vpoxdtrace)
        exec "$INSTALL_DIR/VPoxDTrace" "$@"
        ;;
    vpoxwebsrv)
        exec "$INSTALL_DIR/vpoxwebsrv" "$@"
        ;;
    *)
        echo "Unknown application - $APP"
        exit 1
        ;;
esac
exit 0
