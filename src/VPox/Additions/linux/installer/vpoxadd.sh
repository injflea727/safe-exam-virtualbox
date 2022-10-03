#! /bin/sh
# $Id: vpoxadd.sh $
## @file
# Linux Additions kernel module init script ($Revision: 152074 $)
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

# X-Start-Before is a Debian Addition which we use when converting to
# a systemd unit.  X-Service-Type is our own invention, also for systemd.

# chkconfig: 345 10 90
# description: VirtualPox Linux Additions kernel modules
#
### BEGIN INIT INFO
# Provides:       vpoxadd
# Required-Start:
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# X-Start-Before: display-manager
# X-Service-Type: oneshot
# Description:    VirtualPox Linux Additions kernel modules
### END INIT INFO

## @todo This file duplicates a lot of script with vpoxdrv.sh.  When making
# changes please try to reduce differences between the two wherever possible.

# Testing:
# * Should fail if the configuration file is missing or missing INSTALL_DIR or
#   INSTALL_VER entries.
# * vpoxadd user and vpoxsf groups should be created if they do not exist - test
#   by removing them before installing.
# * Shared folders can be mounted and auto-mounts accessible to vpoxsf group,
#   including on recent Fedoras with SELinux.
# * Setting INSTALL_NO_MODULE_BUILDS inhibits modules and module automatic
#   rebuild script creation; otherwise modules, user, group, rebuild script,
#   udev rule and shared folder mount helper should be created/set up.
# * Setting INSTALL_NO_MODULE_BUILDS inhibits module load and unload on start
#   and stop.
# * Uninstalling the Additions and re-installing them does not trigger warnings.

export LC_ALL=C
PATH=$PATH:/bin:/sbin:/usr/sbin
PACKAGE=VPoxGuestAdditions
MODPROBE=/sbin/modprobe
OLDMODULES="vpoxguest vpoxadd vpoxsf vpoxvfs vpoxvideo"
SERVICE="VirtualPox Guest Additions"
## systemd logs information about service status, otherwise do that ourselves.
QUIET=
test -z "${TARGET_VER}" && TARGET_VER=`uname -r`
# Marker to ignore a particular kernel version which was already installed.
SKIPFILE_BASE=/var/lib/VPoxGuestAdditions/skip
export VPOX_KBUILD_TYPE
export USERNAME

setup_log()
{
    test -z "${LOG}" || return 0
    # Rotate log files
    LOG="/var/log/vpoxadd-setup.log"
    mv "${LOG}.3" "${LOG}.4" 2>/dev/null
    mv "${LOG}.2" "${LOG}.3" 2>/dev/null
    mv "${LOG}.1" "${LOG}.2" 2>/dev/null
    mv "${LOG}" "${LOG}.1" 2>/dev/null
}

if $MODPROBE -c 2>/dev/null | grep -q '^allow_unsupported_modules  *0'; then
  MODPROBE="$MODPROBE --allow-unsupported-modules"
fi

# Preamble for Gentoo
if [ "`which $0`" = "/sbin/rc" ]; then
    shift
fi

begin()
{
    test -n "${QUIET}" || echo "${SERVICE}: ${1}"
}

info()
{
    if test -z "${QUIET}"; then
        echo "${SERVICE}: $1" | fold -s
    else
        echo "$1" | fold -s
    fi
}

fail()
{
    log "${1}"
    echo "$1" >&2
    echo "The log file $LOG may contain further information." >&2
    exit 1
}

log()
{
    setup_log
    echo "${1}" >> "${LOG}"
}

module_build_log()
{
    log "Error building the module.  Build output follows."
    echo ""
    echo "${1}" >> "${LOG}"
}

dev=/dev/vpoxguest
userdev=/dev/vpoxuser
config=/var/lib/VPoxGuestAdditions/config
owner=vpoxadd
group=1

if test -r $config; then
  . $config
else
  fail "Configuration file $config not found"
fi
test -n "$INSTALL_DIR" -a -n "$INSTALL_VER" ||
  fail "Configuration file $config not complete"
