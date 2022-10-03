#!/sbin/sh
# $Id: smf-vpoxballoonctrl.sh $

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

#
# smf-vpoxballoonctrl method
#
# Argument is the method name (start, stop, ...)

. /lib/svc/share/smf_include.sh

VW_OPT="$1"
VW_EXIT=0

case $VW_OPT in
    start)
        if [ ! -f /opt/VirtualPox/VPoxBalloonCtrl ]; then
            echo "ERROR: /opt/VirtualPox/VPoxBalloonCtrl does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        if [ ! -x /opt/VirtualPox/VPoxBalloonCtrl ]; then
            echo "ERROR: /opt/VirtualPox/VPoxBalloonCtrl is not executable."
            return $SMF_EXIT_ERR_CONFIG
        fi

        # Get svc configuration
        VPOXWATCHDOG_USER=`/usr/bin/svcprop -p config/user $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_USER=
        VPOXWATCHDOG_BALLOON_INTERVAL=`/usr/bin/svcprop -p config/balloon_interval $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_BALLOON_INTERVAL=
        VPOXWATCHDOG_BALLOON_INCREMENT=`/usr/bin/svcprop -p config/balloon_increment $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_BALLOON_INCREMENT=
        VPOXWATCHDOG_BALLOON_DECREMENT=`/usr/bin/svcprop -p config/balloon_decrement $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_BALLOON_DECREMENT=
        VPOXWATCHDOG_BALLOON_LOWERLIMIT=`/usr/bin/svcprop -p config/balloon_lowerlimit $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_BALLOON_LOWERLIMIT=
        VPOXWATCHDOG_BALLOON_SAFETYMARGIN=`/usr/bin/svcprop -p config/balloon_safetymargin $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_BALLOON_SAFETYMARGIN=
        VPOXWATCHDOG_ROTATE=`/usr/bin/svcprop -p config/logrotate $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_ROTATE=
        VPOXWATCHDOG_LOGSIZE=`/usr/bin/svcprop -p config/logsize $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_LOGSIZE=
        VPOXWATCHDOG_LOGINTERVAL=`/usr/bin/svcprop -p config/loginterval $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VPOXWATCHDOG_LOGINTERVAL=

        # Handle legacy parameters, do not add any further ones unless absolutely necessary.
        if [ -z "$VPOXWATCHDOG_BALLOON_INTERVAL" ]; then
            VPOXWATCHDOG_BALLOON_INTERVAL=`/usr/bin/svcprop -p config/interval $SMF_FMRI 2>/dev/null`
            [ $? != 0 ] && VPOXWATCHDOG_BALLOON_INTERVAL=
        fi
        if [ -z "$VPOXWATCHDOG_BALLOON_INCREMENT" ]; then
            VPOXWATCHDOG_BALLOON_INCREMENT=`/usr/bin/svcprop -p config/increment $SMF_FMRI 2>/dev/null`
            [ $? != 0 ] && VPOXWATCHDOG_BALLOON_INCREMENT=
        fi
        if [ -z "$VPOXWATCHDOG_BALLOON_DECREMENT" ]; then
            VPOXWATCHDOG_BALLOON_DECREMENT=`/usr/bin/svcprop -p config/decrement $SMF_FMRI 2>/dev/null`
            [ $? != 0 ] && VPOXWATCHDOG_BALLOON_DECREMENT=
        fi
        if [ -z "$VPOXWATCHDOG_BALLOON_LOWERLIMIT" ]; then
            VPOXWATCHDOG_BALLOON_LOWERLIMIT=`/usr/bin/svcprop -p config/lowerlimit $SMF_FMRI 2>/dev/null`
            [ $? != 0 ] && VPOXWATCHDOG_BALLOON_LOWERLIMIT=
        fi
        if [ -z "$VPOXWATCHDOG_BALLOON_SAFETYMARGIN" ]; then
            VPOXWATCHDOG_BALLOON_SAFETYMARGIN=`/usr/bin/svcprop -p config/safetymargin $SMF_FMRI 2>/dev/null`
            [ $? != 0 ] && VPOXWATCHDOG_BALLOON_SAFETYMARGIN=
        fi

        # Provide sensible defaults
        [ -z "$VPOXWATCHDOG_USER" ] && VPOXWATCHDOG_USER=root

        # Assemble the parameter list
        PARAMS="--background"
        [ -n "$VPOXWATCHDOG_BALLOON_INTERVAL" ]     && PARAMS="$PARAMS --balloon-interval \"$VPOXWATCHDOG_BALLOON_INTERVAL\""
        [ -n "$VPOXWATCHDOG_BALLOON_INCREMENT" ]    && PARAMS="$PARAMS --balloon-inc \"$VPOXWATCHDOG_BALLOON_INCREMENT\""
        [ -n "$VPOXWATCHDOG_BALLOON_DECREMENT" ]    && PARAMS="$PARAMS --balloon-dec \"$VPOXWATCHDOG_BALLOON_DECREMENT\""
        [ -n "$VPOXWATCHDOG_BALLOON_LOWERLIMIT" ]   && PARAMS="$PARAMS --balloon-lower-limit \"$VPOXWATCHDOG_BALLOON_LOWERLIMIT\""
        [ -n "$VPOXWATCHDOG_BALLOON_SAFETYMARGIN" ] && PARAMS="$PARAMS --balloon-safety-margin \"$VPOXWATCHDOG_BALLOON_SAFETYMARGIN\""
        [ -n "$VPOXWATCHDOG_ROTATE" ]       && PARAMS="$PARAMS -R \"$VPOXWATCHDOG_ROTATE\""
        [ -n "$VPOXWATCHDOG_LOGSIZE" ]      && PARAMS="$PARAMS -S \"$VPOXWATCHDOG_LOGSIZE\""
        [ -n "$VPOXWATCHDOG_LOGINTERVAL" ]  && PARAMS="$PARAMS -I \"$VPOXWATCHDOG_LOGINTERVAL\""

        exec su - "$VPOXWATCHDOG_USER" -c "/opt/VirtualPox/VPoxBalloonCtrl $PARAMS"

        VW_EXIT=$?
        if [ $VW_EXIT != 0 ]; then
            echo "VPoxBalloonCtrl failed with $VW_EXIT."
            VW_EXIT=1
        fi
    ;;
    stop)
        # Kill service contract
        smf_kill_contract $2 TERM 1
    ;;
    *)
        VW_EXIT=$SMF_EXIT_ERR_CONFIG
    ;;
esac

exit $VW_EXIT
