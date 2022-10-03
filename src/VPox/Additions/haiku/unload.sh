#!/bin/bash
# $Id: unload.sh $
## @file
# Driver unload script.
#

#
# Copyright (C) 2012-2020 Oracle Corporation
#
# This file is part of VirtualPox Open Source Edition (OSE), as
# available from http://www.virtualpox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualPox OSE distribution. VirtualPox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

basedir=/boot/home/config/add-ons/
rm -f $basedir/input_server/devices/VPoxMouse
rm -f $basedir/kernel/drivers/bin/vpoxdev
rm -f $basedir/kernel/drivers/dev/misc/vpoxdev
rm -f $basedir/kernel/file_systems/vpoxsf
rm -f $basedir/kernel/generic/vpoxguest
rm -rf /boot/apps/VPoxAdditions

