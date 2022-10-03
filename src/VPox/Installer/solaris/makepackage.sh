#!/bin/sh
# $Id: makepackage.sh $
## @file
# VirtualPox package creation script, Solaris hosts.
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

#
# Usage:
#       makepackage.sh [--hardened] [--ips] $(PATH_TARGET)/install packagename {$(KBUILD_TARGET_ARCH)|neutral} $(VPOX_SVN_REV)


# Parse options.
HARDENED=""
IPS_PACKAGE=""
PACKAGE_SPEC="prototype"
while [ $# -ge 1 ];
do
    case "$1" in
        --hardened)
            HARDENED=1
            ;;
        --ips)
            IPS_PACKAGE=1
            PACKAGE_SPEC="virtualpox.p5m"
            ;;
    *)
        break
        ;;
    esac
    shift
done

if [ -z "$4" ]; then
    echo "Usage: $0 installdir packagename x86|amd64 svnrev"
    echo "-- packagename must not have any extension (e.g. VirtualPox-SunOS-amd64-r28899)"
    exit 1
fi

PKG_BASE_DIR="$1"
PACKAGE_SPEC="$PKG_BASE_DIR/$PACKAGE_SPEC"
VPOX_INSTALLED_DIR=/opt/VirtualPox
if [ -n "$IPS_PACKAGE" ]; then
    VPOX_PKGFILE="$2".p5p
else
    VPOX_PKGFILE="$2".pkg
fi
# VPOX_PKG_ARCH is currently unused.
VPOX_PKG_ARCH="$3"
VPOX_SVN_REV="$4"

if [ -n "$IPS_PACKAGE" ] ; then
    VPOX_PKGNAME=system/virtualpox
else
    VPOX_PKGNAME=SUNWvpox
fi
# any egrep should do the job, the one from /usr/xpg4/bin isn't required
VPOX_EGREP=/usr/bin/egrep
# need dynamic regex support which isn't available in S11 /usr/bin/awk
VPOX_AWK=/usr/xpg4/bin/awk

# bail out on non-zero exit status
set -e

if [ -n "$IPS_PACKAGE" ]; then

package_spec_create()
{
    > "$PACKAGE_SPEC"
}

package_spec_append_info()
{
    : # provided by vpox-ips.mog
}

package_spec_append_content()
{
    rm -rf "$1/vpox-repo"
    pkgsend generate "$1" | pkgfmt >> "$PACKAGE_SPEC"
}

package_spec_append_hardlink()
{
    if [ -f "$3$4/amd64/$2" -o -f "$3$4/i386/$2" ]; then
        echo "hardlink path=$4/$2 target=$1" >> "$PACKAGE_SPEC"
    fi
}

package_spec_fixup_content()
{
    :
}

package_create()
{
    VPOX_DEF_HARDENED=
    [ -z "$HARDENED" ] && VPOX_DEF_HARDENED='#'

    pkgmogrify -DVPOX_PKGNAME="$VPOX_PKGNAME" -DHARDENED_ONLY="$VPOX_DEF_HARDENED" "$PACKAGE_SPEC" "$1/vpox-ips.mog" | pkgfmt > "$PACKAGE_SPEC.1"

    pkgdepend generate -m -d "$1" "$PACKAGE_SPEC.1" | pkgfmt > "$PACKAGE_SPEC.2"

    pkgdepend resolve -m "$PACKAGE_SPEC.2"

    # Too expensive, and in this form not useful since it does not have
    # the package manifests without using options -r (for repo access) and
    # -c (for caching the data). Not viable since the cache would be lost
    # for every build.
    #pkglint "$PACKAGE_SPEC.2.res"

    pkgrepo create "$1/vpox-repo"
    pkgrepo -s "$1/vpox-repo" set publisher/prefix=virtualpox

    # Create package in local file repository
    pkgsend -s "$1/vpox-repo" publish -d "$1" "$PACKAGE_SPEC.2.res"

    pkgrepo -s "$1/vpox-repo" info
    pkgrepo -s "$1/vpox-repo" list

    # Convert into package archive
    rm -f "$1/$2"
    pkgrecv -a -s "$1/vpox-repo" -d "$1/$2" -m latest "$3"
    rm -rf "$1/vpox-repo"
}

else

