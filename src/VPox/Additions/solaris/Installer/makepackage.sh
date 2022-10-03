#!/bin/sh
# $Id: makepackage.sh $
## @file
# VirtualPox Solaris Guest Additions package creation script.
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

#
# Usage:
#       makespackage.sh $(PATH_TARGET)/install packagename svnrev

if test -z "$3"; then
    echo "Usage: $0 installdir packagename svnrev"
    exit 1
fi
ostype=`uname -s`
if test "$ostype" != "Linux" && test "$ostype" != "SunOS" ; then
  echo "Linux/Solaris not detected."
  exit 1
fi

VPOX_BASEPKG_DIR=$1
VPOX_INSTALLED_DIR="$VPOX_BASEPKG_DIR"/opt/VirtualPoxAdditions
VPOX_PKGFILENAME=$2
VPOX_SVN_REV=$3

VPOX_PKGNAME=SUNWvpoxguest
VPOX_AWK=/usr/bin/awk
case "$ostype" in
"SunOS")
  VPOX_GGREP=/usr/sfw/bin/ggrep
  VPOX_SOL_PKG_DEV=/var/spool/pkg
  ;;
*)
  VPOX_GGREP=`which grep`
  VPOX_SOL_PKG_DEV=$4
  ;;
esac
VPOX_AWK=/usr/bin/awk

# check for GNU grep we use which might not ship with all Solaris
if test ! -f "$VPOX_GGREP" && test ! -h "$VPOX_GGREP"; then
    echo "## GNU grep not found in $VPOX_GGREP."
    exit 1
fi

# bail out on non-zero exit status
set -e

# Fixup filelist using awk, the parameters must be in awk syntax
# params: filename condition action
filelist_fixup()
{
    "$VPOX_AWK" 'NF == 6 && '"$2"' { '"$3"' } { print }' "$1" > "tmp-$1"
    mv -f "tmp-$1" "$1"
}

dirlist_fixup()
{
  "$VPOX_AWK" 'NF == 6 && $1 == "d" && '"$2"' { '"$3"' } { print }' "$1" > "tmp-$1"
  mv -f "tmp-$1" "$1"
}

# Create relative hardlinks
cd "$VPOX_INSTALLED_DIR"
ln -f ./VPoxISAExec $VPOX_INSTALLED_DIR/VPoxService
ln -f ./VPoxISAExec $VPOX_INSTALLED_DIR/VPoxClient
ln -f ./VPoxISAExec $VPOX_INSTALLED_DIR/VPoxControl
ln -f ./VPoxISAExec $VPOX_INSTALLED_DIR/vpoxmslnk

# prepare file list
cd "$VPOX_BASEPKG_DIR"
echo 'i pkginfo=./vpoxguest.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vpoxguest.space' >> prototype
echo 'i depend=./vpoxguest.depend' >> prototype
if test -f "./vpoxguest.copyright"; then
    echo 'i copyright=./vpoxguest.copyright' >> prototype
fi

# Exclude directory entries to not cause conflicts (owner,group) with existing directories in the system
find . ! -type d | $VPOX_GGREP -v -E 'prototype|makepackage.sh|vpoxguest.pkginfo|postinstall.sh|preremove.sh|vpoxguest.space|vpoxguest.depend|vpoxguest.copyright' | pkgproto >> prototype

# Include opt/VirtualPoxAdditions and subdirectories as we want uninstall to clean up directory structure as well
find . -type d | $VPOX_GGREP -E 'opt/VirtualPoxAdditions|var/svc/manifest/application/virtualpox' | pkgproto >> prototype

# Include /etc/fs/vpoxfs (as we need to create the subdirectory)
find . -type d | $VPOX_GGREP -E 'etc/fs/vpoxfs' | pkgproto >> prototype


# don't grok for the class files
filelist_fixup prototype '$2 == "none"'                                                                      '$5 = "root"; $6 = "bin"'

# VPoxService requires suid
filelist_fixup prototype '$3 == "opt/VirtualPoxAdditions/VPoxService"'                                       '$4 = "4755"'
filelist_fixup prototype '$3 == "opt/VirtualPoxAdditions/amd64/VPoxService"'                                 '$4 = "4755"'

# Manifest class action scripts
filelist_fixup prototype '$3 == "var/svc/manifest/application/virtualpox/vpoxservice.xml"'                   '$2 = "manifest";$6 = "sys"'
filelist_fixup prototype '$3 == "var/svc/manifest/application/virtualpox/vpoxmslnk.xml"'                     '$2 = "manifest";$6 = "sys"'

# vpoxguest
filelist_fixup prototype '$3 == "usr/kernel/drv/vpoxguest"'                                                  '$6="sys"'
filelist_fixup prototype '$3 == "usr/kernel/drv/amd64/vpoxguest"'                                            '$6="sys"'

# vpoxms
filelist_fixup prototype '$3 == "usr/kernel/drv/vpoxms"'                                                     '$6="sys"'
filelist_fixup prototype '$3 == "usr/kernel/drv/amd64/vpoxms"'                                               '$6="sys"'

# Use 'root' as group so as to match attributes with the previous installation and prevent a conflict. Otherwise pkgadd bails out thinking
# we're violating directory attributes of another (non existing) package
dirlist_fixup prototype  '$3 == "var/svc/manifest/application/virtualpox"'                                   '$6 = "root"'

echo " --- start of prototype  ---"
cat prototype
echo " --- end of prototype --- "

# explicitly set timestamp to shutup warning
VPOXPKG_TIMESTAMP=vpoxguest`date '+%Y%m%d%H%M%S'`_r$VPOX_SVN_REV

# create the package instance
pkgmk -d $VPOX_SOL_PKG_DEV -p $VPOXPKG_TIMESTAMP -o -r .

# translate into package datastream
pkgtrans -s -o "$VPOX_SOL_PKG_DEV" `pwd`/$VPOX_PKGFILENAME "$VPOX_PKGNAME"

rm -rf "$VPOX_SOL_PKG_DEV/$VPOX_PKGNAME"
exit $?