MODULE_SRC="$INSTALL_DIR/src/vpoxguest-$INSTALL_VER"
BUILDINTMP="$MODULE_SRC/build_in_tmp"

# Attempt to detect VirtualPox Guest Additions version and revision information.
VPOXCLIENT="${INSTALL_DIR}/bin/VPoxClient"
VPOX_VERSION="`"$VPOXCLIENT" --version | cut -d r -f1`"
[ -n "$VPOX_VERSION" ] || VPOX_VERSION='unknown'
VPOX_REVISION="r`"$VPOXCLIENT" --version | cut -d r -f2`"
[ "$VPOX_REVISION" != "r" ] || VPOX_REVISION='unknown'

running_vpoxguest()
{
    lsmod | grep -q "vpoxguest[^_-]"
}

running_vpoxadd()
{
    lsmod | grep -q "vpoxadd[^_-]"
}

running_vpoxsf()
{
    lsmod | grep -q "vpoxsf[^_-]"
}

running_vpoxvideo()
{
    lsmod | grep -q "vpoxvideo[^_-]"
}

do_vpoxguest_non_udev()
{
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vpoxguest;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vpoxguest;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -n "$maj" || {
            rmmod vpoxguest 2>/dev/null
            fail "Cannot locate the VirtualPox device"
        }

        mknod -m 0664 $dev c $maj $min || {
            rmmod vpoxguest 2>/dev/null
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi
    chown $owner:$group $dev 2>/dev/null || {
        rm -f $dev 2>/dev/null
        rm -f $userdev 2>/dev/null
        rmmod vpoxguest 2>/dev/null
        fail "Cannot change owner $owner:$group for device $dev"
    }

    if [ ! -c $userdev ]; then
        maj=10
        min=`sed -n 's;\([0-9]\+\) vpoxuser;\1;p' /proc/misc`
        if [ ! -z "$min" ]; then
            mknod -m 0666 $userdev c $maj $min || {
                rm -f $dev 2>/dev/null
                rmmod vpoxguest 2>/dev/null
                fail "Cannot create device $userdev with major $maj and minor $min"
            }
            chown $owner:$group $userdev 2>/dev/null || {
                rm -f $dev 2>/dev/null
                rm -f $userdev 2>/dev/null
                rmmod vpoxguest 2>/dev/null
                fail "Cannot change owner $owner:$group for device $userdev"
            }
        fi
    fi
}

restart()
{
    stop && start
    return 0
}

## Update the initramfs.  Debian and Ubuntu put the graphics driver in, and
# need the touch(1) command below.  Everyone else that I checked just need
# the right module alias file from depmod(1) and only use the initramfs to
# load the root filesystem, not the boot splash.  update-initramfs works
# for the first two and dracut for every one else I checked.  We are only
# interested in distributions recent enough to use the KMS vpoxvideo driver.
update_initramfs()
{
    ## kernel version to update for.
    version="${1}"
    depmod "${version}"
    rm -f "/lib/modules/${version}/initrd/vpoxvideo"
    test ! -d "/lib/modules/${version}/initrd" ||
        test ! -f "/lib/modules/${version}/misc/vpoxvideo.ko" ||
        touch "/lib/modules/${version}/initrd/vpoxvideo"

    # Systems without systemd-inhibit probably don't need their initramfs
    # rebuild here anyway.
    type systemd-inhibit >/dev/null 2>&1 || return
    if type dracut >/dev/null 2>&1; then
        systemd-inhibit --why="Installing VirtualPox Guest Additions" \
            dracut -f --kver "${version}"
    elif type update-initramfs >/dev/null 2>&1; then
        systemd-inhibit --why="Installing VirtualPox Guest Additions" \
            update-initramfs -u -k "${version}"
    fi
}

# Remove any existing VirtualPox guest kernel modules from the disk, but not
# from the kernel as they may still be in use
cleanup_modules()
{
    # Needed for Ubuntu and Debian, see update_initramfs
    rm -f /lib/modules/*/initrd/vpoxvideo
    for i in /lib/modules/*/misc; do
        KERN_VER="${i%/misc}"
        KERN_VER="${KERN_VER#/lib/modules/}"
        unset do_update
        for j in ${OLDMODULES}; do
            test -f "${i}/${j}.ko" && do_update=1 && rm -f "${i}/${j}.ko"
        done
        test -z "$do_update" || update_initramfs "$KERN_VER"
        # Remove empty /lib/modules folders which may have been kept around
        rmdir -p "${i}" 2>/dev/null || true
        unset keep
        for j in /lib/modules/"${KERN_VER}"/*; do
            name="${j##*/}"
            test -d "${name}" || test "${name%%.*}" != modules && keep=1
        done
        if test -z "${keep}"; then
            rm -rf /lib/modules/"${KERN_VER}"
            rm -f /boot/initrd.img-"${KERN_VER}"
        fi
    done
    for i in ${OLDMODULES}; do
        # We no longer support DKMS, remove any leftovers.
        rm -rf "/var/lib/dkms/${i}"*
    done
    rm -f /etc/depmod.d/vpoxvideo-upstream.conf
    rm -f "$SKIPFILE_BASE"-*
}

