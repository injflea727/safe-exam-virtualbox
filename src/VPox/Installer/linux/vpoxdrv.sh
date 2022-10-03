#! /bin/sh
# Oracle VM VirtualPox
# Linux kernel module init script

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

# chkconfig: 345 20 80
# description: VirtualPox Linux kernel module
#
### BEGIN INIT INFO
# Provides:       vpoxdrv
# Required-Start: $syslog
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Short-Description: VirtualPox Linux kernel module
### END INIT INFO

## @todo This file duplicates a lot of script with vpoxadd.sh.  When making
# changes please try to reduce differences between the two wherever possible.

## @todo Remove the stop_vms target so that this script is only relevant to
# kernel modules.  Nice but not urgent.

PATH=/sbin:/bin:/usr/sbin:/usr/bin:$PATH
DEVICE=/dev/vpoxdrv
MODPROBE=/sbin/modprobe
SCRIPTNAME=vpoxdrv.sh

# The below is GNU-specific.  See VPox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
SCRIPT_DIR="${TARGET%/[!/]*}"

if $MODPROBE -c | grep -q '^allow_unsupported_modules  *0'; then
  MODPROBE="$MODPROBE --allow-unsupported-modules"
fi

setup_log()
{
    test -n "${LOG}" && return 0
    # Rotate log files
    LOG="/var/log/vpox-setup.log"
    mv "${LOG}.3" "${LOG}.4" 2>/dev/null
    mv "${LOG}.2" "${LOG}.3" 2>/dev/null
    mv "${LOG}.1" "${LOG}.2" 2>/dev/null
    mv "${LOG}" "${LOG}.1" 2>/dev/null
}

[ -f /etc/vpox/vpox.cfg ] && . /etc/vpox/vpox.cfg
export VPOX_KBUILD_TYPE
export USERNAME
export USER=$USERNAME

if test -n "${INSTALL_DIR}" && test -x "${INSTALL_DIR}/VirtualPox"; then
    MODULE_SRC="${INSTALL_DIR}/src/vpoxhost"
elif test -x /usr/lib/virtualpox/VirtualPox; then
    INSTALL_DIR=/usr/lib/virtualpox
    MODULE_SRC="/usr/share/virtualpox/src/vpoxhost"
elif test -x "${SCRIPT_DIR}/VirtualPox"; then
    # Executing from the build directory
    INSTALL_DIR="${SCRIPT_DIR}"
    MODULE_SRC="${INSTALL_DIR}/src"
else
    # Silently exit if the package was uninstalled but not purged.
    # Applies to Debian packages only (but shouldn't hurt elsewhere)
    exit 0
fi
VIRTUALPOX="${INSTALL_DIR}/VirtualPox"
VPOXMANAGE="${INSTALL_DIR}/VPoxManage"
BUILDINTMP="${MODULE_SRC}/build_in_tmp"
if test -u "${VIRTUALPOX}"; then
    GROUP=root
    DEVICE_MODE=0600
else
    GROUP=vpoxusers
    DEVICE_MODE=0660
fi

KERN_VER=`uname -r`
if test -e "${MODULE_SRC}/vpoxpci"; then
    MODULE_LIST="vpoxdrv vpoxnetflt vpoxnetadp vpoxpci"
else
    MODULE_LIST="vpoxdrv vpoxnetflt vpoxnetadp"
fi
# Secure boot state.
case "`mokutil --sb-state 2>/dev/null`" in
    *"disabled in shim"*) unset HAVE_SEC_BOOT;;
    *"SecureBoot enabled"*) HAVE_SEC_BOOT=true;;
    *) unset HAVE_SEC_BOOT;;
esac
# So far we can only sign modules on Ubuntu and on Debian 10 and later.
DEB_PUB_KEY=/var/lib/shim-signed/mok/MOK.der
DEB_PRIV_KEY=/var/lib/shim-signed/mok/MOK.priv
unset HAVE_DEB_KEY
case "`mokutil --test-key "$DEB_PUB_KEY" 2>/dev/null`" in
    *"is already"*) DEB_KEY_ENROLLED=true;;
    *) unset DEB_KEY_ENROLLED;;
esac

[ -r /etc/default/virtualpox ] && . /etc/default/virtualpox

