#!/bin/bash
# $Id: load.sh $
## @file
# For development.
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
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualPox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

XNU_VERSION=`LC_ALL=C uname -r | LC_ALL=C cut -d . -f 1`
DRVNAME="VPoxDrv.kext"
BUNDLE="org.virtualpox.kext.VPoxDrv"

DIR=`dirname "$0"`
DIR=`cd "$DIR" && pwd`
DIR="$DIR/$DRVNAME"
if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi
if [ -n "$*" ]; then
  OPTS="$*"
else
  OPTS="-t"
fi

# Make sure VPoxUSB is unloaded as it might be using symbols from us.
LOADED=`kextstat -b org.virtualpox.kext.VPoxUSB -l`
if test -n "$LOADED"; then
    echo "load.sh: Unloading org.virtualpox.kext.VPoxUSB..."
    sudo kextunload -v 6 -b org.virtualpox.kext.VPoxUSB
    LOADED=`kextstat -b org.virtualpox.kext.VPoxUSB -l`
    if test -n "$LOADED"; then
        echo "load.sh: failed to unload org.virtualpox.kext.VPoxUSB, see above..."
        exit 1;
    fi
    echo "load.sh: Successfully unloaded org.virtualpox.kext.VPoxUSB"
fi

# Make sure VPoxNetFlt is unloaded as it might be using symbols from us.
LOADED=`kextstat -b org.virtualpox.kext.VPoxNetFlt -l`
if test -n "$LOADED"; then
    echo "load.sh: Unloading org.virtualpox.kext.VPoxNetFlt..."
    sudo kextunload -v 6 -b org.virtualpox.kext.VPoxNetFlt
    LOADED=`kextstat -b org.virtualpox.kext.VPoxNetFlt -l`
    if test -n "$LOADED"; then
        echo "load.sh: failed to unload org.virtualpox.kext.VPoxNetFlt, see above..."
        exit 1;
    fi
    echo "load.sh: Successfully unloaded org.virtualpox.kext.VPoxNetFlt"
fi

# Make sure VPoxNetAdp is unloaded as it might be using symbols from us.
LOADED=`kextstat -b org.virtualpox.kext.VPoxNetAdp -l`
if test -n "$LOADED"; then
    echo "load.sh: Unloading org.virtualpox.kext.VPoxNetAdp..."
    sudo kextunload -v 6 -b org.virtualpox.kext.VPoxNetAdp
    LOADED=`kextstat -b org.virtualpox.kext.VPoxNetAdp -l`
    if test -n "$LOADED"; then
        echo "load.sh: failed to unload org.virtualpox.kext.VPoxNetAdp, see above..."
        exit 1;
    fi
    echo "load.sh: Successfully unloaded org.virtualpox.kext.VPoxNetAdp"
fi

# Try unload any existing instance first.
LOADED=`kextstat -b $BUNDLE -l`
if test -n "$LOADED"; then
    echo "load.sh: Unloading $BUNDLE..."
    sudo kextunload -v 6 -b $BUNDLE
    LOADED=`kextstat -b $BUNDLE -l`
    if test -n "$LOADED"; then
        echo "load.sh: failed to unload $BUNDLE, see above..."
        exit 1;
    fi
    echo "load.sh: Successfully unloaded $BUNDLE"
fi

set -e

# Copy the .kext to the symbols directory and tweak the kextload options.
if test -n "$VPOX_DARWIN_SYMS"; then
    echo "load.sh: copying the extension the symbol area..."
    rm -Rf "$VPOX_DARWIN_SYMS/$DRVNAME"
    mkdir -p "$VPOX_DARWIN_SYMS"
    cp -R "$DIR" "$VPOX_DARWIN_SYMS/"
    OPTS="$OPTS -s $VPOX_DARWIN_SYMS/ "
    sync
fi

trap "sudo chown -R `whoami` $DIR; exit 1" INT
trap "sudo chown -R `whoami` $DIR; exit 1" ERR
# On smbfs, this might succeed just fine but make no actual changes,
# so we might have to temporarily copy the driver to a local directory.
if sudo chown -R root:wheel "$DIR"; then
    OWNER=`/usr/bin/stat -f "%u" "$DIR"`
else
    OWNER=1000
fi
if test "$OWNER" -ne 0; then
    TMP_DIR=/tmp/loaddrv.tmp
    echo "load.sh: chown didn't work on $DIR, using temp location $TMP_DIR/$DRVNAME"

    # clean up first (no sudo rm)
    if test -e "$TMP_DIR"; then
        sudo chown -R `whoami` "$TMP_DIR"
        rm -Rf "$TMP_DIR"
    fi

    # make a copy and switch over DIR
    mkdir -p "$TMP_DIR/"
    sudo cp -Rp "$DIR" "$TMP_DIR/"
    DIR="$TMP_DIR/$DRVNAME"

    # retry
    sudo chown -R root:wheel "$DIR"
fi
sudo chmod -R o-rwx "$DIR"
sync
echo "load.sh: loading $DIR..."

if [ "$XNU_VERSION" -ge "10" ]; then
    echo "${SCRIPT_NAME}.sh: loading $DIR... (kextutil $OPTS \"$DIR\")"
    sudo kextutil $OPTS "$DIR"
else
    sudo kextload $OPTS "$DIR"
fi
sync
sudo chown -R `whoami` "$DIR"
#sudo chmod 666 /dev/vpoxdrv
kextstat | grep org.virtualpox.kext
if [ -n "${VPOX_DARWIN_SYMS}"  -a   "$XNU_VERSION" -ge "10" ]; then
    dsymutil -o "${VPOX_DARWIN_SYMS}/${DRVNAME}.dSYM" "${DIR}/Contents/MacOS/`basename -s .kext ${DRVNAME}`"
    sync
fi

