#!/sbin/sh
# $Id: smf-vpoxautostart.sh $

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

#
# smf-vpoxautostart method
#
# Argument is the method name (start, stop, ...)

. /lib/svc/share/smf_include.sh

VW_OPT="$1"
VW_EXIT=0

case $VW_OPT in
    start)
        if [ ! -f /opt/VirtualPox/VPoxAutostart ]; then
            echo "ERROR: /opt/VirtualPox/VPoxAutostart does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        if [ ! -x /opt/VirtualPox/VPoxAutostart ]; then
            echo "ERROR: /opt/VirtualPox/VPoxAutostart is not exectuable."
            return $SMF_EXIT_ERR_CONFIG
        fi

        # Get svc configuration
        VW_CONFIG=`/usr/bin/svcprop -p config/config $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_CONFIG=
        VW_ROTATE=`/usr/bin/svcprop -p config/logrotate $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_ROTATE=
        VW_LOGSIZE=`/usr/bin/svcprop -p config/logsize $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_LOGSIZE=
        VW_LOGINTERVAL=`/usr/bin/svcprop -p config/loginterval $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_LOGINTERVAL=
        VW_VPOXGROUP=`/usr/bin/svcprop -p config/vpoxgroup $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_VPOXGROUP=

        # Provide sensible defaults
        [ -z "$VW_CONFIG" ] && VW_CONFIG=/etc/vpox/autostart.cfg
        [ -z "$VW_ROTATE" ] && VW_ROTATE=10
        [ -z "$VW_LOGSIZE" ] && VW_LOGSIZE=104857600
        [ -z "$VW_LOGINTERVAL" ] && VW_LOGINTERVAL=86400
        [ -z "$VW_VPOXGROUP" ] && VW_VPOXGROUP=staff

        # Get all users
        for VW_USER in `logins -g $VW_VPOXGROUP | cut -d' ' -f1`
        do
            su - "$VW_USER" -c "/opt/VirtualPox/VPoxAutostart --background --start --config \"$VW_CONFIG\" --logrotate \"$VW_ROTATE\" --logsize \"$VW_LOGSIZE\" --loginterval \"$VW_LOGINTERVAL\""

            VW_EXIT=$?
            if [ $VW_EXIT != 0 ]; then
                echo "VPoxAutostart failed with $VW_EXIT."
                VW_EXIT=1
                break
            fi
        done
    ;;
    stop)
        if [ ! -f /opt/VirtualPox/VPoxAutostart ]; then
            echo "ERROR: /opt/VirtualPox/VPoxAutostart does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        if [ ! -x /opt/VirtualPox/VPoxAutostart ]; then
            echo "ERROR: /opt/VirtualPox/VPoxAutostart is not executable."
            return $SMF_EXIT_ERR_CONFIG
        fi

        # Get svc configuration
        VW_CONFIG=`/usr/bin/svcprop -p config/config $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_CONFIG=
        VW_ROTATE=`/usr/bin/svcprop -p config/logrotate $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_ROTATE=
        VW_LOGSIZE=`/usr/bin/svcprop -p config/logsize $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_LOGSIZE=
        VW_LOGINTERVAL=`/usr/bin/svcprop -p config/loginterval $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_LOGINTERVAL=
        VW_VPOXGROUP=`/usr/bin/svcprop -p config/vpoxgroup $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_VPOXGROUP=

        # Provide sensible defaults
        [ -z "$VW_CONFIG" ] && VW_CONFIG=/etc/vpox/autostart.cfg
        [ -z "$VW_ROTATE" ] && VW_ROTATE=10
        [ -z "$VW_LOGSIZE" ] && VW_LOGSIZE=104857600
        [ -z "$VW_LOGINTERVAL" ] && VW_LOGINTERVAL=86400
        [ -z "$VW_VPOXGROUP" ] && VW_VPOXGROUP=staff

        # Get all users
        for VW_USER in `logins -g $VW_VPOXGROUP | cut -d' ' -f1`
        do
            su - "$VW_USER" -c "/opt/VirtualPox/VPoxAutostart --stop --config \"$VW_CONFIG\" --logrotate \"$VW_ROTATE\" --logsize \"$VW_LOGSIZE\" --loginterval \"$VW_LOGINTERVAL\""

            VW_EXIT=$?
            if [ $VW_EXIT != 0 ]; then
                echo "VPoxAutostart failed with $VW_EXIT."
                VW_EXIT=1
                break
            fi
        done
    ;;
    *)
        VW_EXIT=$SMF_EXIT_ERR_CONFIG
    ;;
esac

exit $VW_EXIT