# Preamble for Gentoo
if [ "`which $0`" = "/sbin/rc" ]; then
    shift
fi

begin_msg()
{
    test -n "${2}" && echo "${SCRIPTNAME}: ${1}."
    logger -t "${SCRIPTNAME}" "${1}."
}

succ_msg()
{
    logger -t "${SCRIPTNAME}" "${1}."
}

fail_msg()
{
    echo "${SCRIPTNAME}: failed: ${1}." >&2
    logger -t "${SCRIPTNAME}" "failed: ${1}."
}

failure()
{
    fail_msg "$1"
    exit 1
}

running()
{
    lsmod | grep -q "$1[^_-]"
}

log()
{
    setup_log
    echo "${1}" >> "${LOG}"
}

module_build_log()
{
    setup_log
    echo "${1}" | egrep -v \
        "^test -e include/generated/autoconf.h|^echo >&2|^/bin/false\)$" \
        >> "${LOG}"
}

# Detect VirtualPox version info or report error.
VPOX_VERSION="`"$VPOXMANAGE" -v | cut -d r -f1`"
[ -n "$VPOX_VERSION" ] || failure 'Cannot detect VirtualPox version number'
VPOX_REVISION="r`"$VPOXMANAGE" -v | cut -d r -f2`"
[ "$VPOX_REVISION" != "r" ] || failure 'Cannot detect VirtualPox revision number'

## Output the vpoxdrv part of our udev rule.  This is redirected to the right file.
udev_write_vpoxdrv() {
    VPOXDRV_GRP="$1"
    VPOXDRV_MODE="$2"

    echo "KERNEL==\"vpoxdrv\", NAME=\"vpoxdrv\", OWNER=\"root\", GROUP=\"$VPOXDRV_GRP\", MODE=\"$VPOXDRV_MODE\""
    echo "KERNEL==\"vpoxdrvu\", NAME=\"vpoxdrvu\", OWNER=\"root\", GROUP=\"root\", MODE=\"0666\""
    echo "KERNEL==\"vpoxnetctl\", NAME=\"vpoxnetctl\", OWNER=\"root\", GROUP=\"$VPOXDRV_GRP\", MODE=\"$VPOXDRV_MODE\""
}

## Output the USB part of our udev rule.  This is redirected to the right file.
udev_write_usb() {
    INSTALLATION_DIR="$1"
    USB_GROUP="$2"

    echo "SUBSYSTEM==\"usb_device\", ACTION==\"add\", RUN+=\"$INSTALLATION_DIR/VPoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}${USB_GROUP}\""
    echo "SUBSYSTEM==\"usb\", ACTION==\"add\", ENV{DEVTYPE}==\"usb_device\", RUN+=\"$INSTALLATION_DIR/VPoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}${USB_GROUP}\""
    echo "SUBSYSTEM==\"usb_device\", ACTION==\"remove\", RUN+=\"$INSTALLATION_DIR/VPoxCreateUSBNode.sh --remove \$major \$minor\""
    echo "SUBSYSTEM==\"usb\", ACTION==\"remove\", ENV{DEVTYPE}==\"usb_device\", RUN+=\"$INSTALLATION_DIR/VPoxCreateUSBNode.sh --remove \$major \$minor\""
}

## Generate our udev rule file.  This takes a change in udev rule syntax in
## version 55 into account.  It only creates rules for USB for udev versions
## recent enough to support USB device nodes.
generate_udev_rule() {
    VPOXDRV_GRP="$1"      # The group owning the vpoxdrv device
    VPOXDRV_MODE="$2"     # The access mode for the vpoxdrv device
    INSTALLATION_DIR="$3" # The directory VirtualPox is installed in
    USB_GROUP="$4"        # The group that has permission to access USB devices
    NO_INSTALL="$5"       # Set this to "1" to remove but not re-install rules

    # Extra space!
    case "$USB_GROUP" in ?*) USB_GROUP=" $USB_GROUP" ;; esac
    case "$NO_INSTALL" in "1") return ;; esac
    udev_write_vpoxdrv "$VPOXDRV_GRP" "$VPOXDRV_MODE"
    udev_write_usb "$INSTALLATION_DIR" "$USB_GROUP"
}

