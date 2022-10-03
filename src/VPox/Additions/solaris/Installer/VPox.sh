#!/bin/sh
# $Id: VPox.sh $
## @file
# VirtualPox startup script for Solaris Guests Additions
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

CURRENT_ISA=`isainfo -k`
if test "$CURRENT_ISA" = "amd64"; then
    INSTALL_DIR="/opt/VirtualPoxAdditions/amd64"
else
    INSTALL_DIR="/opt/VirtualPoxAdditions"
fi

APP=`basename $0`
case "$APP" in
    VPoxClient)
        exec "$INSTALL_DIR/VPoxClient" "$@"
        ;;
    VPoxService)
        exec "$INSTALL_DIR/VPoxService" "$@"
        ;;
    VPoxControl)
        exec "$INSTALL_DIR/VPoxControl" "$@"
        ;;
    *)
        echo "Unknown application - $APP"
        exit 1
        ;;
esac
exit 0