package_spec_create()
{
    > "$PACKAGE_SPEC"
}

package_spec_append_info()
{
    echo 'i pkginfo=vpox.pkginfo' >> "$PACKAGE_SPEC"
    echo 'i checkinstall=checkinstall.sh' >> "$PACKAGE_SPEC"
    echo 'i postinstall=postinstall.sh' >> "$PACKAGE_SPEC"
    echo 'i preremove=preremove.sh' >> "$PACKAGE_SPEC"
    echo 'i space=vpox.space' >> "$PACKAGE_SPEC"
}

# Our package is a non-relocatable package.
#
# pkgadd will take care of "relocating" them when they are used for remote installations using
# $PKG_INSTALL_ROOT and not $BASEDIR. Seems this little subtlety led to it's own page:
# https://docs.oracle.com/cd/E19253-01/820-4042/package-2/index.html

package_spec_append_content()
{
    cd "$1"
    # Exclude directories to not cause install-time conflicts with existing system directories
    find . ! -type d | "$VPOX_EGREP" -v '^\./(LICENSE|prototype|makepackage\.sh|vpox\.pkginfo|postinstall\.sh|checkinstall\.sh|preremove\.sh|vpox\.space|vpox-ips.mog|virtualpox\.p5m.*)$' | LC_COLLATE=C sort | pkgproto >> "$PACKAGE_SPEC"
    cd -
    "$VPOX_AWK" 'NF == 3 && $1 == "s" && $2 == "none" { $3="/"$3 } { print }' "$PACKAGE_SPEC" > "$PACKAGE_SPEC.tmp"
    mv -f "$PACKAGE_SPEC.tmp" "$PACKAGE_SPEC"
    "$VPOX_AWK" 'NF == 6 && ($1 == "f" || $1 == "l") && ($2 == "none" || $2 == "manifest") { $3="/"$3 } { print }' "$PACKAGE_SPEC" > "$PACKAGE_SPEC.tmp"
    mv -f "$PACKAGE_SPEC.tmp" "$PACKAGE_SPEC"

    cd "$1"
    # Include opt/VirtualPox and subdirectories as we want uninstall to clean up directory structure.
    # Include var/svc for manifest class action script does not create them.
    find . -type d | "$VPOX_EGREP" 'opt/VirtualPox|var/svc/manifest/application/virtualpox' | LC_COLLATE=C sort | pkgproto >> "$PACKAGE_SPEC"
    cd -
    "$VPOX_AWK" 'NF == 6 && $1 == "d" && $2 == "none" { $3="/"$3 } { print }' "$PACKAGE_SPEC" > "$PACKAGE_SPEC.tmp"
    mv -f "$PACKAGE_SPEC.tmp" "$PACKAGE_SPEC"
}

package_spec_append_hardlink()
{
    if [ -f "$3$4/amd64/$2" -o -f "$3$4/i386/$2" ]; then
        echo "l none $4/$2=$1" >> "$PACKAGE_SPEC"
    fi
}

# Fixup filelist using awk, the parameters must be in awk syntax
# params: filename condition action
package_spec_fixup_filelist()
{
    "$VPOX_AWK" 'NF == 6 && '"$1"' { '"$2"' } { print }' "$PACKAGE_SPEC" > "$PACKAGE_SPEC.tmp"
    mv -f "$PACKAGE_SPEC.tmp" "$PACKAGE_SPEC"
}

package_spec_fixup_dirlist()
{
    "$VPOX_AWK" 'NF == 6 && $1 == "d" && '"$1"' { '"$2"' } { print }' "$PACKAGE_SPEC" > "$PACKAGE_SPEC.tmp"
    mv -f "$PACKAGE_SPEC.tmp" "$PACKAGE_SPEC"
}

