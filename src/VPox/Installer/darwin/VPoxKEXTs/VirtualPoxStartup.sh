#!/bin/sh
# $Id: VirtualPoxStartup.sh $
## @file
# Startup service for loading the kernel extensions and select the set of VPox
# binaries that matches the kernel architecture.
#

#
# Copyright (C) 2007-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

if false; then
    . /etc/rc.common
else
    # Fake the startup item functions we're using.

    ConsoleMessage()
    {
        if [ "$1" != "-f" ]; then
            echo "$@"
        else
            shift
            echo "Fatal error: $@"
            exit 1;
        fi
    }

    RunService()
    {
        case "$1" in
            "start")
                StartService
                exit $?;
                ;;
            "stop")
                StopService
                exit $?;
                ;;
            "restart")
                RestartService
                exit $?;
                ;;
            "launchd")
                if RestartService; then
                    while true;
                    do
                        sleep 3600
                    done
                fi
                exit $?;
                ;;
             **)
                echo "Error: Unknown action '$1'"
                exit 1;
        esac
    }
fi


StartService()
{
    VPOX_RC=0
    VPOXDRV="VPoxDrv"
    VPOXUSB="VPoxUSB"
    MACOS_VERSION_MAJOR=$(sw_vers -productVersion | /usr/bin/sed -e 's/^\([0-9]*\).*$/\1/')

    #
    # Check that all the directories exist first.
    #
    if [ ! -d "/Library/Application Support/VirtualPox/${VPOXDRV}.kext" ]; then
        ConsoleMessage "Error: /Library/Application Support/VirtualPox/${VPOXDRV}.kext is missing"
        VPOX_RC=1
    fi
    if [ ! -d "/Library/Application Support/VirtualPox/${VPOXUSB}.kext" ]; then
        ConsoleMessage "Error: /Library/Application Support/VirtualPox/${VPOXUSB}.kext is missing"
        VPOX_RC=1
    fi
    if [ ! -d "/Library/Application Support/VirtualPox/VPoxNetFlt.kext" ]; then
        ConsoleMessage "Error: /Library/Application Support/VirtualPox/VPoxNetFlt.kext is missing"
        VPOX_RC=1
    fi
    if [ ! -d "/Library/Application Support/VirtualPox/VPoxNetAdp.kext" ]; then
        ConsoleMessage "Error: /Library/Application Support/VirtualPox/VPoxNetAdp.kext is missing"
        VPOX_RC=1
    fi

    #
    # Check that no drivers are currently running.
    # (Try stop the service if this is the case.)
    #
    if [ $VPOX_RC -eq 0 ]; then
        if [[ ${MACOS_VERSION_MAJOR} -lt 11 ]]; then
            if kextstat -lb org.virtualpox.kext.VPoxDrv 2>&1 | grep -q org.virtualpox.kext.VPoxDrv; then
                ConsoleMessage "Error: ${VPOXDRV}.kext is already loaded"
                VPOX_RC=1
            fi
            if kextstat -lb org.virtualpox.kext.VPoxUSB 2>&1 | grep -q org.virtualpox.kext.VPoxUSB; then
                ConsoleMessage "Error: ${VPOXUSB}.kext is already loaded"
                VPOX_RC=1
            fi
            if kextstat -lb org.virtualpox.kext.VPoxNetFlt 2>&1 | grep -q org.virtualpox.kext.VPoxNetFlt; then
                ConsoleMessage "Error: VPoxNetFlt.kext is already loaded"
                VPOX_RC=1
            fi
            if kextstat -lb org.virtualpox.kext.VPoxNetAdp 2>&1 | grep -q org.virtualpox.kext.VPoxNetAdp; then
                ConsoleMessage "Error: VPoxNetAdp.kext is already loaded"
                VPOX_RC=1
            fi
        else
            #
            # Use kmutil directly on BigSur or grep will erroneously trigger because kextstat dumps the kmutil
            # invocation to stdout...
            #
            if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxDrv 2>&1 | grep -q org.virtualpox.kext.VPoxDrv; then
                ConsoleMessage "Error: ${VPOXDRV}.kext is already loaded"
                VPOX_RC=1
            fi
            if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxUSB 2>&1 | grep -q org.virtualpox.kext.VPoxUSB; then
                ConsoleMessage "Error: ${VPOXUSB}.kext is already loaded"
                VPOX_RC=1
            fi
            if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxNetFlt 2>&1 | grep -q org.virtualpox.kext.VPoxNetFlt; then
                ConsoleMessage "Error: VPoxNetFlt.kext is already loaded"
                VPOX_RC=1
            fi
            if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxNetAdp 2>&1 | grep -q org.virtualpox.kext.VPoxNetAdp; then
                ConsoleMessage "Error: VPoxNetAdp.kext is already loaded"
                VPOX_RC=1
            fi
        fi
    fi

    #
    # Load the drivers.
    #
    if [ $VPOX_RC -eq 0 ]; then
        if [[ ${MACOS_VERSION_MAJOR} -lt 11 ]]; then
            ConsoleMessage "Loading ${VPOXDRV}.kext"
            if ! kextload "/Library/Application Support/VirtualPox/${VPOXDRV}.kext"; then
                ConsoleMessage "Error: Failed to load /Library/Application Support/VirtualPox/${VPOXDRV}.kext"
                VPOX_RC=1
            fi

            ConsoleMessage "Loading ${VPOXUSB}.kext"
            if ! kextload -d "/Library/Application Support/VirtualPox/${VPOXDRV}.kext" "/Library/Application Support/VirtualPox/${VPOXUSB}.kext"; then
                ConsoleMessage "Error: Failed to load /Library/Application Support/VirtualPox/${VPOXUSB}.kext"
                VPOX_RC=1
            fi

            ConsoleMessage "Loading VPoxNetFlt.kext"
            if ! kextload -d "/Library/Application Support/VirtualPox/${VPOXDRV}.kext" "/Library/Application Support/VirtualPox/VPoxNetFlt.kext"; then
                ConsoleMessage "Error: Failed to load /Library/Application Support/VirtualPox/VPoxNetFlt.kext"
                VPOX_RC=1
            fi

            ConsoleMessage "Loading VPoxNetAdp.kext"
            if ! kextload -d "/Library/Application Support/VirtualPox/${VPOXDRV}.kext" "/Library/Application Support/VirtualPox/VPoxNetAdp.kext"; then
                ConsoleMessage "Error: Failed to load /Library/Application Support/VirtualPox/VPoxNetAdp.kext"
                VPOX_RC=1
            fi
        else
            #
            # On BigSur we can only load by bundle ID because the drivers are baked into a kext collection image
            # and the real path is never loaded actually.
            #
            ConsoleMessage "Loading ${VPOXDRV}.kext"
            if ! kmutil load -b org.virtualpox.kext.VPoxDrv; then
                ConsoleMessage "Error: Failed to load org.virtualpox.kext.VPoxDrv"
                VPOX_RC=1
            fi

            ConsoleMessage "Loading ${VPOXUSB}.kext"
            if ! kmutil load -b org.virtualpox.kext.VPoxUSB; then
                ConsoleMessage "Error: Failed to load org.virtualpox.kext.VPoxUSB"
                VPOX_RC=1
            fi

            ConsoleMessage "Loading VPoxNetFlt.kext"
            if ! kmutil load -b org.virtualpox.kext.VPoxNetFlt; then
                ConsoleMessage "Error: Failed to load org.virtualpox.kext.VPoxNetFlt"
                VPOX_RC=1
            fi

            ConsoleMessage "Loading VPoxNetAdp.kext"
            if ! kmutil load -b org.virtualpox.kext.VPoxNetAdp; then
                ConsoleMessage "Error: Failed to load org.virtualpox.kext.VPoxNetAdp"
                VPOX_RC=1
            fi
        fi

        if [ $VPOX_RC -ne 0 ]; then
            # unload the drivers (ignoring failures)
            kextunload -b org.virtualpox.kext.VPoxNetAdp
            kextunload -b org.virtualpox.kext.VPoxNetFlt
            kextunload -b org.virtualpox.kext.VPoxUSB
            kextunload -b org.virtualpox.kext.VPoxDrv
        fi
    fi

    #
    # Set the error on failure.
    #
    if [ "$VPOX_RC" -ne "0" ]; then
        ConsoleMessage -f VirtualPox
        exit $VPOX_RC
    fi
}


