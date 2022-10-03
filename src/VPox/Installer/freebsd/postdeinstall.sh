#!/bin/sh
## @file
#
# VirtualPox postdeinstall script for FreeBSD.
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

echo "Removing kernel modules, please wait..."

kldunload vpoxnetadp
kldunload vpoxnetflt
kldunload vpoxdrv
rm /boot/kernel/vpoxnetflt.ko
rm /boot/kernel/vpoxnetadp.ko
rm /boot/kernel/vpoxdrv.ko
kldxref -R /boot

echo "Kernel modules successfully removed."

exit 0

