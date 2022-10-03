#!/bin/sh
# $Id$
## @file
# Use this script in conjunction with snapshot-ose.sh
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

vpoxdir=`pwd`
if [ ! -r "$vpoxdir/Config.kmk" -o ! -r "$vpoxdir/Doxyfile.Core" ]; then
  echo "Is $vpoxdir really a VPox tree?"
  exit 1
fi
if [ -r "$vpoxdir/src/VPox/RDP/server/server.cpp" ]; then
  echo "Found RDP stuff, refused to build OSE tarball!"
  exit 1
fi
vermajor=`grep "^VPOX_VERSION_MAJOR *=" "$vpoxdir/Version.kmk"|sed -e "s|.*= *\(.*\)|\1|g"`
verminor=`grep "^VPOX_VERSION_MINOR *=" "$vpoxdir/Version.kmk"|sed -e "s|.*= *\(.*\)|\1|g"`
verbuild=`grep "^VPOX_VERSION_BUILD *=" "$vpoxdir/Version.kmk"|sed -e "s|.*= *\(.*\)|\1|g"`
verpre=`grep "^VPOX_VERSION_PRERELEASE *=" "$vpoxdir/Version.kmk"|sed -e "s|.*= *\(.*\)|\1|g"`
verpub=`grep "^VPOX_BUILD_PUBLISHER *=" "$vpoxdir/Version.kmk"|sed -e "s|.*= *\(.*\)|\1|g"`
verstr="$vermajor.$verminor.$verbuild"
[ -n "$verpre" ] && verstr="$verstr"_"$verpre"
[ -n "$verpub" ] && verstr="$verstr$verpub"
rootpath=`cd ..;pwd`
rootname="VirtualPox-$verstr"
if [ $# -eq 1 ]; then
    tarballname="$1"
else
    tarballname="$rootpath/$rootname.tar.bz2"
fi
rm -f "$rootpath/$rootname"
ln -s `basename "$vpoxdir"` "$rootpath/$rootname"
if [ $? -ne 0 ]; then
  echo "Cannot create root directory link!"
  exit 1
fi
tar \
  --create \
  --bzip2 \
  --dereference \
  --owner 0 \
  --group 0 \
  --totals \
  --exclude=.svn \
  --exclude="$rootname/out" \
  --exclude="$rootname/env.sh" \
  --exclude="$rootname/configure.log" \
  --exclude="$rootname/build.log" \
  --exclude="$rootname/AutoConfig.kmk" \
  --exclude="$rootname/LocalConfig.kmk" \
  --exclude="$rootname/prebuild" \
  --directory "$rootpath" \
  --file "$tarballname" \
  "$rootname"
echo "Successfully created $tarballname"
rm -f "$rootpath/$rootname"
