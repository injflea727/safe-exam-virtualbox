#!/bin/sh
#
# Oracle VM VirtualPox
# VirtualPox linux installation script

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

# Testing:
# * After successful installation, 0 is returned if the vpoxdrv module version
#   built matches the one loaded.
# * If the kernel modules cannot be built (run the installer with KERN_VER=none)
#   or loaded (run with KERN_VER=<installed non-current version>)
#   then 1 is returned.

PATH=$PATH:/bin:/sbin:/usr/sbin

# Include routines and utilities needed by the installer
. ./routines.sh

LOG="/var/log/vpox-install.log"
VERSION="_VERSION_"
SVNREV="_SVNREV_"
BUILD="_BUILD_"
ARCH="_ARCH_"
HARDENED="_HARDENED_"
# The "BUILD_" prefixes prevent the variables from being overwritten when we
# read the configuration from the previous installation.
BUILD_VPOX_KBUILD_TYPE="_BUILDTYPE_"
BUILD_USERNAME="_USERNAME_"
CONFIG_DIR="/etc/vpox"
CONFIG="vpox.cfg"
CONFIG_FILES="filelist"
DEFAULT_FILES=`pwd`/deffiles
GROUPNAME="vpoxusers"
INSTALLATION_DIR="_INSTALLATION_DIR_"
LICENSE_ACCEPTED=""
PREV_INSTALLATION=""
PYTHON="_PYTHON_"
ACTION=""
SELF=$1
RC_SCRIPT=0
if [ -n "$HARDENED" ]; then
    VPOXDRV_MODE=0600
    VPOXDRV_GRP="root"
else
    VPOXDRV_MODE=0660
    VPOXDRV_GRP=$GROUPNAME
fi
VPOXUSB_MODE=0664
VPOXUSB_GRP=$GROUPNAME

## Were we able to stop any previously running Additions kernel modules?
MODULES_STOPPED=1


##############################################################################
# Helper routines                                                            #
##############################################################################

usage() {
    info ""
    info "Usage: install | uninstall"
    info ""
    info "Example:"
    info "$SELF install"
    exit 1
}

module_loaded() {
    lsmod | grep -q "vpoxdrv[^_-]"
}

# This routine makes sure that there is no previous installation of
# VirtualPox other than one installed using this install script or a
# compatible method.  We do this by checking for any of the VirtualPox
# applications in /usr/bin.  If these exist and are not symlinks into
# the installation directory, then we assume that they are from an
# incompatible previous installation.

## Helper routine: test for a particular VirtualPox binary and see if it
## is a link into a previous installation directory
##
## Arguments: 1) the binary to search for and
##            2) the installation directory (if any)
## Returns: false if an incompatible version was detected, true otherwise
check_binary() {
    binary=$1
    install_dir=$2
    test ! -e $binary 2>&1 > /dev/null ||
        ( test -n "$install_dir" &&
              readlink $binary 2>/dev/null | grep "$install_dir" > /dev/null
        )
}

## Main routine
##
## Argument: the directory where the previous installation should be
##           located.  If this is empty, then we will assume that any
##           installation of VirtualPox found is incompatible with this one.
## Returns: false if an incompatible installation was found, true otherwise
check_previous() {
    install_dir=$1
    # These should all be symlinks into the installation folder
    check_binary "/usr/bin/VirtualPox" "$install_dir" &&
    check_binary "/usr/bin/VPoxManage" "$install_dir" &&
    check_binary "/usr/bin/VPoxSDL" "$install_dir" &&
    check_binary "/usr/bin/VPoxVRDP" "$install_dir" &&
    check_binary "/usr/bin/VPoxHeadless" "$install_dir" &&
    check_binary "/usr/bin/VPoxDTrace" "$install_dir" &&
    check_binary "/usr/bin/VPoxBugReport" "$install_dir" &&
    check_binary "/usr/bin/VPoxBalloonCtrl" "$install_dir" &&
    check_binary "/usr/bin/VPoxAutostart" "$install_dir" &&
    check_binary "/usr/bin/vpoxwebsrv" "$install_dir" &&
    check_binary "/usr/bin/vpox-img" "$install_dir" &&
    check_binary "/usr/bin/vpoximg-mount" "$install_dir" &&
    check_binary "/sbin/rcvpoxdrv" "$install_dir"
}

##############################################################################
# Main script                                                                #
##############################################################################

info "VirtualPox Version $VERSION r$SVNREV ($BUILD) installer"


# Make sure that we were invoked as root...
check_root

