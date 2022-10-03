#!/bin/sh
# $Id$
## @file
# Create a tar archive containing the sources of the vpoxdrv kernel module.
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
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualPox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

# The below is GNU-specific.  See VPox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_DIR="${TARGET%/[!/]*}"

# What this script does:
usage_msg="\
Usage: `basename ${0}` --file <path>|--folder <path> \
    [--override-svn-rev <rev>] [--extra-version-string <string>] [--without-hardening]

Exports the VirtualPox host kernel modules to the tar.gz archive or folder in \
<path>, optionally adjusting the Make files to build them without hardening.

Examples:
  `basename ${0}` --file /tmp/vpoxhost.tar.gz
  `basename ${0}` --folder /tmp/tmpdir --without-hardening"

usage()
{
    case "${1}" in
    0)
        echo "${usage_msg}" | fold -s -w80 ;;
    *)
        echo "${usage_msg}" | fold -s -w80 >&2 ;;
    esac
    exit "${1}"
}

fail()
{
    echo "${1}" | fold -s -w80 >&2
    exit 1
}

unset FILE FOLDER
VPOX_WITH_HARDENING=1
while test -n "${1}"; do
    case "${1}" in
    --file)
        FILE="${2}"
        shift 2 ;;
    --folder)
        FOLDER="${2}"
        shift 2 ;;
    --override-svn-rev)
        OVERRIDE_SVN_REV="${2}"
        shift 2 ;;
    --extra-version-string)
        EXTRA_VERSION_STRING="${2}"
        shift 2 ;;
    --without-hardening)
        unset VPOX_WITH_HARDENING
        shift ;;
    -h|--help)
        usage 0 ;;
    *)
        echo "Unknown parameter ${1}" >&2
        usage 1 ;;
    esac
done
test -z "$FILE" || test -z "$FOLDER" ||
    fail "Only one of --file and --folder may be used"
test -n "$FILE" || test -n "$FOLDER" || usage 1

if test -n "$FOLDER"; then
    PATH_TMP="$FOLDER"
else
    PATH_TMP="`cd \`dirname $FILE\`; pwd`/.vpox_modules"
    FILE_OUT="`cd \`dirname $FILE\`; pwd`/`basename $FILE`"
fi
PATH_OUT=$PATH_TMP
PATH_ROOT="`cd ${MY_DIR}/../../../..; pwd`"
PATH_LOG=/tmp/vpox-export-host.log
PATH_LINUX="$PATH_ROOT/src/VPox/HostDrivers/linux"
PATH_VPOXDRV="$PATH_ROOT/src/VPox/HostDrivers/Support"
PATH_VPOXNET="$PATH_ROOT/src/VPox/HostDrivers/VPoxNetFlt"
PATH_VPOXADP="$PATH_ROOT/src/VPox/HostDrivers/VPoxNetAdp"
PATH_VPOXPCI="$PATH_ROOT/src/VPox/HostDrivers/VPoxPci"

VPOX_VERSION_MAJOR=`sed -e "s/^ *VPOX_VERSION_MAJOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_VERSION_MINOR=`sed -e "s/^ *VPOX_VERSION_MINOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_VERSION_BUILD=`sed -e "s/^ *VPOX_VERSION_BUILD *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_VERSION_STRING=$VPOX_VERSION_MAJOR.$VPOX_VERSION_MINOR.$VPOX_VERSION_BUILD
VPOX_VERSION_BUILD=`sed -e "s/^ *VPOX_VERSION_BUILD *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VPOX_SVN_CONFIG_REV=`sed -e 's/^ *VPOX_SVN_REV_CONFIG_FALLBACK *:= \+\$(patsubst *%:,, *\$Rev: *\([0-9]\+\) *\$ *) */\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_SVN_VERSION_REV=`sed -e 's/^ *VPOX_SVN_REV_VERSION_FALLBACK *:= \+\$(patsubst *%:,, *\$Rev: *\([0-9]\+\) *\$ *) */\1/;t;d' $PATH_ROOT/Version.kmk`

if [ -n "$OVERRIDE_SVN_REV" ]; then
    VPOX_SVN_REV=$OVERRIDE_SVN_REV
