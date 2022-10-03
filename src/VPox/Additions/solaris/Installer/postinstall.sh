#!/bin/sh
# $Id: postinstall.sh $
## @file
# VirtualPox postinstall script for Solaris Guest Additions.
#

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
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualPox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

# LC_ALL should take precedence over LC_* and LANG but whatever...
LC_ALL=C
export LC_ALL

LANG=C
export LANG

# uncompress(directory, file)
# Updates package metadata and uncompresses the file.
uncompress_file()
{
    if test -z "$1" || test -z "$2"; then
        echo "missing argument to uncompress_file()"
        return 1
    fi

    # Remove compressed path from the pkg
    /usr/sbin/removef $PKGINST "$1/$2.Z" 1>/dev/null

    # Add uncompressed path to the pkg
    /usr/sbin/installf -c none $PKGINST "$1/$2" f

    # Uncompress the file (removes compressed file when done)
    uncompress -f "$1/$2.Z" > /dev/null 2>&1
}

# uncompress_files(directory_with_*.Z_files)
uncompress_files()
{
    for i in "${1}/"*.Z; do
        uncompress_file "${1}" "`basename \"${i}\" .Z`"
    done
}

solaris64dir="amd64"
solaris32dir="i386"
vpoxadditions_path="$BASEDIR/opt/VirtualPoxAdditions"
vpoxadditions32_path=$vpoxadditions_path/$solaris32dir
vpoxadditions64_path=$vpoxadditions_path/$solaris64dir

# get the current zone
currentzone=`zonename`
# get what ISA the guest is running
cputype=`isainfo -k`
if test "$cputype" = "amd64"; then
    isadir=$solaris64dir
else
    isadir=""
fi

vpoxadditionsisa_path=$vpoxadditions_path/$isadir


# uncompress if necessary
if test -f "$vpoxadditions32_path/VPoxClient.Z" || test -f "$vpoxadditions64_path/VPoxClient.Z"; then
    echo "Uncompressing files..."
    if test -f "$vpoxadditions32_path/VPoxClient.Z"; then
        uncompress_files "$vpoxadditions32_path"
    fi
    if test -f "$vpoxadditions64_path/VPoxClient.Z"; then
        uncompress_files "$vpoxadditions64_path"
    fi
fi


if test "$currentzone" = "global"; then
    # vpoxguest.sh would've been installed, we just need to call it.
    echo "Configuring VirtualPox guest kernel module..."
    # stop all previous modules (vpoxguest, vpoxfs) and start only starts vpoxguest
    $vpoxadditions_path/vpoxguest.sh stopall silentunload
    $vpoxadditions_path/vpoxguest.sh start

    # Figure out group to use for /etc/devlink.tab (before Solaris 11 SRU6
    # it was always using group sys)
    group=sys
    if [ -f /etc/dev/reserved_devnames ]; then
        # Solaris 11 SRU6 and later use group root (check a file which isn't
        # tainted by VirtualPox install scripts and allow no other group)
        refgroup=`LC_ALL=C /usr/bin/ls -lL /etc/dev/reserved_devnames | awk '{ print $4 }' 2>/dev/null`
        if [ $? -eq 0 -a "x$refgroup" = "xroot" ]; then
            group=root
        fi
        unset refgroup
    fi

    sed -e '/name=vpoxguest/d' /etc/devlink.tab > /etc/devlink.vpox
    echo "type=ddi_pseudo;name=vpoxguest	\D" >> /etc/devlink.vpox
    chmod 0644 /etc/devlink.vpox
    chown root:$group /etc/devlink.vpox
    mv -f /etc/devlink.vpox /etc/devlink.tab

    # create the device link
    /usr/sbin/devfsadm -i vpoxguest
fi


# check if X.Org exists (snv_130 and higher have /usr/X11/* as /usr/*)
if test -f "/usr/bin/Xorg"; then
    xorgbin="/usr/bin/Xorg"
elif test -f "/usr/X11/bin/Xorg"; then
    xorgbin="/usr/X11/bin/Xorg"
else
    xorgbin=""
    retval=0
fi

# create links
echo "Creating links..."
if test "$currentzone" = "global"; then
    /usr/sbin/installf -c none $PKGINST /dev/vpoxguest=../devices/pci@0,0/pci80ee,cafe@4:vpoxguest s
    /usr/sbin/installf -c none $PKGINST /dev/vpoxms=../devices/pseudo/vpoxms@0:vpoxms s
