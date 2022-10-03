#!/bin/bash
# $Id: VirtualPox_Uninstall.tool $
## @file
# VirtualPox Uninstaller Script.
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

# Override any funny stuff from the user.
export PATH="/bin:/usr/bin:/sbin:/usr/sbin:$PATH"

#
# Display a simple welcome message first.
#
echo ""
echo "Welcome to the VirtualPox uninstaller script."
echo ""

#
# Check for arguments and display
#
my_default_prompt=0
if test "$#" != "0"; then
    if test "$#" != "1" -o "$1" != "--unattended"; then
        echo "Error: Unknown argument(s): $*"
        echo ""
        echo "Usage: uninstall.sh [--unattended]"
        echo ""
        echo "If the '--unattended' option is not given, you will be prompted"
        echo "for a Yes/No before doing the actual uninstallation."
        echo ""
        exit 4;
    fi
    my_default_prompt="Yes"
fi

#
# Collect directories and files to remove.
# Note: Do NOT attempt adding directories or filenames with spaces!
#
declare -a my_directories
declare -a my_files

# Users files first
test -f "${HOME}/Library/LaunchAgents/org.virtualpox.vpoxwebsrv.plist"  && my_files+=("${HOME}/Library/LaunchAgents/org.virtualpox.vpoxwebsrv.plist")

test -d /Library/StartupItems/VirtualPox/          && my_directories+=("/Library/StartupItems/VirtualPox/")
test -d /Library/Receipts/VPoxStartupItems.pkg/    && my_directories+=("/Library/Receipts/VPoxStartupItems.pkg/")

test -d "/Library/Application Support/VirtualPox/LaunchDaemons/"    && my_directories+=("/Library/Application Support/VirtualPox/LaunchDaemons/")
test -d "/Library/Application Support/VirtualPox/VPoxDrv.kext/"     && my_directories+=("/Library/Application Support/VirtualPox/VPoxDrv.kext/")
test -d "/Library/Application Support/VirtualPox/VPoxUSB.kext/"     && my_directories+=("/Library/Application Support/VirtualPox/VPoxUSB.kext/")
test -d "/Library/Application Support/VirtualPox/VPoxNetFlt.kext/"  && my_directories+=("/Library/Application Support/VirtualPox/VPoxNetFlt.kext/")
test -d "/Library/Application Support/VirtualPox/VPoxNetAdp.kext/"  && my_directories+=("/Library/Application Support/VirtualPox/VPoxNetAdp.kext/")
# Pre 4.3.0rc1 locations:
test -d /Library/Extensions/VPoxDrv.kext/          && my_directories+=("/Library/Extensions/VPoxDrv.kext/")
test -d /Library/Extensions/VPoxUSB.kext/          && my_directories+=("/Library/Extensions/VPoxUSB.kext/")
test -d /Library/Extensions/VPoxNetFlt.kext/       && my_directories+=("/Library/Extensions/VPoxNetFlt.kext/")
test -d /Library/Extensions/VPoxNetAdp.kext/       && my_directories+=("/Library/Extensions/VPoxNetAdp.kext/")
# Tiger support is obsolete, but we leave it here for a clean removing of older
# VirtualPox versions
test -d /Library/Extensions/VPoxDrvTiger.kext/     && my_directories+=("/Library/Extensions/VPoxDrvTiger.kext/")
test -d /Library/Extensions/VPoxUSBTiger.kext/     && my_directories+=("/Library/Extensions/VPoxUSBTiger.kext/")
test -d /Library/Receipts/VPoxKEXTs.pkg/           && my_directories+=("/Library/Receipts/VPoxKEXTs.pkg/")