elif [ "$VPOX_SVN_CONFIG_REV" -gt "$VPOX_SVN_VERSION_REV" ]; then
    VPOX_SVN_REV=$VPOX_SVN_CONFIG_REV
else
    VPOX_SVN_REV=$VPOX_SVN_VERSION_REV
fi
VPOX_VENDOR=`sed -e 's/^ *VPOX_VENDOR *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_VENDOR_SHORT=`sed -e 's/^ *VPOX_VENDOR_SHORT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_PRODUCT=`sed -e 's/^ *VPOX_PRODUCT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VPOX_C_YEAR=`date +%Y`
VPOX_WITH_PCI_PASSTHROUGH=`sed -e '/^ *VPOX_WITH_PCI_PASSTHROUGH *[:]\?= */!d' -e 's/ *#.*$//' -e 's/^.*= *//' $PATH_ROOT/Config.kmk`

. $PATH_VPOXDRV/linux/files_vpoxdrv
. $PATH_VPOXNET/linux/files_vpoxnetflt
. $PATH_VPOXADP/linux/files_vpoxnetadp
if [ "${VPOX_WITH_PCI_PASSTHROUGH}" = "1" ]; then
    . $PATH_VPOXPCI/linux/files_vpoxpci
fi

# Temporary path for creating the modules, will be removed later
rm -rf "$PATH_TMP"
mkdir $PATH_TMP || exit 1

# Create auto-generated version file, needed by all modules
echo "#ifndef ___version_generated_h___" > $PATH_TMP/version-generated.h
echo "#define ___version_generated_h___" >> $PATH_TMP/version-generated.h
echo "" >> $PATH_TMP/version-generated.h
echo "#define VPOX_VERSION_MAJOR $VPOX_VERSION_MAJOR" >> $PATH_TMP/version-generated.h
echo "#define VPOX_VERSION_MINOR $VPOX_VERSION_MINOR" >> $PATH_TMP/version-generated.h
echo "#define VPOX_VERSION_BUILD $VPOX_VERSION_BUILD" >> $PATH_TMP/version-generated.h
echo "#define VPOX_VERSION_STRING_RAW \"$VPOX_VERSION_MAJOR.$VPOX_VERSION_MINOR.$VPOX_VERSION_BUILD\"" >> $PATH_TMP/version-generated.h
echo "#define VPOX_VERSION_STRING \"$VPOX_VERSION_STRING\"" >> $PATH_TMP/version-generated.h
echo "#define VPOX_API_VERSION_STRING \"${VPOX_VERSION_MAJOR}_${VPOX_VERSION_MINOR}\"" >> $PATH_TMP/version-generated.h
[ -n "$EXTRA_VERSION_STRING" ] && echo "#define VPOX_EXTRA_VERSION_STRING \" ${EXTRA_VERSION_STRING}\"" >> $PATH_TMP/version-generated.h
echo "#define VPOX_PRIVATE_BUILD_DESC \"Private build with export_modules\"" >> $PATH_TMP/version-generated.h
echo "" >> $PATH_TMP/version-generated.h
echo "#endif" >> $PATH_TMP/version-generated.h

# Create auto-generated revision file, needed by all modules
echo "#ifndef __revision_generated_h__" > $PATH_TMP/revision-generated.h
echo "#define __revision_generated_h__" >> $PATH_TMP/revision-generated.h
echo "" >> $PATH_TMP/revision-generated.h
echo "#define VPOX_SVN_REV $VPOX_SVN_REV" >> $PATH_TMP/revision-generated.h
echo "" >> $PATH_TMP/revision-generated.h
echo "#endif" >> $PATH_TMP/revision-generated.h

# Create auto-generated product file, needed by all modules
echo "#ifndef ___product_generated_h___" > $PATH_TMP/product-generated.h
echo "#define ___product_generated_h___" >> $PATH_TMP/product-generated.h
echo "" >> $PATH_TMP/product-generated.h
echo "#define VPOX_VENDOR \"$VPOX_VENDOR\"" >> $PATH_TMP/product-generated.h
echo "#define VPOX_VENDOR_SHORT \"$VPOX_VENDOR_SHORT\"" >> $PATH_TMP/product-generated.h
echo "" >> $PATH_TMP/product-generated.h
echo "#define VPOX_PRODUCT \"$VPOX_PRODUCT\"" >> $PATH_TMP/product-generated.h
echo "#define VPOX_C_YEAR \"$VPOX_C_YEAR\"" >> $PATH_TMP/product-generated.h
echo "" >> $PATH_TMP/product-generated.h
echo "#endif" >> $PATH_TMP/product-generated.h

