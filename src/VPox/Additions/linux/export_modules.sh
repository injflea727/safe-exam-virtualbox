#!/bin/sh
# $Id$
## @file
# Create a tar archive containing the sources of the Linux guest kernel modules.
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

export LC_ALL=C

# The below is GNU-specific.  See VPox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_DIR="${TARGET%/[!/]*}"

if [ -z "${1}" ] || { [ "x${1}" = x--folder ] && [ -z "${2}" ]; }; then
    echo "Usage: $0 <filename.tar.gz>"
    echo "  Export VirtualPox kernel modules to <filename.tar.gz>."
    echo "Usage: $0 --folder <folder>"
    echo "  Copy VirtualPox kernel module source to <folder>."
    exit 1
fi

if test "x${1}" = x--folder; then
    PATH_OUT="${2}"
else
    PATH_OUT="`cd \`dirname $1\`; pwd`/.vpox_modules"
    FILE_OUT="`cd \`dirname $1\`; pwd`/`basename $1`"
fi
PATH_ROOT="`cd ${MY_DIR}/../../../..; pwd`"
PATH_LOG=/tmp/vpox-export-guest.log
PATH_LINUX="$PATH_ROOT/src/VPox/Additions/linux"
PATH_VPOXGUEST="$PATH_ROOT/src/VPox/Additions/common/VPoxGuest"
PATH_VPOXSF="$PATH_ROOT/src/VPox/Additions/linux/sharedfolders"
PATH_VPOXVIDEO="$PATH_ROOT/src/VPox/Additions/linux/drm"