test -f /usr/bin/VirtualPox                        && my_files+=("/usr/bin/VirtualPox")
test -f /usr/bin/VirtualPoxVM                      && my_files+=("/usr/bin/VirtualPoxVM")
test -f /usr/bin/VPoxManage                        && my_files+=("/usr/bin/VPoxManage")
test -f /usr/bin/VPoxVRDP                          && my_files+=("/usr/bin/VPoxVRDP")
test -f /usr/bin/VPoxHeadless                      && my_files+=("/usr/bin/VPoxHeadless")
test -f /usr/bin/vpoxwebsrv                        && my_files+=("/usr/bin/vpoxwebsrv")
test -f /usr/bin/VPoxBugReport                     && my_files+=("/usr/bin/VPoxBugReport")
test -f /usr/bin/VPoxBalloonCtrl                   && my_files+=("/usr/bin/VPoxBalloonCtrl")
test -f /usr/bin/VPoxAutostart                     && my_files+=("/usr/bin/VPoxAutostart")
test -f /usr/bin/VPoxDTrace                        && my_files+=("/usr/bin/VPoxDTrace")
test -f /usr/bin/vpox-img                          && my_files+=("/usr/bin/vpox-img")
test -f /usr/local/bin/VirtualPox                  && my_files+=("/usr/local/bin/VirtualPox")
test -f /usr/local/bin/VirtualPoxVM                && my_files+=("/usr/local/bin/VirtualPoxVM")
test -f /usr/local/bin/VPoxManage                  && my_files+=("/usr/local/bin/VPoxManage")
test -f /usr/local/bin/VPoxVRDP                    && my_files+=("/usr/local/bin/VPoxVRDP")
test -f /usr/local/bin/VPoxHeadless                && my_files+=("/usr/local/bin/VPoxHeadless")
test -f /usr/local/bin/vpoxwebsrv                  && my_files+=("/usr/local/bin/vpoxwebsrv")
test -f /usr/local/bin/VPoxBugReport               && my_files+=("/usr/local/bin/VPoxBugReport")
test -f /usr/local/bin/VPoxBalloonCtrl             && my_files+=("/usr/local/bin/VPoxBalloonCtrl")
test -f /usr/local/bin/VPoxAutostart               && my_files+=("/usr/local/bin/VPoxAutostart")
test -f /usr/local/bin/VPoxDTrace                  && my_files+=("/usr/local/bin/VPoxDTrace")
test -f /usr/local/bin/vpox-img                    && my_files+=("/usr/local/bin/vpox-img")
test -d /Library/Receipts/VirtualPoxCLI.pkg/       && my_directories+=("/Library/Receipts/VirtualPoxCLI.pkg/")
test -f /Library/LaunchDaemons/org.virtualpox.startup.plist && my_files+=("/Library/LaunchDaemons/org.virtualpox.startup.plist")

test -d /Applications/VirtualPox.app/              && my_directories+=("/Applications/VirtualPox.app/")
test -d /Library/Receipts/VirtualPox.pkg/          && my_directories+=("/Library/Receipts/VirtualPox.pkg/")

# legacy
test -d /Library/Receipts/VPoxDrv.pkg/             && my_directories+=("/Library/Receipts/VPoxDrv.pkg/")
test -d /Library/Receipts/VPoxUSB.pkg/             && my_directories+=("/Library/Receipts/VPoxUSB.pkg/")

# python stuff
python_versions="2.3 2.5 2.6 2.7"
for p in $python_versions; do
    test -f /Library/Python/$p/site-packages/vpoxapi/VirtualPox_constants.py  && my_files+=("/Library/Python/$p/site-packages/vpoxapi/VirtualPox_constants.py")
    test -f /Library/Python/$p/site-packages/vpoxapi/VirtualPox_constants.pyc && my_files+=("/Library/Python/$p/site-packages/vpoxapi/VirtualPox_constants.pyc")
    test -f /Library/Python/$p/site-packages/vpoxapi/__init__.py              && my_files+=("/Library/Python/$p/site-packages/vpoxapi/__init__.py")
    test -f /Library/Python/$p/site-packages/vpoxapi/__init__.pyc             && my_files+=("/Library/Python/$p/site-packages/vpoxapi/__init__.pyc")
    test -f /Library/Python/$p/site-packages/vpoxapi-1.0-py$p.egg-info        && my_files+=("/Library/Python/$p/site-packages/vpoxapi-1.0-py$p.egg-info")
    test -d /Library/Python/$p/site-packages/vpoxapi/                         && my_directories+=("/Library/Python/$p/site-packages/vpoxapi/")