# Build and install the VirtualPox guest kernel modules
setup_modules()
{
    KERN_VER="$1"
    test -n "$KERN_VER" || return 1
    # Match (at least): vpoxguest.o; vpoxguest.ko; vpoxguest.ko.xz
    set /lib/modules/"$KERN_VER"/misc/vpoxguest.*o*
    test ! -f "$1" || return 0
    test -d /lib/modules/"$KERN_VER"/build || return 0
    export KERN_VER
    info "Building the modules for kernel $KERN_VER."

    # Detect if kernel was built with clang.
    unset LLVM
    vpox_cc_is_clang=$(/lib/modules/"$KERN_VER"/build/scripts/config \
        --file /lib/modules/"$KERN_VER"/build/.config \
        --state CONFIG_CC_IS_CLANG 2>/dev/null)
    if test "${vpox_cc_is_clang}" = "y"; then
        info "Using clang compiler."
        export LLVM=1
    fi

    log "Building the main Guest Additions $INSTALL_VER module for kernel $KERN_VER."
    if ! myerr=`$BUILDINTMP \
        --save-module-symvers /tmp/vpoxguest-Module.symvers \
        --module-source $MODULE_SRC/vpoxguest \
        --no-print-directory install 2>&1`; then
        # If check_module_dependencies.sh fails it prints a message itself.
        module_build_log "$myerr"
        "${INSTALL_DIR}"/other/check_module_dependencies.sh 2>&1 &&
            info "Look at $LOG to find out what went wrong"
        return 0
    fi
    log "Building the shared folder support module."
    if ! myerr=`$BUILDINTMP \
        --use-module-symvers /tmp/vpoxguest-Module.symvers \
        --module-source $MODULE_SRC/vpoxsf \
        --no-print-directory install 2>&1`; then
        module_build_log "$myerr"
        info  "Look at $LOG to find out what went wrong"
        return 0
    fi
    log "Building the graphics driver module."
    if ! myerr=`$BUILDINTMP \
        --use-module-symvers /tmp/vpoxguest-Module.symvers \
        --module-source $MODULE_SRC/vpoxvideo \
        --no-print-directory install 2>&1`; then
        module_build_log "$myerr"
        info "Look at $LOG to find out what went wrong"
    fi
    [ -d /etc/depmod.d ] || mkdir /etc/depmod.d
    echo "override vpoxguest * misc" > /etc/depmod.d/vpoxvideo-upstream.conf
    echo "override vpoxsf * misc" >> /etc/depmod.d/vpoxvideo-upstream.conf
    echo "override vpoxvideo * misc" >> /etc/depmod.d/vpoxvideo-upstream.conf
    update_initramfs "${KERN_VER}"
    return 0
}

create_vpox_user()
{
    # This is the LSB version of useradd and should work on recent
    # distributions
    useradd -d /var/run/vpoxadd -g 1 -r -s /bin/false vpoxadd >/dev/null 2>&1 || true
    # And for the others, we choose a UID ourselves
    useradd -d /var/run/vpoxadd -g 1 -u 501 -o -s /bin/false vpoxadd >/dev/null 2>&1 || true

}