# vpoxdrv (VirtualPox host kernel module)
mkdir $PATH_TMP/vpoxdrv || exit 1
for f in $FILES_VPOXDRV_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vpoxdrv/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VPOXDRV_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_TMP/vpoxdrv/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VPOX_WITH_HARDENING" ]; then
    sed -e "s;VPOX_WITH_EFLAGS_AC_SET_IN_VPOXDRV;;g" \
        -e "s;IPRT_WITH_EFLAGS_AC_PRESERVING;;g" \
        < $PATH_VPOXDRV/linux/Makefile > $PATH_TMP/vpoxdrv/Makefile
else
    sed -e "s;VPOX_WITH_HARDENING;;g" \
        -e "s;VPOX_WITH_EFLAGS_AC_SET_IN_VPOXDRV;;g" \
        -e "s;IPRT_WITH_EFLAGS_AC_PRESERVING;;g" \
        < $PATH_VPOXDRV/linux/Makefile > $PATH_TMP/vpoxdrv/Makefile
fi

# vpoxnetflt (VirtualPox netfilter kernel module)
mkdir $PATH_TMP/vpoxnetflt || exit 1
for f in $VPOX_VPOXNETFLT_SOURCES; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vpoxnetflt/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VPOX_WITH_HARDENING" ]; then
    cat                                   $PATH_VPOXNET/linux/Makefile > $PATH_TMP/vpoxnetflt/Makefile
else
    sed -e "s;VPOX_WITH_HARDENING;;g" < $PATH_VPOXNET/linux/Makefile > $PATH_TMP/vpoxnetflt/Makefile
fi

# vpoxnetadp (VirtualPox network adapter kernel module)
mkdir $PATH_TMP/vpoxnetadp || exit 1
for f in $VPOX_VPOXNETADP_SOURCES; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vpoxnetadp/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VPOX_WITH_HARDENING" ]; then
    cat                                   $PATH_VPOXADP/linux/Makefile > $PATH_TMP/vpoxnetadp/Makefile
else
    sed -e "s;VPOX_WITH_HARDENING;;g" < $PATH_VPOXADP/linux/Makefile > $PATH_TMP/vpoxnetadp/Makefile
fi

# vpoxpci (VirtualPox host PCI access kernel module)
if [ "${VPOX_WITH_PCI_PASSTHROUGH}" = "1" ]; then
    mkdir $PATH_TMP/vpoxpci || exit 1
    for f in $VPOX_VPOXPCI_SOURCES; do
        install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vpoxpci/`echo $f|cut -d'>' -f2`"
    done
    if [ -n "$VPOX_WITH_HARDENING" ]; then
        cat                                   $PATH_VPOXPCI/linux/Makefile > $PATH_TMP/vpoxpci/Makefile
    else
        sed -e "s;VPOX_WITH_HARDENING;;g" < $PATH_VPOXPCI/linux/Makefile > $PATH_TMP/vpoxpci/Makefile
    fi
fi

install -D -m 0644 $PATH_LINUX/Makefile $PATH_TMP/Makefile
install -D -m 0755 $PATH_LINUX/build_in_tmp $PATH_TMP/build_in_tmp

# Only temporary, omit from archive
rm $PATH_TMP/version-generated.h
rm $PATH_TMP/revision-generated.h
rm $PATH_TMP/product-generated.h

# If we are exporting to a folder then stop now.
test -z "$FOLDER" || exit 0

# Do a test build
echo Doing a test build, this may take a while.
make -C $PATH_TMP > $PATH_LOG 2>&1 &&
    make -C $PATH_TMP clean >> $PATH_LOG 2>&1 ||
    echo "Warning: test build failed.  Please check $PATH_LOG"

# Create the archive
tar -czf $FILE_OUT -C $PATH_TMP . || exit 1

# Remove the temporary directory
rm -r $PATH_TMP