## Install udev rule (disable with INSTALL_NO_UDEV=1 in
## /etc/default/virtualpox).
install_udev() {
    VPOXDRV_GRP="$1"      # The group owning the vpoxdrv device
    VPOXDRV_MODE="$2"     # The access mode for the vpoxdrv device
    INSTALLATION_DIR="$3" # The directory VirtualPox is installed in
    USB_GROUP="$4"        # The group that has permission to access USB devices
    NO_INSTALL="$5"       # Set this to "1" to remove but not re-install rules

    if test -d /etc/udev/rules.d; then
        generate_udev_rule "$VPOXDRV_GRP" "$VPOXDRV_MODE" "$INSTALLATION_DIR" \
                           "$USB_GROUP" "$NO_INSTALL"
    fi
    # Remove old udev description file
    rm -f /etc/udev/rules.d/10-vpoxdrv.rules 2> /dev/null
}

## Create a usb device node for a given sysfs path to a USB device.
install_create_usb_node_for_sysfs() {
    path="$1"           # sysfs path for the device
    usb_createnode="$2" # Path to the USB device node creation script
    usb_group="$3"      # The group to give ownership of the node to
    if test -r "${path}/dev"; then
        dev="`cat "${path}/dev" 2> /dev/null`"
        major="`expr "$dev" : '\(.*\):' 2> /dev/null`"
        minor="`expr "$dev" : '.*:\(.*\)' 2> /dev/null`"
        class="`cat ${path}/bDeviceClass 2> /dev/null`"
        sh "${usb_createnode}" "$major" "$minor" "$class" \
              "${usb_group}" 2>/dev/null
    fi
}

udev_rule_file=/etc/udev/rules.d/60-vpoxdrv.rules
sysfs_usb_devices="/sys/bus/usb/devices/*"

## Install udev rules and create device nodes for usb access
setup_usb() {
    VPOXDRV_GRP="$1"      # The group that should own /dev/vpoxdrv
    VPOXDRV_MODE="$2"     # The mode to be used for /dev/vpoxdrv
    INSTALLATION_DIR="$3" # The directory VirtualPox is installed in
    USB_GROUP="$4"        # The group that should own the /dev/vpoxusb device
                          # nodes unless INSTALL_NO_GROUP=1 in
                          # /etc/default/virtualpox.  Optional.
    usb_createnode="$INSTALLATION_DIR/VPoxCreateUSBNode.sh"
    # install udev rule (disable with INSTALL_NO_UDEV=1 in
    # /etc/default/virtualpox)
    if [ "$INSTALL_NO_GROUP" != "1" ]; then
        usb_group=$USB_GROUP
        vpoxdrv_group=$VPOXDRV_GRP
    else
        usb_group=root
        vpoxdrv_group=root
    fi
    install_udev "${vpoxdrv_group}" "$VPOXDRV_MODE" \
                 "$INSTALLATION_DIR" "${usb_group}" \
                 "$INSTALL_NO_UDEV" > ${udev_rule_file}
    # Build our device tree
    for i in ${sysfs_usb_devices}; do  # This line intentionally without quotes.
        install_create_usb_node_for_sysfs "$i" "${usb_createnode}" \
                                          "${usb_group}"
    done
}

cleanup_usb()
{
    # Remove udev description file
    rm -f /etc/udev/rules.d/60-vpoxdrv.rules
    rm -f /etc/udev/rules.d/10-vpoxdrv.rules

    # Remove our USB device tree
    rm -rf /dev/vpoxusb
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
    [ "$(module_available vpoxdrv)"    = "1" ] || return
    [ "$(module_available vpoxnetflt)" = "1" ] || return
    [ "$(module_available vpoxnetadp)" = "1" ] || return

    # All modules are in place.
    echo "1"
}