fi

# Install Xorg components to the required places
if test ! -z "$xorgbin"; then
    xorgversion_long=`$xorgbin -version 2>&1 | grep "X Window System Version"`
    xorgversion=`/usr/bin/expr "${xorgversion_long}" : 'X Window System Version \([^ ]*\)'`
    if test -z "$xorgversion_long"; then
        xorgversion_long=`$xorgbin -version 2>&1 | grep "X.Org X Server"`
        xorgversion=`/usr/bin/expr "${xorgversion_long}" : 'X.Org X Server \([^ ]*\)'`
    fi

    # "X.Y.Z" - strip off all numerics after the 2nd '.' character, e.g. "1.11.3" -> "1.11"
    # Then the next sed, strips of all '.' characters, "1.11" -> "111".
    fileversion=`echo $xorgversion | sed "s/\.[0-9]*//2" | sed "s/\.//"`
    vpoxvideo_src="vpoxvideo_drv_$fileversion.so"

    # Handle exceptions now where the X.org version does not exactly match the file-version.
    case "$xorgversion" in
        1.5.99 )
            vpoxvideo_src="vpoxvideo_drv_16.so"
            ;;
        7.2.* )
            vpoxvideo_src="vpoxvideo_drv_71.so"
            ;;
        6.9.* )
            vpoxvideo_src="vpoxvideo_drv_70.so"
            ;;
    esac

    retval=0
    if test -z "$vpoxvideo_src"; then
        echo "*** Unknown version of the X Window System installed."
        echo "*** Failed to install the VirtualPox X Window System drivers."

        # Exit as partially failed installation
        retval=2
    elif test ! -f "$vpoxadditions32_path/$vpoxvideo_src" && test ! -f "$vpoxadditions64_path/$vpoxvideo_src"; then
        # Xorg 1.19 and later already contain a driver for vpoxvideo.
        echo "As of X.Org Server 1.19, the VirtualPox graphics driver (vpoxvideo) is part"
        echo "of Solaris.  Please install it from the package repository if necessary."
    else
        echo "Installing video driver for X.Org $xorgversion..."

        # Determine destination paths (snv_130 and above use "/usr/lib/xorg", older use "/usr/X11/lib"
        vpoxvideo32_dest_base="/usr/lib/xorg/modules/drivers"
        if test ! -d $vpoxvideo32_dest_base; then
            vpoxvideo32_dest_base="/usr/X11/lib/modules/drivers"
        fi

        vpoxvideo64_dest_base=$vpoxvideo32_dest_base/$solaris64dir

        # snv_163 drops 32-bit support completely, and uses 32-bit locations for the 64-bit stuff. Ugly.
        # We try to detect this by looking at bitness of "vesa_drv.so", and adjust our destination paths accordingly.
        # We do not rely on using Xorg -version's ABI output because some builds (snv_162 iirc) have 64-bit ABI with
        # 32-bit file locations.
        if test -f "$vpoxvideo32_dest_base/vesa_drv.so"; then
            bitsize=`file "$vpoxvideo32_dest_base/vesa_drv.so" | grep -i "32-bit"`
            skip32="no"
        else
            echo "* Warning vesa_drv.so missing. Assuming Xorg ABI is 64-bit..."
        fi

        if test -z "$bitsize"; then
            skip32="yes"
            vpoxvideo64_dest_base=$vpoxvideo32_dest_base
        fi

        # Make sure destination path exists
        if test ! -d $vpoxvideo64_dest_base; then
            echo "*** Missing destination paths for video module. Aborting."
            echo "*** Failed to install the VirtualPox X Window System driver."

            # Exit as partially failed installation
            retval=2
        else
            # 32-bit x11 drivers
            if test "$skip32" = "no" && test -f "$vpoxadditions32_path/$vpoxvideo_src"; then
                vpoxvideo_dest="$vpoxvideo32_dest_base/vpoxvideo_drv.so"
                /usr/sbin/installf -c none $PKGINST "$vpoxvideo_dest" f
                cp "$vpoxadditions32_path/$vpoxvideo_src" "$vpoxvideo_dest"

                # Removing redundant names from pkg and files from disk
                /usr/sbin/removef $PKGINST $vpoxadditions32_path/vpoxvideo_drv_* 1>/dev/null
                rm -f $vpoxadditions32_path/vpoxvideo_drv_*
            fi

            # 64-bit x11 drivers
            if test -f "$vpoxadditions64_path/$vpoxvideo_src"; then
                vpoxvideo_dest="$vpoxvideo64_dest_base/vpoxvideo_drv.so"
                /usr/sbin/installf -c none $PKGINST "$vpoxvideo_dest" f
                cp "$vpoxadditions64_path/$vpoxvideo_src" "$vpoxvideo_dest"

                # Removing redundant names from pkg and files from disk
                /usr/sbin/removef $PKGINST $vpoxadditions64_path/vpoxvideo_drv_* 1>/dev/null
                rm -f $vpoxadditions64_path/vpoxvideo_drv_*
            fi

            # Some distros like Indiana have no xorg.conf, deal with this
            if test ! -f '/etc/X11/xorg.conf' && test ! -f '/etc/X11/.xorg.conf'; then

                # Xorg 1.3.x+ should use the modeline less Xorg confs while older should
                # use ones with all the video modelines in place. Argh.
                xorgconf_file="solaris_xorg_modeless.conf"
                xorgconf_unfit="solaris_xorg.conf"
                case "$xorgversion" in
                    7.1.* | 7.2.* | 6.9.* | 7.0.* )
                        xorgconf_file="solaris_xorg.conf"
                        xorgconf_unfit="solaris_xorg_modeless.conf"
                        ;;
                esac

                /usr/sbin/removef $PKGINST $vpoxadditions_path/$xorgconf_file 1>/dev/null
                mv -f $vpoxadditions_path/$xorgconf_file /etc/X11/.xorg.conf

                /usr/sbin/removef $PKGINST $vpoxadditions_path/$xorgconf_unfit 1>/dev/null
                rm -f $vpoxadditions_path/$xorgconf_unfit
            fi

            # Check for VirtualPox graphics card
            # S10u10's prtconf doesn't support the '-d' option, so let's use -v even though it's slower.
            is_vpoxgraphics=`prtconf -v | grep -i pci80ee,beef`
            if test "$?" -eq 0; then
                drivername="vpoxvideo"
            else
                # Check for VMware graphics card
                is_vmwaregraphics=`prtconf -v | grep -i pci15ad,405`
                if test "$?" -eq 0; then
                    echo "Configuring X.Org to use VMware SVGA graphics driver..."
                    drivername="vmware"
                fi
            fi

            # Adjust xorg.conf with video driver sections if a supported graphics card is found
            if test ! -z "$drivername"; then
                $vpoxadditions_path/x11config15sol.pl "$drivername"
            else
                # No supported graphics card found, do nothing.
                echo "## No supported graphics card found. Skipped configuring of X.org drivers."
            fi
        fi
    fi


    # Setup our VPoxClient
    echo "Configuring client..."
    vpoxclient_src=$vpoxadditions_path
    vpoxclient_dest="/usr/share/gnome/autostart"
    clientinstalled=0
    if test -d "$vpoxclient_dest"; then
        /usr/sbin/installf -c none $PKGINST $vpoxclient_dest/vpoxclient.desktop=$vpoxadditions_path/vpoxclient.desktop s
        clientinstalled=1
    fi
    vpoxclient_dest="/usr/dt/config/Xsession.d"
    if test -d "$vpoxclient_dest"; then
        /usr/sbin/installf -c none $PKGINST $vpoxclient_dest/1099.vpoxclient=$vpoxadditions_path/1099.vpoxclient s
        clientinstalled=1
    fi

    # Try other autostart locations if none of the above ones work
    if test $clientinstalled -eq 0; then
        vpoxclient_dest="/etc/xdg/autostart"
        if test -d "$vpoxclient_dest"; then
            /usr/sbin/installf -c none $PKGINST $vpoxclient_dest/1099.vpoxclient=$vpoxadditions_path/1099.vpoxclient s
            clientinstalled=1
        else
            echo "*** Failed to configure client, couldn't find any autostart directory!"
            # Exit as partially failed installation
            retval=2
        fi
    fi