# Set up logging before anything else
create_log $LOG

log "VirtualPox $VERSION r$SVNREV installer, built $BUILD."
log ""
log "Testing system setup..."

# Sanity check: figure out whether build arch matches uname arch
cpu=`uname -m`;
case "$cpu" in
  i[3456789]86|x86)
    cpu="x86"
    ;;
  x86_64)
    cpu="amd64"
    ;;
esac
if [ "$cpu" != "$ARCH" ]; then
  info "Detected unsupported $cpu environment."
  log "Detected unsupported $cpu environment."
  exit 1
fi

# Sensible default actions
ACTION="install"
BUILD_MODULE="true"
unset FORCE_UPGRADE
while true
do
    if [ "$2" = "" ]; then
        break
    fi
    shift
    case "$1" in
        install|--install)
            ACTION="install"
            ;;

        uninstall|--uninstall)
            ACTION="uninstall"
            ;;

        force|--force)
            FORCE_UPGRADE=1
            ;;
        license_accepted_unconditionally|--license_accepted_unconditionally)
            # Legacy option
            ;;
        no_module|--no_module)
            BUILD_MODULE=""
            ;;
        *)
            if [ "$ACTION" = "" ]; then
                info "Unknown command '$1'."
                usage
            fi
            info "Specifying an installation path is not allowed -- using _INSTALLATION_DIR_!"
            ;;
    esac
done