done

#
# Collect KEXTs to remove.
# Note that the unload order is significant.
#
declare -a my_kexts
for kext in org.virtualpox.kext.VPoxUSB org.virtualpox.kext.VPoxNetFlt org.virtualpox.kext.VPoxNetAdp org.virtualpox.kext.VPoxDrv; do
    if /usr/sbin/kextstat -b $kext -l | grep -q $kext; then
        my_kexts+=("$kext")
    fi
done

#
# Collect packages to forget
#
my_pb='org\.virtualpox\.pkg\.'
my_pkgs=`/usr/sbin/pkgutil --pkgs="${my_pb}vpoxkexts|${my_pb}vpoxstartupitems|${my_pb}virtualpox|${my_pb}virtualpoxcli"`

#
# Did we find anything to uninstall?
#
if test -z "${my_directories[*]}"  -a  -z "${my_files[*]}"   -a  -z "${my_kexts[*]}"  -a  -z "$my_pkgs"; then
    echo "No VirtualPox files, directories, KEXTs or packages to uninstall."
    echo "Done."
    exit 0;
fi

#
# Look for running VirtualPox processes and warn the user
# if something is running. Since deleting the files of
# running processes isn't fatal as such, we will leave it
# to the user to choose whether to continue or not.
#
# Note! comm isn't supported on Tiger, so we make -c to do the stripping.
#
my_processes="`ps -axco 'pid uid command' | grep -wEe '(VirtualPox|VirtualPoxVM|VPoxManage|VPoxHeadless|vpoxwebsrv|VPoxXPCOMIPCD|VPoxSVC|VPoxNetDHCP|VPoxNetNAT)' | grep -vw grep | grep -vw VirtualPox_Uninstall.tool | tr '\n' '\a'`";
if test -n "$my_processes"; then
    echo 'Warning! Found the following active VirtualPox processes:'
    echo "$my_processes" | tr '\a' '\n'
    echo ""
    echo "We recommend that you quit all VirtualPox processes before"
    echo "uninstalling the product."
    echo ""
    if test "$my_default_prompt" != "Yes"; then
        echo "Do you wish to continue none the less (Yes/No)?"
        read my_answer
        if test "$my_answer" != "Yes"  -a  "$my_answer" != "YES"  -a  "$my_answer" != "yes"; then
            echo "Aborting uninstall. (answer: '$my_answer')".
            exit 2;
        fi
        echo ""
        my_answer=""
    fi
fi

#
# Display the files and directories that will be removed
# and get the user's consent before continuing.
#
if test -n "${my_files[*]}"  -o  -n "${my_directories[*]}"; then
    echo "The following files and directories (bundles) will be removed:"
    for file in "${my_files[@]}";       do echo "    $file"; done
    for dir  in "${my_directories[@]}"; do echo "    $dir"; done
    echo ""
fi
if test -n "${my_kexts[*]}"; then
    echo "And the following KEXTs will be unloaded:"
    for kext in "${my_kexts[@]}";       do echo "    $kext"; done
    echo ""
fi
if test -n "$my_pkgs"; then
    echo "And the traces of following packages will be removed:"
    for kext in $my_pkgs;       do echo "    $kext"; done
    echo ""
fi

if test "$my_default_prompt" != "Yes"; then
    echo "Do you wish to uninstall VirtualPox (Yes/No)?"
    read my_answer
    if test "$my_answer" != "Yes"  -a  "$my_answer" != "YES"  -a  "$my_answer" != "yes"; then
        echo "Aborting uninstall. (answer: '$my_answer')".
        exit 2;
    fi
    echo ""