VPOX_VERSION_MAJOR=`sed -e "s/^ *VPOX_VERSION_MAJOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_VERSION_MINOR=`sed -e "s/^ *VPOX_VERSION_MINOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_VERSION_BUILD=`sed -e "s/^ *VPOX_VERSION_BUILD *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_SVN_CONFIG_REV=`sed -e 's/^ *VPOX_SVN_REV_CONFIG_FALLBACK *:= \+\$(patsubst *%:,, *\$Rev: *\([0-9]\+\) *\$ *) */\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_SVN_VERSION_REV=`sed -e 's/^ *VPOX_SVN_REV_VERSION_FALLBACK *:= \+\$(patsubst *%:,, *\$Rev: *\([0-9]\+\) *\$ *) */\1/;t;d' $PATH_ROOT/Version.kmk`
if [ "$VPOX_SVN_CONFIG_REV" -gt "$VPOX_SVN_VERSION_REV" ]; then
    VPOX_SVN_REV=$VPOX_SVN_CONFIG_REV
else
    VPOX_SVN_REV=$VPOX_SVN_VERSION_REV
fi
VPOX_VENDOR=`sed -e 's/^ *VPOX_VENDOR *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_VENDOR_SHORT=`sed -e 's/^ *VPOX_VENDOR_SHORT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_PRODUCT=`sed -e 's/^ *VPOX_PRODUCT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_C_YEAR=`date +%Y`

. $PATH_VPOXGUEST/linux/files_vpoxguest
. $PATH_VPOXSF/files_vpoxsf
. $PATH_VPOXVIDEO/files_vpoxvideo_drv

# Temporary path for creating the modules, will be removed later
mkdir -p $PATH_OUT || exit 1

# Create auto-generated version file, needed by all modules
echo "#ifndef ___version_generated_h___" > $PATH_OUT/version-generated.h
echo "#define ___version_generated_h___" >> $PATH_OUT/version-generated.h
echo "" >> $PATH_OUT/version-generated.h
echo "#define VPOX_VERSION_MAJOR $VPOX_VERSION_MAJOR" >> $PATH_OUT/version-generated.h
echo "#define VPOX_VERSION_MINOR $VPOX_VERSION_MINOR" >> $PATH_OUT/version-generated.h
echo "#define VPOX_VERSION_BUILD $VPOX_VERSION_BUILD" >> $PATH_OUT/version-generated.h
echo "#define VPOX_VERSION_STRING_RAW \"$VPOX_VERSION_MAJOR.$VPOX_VERSION_MINOR.$VPOX_VERSION_BUILD\"" >> $PATH_OUT/version-generated.h
echo "#define VPOX_VERSION_STRING \"$VPOX_VERSION_MAJOR.$VPOX_VERSION_MINOR.$VPOX_VERSION_BUILD\"" >> $PATH_OUT/version-generated.h
echo "#define VPOX_API_VERSION_STRING \"${VPOX_VERSION_MAJOR}_${VPOX_VERSION_MINOR}\"" >> $PATH_OUT/version-generated.h
echo "#define VPOX_PRIVATE_BUILD_DESC \"Private build with export_modules\"" >> $PATH_OUT/version-generated.h
echo "" >> $PATH_OUT/version-generated.h
echo "#endif" >> $PATH_OUT/version-generated.h

# Create auto-generated revision file, needed by all modules
echo "#ifndef __revision_generated_h__" > $PATH_OUT/revision-generated.h
echo "#define __revision_generated_h__" >> $PATH_OUT/revision-generated.h
echo "" >> $PATH_OUT/revision-generated.h
echo "#define VPOX_SVN_REV $VPOX_SVN_REV" >> $PATH_OUT/revision-generated.h
echo "" >> $PATH_OUT/revision-generated.h
echo "#endif" >> $PATH_OUT/revision-generated.h

# Create auto-generated product file, needed by all modules
echo "#ifndef ___product_generated_h___" > $PATH_OUT/product-generated.h
echo "#define ___product_generated_h___" >> $PATH_OUT/product-generated.h
echo "" >> $PATH_OUT/product-generated.h
echo "#define VPOX_VENDOR \"$VPOX_VENDOR\"" >> $PATH_OUT/product-generated.h
echo "#define VPOX_VENDOR_SHORT \"$VPOX_VENDOR_SHORT\"" >> $PATH_OUT/product-generated.h
echo "" >> $PATH_OUT/product-generated.h
echo "#define VPOX_PRODUCT \"$VPOX_PRODUCT\"" >> $PATH_OUT/product-generated.h
echo "#define VPOX_C_YEAR \"$VPOX_C_YEAR\"" >> $PATH_OUT/product-generated.h
echo "" >> $PATH_OUT/product-generated.h
echo "#endif" >> $PATH_OUT/product-generated.h

# vpoxguest (VirtualPox guest kernel module)
mkdir $PATH_OUT/vpoxguest || exit 1
for f in $FILES_VPOXGUEST_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_OUT/vpoxguest/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VPOXGUEST_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_OUT/vpoxguest/`echo $f|cut -d'>' -f2`"
done

# vpoxsf (VirtualPox guest kernel module for shared folders)
mkdir $PATH_OUT/vpoxsf || exit 1
for f in $FILES_VPOXSF_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_OUT/vpoxsf/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VPOXSF_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_OUT/vpoxsf/`echo $f|cut -d'>' -f2`"
done

# vpoxvideo (VirtualPox guest kernel module for drm support)
mkdir $PATH_OUT/vpoxvideo || exit 1
for f in $FILES_VPOXVIDEO_DRM_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_OUT/vpoxvideo/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VPOXVIDEO_DRM_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_OUT/vpoxvideo/`echo $f|cut -d'>' -f2`"
done
sed -f $PATH_VPOXVIDEO/indent.sed -i $PATH_OUT/vpoxvideo/*.[ch]

# convenience Makefile
install -D -m 0644 $PATH_LINUX/Makefile "$PATH_OUT/Makefile"

# Only temporary, omit from archive
rm $PATH_OUT/version-generated.h
rm $PATH_OUT/revision-generated.h
rm $PATH_OUT/product-generated.h

# If we are exporting to a folder then stop now.
test "x${1}" = x--folder && exit 0

# Do a test build
echo Doing a test build, this may take a while.
make -C $PATH_OUT > $PATH_LOG 2>&1 &&
    make -C $PATH_OUT clean >> $PATH_LOG 2>&1 ||
    echo "Warning: test build failed.  Please check $PATH_LOG"

# Create the archive
tar -czf $FILE_OUT -C $PATH_OUT . || exit 1

# Remove the temporary directory
rm -r $PATH_OUT