if [ "$ACTION" = "install" ]; then
    # Choose a proper umask
    umask 022

    # Find previous installation
    if test -r "$CONFIG_DIR/$CONFIG"; then
        . $CONFIG_DIR/$CONFIG
        PREV_INSTALLATION=$INSTALL_DIR
    fi
    if ! check_previous $INSTALL_DIR && test -z "$FORCE_UPGRADE"
    then
        info
        info "You appear to have a version of VirtualPox on your system which was installed"
        info "from a different source or using a different type of installer (or a damaged"
        info "installation of VirtualPox).  We strongly recommend that you remove it before"
        info "installing this version of VirtualPox."
        info
        info "Do you wish to continue anyway? [yes or no]"
        read reply dummy
        if ! expr "$reply" : [yY] && ! expr "$reply" : [yY][eE][sS]
        then
            info
            info "Cancelling installation."
            log "User requested cancellation of the installation"
            exit 1
        fi
    fi

    # Do additional clean-up in case some-one is running from a build folder.
    ./prerm-common.sh || exit 1

    # Remove previous installation
    test "${BUILD_MODULE}" = true || VPOX_DONT_REMOVE_OLD_MODULES=1

    if [ -n "$PREV_INSTALLATION" ]; then
        [ -n "$INSTALL_REV" ] && INSTALL_REV=" r$INSTALL_REV"
        info "Removing previous installation of VirtualPox $INSTALL_VER$INSTALL_REV from $PREV_INSTALLATION"
        log "Removing previous installation of VirtualPox $INSTALL_VER$INSTALL_REV from $PREV_INSTALLATION"
        log ""

        VPOX_NO_UNINSTALL_MESSAGE=1
        # This also checks $BUILD_MODULE and $VPOX_DONT_REMOVE_OLD_MODULES
        . ./uninstall.sh
    fi

    mkdir -p -m 755 $CONFIG_DIR
    touch $CONFIG_DIR/$CONFIG

    info "Installing VirtualPox to $INSTALLATION_DIR"
    log "Installing VirtualPox to $INSTALLATION_DIR"
    log ""

    # Verify the archive
    mkdir -p -m 755 $INSTALLATION_DIR
    bzip2 -d -c VirtualPox.tar.bz2 > VirtualPox.tar
    if ! tar -tf VirtualPox.tar > $CONFIG_DIR/$CONFIG_FILES; then
        rmdir $INSTALLATION_DIR 2> /dev/null
        rm -f $CONFIG_DIR/$CONFIG 2> /dev/null
        rm -f $CONFIG_DIR/$CONFIG_FILES 2> /dev/null
        log 'Error running "bzip2 -d -c VirtualPox.tar.bz2" or "tar -tf VirtualPox.tar".'
        abort "Error installing VirtualPox.  Installation aborted"
    fi

    # Create installation directory and install
    if ! tar -xf VirtualPox.tar -C $INSTALLATION_DIR; then
        cwd=`pwd`
        cd $INSTALLATION_DIR
        rm -f `cat $CONFIG_DIR/$CONFIG_FILES` 2> /dev/null
        cd $pwd
        rmdir $INSTALLATION_DIR 2> /dev/null
        rm -f $CONFIG_DIR/$CONFIG 2> /dev/null
        log 'Error running "tar -xf VirtualPox.tar -C '"$INSTALLATION_DIR"'".'
        abort "Error installing VirtualPox.  Installation aborted"
    fi

    cp uninstall.sh $INSTALLATION_DIR
    echo "uninstall.sh" >> $CONFIG_DIR/$CONFIG_FILES

    # Hardened build: Mark selected binaries set-user-ID-on-execution,
    #                 create symlinks for working around unsupported $ORIGIN/.. in VPoxC.so (setuid),
    #                 and finally make sure the directory is only writable by the user (paranoid).
    if [ -n "$HARDENED" ]; then
        if [ -f $INSTALLATION_DIR/VirtualPoxVM ]; then
            test -e $INSTALLATION_DIR/VirtualPoxVM   && chmod 4511 $INSTALLATION_DIR/VirtualPoxVM
        else
            test -e $INSTALLATION_DIR/VirtualPox     && chmod 4511 $INSTALLATION_DIR/VirtualPox
        fi
        test -e $INSTALLATION_DIR/VPoxSDL        && chmod 4511 $INSTALLATION_DIR/VPoxSDL
        test -e $INSTALLATION_DIR/VPoxHeadless   && chmod 4511 $INSTALLATION_DIR/VPoxHeadless
        test -e $INSTALLATION_DIR/VPoxNetDHCP    && chmod 4511 $INSTALLATION_DIR/VPoxNetDHCP
        test -e $INSTALLATION_DIR/VPoxNetNAT     && chmod 4511 $INSTALLATION_DIR/VPoxNetNAT

        ln -sf $INSTALLATION_DIR/VPoxVMM.so   $INSTALLATION_DIR/components/VPoxVMM.so
        ln -sf $INSTALLATION_DIR/VPoxRT.so    $INSTALLATION_DIR/components/VPoxRT.so

        chmod go-w $INSTALLATION_DIR
    fi

    # This binaries need to be suid root in any case, even if not hardened
    test -e $INSTALLATION_DIR/VPoxNetAdpCtl && chmod 4511 $INSTALLATION_DIR/VPoxNetAdpCtl
    test -e $INSTALLATION_DIR/VPoxVolInfo && chmod 4511 $INSTALLATION_DIR/VPoxVolInfo

    # Write the configuration.  Needs to be done before the vpoxdrv service is
    # started.
    echo "# VirtualPox installation directory" > $CONFIG_DIR/$CONFIG
    echo "INSTALL_DIR='$INSTALLATION_DIR'" >> $CONFIG_DIR/$CONFIG
    echo "# VirtualPox version" >> $CONFIG_DIR/$CONFIG
    echo "INSTALL_VER='$VERSION'" >> $CONFIG_DIR/$CONFIG
    echo "INSTALL_REV='$SVNREV'" >> $CONFIG_DIR/$CONFIG
    echo "# Build type and user name for logging purposes" >> $CONFIG_DIR/$CONFIG
    echo "VPOX_KBUILD_TYPE='$BUILD_VPOX_KBUILD_TYPE'" >> $CONFIG_DIR/$CONFIG
    echo "USERNAME='$BUILD_USERNAME'" >> $CONFIG_DIR/$CONFIG

    # Create users group
    groupadd -r -f $GROUPNAME 2> /dev/null

    # Create symlinks to start binaries
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VirtualPox
    if [ -f $INSTALLATION_DIR/VirtualPoxVM ]; then
        ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VirtualPoxVM
    fi
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxManage
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxSDL
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxVRDP
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxHeadless
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxBalloonCtrl
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxBugReport
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxAutostart
    ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/vpoxwebsrv
    ln -sf $INSTALLATION_DIR/vpox-img /usr/bin/vpox-img
    ln -sf $INSTALLATION_DIR/vpoximg-mount /usr/bin/vpoximg-mount
    ln -sf $INSTALLATION_DIR/VPox.png /usr/share/pixmaps/VPox.png
    if [ -f $INSTALLATION_DIR/VPoxDTrace ]; then
        ln -sf $INSTALLATION_DIR/VPox.sh /usr/bin/VPoxDTrace
    fi
    # Unity and Nautilus seem to look here for their icons
    ln -sf $INSTALLATION_DIR/icons/128x128/virtualpox.png /usr/share/pixmaps/virtualpox.png
    ln -sf $INSTALLATION_DIR/virtualpox.desktop /usr/share/applications/virtualpox.desktop
    ln -sf $INSTALLATION_DIR/virtualpox.xml /usr/share/mime/packages/virtualpox.xml
    ln -sf $INSTALLATION_DIR/rdesktop-vrdp /usr/bin/rdesktop-vrdp
    ln -sf $INSTALLATION_DIR/src/vpoxhost /usr/src/vpoxhost-_VERSION_

    # Convenience symlinks. The creation fails if the FS is not case sensitive
    ln -sf VirtualPox /usr/bin/virtualpox > /dev/null 2>&1
    if [ -f $INSTALLATION_DIR/VirtualPoxVM ]; then
        ln -sf VirtualPoxVM /usr/bin/virtualpoxvm > /dev/null 2>&1
    fi
    ln -sf VPoxManage /usr/bin/vpoxmanage > /dev/null 2>&1
    ln -sf VPoxSDL /usr/bin/vpoxsdl > /dev/null 2>&1
    ln -sf VPoxHeadless /usr/bin/vpoxheadless > /dev/null 2>&1
    ln -sf VPoxBugReport /usr/bin/vpoxbugreport > /dev/null 2>&1
    if [ -f $INSTALLATION_DIR/VPoxDTrace ]; then
        ln -sf VPoxDTrace /usr/bin/vpoxdtrace > /dev/null 2>&1
    fi

    # Create legacy symlinks if necesary for Qt5/xcb stuff.
    if [ -d $INSTALLATION_DIR/legacy ]; then
        if ! /sbin/ldconfig -p | grep -q "\<libxcb\.so\.1\>"; then
            for f in `ls -1 $INSTALLATION_DIR/legacy/`; do
                ln -s $INSTALLATION_DIR/legacy/$f $INSTALLATION_DIR/$f
                echo $INSTALLATION_DIR/$f >> $CONFIG_DIR/$CONFIG_FILES
            done
        fi
    fi

    # Icons
    cur=`pwd`
    cd $INSTALLATION_DIR/icons
    for i in *; do
        cd $i
        if [ -d /usr/share/icons/hicolor/$i ]; then
            for j in *; do
                if expr "$j" : "virtualpox\..*" > /dev/null; then
                    dst=apps
                else
                    dst=mimetypes
                fi
                if [ -d /usr/share/icons/hicolor/$i/$dst ]; then
                    ln -s $INSTALLATION_DIR/icons/$i/$j /usr/share/icons/hicolor/$i/$dst/$j
                    echo /usr/share/icons/hicolor/$i/$dst/$j >> $CONFIG_DIR/$CONFIG_FILES
                fi
            done
        fi
        cd -
    done
    cd $cur

    # Update the MIME database
    update-mime-database /usr/share/mime 2>/dev/null

    # Update the desktop database
    update-desktop-database -q 2>/dev/null

    # If Python is available, install Python bindings
    if [ -n "$PYTHON" ]; then
      maybe_run_python_bindings_installer $INSTALLATION_DIR $CONFIG_DIR $CONFIG_FILES
    fi

    # Do post-installation common to all installer types, currently service
    # script set-up.
    if test "${BUILD_MODULE}" = "true"; then
      START_SERVICES=
    else
      START_SERVICES="--nostart"
    fi
    "${INSTALLATION_DIR}/prerm-common.sh" >> "${LOG}"

    # Now check whether the kernel modules were stopped.
    lsmod | grep -q vpoxdrv && MODULES_STOPPED=

    "${INSTALLATION_DIR}/postinst-common.sh" ${START_SERVICES} >> "${LOG}"

    info ""
    info "VirtualPox has been installed successfully."
    info ""
    info "You will find useful information about using VirtualPox in the user manual"
    info "  $INSTALLATION_DIR/UserManual.pdf"
    info "and in the user FAQ"
    info "  http://www.virtualpox.org/wiki/User_FAQ"
    info ""
    info "We hope that you enjoy using VirtualPox."
    info ""

    # And do a final test as to whether the kernel modules were properly created
    # and loaded.  Return 0 if both are true, 1 if not.
    test -n "${MODULES_STOPPED}" &&
        modinfo vpoxdrv >/dev/null 2>&1 &&
        lsmod | grep -q vpoxdrv ||
        abort "The installation log file is at ${LOG}."

    log "Installation successful"
elif [ "$ACTION" = "uninstall" ]; then
    . ./uninstall.sh
fi
exit $RC_SCRIPT