start()
{
    begin_msg "Starting VirtualPox services" console
    if [ -d /proc/xen ]; then
        failure "Running VirtualPox in a Xen environment is not supported"
    fi
    if test -n "$HAVE_SEC_BOOT" && test -z "$DEB_KEY_ENROLLED"; then
        if test -n "$HAVE_DEB_KEY"; then
            begin_msg "You must re-start your system to finish Debian secure boot set-up." console
        else
            begin_msg "You must sign these kernel modules before using VirtualPox:
  $MODULE_LIST
See the documentation for your Linux distribution." console
        fi
    fi

    if ! running vpoxdrv; then

        # Check if system already has matching modules installed.
        [ "$(setup_complete)" = "1" ] || setup

        if ! rm -f $DEVICE; then
            failure "Cannot remove $DEVICE"
        fi
        if ! $MODPROBE vpoxdrv > /dev/null 2>&1; then
            failure "modprobe vpoxdrv failed. Please use 'dmesg' to find out why"
        fi
        sleep .2
    fi
    # ensure the character special exists
    if [ ! -c $DEVICE ]; then
        MAJOR=`sed -n 's;\([0-9]\+\) vpoxdrv$;\1;p' /proc/devices`
        if [ ! -z "$MAJOR" ]; then
            MINOR=0
        else
            MINOR=`sed -n 's;\([0-9]\+\) vpoxdrv$;\1;p' /proc/misc`
            if [ ! -z "$MINOR" ]; then
                MAJOR=10
            fi
        fi
        if [ -z "$MAJOR" ]; then
            rmmod vpoxdrv 2>/dev/null
            failure "Cannot locate the VirtualPox device"
        fi
        if ! mknod -m 0660 $DEVICE c $MAJOR $MINOR 2>/dev/null; then
            rmmod vpoxdrv 2>/dev/null
            failure "Cannot create device $DEVICE with major $MAJOR and minor $MINOR"
        fi
    fi
    # ensure permissions
    if ! chown :"${GROUP}" $DEVICE 2>/dev/null; then
        rmmod vpoxpci 2>/dev/null
        rmmod vpoxnetadp 2>/dev/null
        rmmod vpoxnetflt 2>/dev/null
        rmmod vpoxdrv 2>/dev/null
        failure "Cannot change group ${GROUP} for device $DEVICE"
    fi
    if ! $MODPROBE vpoxnetflt > /dev/null 2>&1; then
        failure "modprobe vpoxnetflt failed. Please use 'dmesg' to find out why"
    fi
    if ! $MODPROBE vpoxnetadp > /dev/null 2>&1; then
        failure "modprobe vpoxnetadp failed. Please use 'dmesg' to find out why"
    fi
    if test -e "${MODULE_SRC}/vpoxpci" && ! $MODPROBE vpoxpci > /dev/null 2>&1; then
        failure "modprobe vpoxpci failed. Please use 'dmesg' to find out why"
    fi
    # Create the /dev/vpoxusb directory if the host supports that method
    # of USB access.  The USB code checks for the existance of that path.
    if grep -q usb_device /proc/devices; then
        mkdir -p -m 0750 /dev/vpoxusb 2>/dev/null
        chown root:vpoxusers /dev/vpoxusb 2>/dev/null
    fi
    # Remove any kernel modules left over from previously installed kernels.
    cleanup only_old
    succ_msg "VirtualPox services started"
}

stop()
{
    begin_msg "Stopping VirtualPox services" console

    if running vpoxpci; then
        if ! rmmod vpoxpci 2>/dev/null; then
            failure "Cannot unload module vpoxpci"
        fi
    fi
    if running vpoxnetadp; then
        if ! rmmod vpoxnetadp 2>/dev/null; then
            failure "Cannot unload module vpoxnetadp"
        fi
    fi
    if running vpoxdrv; then
        if running vpoxnetflt; then
            if ! rmmod vpoxnetflt 2>/dev/null; then
                failure "Cannot unload module vpoxnetflt"
            fi
        fi
        if ! rmmod vpoxdrv 2>/dev/null; then
            failure "Cannot unload module vpoxdrv"
        fi
        if ! rm -f $DEVICE; then
            failure "Cannot unlink $DEVICE"
        fi
    fi
    succ_msg "VirtualPox services stopped"
}