create_udev_rule()
{
    # Create udev description file
    if [ -d /etc/udev/rules.d ]; then
        udev_call=""
        udev_app=`which udevadm 2> /dev/null`
        if [ $? -eq 0 ]; then
            udev_call="${udev_app} version 2> /dev/null"
        else
            udev_app=`which udevinfo 2> /dev/null`
            if [ $? -eq 0 ]; then
                udev_call="${udev_app} -V 2> /dev/null"
            fi
        fi
        udev_fix="="
        if [ "${udev_call}" != "" ]; then
            udev_out=`${udev_call}`
            udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
            if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
               udev_fix=""
            fi
        fi
        ## @todo 60-vpoxadd.rules -> 60-vpoxguest.rules ?
        echo "KERNEL=${udev_fix}\"vpoxguest\", NAME=\"vpoxguest\", OWNER=\"vpoxadd\", MODE=\"0660\"" > /etc/udev/rules.d/60-vpoxadd.rules
        echo "KERNEL=${udev_fix}\"vpoxuser\", NAME=\"vpoxuser\", OWNER=\"vpoxadd\", MODE=\"0666\"" >> /etc/udev/rules.d/60-vpoxadd.rules
        # Make sure the new rule is noticed.
        udevadm control --reload >/dev/null 2>&1 || true
        udevcontrol reload_rules >/dev/null 2>&1 || true
    fi
}

create_module_rebuild_script()
{
    # And a post-installation script for rebuilding modules when a new kernel
    # is installed.
    mkdir -p /etc/kernel/postinst.d /etc/kernel/prerm.d
    cat << EOF > /etc/kernel/postinst.d/vpoxadd
#!/bin/sh
# This only works correctly on Debian derivatives - Red Hat calls it before
# installing the right header files.
/sbin/rcvpoxadd quicksetup "\${1}"
exit 0
EOF
    cat << EOF > /etc/kernel/prerm.d/vpoxadd
#!/bin/sh
for i in ${OLDMODULES}; do rm -f /lib/modules/"\${1}"/misc/"\${i}".ko; done
rmdir -p /lib/modules/"\$1"/misc 2>/dev/null || true
exit 0
EOF
    chmod 0755 /etc/kernel/postinst.d/vpoxadd /etc/kernel/prerm.d/vpoxadd
}

shared_folder_setup()
{
    # Add a group "vpoxsf" for Shared Folders access
    # All users which want to access the auto-mounted Shared Folders have to
    # be added to this group.
    groupadd -r -f vpoxsf >/dev/null 2>&1

    # Put the mount.vpoxsf mount helper in the right place.
    ## @todo It would be nicer if the kernel module just parsed parameters
    # itself instead of needing a separate binary to do that.
    ln -sf "${INSTALL_DIR}/other/mount.vpoxsf" /sbin
    # SELinux security context for the mount helper.
    if test -e /etc/selinux/config; then
        # This is correct.  semanage maps this to the real path, and it aborts
        # with an error, telling you what you should have typed, if you specify
        # the real path.  The "chcon" is there as a back-up for old guests.
        command -v semanage > /dev/null &&
            semanage fcontext -a -t mount_exec_t "${INSTALL_DIR}/other/mount.vpoxsf"
        chcon -t mount_exec_t "${INSTALL_DIR}/other/mount.vpoxsf" 2>/dev/null
    fi
}

# Returns path to module file as seen by modinfo(8) or empty string.
module_path()
{
    mod="$1"
    [ -n "$mod" ] || return

    modinfo "$mod" 2>/dev/null | grep -e "^filename:" | tr -s ' ' | cut -d " " -f2
}

# Returns module version if module is available or empty string.
module_version()
{
    mod="$1"
    [ -n "$mod" ] || return

    modinfo "$mod" 2>/dev/null | grep -e "^version:" | tr -s ' ' | cut -d " " -f2
}

