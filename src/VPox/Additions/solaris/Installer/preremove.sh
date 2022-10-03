#!/bin/sh
# $Id: preremove.sh $
## @file
# VirtualPox preremove script for Solaris Guest Additions.
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

LC_ALL=C
export LC_ALL

LANG=C
export LANG

echo "Removing VirtualPox service..."

# stop and unregister VPoxService
/usr/sbin/svcadm disable -s virtualpox/vpoxservice
# Don't need to delete, taken care of by the manifest action
# /usr/sbin/svccfg delete svc:/application/virtualpox/vpoxservice:default
/usr/sbin/svcadm restart svc:/system/manifest-import:default

# stop VPoxClient
pkill -INT VPoxClient

echo "Removing VirtualPox kernel modules..."

# vpoxguest.sh would've been installed, we just need to call it.
/opt/VirtualPoxAdditions/vpoxguest.sh stopall silentunload

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

# remove devlink.tab entry for vpoxguest
sed -e '/name=vpoxguest/d' /etc/devlink.tab > /etc/devlink.vpox
chmod 0644 /etc/devlink.vpox
chown root:$group /etc/devlink.vpox
mv -f /etc/devlink.vpox /etc/devlink.tab

# remove the link
if test -h "/dev/vpoxguest" || test -f "/dev/vpoxguest"; then
    rm -f /dev/vpoxdrv
fi
if test -h "/dev/vpoxms" || test -f "/dev/vpoxms"; then
    rm -f /dev/vpoxms
fi

# Try and restore xorg.conf!
echo "Restoring X.Org..."
/opt/VirtualPoxAdditions/x11restore.pl


echo "Done."