else
    echo "(*) X.Org not found, skipped configuring X.Org guest additions."
fi


# Shared Folder kernel module (different for S10 & Nevada)
osverstr=`uname -r`
vpoxfsmod="vpoxfs"
vpoxfsunused="vpoxfs_s10"
if test "$osverstr" = "5.10"; then
    vpoxfsmod="vpoxfs_s10"
    vpoxfsunused="vpoxfs"
fi

# Move the appropriate module to kernel/fs & remove the unused module name from pkg and file from disk
# 64-bit shared folder module
if test -f "$vpoxadditions64_path/$vpoxfsmod"; then
    echo "Installing 64-bit shared folders module..."
    /usr/sbin/installf -c none $PKGINST "/usr/kernel/fs/$solaris64dir/vpoxfs" f
    mv -f $vpoxadditions64_path/$vpoxfsmod /usr/kernel/fs/$solaris64dir/vpoxfs
    /usr/sbin/removef $PKGINST $vpoxadditions64_path/$vpoxfsmod 1>/dev/null
    /usr/sbin/removef $PKGINST $vpoxadditions64_path/$vpoxfsunused 1>/dev/null
    rm -f $vpoxadditions64_path/$vpoxfsunused
fi

# 32-bit shared folder module
if test -f "$vpoxadditions32_path/$vpoxfsmod"; then
    echo "Installing 32-bit shared folders module..."
    /usr/sbin/installf -c none $PKGINST "/usr/kernel/fs/vpoxfs" f
    mv -f $vpoxadditions32_path/$vpoxfsmod /usr/kernel/fs/vpoxfs
    /usr/sbin/removef $PKGINST $vpoxadditions32_path/$vpoxfsmod 1>/dev/null
    /usr/sbin/removef $PKGINST $vpoxadditions32_path/$vpoxfsunused 1>/dev/null
    rm -f $vpoxadditions32_path/$vpoxfsunused