# Returns module revision if module is available in the system or empty string.
module_revision()
{
    mod="$1"
    [ -n "$mod" ] || return

    modinfo "$mod" 2>/dev/null | grep -e "^version:" | tr -s ' ' | cut -d " " -f3
}

# Returns "1" if externally built module is available in the system and its
# version and revision number do match to current VirtualPox installation.
# Or empty string otherwise.
module_available()
{
    mod="$1"
    [ -n "$mod" ] || return

    [ "$VPOX_VERSION" = "$(module_version "$mod")" ] || return
    [ "$VPOX_REVISION" = "$(module_revision "$mod")" ] || return

    # Check if module belongs to VirtualPox installation.
    #
    # We have a convention that only modules from /lib/modules/*/misc
    # belong to us. Modules from other locations are treated as
    # externally built.
    mod_path="$(module_path "$mod")"

    # If module path points to a symbolic link, resolve actual file location.
    [ -L "$mod_path" ] && mod_path="$(readlink -e -- "$mod_path")"

    # File exists?
    [ -f "$mod_path" ] || return

    # Extract last component of module path and check whether it is located
    # outside of /lib/modules/*/misc.
    mod_dir="$(dirname "$mod_path" | sed 's;^.*/;;')"
    [ "$mod_dir" = "misc" ] || return

    echo "1"
}

# Check if required modules are installed in the system and versions match.
setup_complete()
{
    [ "$(module_available vpoxguest)"   = "1" ] || return
    [ "$(module_available vpoxsf)"      = "1" ] || return

    # All modules are in place.
    echo "1"
}

# setup_script
setup()
{
    # chcon is needed on old Fedora/Redhat systems.  No one remembers which.
    test ! -e /etc/selinux/config ||
        chcon -t bin_t "$BUILDINTMP" 2>/dev/null

    if test -z "$INSTALL_NO_MODULE_BUILDS"; then
        # Check whether modules setup is already complete for currently running kernel.
        # Prevent unnecessary rebuilding in order to speed up booting process.
        if test "$(setup_complete)" = "1"; then
            info "VirtualPox Guest Additions kernel modules $VPOX_VERSION $VPOX_REVISION are"
            info "already available for kernel $TARGET_VER and do not require to be rebuilt."
        else
            info "Building the VirtualPox Guest Additions kernel modules.  This may take a while."
            info "To build modules for other installed kernels, run"
            info "  /sbin/rcvpoxadd quicksetup <version>"
            info "or"
            info "  /sbin/rcvpoxadd quicksetup all"
            if test -d /lib/modules/"$TARGET_VER"/build; then
                setup_modules "$TARGET_VER"
                depmod
            else
                info "Kernel headers not found for target kernel $TARGET_VER. \
Please install them and execute
  /sbin/rcvpoxadd setup"
            fi
        fi
    fi
    create_vpox_user
    create_udev_rule
    test -n "${INSTALL_NO_MODULE_BUILDS}" || create_module_rebuild_script
    shared_folder_setup
    if  running_vpoxguest || running_vpoxadd; then
        info "Running kernel modules will not be replaced until the system is restarted"
    fi

    # Put the X.Org driver in place.  This is harmless if it is not needed.
    # Also set up the OpenGL library.
    myerr=`"${INSTALL_DIR}/init/vpoxadd-x11" setup 2>&1`
    test -z "${myerr}" || log "${myerr}"

    return 0
}

# cleanup_script
cleanup()
{
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        # Delete old versions of VPox modules.
        cleanup_modules
        depmod

        # Remove old module sources
        for i in $OLDMODULES; do
          rm -rf /usr/src/$i-*
        done
    fi

    # Clean-up X11-related bits
    "${INSTALL_DIR}/init/vpoxadd-x11" cleanup

    # Remove other files
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        rm -f /etc/kernel/postinst.d/vpoxadd /etc/kernel/prerm.d/vpoxadd
        rmdir -p /etc/kernel/postinst.d /etc/kernel/prerm.d 2>/dev/null || true
    fi
    rm -f /sbin/mount.vpoxsf 2>/dev/null
    rm -f /etc/udev/rules.d/60-vpoxadd.rules 2>/dev/null
    udevadm control --reload >/dev/null 2>&1 || true
    udevcontrol reload_rules >/dev/null 2>&1 || true
}

