#!/bin/sh
# $Id: postinst-common.sh $
## @file
# Oracle VM VirtualPox
# VirtualPox Linux post-installer common portions
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

# Put bits of the post-installation here which should work the same for all of
# the Linux installers.  We do not use special helpers (e.g. dh_* on Debian),
# but that should not matter, as we know what those helpers actually do, and we
# have to work on those systems anyway when installed using the all
# distributions installer.
#
# We assume that all required files are in the same folder as this script
# (e.g. /opt/VirtualPox, /usr/lib/VirtualPox, the build output directory).

# The below is GNU-specific.  See VPox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_PATH="${TARGET%/[!/]*}"
cd "${MY_PATH}"
. "./routines.sh"

START=true
while test -n "${1}"; do
    case "${1}" in
        --nostart)
            START=
            ;;
        *)
            echo "Bad argument ${1}" >&2
            exit 1
            ;;
    esac
    shift
done

# Remove any traces of DKMS from previous installations.
for i in vpoxhost vpoxdrv vpoxnetflt vpoxnetadp; do
    rm -rf "/var/lib/dkms/${i}"*
done

# Install runlevel scripts and systemd unit files
install_init_script "${MY_PATH}/vpoxdrv.sh" vpoxdrv
install_init_script "${MY_PATH}/vpoxballoonctrl-service.sh" vpoxballoonctrl-service
install_init_script "${MY_PATH}/vpoxautostart-service.sh" vpoxautostart-service
install_init_script "${MY_PATH}/vpoxweb-service.sh" vpoxweb-service
finish_init_script_install

delrunlevel vpoxdrv
addrunlevel vpoxdrv
delrunlevel vpoxballoonctrl-service
addrunlevel vpoxballoonctrl-service
delrunlevel vpoxautostart-service
addrunlevel vpoxautostart-service
delrunlevel vpoxweb-service
addrunlevel vpoxweb-service

ln -sf "${MY_PATH}/postinst-common.sh" /sbin/vpoxconfig

# Set SELinux permissions
# XXX SELinux: allow text relocation entries
if [ -x /usr/bin/chcon ]; then
    chcon -t texrel_shlib_t "${MY_PATH}"/*VPox* > /dev/null 2>&1
    chcon -t texrel_shlib_t "${MY_PATH}"/VPoxAuth.so \
        > /dev/null 2>&1
    chcon -t texrel_shlib_t "${MY_PATH}"/VirtualPox.so \
        > /dev/null 2>&1
    chcon -t texrel_shlib_t "${MY_PATH}"/components/VPox*.so \
        > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/VirtualPox > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/VPoxSDL > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/VPoxHeadless \
        > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/VPoxNetDHCP \
        > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/VPoxNetNAT \
        > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/VPoxExtPackHelperApp \
        > /dev/null 2>&1
    chcon -t java_exec_t    "${MY_PATH}"/vpoxwebsrv > /dev/null 2>&1
    chcon -t bin_t          "${MY_PATH}"/src/vpoxhost/build_in_tmp \
         > /dev/null 2>&1
    chcon -t bin_t          /usr/share/virtualpox/src/vpoxhost/build_in_tmp \
         > /dev/null 2>&1
fi

test -n "${START}" &&
{
    if ! "${MY_PATH}/vpoxdrv.sh" setup; then
        "${MY_PATH}/check_module_dependencies.sh" >&2
        echo >&2
        echo "There were problems setting up VirtualPox.  To re-start the set-up process, run" >&2
        echo "  /sbin/vpoxconfig" >&2
        echo "as root.  If your system is using EFI Secure Boot you may need to sign the" >&2
        echo "kernel modules (vpoxdrv, vpoxnetflt, vpoxnetadp, vpoxpci) before you can load" >&2
        echo "them. Please see your Linux system's documentation for more information." >&2
    else
        start_init_script vpoxdrv
        start_init_script vpoxballoonctrl-service
        start_init_script vpoxautostart-service
        start_init_script vpoxweb-service
    fi
}