fi

my_fuse_macos_core_uninstall=0
if test "$my_default_prompt" != "Yes" -a -f "/Library/Filesystems/osxfuse.fs/Contents/Resources/uninstall_osxfuse.app/Contents/Resources/Scripts/uninstall_osxfuse.sh"; then
    echo "VirtualPox detected the FUSE for macOS core package which might've been installed"
    echo "by VirtualPox itself for the vpoximg-mount utility. Do you wish to uninstall"
    echo "the FUSE for macOS core package (Yes/No)?"
    read my_answer
    if test "$my_answer" == "Yes"  -o  "$my_answer" == "YES"  -o  "$my_answer" == "yes"; then
        my_fuse_macos_core_uninstall=1;
    fi
    echo ""
fi

#
# Unregister has to be done before the files are removed.
#
LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister
if [ -e ${LSREGISTER} ]; then
    ${LSREGISTER} -u /Applications/VirtualPox.app > /dev/null
    ${LSREGISTER} -u /Applications/VirtualPox.app/Contents/Resources/vmstarter.app > /dev/null
fi

#
# Display the sudo usage instructions and execute the command.
#
echo "The uninstallation processes requires administrative privileges"
echo "because some of the installed files cannot be removed by a normal"
echo "user. You may be prompted for your password now..."
echo ""

if test -n "${my_files[*]}"  -o  -n "${my_directories[*]}"; then
    /usr/bin/sudo -p "Please enter %u's password:" /bin/rm -Rf "${my_files[@]}" "${my_directories[@]}"
    my_rc=$?
    if test "$my_rc" -ne 0; then
        echo "An error occurred durning 'sudo rm', there should be a message above. (rc=$my_rc)"
        test -x /usr/bin/sudo || echo "warning: Cannot find /usr/bin/sudo or it's not an executable."
        test -x /bin/rm       || echo "warning: Cannot find /bin/rm or it's not an executable"
        echo ""
        echo "The uninstall failed. Please retry."
        test "$my_default_prompt" != "Yes" && read -p "Press <ENTER> to exit"
        exit 1;
    fi
fi

if test "$my_fuse_macos_core_uninstall" != 0; then
    echo "Uninstalling the FUSE for macOS core package"
    /usr/bin/sudo -p "Please enter %u's password:" /Library/Filesystems/osxfuse.fs/Contents/Resources/uninstall_osxfuse.app/Contents/Resources/Scripts/uninstall_osxfuse.sh
fi

my_rc=0
for kext in "${my_kexts[@]}"; do
    echo unloading $kext
    /usr/bin/sudo -p "Please enter %u's password (unloading $kext):" /sbin/kextunload -m $kext
    my_rc2=$?
    if test "$my_rc2" -ne 0; then
        echo "An error occurred durning 'sudo /sbin/kextunload -m $kext', there should be a message above. (rc=$my_rc2)"
        test -x /usr/bin/sudo    || echo "warning: Cannot find /usr/bin/sudo or it's not an executable."
        test -x /sbin/kextunload || echo "warning: Cannot find /sbin/kextunload or it's not an executable"
        my_rc=$my_rc2
    fi
done
if test "$my_rc" -eq 0; then
    echo "Successfully unloaded VirtualPox kernel extensions."
else
    echo "Failed to unload one or more KEXTs, please reboot the machine to complete the uninstall."
    test "$my_default_prompt" != "Yes" && read -p "Press <ENTER> to exit"
    exit 1;
fi

# Cleaning up pkgutil database
for my_pkg in $my_pkgs; do
    /usr/bin/sudo -p "Please enter %u's password (removing $my_pkg):" /usr/sbin/pkgutil --forget "$my_pkg"
done

echo "Done."
exit 0;

