#!/bin/sh
# $Id: prerm-common.sh $
## @file
# Oracle VM VirtualPox
# VirtualPox Linux pre-uninstaller common portions
#

#
# Copyright (C) 2015-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# Put bits of the pre-uninstallation here which should work the same for all of
# the Linux installers.  We do not use special helpers (e.g. dh_* on Debian),
# but that should not matter, as we know what those helpers actually do, and we
# have to work on those systems anyway when installed using the all
# distributions installer.
#
# We assume that all required files are in the same folder as this script
# (e.g. /opt/VirtualPox, /usr/lib/VirtualPox, the build output directory).
#
# Script exit status: 0 on success, 1 if VirtualPox is running and can not be
# stopped (installers may show an error themselves or just pass on standard
# error).


# The below is GNU-specific.  See VPox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_PATH="${TARGET%/[!/]*}"
cd "${MY_PATH}"
. "./routines.sh"

# Stop the ballon control service
stop_init_script vpoxballoonctrl-service >/dev/null 2>&1
# Stop the autostart service
stop_init_script vpoxautostart-service >/dev/null 2>&1
# Stop the web service
stop_init_script vpoxweb-service >/dev/null 2>&1
# Do this check here after we terminated the web service: check whether VPoxSVC
# is running and exit if it can't be stopped.
check_running
# Terminate VPoxNetDHCP if running
terminate_proc VPoxNetDHCP
# Terminate VPoxNetNAT if running
terminate_proc VPoxNetNAT
delrunlevel vpoxballoonctrl-service
remove_init_script vpoxballoonctrl-service
delrunlevel vpoxautostart-service
remove_init_script vpoxautostart-service
delrunlevel vpoxweb-service
remove_init_script vpoxweb-service
# Stop kernel module and uninstall runlevel script
stop_init_script vpoxdrv >/dev/null 2>&1
delrunlevel vpoxdrv
remove_init_script vpoxdrv
# And do final clean-up
"${MY_PATH}/vpoxdrv.sh" cleanup >/dev/null  # Do not silence errors for now
# Stop host networking and uninstall runlevel script (obsolete)
stop_init_script vpoxnet >/dev/null 2>&1
delrunlevel vpoxnet >/dev/null 2>&1
remove_init_script vpoxnet >/dev/null 2>&1
finish_init_script_install
rm -f /sbin/vpoxconfig
exit 0