start()
{
    begin "Starting."
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        # We want to build modules for newly installed kernels on shutdown, so
        # mark the ones already present.  These will be ignored on shutdown.
        rm -f "$SKIPFILE_BASE"-*
        for setupi in /lib/modules/*; do
            KERN_VER="${setupi##*/}"
            # For a full setup, mark kernels we do not want to build.
            touch "$SKIPFILE_BASE"-"$KERN_VER"
        done
    fi
    setup
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        test -d /sys &&
            ps -A -o comm | grep -q '/*udevd$' 2>/dev/null ||
            no_udev=1
        running_vpoxguest || {
            rm -f $dev || {
                fail "Cannot remove $dev"
            }
            rm -f $userdev || {
                fail "Cannot remove $userdev"
            }
            $MODPROBE vpoxguest >/dev/null 2>&1 ||
                fail "modprobe vpoxguest failed"
            case "$no_udev" in 1)
                sleep .5;;
            esac
            $MODPROBE vpoxsf > /dev/null 2>&1 ||
                info "modprobe vpoxsf failed"
        }
        case "$no_udev" in 1)
            do_vpoxguest_non_udev;;
        esac
    fi  # INSTALL_NO_MODULE_BUILDS

    return 0
}

stop()
{
    begin "Stopping."
    if test -z "${INSTALL_NO_MODULE_BUILDS}"; then
        # We want to build modules for newly installed kernels on shutdown, so
        # check which we marked at start-up.
        for setupi in /lib/modules/*; do
            KERN_VER="${setupi##*/}"
            # For a full setup, mark kernels we do not want to build.
            test -f "$SKIPFILE_BASE"-"$KERN_VER" || setup_modules "$KERN_VER"
        done
    fi
    if test -r /etc/ld.so.conf.d/00vpoxvideo.conf; then
        rm /etc/ld.so.conf.d/00vpoxvideo.conf
        ldconfig
    fi
    if ! umount -a -t vpoxsf 2>/dev/null; then
        # Make sure we only fail, if there are truly no more vpoxsf
        # mounts in the system.
        [ -n "$(findmnt -t vpoxsf)" ] && fail "Cannot unmount vpoxsf folders"
    fi
    test -n "${INSTALL_NO_MODULE_BUILDS}" ||
        info "You may need to restart your guest system to finish removing guest drivers."
    return 0
}

dmnstatus()
{
    if running_vpoxguest; then
        echo "The VirtualPox Additions are currently running."
    else
        echo "The VirtualPox Additions are not currently running."
    fi
}

for i; do
    case "$i" in quiet) QUIET=yes;; esac
done
case "$1" in
# Does setup without clean-up first and marks all kernels currently found on the
# system so that we can see later if any were added.
start)
    start
    ;;
# Tries to build kernel modules for kernels added since start.  Tries to unmount
# shared folders.  Uninstalls our Chromium 3D libraries since we can't always do
# this fast enough at start time if we discover we do not want to use them.
stop)
    stop
    ;;
restart)
    restart
    ;;
# Setup does a clean-up (see below) and re-does all Additions-specific
# configuration of the guest system, including building kernel modules for the
# current kernel.
setup)
    cleanup && start
    ;;
# Builds kernel modules for the specified kernels if they are not already built.
quicksetup)
    if test x"$2" = xall; then
       for topi in /lib/modules/*; do
           KERN_VER="${topi%/misc}"
           KERN_VER="${KERN_VER#/lib/modules/}"
           setup_modules "$KERN_VER"
        done
    elif test -n "$2"; then
        setup_modules "$2"
    else
        setup_modules "$TARGET_VER"
    fi
    ;;
# Clean-up removes all Additions-specific configuration of the guest system,
# including all kernel modules.
cleanup)
    cleanup
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status|setup|quicksetup|cleanup} [quiet]"
    exit 1
esac

exit