StopService()
{
    VPOX_RC=0
    VPOXDRV="VPoxDrv"
    VPOXUSB="VPoxUSB"
    MACOS_VERSION_MAJOR=$(sw_vers -productVersion | /usr/bin/sed -e 's/^\([0-9]*\).*$/\1/')

    if [[ ${MACOS_VERSION_MAJOR} -lt 11 ]]; then
        if kextstat -lb org.virtualpox.kext.VPoxUSB 2>&1 | grep -q org.virtualpox.kext.VPoxUSB; then
            ConsoleMessage "Unloading ${VPOXUSB}.kext"
            if ! kextunload -m org.virtualpox.kext.VPoxUSB; then
                ConsoleMessage "Error: Failed to unload VPoxUSB.kext"
                VPOX_RC=1
            fi
        fi

        if kextstat -lb org.virtualpox.kext.VPoxNetFlt 2>&1 | grep -q org.virtualpox.kext.VPoxNetFlt; then
            ConsoleMessage "Unloading VPoxNetFlt.kext"
            if ! kextunload -m org.virtualpox.kext.VPoxNetFlt; then
                ConsoleMessage "Error: Failed to unload VPoxNetFlt.kext"
                VPOX_RC=1
            fi
        fi

        if kextstat -lb org.virtualpox.kext.VPoxNetAdp 2>&1 | grep -q org.virtualpox.kext.VPoxNetAdp; then
            ConsoleMessage "Unloading VPoxNetAdp.kext"
            if ! kextunload -m org.virtualpox.kext.VPoxNetAdp; then
                ConsoleMessage "Error: Failed to unload VPoxNetAdp.kext"
                VPOX_RC=1
            fi
        fi

        # This must come last because of dependencies.
        if kextstat -lb org.virtualpox.kext.VPoxDrv 2>&1 | grep -q org.virtualpox.kext.VPoxDrv; then
            ConsoleMessage "Unloading ${VPOXDRV}.kext"
            if ! kextunload -m org.virtualpox.kext.VPoxDrv; then
                ConsoleMessage "Error: Failed to unload VPoxDrv.kext"
                VPOX_RC=1
            fi
        fi
    else
        if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxUSB 2>&1 | grep -q org.virtualpox.kext.VPoxUSB; then
            ConsoleMessage "Unloading ${VPOXUSB}.kext"
            if ! kmutil unload -b org.virtualpox.kext.VPoxUSB; then
                ConsoleMessage "Error: Failed to unload VPoxUSB.kext"
                VPOX_RC=1
            fi
        fi

        if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxNetFlt 2>&1 | grep -q org.virtualpox.kext.VPoxNetFlt; then
            ConsoleMessage "Unloading VPoxNetFlt.kext"
            if ! kmutil unload -b org.virtualpox.kext.VPoxNetFlt; then
                ConsoleMessage "Error: Failed to unload VPoxNetFlt.kext"
                VPOX_RC=1
            fi
        fi

        if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxNetAdp 2>&1 | grep -q org.virtualpox.kext.VPoxNetAdp; then
            ConsoleMessage "Unloading VPoxNetAdp.kext"
            if ! kmutil unload -b org.virtualpox.kext.VPoxNetAdp; then
                ConsoleMessage "Error: Failed to unload VPoxNetAdp.kext"
                VPOX_RC=1
            fi
        fi

        # This must come last because of dependencies.
        if kmutil showloaded --list-only -b org.virtualpox.kext.VPoxDrv 2>&1 | grep -q org.virtualpox.kext.VPoxDrv; then
            ConsoleMessage "Unloading ${VPOXDRV}.kext"
            if ! kmutil unload -b org.virtualpox.kext.VPoxDrv; then
                ConsoleMessage "Error: Failed to unload VPoxDrv.kext"
                VPOX_RC=1
            fi
        fi
    fi

    # Set the error on failure.
    if [ "$VPOX_RC" -ne "0" ]; then
        ConsoleMessage -f VirtualPox
        exit $VPOX_RC
    fi
}


RestartService()
{
    StopService
    StartService
}


RunService "$1"

