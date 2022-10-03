#!/bin/sh
## @file
# Oracle VM VirtualPox startup script, Solaris hosts.
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

CURRENT_ISA=`isainfo -k`
if test "$CURRENT_ISA" = "amd64"; then
    INSTALL_DIR="/opt/VirtualPox/amd64"
else
    INSTALL_DIR="/opt/VirtualPox/i386"
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
    VPoxBugReport|vpoxbugreport)
        exec "$INSTALL_DIR/VPoxBugReport" "$@"
        ;;
    VPoxBalloonCtrl|vpoxballoonctrl)
        exec "$INSTALL_DIR/VPoxBalloonCtrl" "$@"
        ;;
    VPoxAutostart|vpoxautostart)
        exec "$INSTALL_DIR/VPoxAutostart" "$@"
        ;;
    VPoxDTrace|vpoxdtrace)
        exec "$INSTALL_DIR/VPoxDTrace" "$@"
        ;;
    vpoxwebsrv)
        exec "$INSTALL_DIR/vpoxwebsrv" "$@"
        ;;
    VPoxQtconfig)
        exec "$INSTALL_DIR/VPoxQtconfig" "$@"
        ;;
    vpox-img)
        exec "$INSTALL_DIR/vpox-img" "$0"
        ;;
    *)
        echo "Unknown application - $APP"
        exit 1
        ;;
esac
exit 0

