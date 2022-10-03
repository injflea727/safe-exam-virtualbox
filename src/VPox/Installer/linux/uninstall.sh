#!/bin/sh
#
# Oracle VM VirtualPox
# VirtualPox linux uninstallation script

#
# Copyright (C) 2009-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# The below is GNU-specific.  See VPox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_PATH="${TARGET%/[!/]*}"
. "${MY_PATH}/routines.sh"

if [ -z "$ro_LOG_FILE" ]; then
    create_log "/var/log/vpox-uninstall.log"
fi

if [ -z "VPOX_NO_UNINSTALL_MESSAGE" ]; then
    info "Uninstalling VirtualPox"
    log "Uninstalling VirtualPox"
    log ""
fi

check_root

[ -z "$CONFIG_DIR" ]    && CONFIG_DIR="/etc/vpox"
[ -z "$CONFIG" ]        && CONFIG="vpox.cfg"
[ -z "$CONFIG_FILES" ]  && CONFIG_FILES="filelist"
[ -z "$DEFAULT_FILES" ] && DEFAULT_FILES=`pwd`/deffiles

# Find previous installation
if [ -r $CONFIG_DIR/$CONFIG ]; then
    . $CONFIG_DIR/$CONFIG
    PREV_INSTALLATION=$INSTALL_DIR
fi

# Remove previous installation
if [ "$PREV_INSTALLATION" = "" ]; then
    log "Unable to find a VirtualPox installation, giving up."
    abort "Couldn't find a VirtualPox installation to uninstall."
fi

# Do pre-removal common to all installer types, currently service script
# clean-up.
"${MY_PATH}/prerm-common.sh" || exit 1

# Remove kernel module installed
if [ -z "$VPOX_DONT_REMOVE_OLD_MODULES" ]; then
    rm -f /usr/src/vpoxhost-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vpoxdrv-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vpoxnetflt-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vpoxnetadp-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vpoxpci-$INSTALL_VER 2> /dev/null
fi

# Remove symlinks
rm -f \
  /usr/bin/VirtualPox \
  /usr/bin/VirtualPoxVM \
  /usr/bin/VPoxManage \
  /usr/bin/VPoxSDL \
  /usr/bin/VPoxVRDP \
  /usr/bin/VPoxHeadless \
  /usr/bin/VPoxDTrace \
  /usr/bin/VPoxBugReport \
  /usr/bin/VPoxBalloonCtrl \
  /usr/bin/VPoxAutostart \
  /usr/bin/VPoxNetDHCP \
  /usr/bin/VPoxNetNAT \
  /usr/bin/vpoxwebsrv \
  /usr/bin/vpox-img \
  /usr/bin/vpoximg-mount \
  /usr/bin/VPoxAddIF \
  /usr/bin/VPoxDeleteIf \
  /usr/bin/VPoxTunctl \
  /usr/bin/virtualpox \
  /usr/bin/virtualpoxvm \
  /usr/share/pixmaps/VPox.png \
  /usr/share/pixmaps/virtualpox.png \
  /usr/share/applications/virtualpox.desktop \
  /usr/share/mime/packages/virtualpox.xml \
  /usr/bin/rdesktop-vrdp \
  /usr/bin/virtualpox \
  /usr/bin/vpoxmanage \
  /usr/bin/vpoxsdl \
  /usr/bin/vpoxheadless \
  /usr/bin/vpoxdtrace \
  /usr/bin/vpoxbugreport \
  $PREV_INSTALLATION/components/VPoxVMM.so \
  $PREV_INSTALLATION/components/VPoxREM.so \
  $PREV_INSTALLATION/components/VPoxRT.so \
  $PREV_INSTALLATION/components/VPoxDDU.so \
  $PREV_INSTALLATION/components/VPoxXPCOM.so \
  $PREV_INSTALLATION/VPoxREM.so \
  $PREV_INSTALLATION/VPoxVRDP \
  $PREV_INSTALLATION/VPoxVRDP.so \
  2> /dev/null

cwd=`pwd`
if [ -f $PREV_INSTALLATION/src/Makefile ]; then
    cd $PREV_INSTALLATION/src
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vpoxdrv/Makefile ]; then
    cd $PREV_INSTALLATION/src/vpoxdrv
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vpoxnetflt/Makefile ]; then
    cd $PREV_INSTALLATION/src/vpoxnetflt
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vpoxnetadp/Makefile ]; then
    cd $PREV_INSTALLATION/src/vpoxnetadp
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vpoxpci/Makefile ]; then
    cd $PREV_INSTALLATION/src/vpoxpci
    make clean > /dev/null 2>&1
fi
cd $PREV_INSTALLATION
if [ -r $CONFIG_DIR/$CONFIG_FILES ]; then
    rm -f `cat $CONFIG_DIR/$CONFIG_FILES` 2> /dev/null
elif [ -n "$DEFAULT_FILES" -a -r "$DEFAULT_FILES" ]; then
    DEFAULT_FILE_NAMES=""
    . $DEFAULT_FILES
    for i in "$DEFAULT_FILE_NAMES"; do
        rm -f $i 2> /dev/null
    done
fi
for file in `find $PREV_INSTALLATION 2> /dev/null`; do
    rmdir -p $file 2> /dev/null
done
cd $cwd
mkdir -p $PREV_INSTALLATION 2> /dev/null # The above actually removes the current directory and parents!
rmdir $PREV_INSTALLATION 2> /dev/null
rm -r $CONFIG_DIR/$CONFIG 2> /dev/null

if [ -z "$VPOX_NO_UNINSTALL_MESSAGE" ]; then
    rm -r $CONFIG_DIR/$CONFIG_FILES 2> /dev/null
    rmdir $CONFIG_DIR 2> /dev/null
    [ -n "$INSTALL_REV" ] && INSTALL_REV=" r$INSTALL_REV"
    info "VirtualPox $INSTALL_VER$INSTALL_REV has been removed successfully."
    log "Successfully $INSTALL_VER$INSTALL_REV removed VirtualPox."
fi
update-mime-database /usr/share/mime >/dev/null 2>&1