# enter the following variables in /etc/default/virtualpox:
#   SHUTDOWN_USERS="foo bar"
#     check for running VMs of user foo and user bar
#   SHUTDOWN=poweroff
#   SHUTDOWN=acpibutton
#   SHUTDOWN=savestate
#     select one of these shutdown methods for running VMs
stop_vms()
{
    wait=0
    for i in $SHUTDOWN_USERS; do
        # don't create the ipcd directory with wrong permissions!
        if [ -d /tmp/.vpox-$i-ipc ]; then
            export VPOX_IPC_SOCKETID="$i"
            VMS=`$VPOXMANAGE --nologo list runningvms | sed -e 's/^".*".*{\(.*\)}/\1/' 2>/dev/null`
            if [ -n "$VMS" ]; then
                if [ "$SHUTDOWN" = "poweroff" ]; then
                    begin_msg "Powering off remaining VMs"
                    for v in $VMS; do
                        $VPOXMANAGE --nologo controlvm $v poweroff
                    done
                    succ_msg "Remaining VMs powered off"
                elif [ "$SHUTDOWN" = "acpibutton" ]; then
                    begin_msg "Sending ACPI power button event to remaining VMs"
                    for v in $VMS; do
                        $VPOXMANAGE --nologo controlvm $v acpipowerbutton
                        wait=30
                    done
                    succ_msg "ACPI power button event sent to remaining VMs"
                elif [ "$SHUTDOWN" = "savestate" ]; then
                    begin_msg "Saving state of remaining VMs"
                    for v in $VMS; do
                        $VPOXMANAGE --nologo controlvm $v savestate
                    done
                    succ_msg "State of remaining VMs saved"
                fi
            fi
        fi
    done
    # wait for some seconds when doing ACPI shutdown
    if [ "$wait" -ne 0 ]; then
        begin_msg "Waiting for $wait seconds for VM shutdown"
        sleep $wait
        succ_msg "Waited for $wait seconds for VM shutdown"
    fi
}