package_spec_fixup_content()
{
    # fix up file permissions (owner/group)
    # don't grok for class-specific files (like sed, if any)
    package_spec_fixup_filelist '$2 == "none"'                                                                  '$5 = "root"; $6 = "bin"'

    # HostDriver vpoxdrv
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/vpoxdrv.conf"'                               '$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/amd64/vpoxdrv"'                              '$6 = "sys"'

    # NetFilter vpoxflt
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/vpoxflt.conf"'                               '$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/amd64/vpoxflt"'                              '$6 = "sys"'

    # NetFilter vpoxbow
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/vpoxbow.conf"'                               '$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/amd64/vpoxbow"'                              '$6 = "sys"'

    # NetAdapter vpoxnet
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/vpoxnet.conf"'                               '$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/amd64/vpoxnet"'                              '$6 = "sys"'

    # USBMonitor vpoxusbmon
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/vpoxusbmon.conf"'                            '$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/amd64/vpoxusbmon"'                           '$6 = "sys"'

    # USB Client vpoxusb
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/vpoxusb.conf"'                               '$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/platform/i86pc/kernel/drv/amd64/vpoxusb"'                              '$6 = "sys"'

    # Manifest class action scripts
    package_spec_fixup_filelist '$3 == "/var/svc/manifest/application/virtualpox/virtualpox-webservice.xml"'    '$2 = "manifest";$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/var/svc/manifest/application/virtualpox/virtualpox-balloonctrl.xml"'   '$2 = "manifest";$6 = "sys"'
    package_spec_fixup_filelist '$3 == "/var/svc/manifest/application/virtualpox/virtualpox-zoneaccess.xml"'    '$2 = "manifest";$6 = "sys"'

    # Use 'root' as group so as to match attributes with the previous installation and prevent a conflict. Otherwise pkgadd bails out thinking
    # we're violating directory attributes of another (non existing) package
    package_spec_fixup_dirlist '$3 == "/var/svc/manifest/application/virtualpox"'                               '$6 = "root"'

    # Hardening requires some executables to be marked setuid.
    if [ -n "$HARDENED" ]; then
        package_spec_fixup_filelist '(   $3 == "/opt/VirtualPox/amd64/VirtualPoxVM" \
                                      || $3 == "/opt/VirtualPox/amd64/VPoxHeadless" \
                                      || $3 == "/opt/VirtualPox/amd64/VPoxSDL" \
                                      || $3 == "/opt/VirtualPox/i386/VirtualPox" \
                                      || $3 == "/opt/VirtualPox/i386/VPoxHeadless" \
                                      || $3 == "/opt/VirtualPox/i386/VPoxSDL" )'                                '$4 = "4755"'
    fi

    # Other executables that need setuid root (hardened or otherwise)
    package_spec_fixup_filelist '(   $3 == "/opt/VirtualPox/amd64/VPoxNetAdpCtl" \
                                  || $3 == "/opt/VirtualPox/i386/VPoxNetAdpCtl" \
                                  || $3 == "/opt/VirtualPox/amd64/VPoxNetDHCP" \
                                  || $3 == "/opt/VirtualPox/i386/VPoxNetDHCP" \
                                  || $3 == "/opt/VirtualPox/amd64/VPoxNetNAT" \
                                  || $3 == "/opt/VirtualPox/i386/VPoxNetNAT" )'                                 '$4 = "4755"'

    echo " --- start of $PACKAGE_SPEC  ---"
    cat "$PACKAGE_SPEC"
    echo " --- end of $PACKAGE_SPEC ---"
}

package_create()
{
    # Create the package instance
    pkgmk -o -f "$PACKAGE_SPEC" -r "$1"

    # Translate into package datastream
    pkgtrans -s -o /var/spool/pkg "$1/$2" "$3"

    rm -rf "/var/spool/pkg/$2"
}

fi


# Prepare package spec
package_spec_create

# Metadata
package_spec_append_info "$PKG_BASE_DIR"

# File and direcory list
package_spec_append_content "$PKG_BASE_DIR"

# Add hardlinks for executables to launch the 32-bit or 64-bit executable
for f in VPoxManage VPoxSDL VPoxAutostart vpoxwebsrv VPoxZoneAccess VPoxSVC VPoxBugReport VPoxBalloonCtrl VPoxTestOGL VirtualPox VirtualPoxVM vpox-img VPoxHeadless; do
    package_spec_append_hardlink VPoxISAExec    $f  "$PKG_BASE_DIR" "$VPOX_INSTALLED_DIR"
done

package_spec_fixup_content

package_create "$PKG_BASE_DIR" "$VPOX_PKGFILE" "$VPOX_PKGNAME" "$VPOX_SVN_REV"

echo "## Package file created successfully!"

exit $?