fi

# Add a group "vpoxsf" for Shared Folders access
# All users which want to access the auto-mounted Shared Folders have to
# be added to this group.
groupadd vpoxsf >/dev/null 2>&1

# Move the pointer integration module to kernel/drv & remove the unused module name from pkg and file from disk

# Finalize
/usr/sbin/removef -f $PKGINST
/usr/sbin/installf -f $PKGINST


if test "$currentzone" = "global"; then
    /usr/sbin/devfsadm -i vpoxguest

    # Setup VPoxService and vpoxmslnk and start the services automatically
    echo "Configuring services (this might take a while)..."
    cmax=32
    cslept=0
    success=0
    sync

    # Since S11 the way to import a manifest is via restarting manifest-import which is asynchronous and can
    # take a while to complete, using disable/enable -s doesn't work either. So we restart it, and poll in
    # 1 second intervals to see if our service has been successfully imported and timeout after 'cmax' seconds.
    /usr/sbin/svcadm restart svc:system/manifest-import:default
    /usr/bin/svcs virtualpox/vpoxservice >/dev/null 2>&1 && /usr/bin/svcs virtualpox/vpoxmslnk >/dev/null 2>&1
    while test "$?" -ne 0;
    do
        sleep 1
        cslept=`expr $cslept + 1`
        if test "$cslept" -eq "$cmax"; then
            success=1
            break
        fi
        /usr/bin/svcs virtualpox/vpoxservice >/dev/null 2>&1 && /usr/bin/svcs virtualpox/vpoxmslnk >/dev/null 2>&1
    done
    if test "$success" -eq 0; then
        echo "Enabling services..."
        /usr/sbin/svcadm enable -s virtualpox/vpoxservice
        /usr/sbin/svcadm enable -s virtualpox/vpoxmslnk
    else
        echo "## Service import failed."
        echo "## See /var/svc/log/system-manifest-import:default.log for details."
        # Exit as partially failed installation
        retval=2
    fi

    # Update boot archive
    BOOTADMBIN=/sbin/bootadm
    if test -x "$BOOTADMBIN"; then
        if test -h "/dev/vpoxguest"; then
            echo "Updating boot archive..."
            $BOOTADMBIN update-archive > /dev/null
        else
            echo "## Guest kernel module doesn't seem to be up. Skipped explicit boot-archive update."
        fi
    else
        echo "## $BOOTADMBIN not found/executable. Skipped explicit boot-archive update."
    fi
fi

echo "Done."
if test $retval -eq 0; then
    if test ! -z "$xorgbin"; then
        echo "Please re-login to activate the X11 guest additions."
    fi
    echo "If you have just un-installed the previous guest additions a REBOOT is required."
fi
exit $retval