cleanup()
{
    # If this is set, only remove kernel modules for no longer installed
    # kernels.  Note that only generated kernel modules should be placed
    # in /lib/modules/*/misc.  Anything that we should not remove automatically
    # should go elsewhere.
    only_old="${1}"
    for i in /lib/modules/*; do
        # Check whether we are only cleaning up for uninstalled kernels.
        test -n "${only_old}" && test -e "${i}/kernel/drivers" && continue
        # We could just do "rm -f", but we only want to try deleting folders if
        # we are sure they were ours, i.e. they had our modules in beforehand.
        if    test -e "${i}/misc/vpoxdrv.ko" \
           || test -e "${i}/misc/vpoxnetadp.ko" \
           || test -e "${i}/misc/vpoxnetflt.ko" \
           || test -e "${i}/misc/vpoxpci.ko"; then
            rm -f "${i}/misc/vpoxdrv.ko" "${i}/misc/vpoxnetadp.ko" \
                  "${i}/misc/vpoxnetflt.ko" "${i}/misc/vpoxpci.ko"
            version=`expr "${i}" : "/lib/modules/\(.*\)"`
            depmod -a "${version}"
            sync
        fi
        # Remove the kernel version folder if it was empty except for us.
        test   "`echo ${i}/misc/* ${i}/misc/.?* ${i}/* ${i}/.?*`" \
             = "${i}/misc/* ${i}/misc/.. ${i}/misc ${i}/.." &&
            rmdir "${i}/misc" "${i}"  # We used to leave empty folders.
    done
}

# setup_script
setup()
{
    begin_msg "Building VirtualPox kernel modules" console
    log "Building the main VirtualPox module."

    # Detect if kernel was built with clang.
    unset LLVM
    vpox_cc_is_clang=$(/lib/modules/"$KERN_VER"/build/scripts/config \
        --file /lib/modules/"$KERN_VER"/build/.config \
        --state CONFIG_CC_IS_CLANG 2>/dev/null)
    if test "${vpox_cc_is_clang}" = "y"; then
        log "Using clang compiler."
        export LLVM=1
    fi

    if ! myerr=`$BUILDINTMP \
        --save-module-symvers /tmp/vpoxdrv-Module.symvers \
        --module-source "$MODULE_SRC/vpoxdrv" \
        --no-print-directory install 2>&1`; then
        "${INSTALL_DIR}/check_module_dependencies.sh" || exit 1
        log "Error building the module:"
        module_build_log "$myerr"
        failure "Look at $LOG to find out what went wrong"
    fi
    log "Building the net filter module."
    if ! myerr=`$BUILDINTMP \
        --use-module-symvers /tmp/vpoxdrv-Module.symvers \
        --module-source "$MODULE_SRC/vpoxnetflt" \
        --no-print-directory install 2>&1`; then
        log "Error building the module:"
        module_build_log "$myerr"
        failure "Look at $LOG to find out what went wrong"
    fi
    log "Building the net adaptor module."
    if ! myerr=`$BUILDINTMP \
        --use-module-symvers /tmp/vpoxdrv-Module.symvers \
        --module-source "$MODULE_SRC/vpoxnetadp" \
        --no-print-directory install 2>&1`; then
        log "Error building the module:"
        module_build_log "$myerr"
        failure "Look at $LOG to find out what went wrong"
    fi
    if test -e "$MODULE_SRC/vpoxpci"; then
        log "Building the PCI pass-through module."
        if ! myerr=`$BUILDINTMP \
            --use-module-symvers /tmp/vpoxdrv-Module.symvers \
            --module-source "$MODULE_SRC/vpoxpci" \
            --no-print-directory install 2>&1`; then
            log "Error building the module:"
            module_build_log "$myerr"
            failure "Look at $LOG to find out what went wrong"
        fi
    fi
    rm -f /etc/vpox/module_not_compiled
    depmod -a
    sync
    succ_msg "VirtualPox kernel modules built"
    # Secure boot on Ubuntu and Debian.
    if test -n "$HAVE_SEC_BOOT" &&
        type update-secureboot-policy >/dev/null 2>&1; then
        SHIM_NOTRIGGER=y update-secureboot-policy --new-key
    fi
    if test -f "$DEB_PUB_KEY" && test -f "$DEB_PRIV_KEY"; then
        HAVE_DEB_KEY=true
        for i in $MODULE_LIST; do
            kmodsign sha512 /var/lib/shim-signed/mok/MOK.priv \
                /var/lib/shim-signed/mok/MOK.der \
                /lib/modules/"$KERN_VER"/misc/"$i".ko
        done
        # update-secureboot-policy "expects" DKMS modules.
        # Work around this and talk to the authors as soon
        # as possible to fix it.
        mkdir -p /var/lib/dkms/vpox-temp
        update-secureboot-policy --enroll-key 2>/dev/null ||
            begin_msg "Failed to enroll secure boot key." console
        rmdir -p /var/lib/dkms/vpox-temp 2>/dev/null
    fi
}

dmnstatus()
{
    if running vpoxdrv; then
        str="vpoxdrv"
        if running vpoxnetflt; then
            str="$str, vpoxnetflt"
            if running vpoxnetadp; then
                str="$str, vpoxnetadp"
            fi
        fi
        if running vpoxpci; then
            str="$str, vpoxpci"
        fi
        echo "VirtualPox kernel modules ($str) are loaded."
        for i in $SHUTDOWN_USERS; do
            # don't create the ipcd directory with wrong permissions!
            if [ -d /tmp/.vpox-$i-ipc ]; then
                export VPOX_IPC_SOCKETID="$i"
                VMS=`$VPOXMANAGE --nologo list runningvms | sed -e 's/^".*".*{\(.*\)}/\1/' 2>/dev/null`
                if [ -n "$VMS" ]; then
                    echo "The following VMs are currently running:"
                    for v in $VMS; do
                       echo "  $v"
                    done
                fi
            fi
        done
    else
        echo "VirtualPox kernel module is not loaded."
    fi
}

case "$1" in
start)
    start
    ;;
stop)
    stop_vms
    stop
    ;;
stop_vms)
    stop_vms
    ;;
restart)
    stop && start
    ;;
setup)
    test -n "${2}" && export KERN_VER="${2}"
    # Create udev rule and USB device nodes.
    ## todo Wouldn't it make more sense to install the rule to /lib/udev?  This
    ## is not a user-created configuration file after all.
    ## todo Do we need a udev rule to create /dev/vpoxdrv[u] at all?  We have
    ## working fall-back code here anyway, and the "right" code is more complex
    ## than the fall-back.  Unnecessary duplication?
    stop && cleanup
    setup_usb "$GROUP" "$DEVICE_MODE" "$INSTALL_DIR"
    start
    ;;
cleanup)
    stop && cleanup
    cleanup_usb
    ;;
force-reload)
    stop
    start
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|stop_vms|restart|setup|cleanup|force-reload|status}"
    exit 1
esac

exit 0
