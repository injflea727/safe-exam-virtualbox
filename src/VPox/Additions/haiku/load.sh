#!/bin/bash
# $Id: load.sh $
## @file
# Driver load script.
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

outdir=out/haiku.x86/debug/bin/additions
instdir=/boot/apps/VPoxAdditions


# vpoxguest
mkdir -p ~/config/add-ons/kernel/generic/
cp $outdir/vpoxguest ~/config/add-ons/kernel/generic/

# vpoxdev
mkdir -p ~/config/add-ons/kernel/drivers/dev/misc/
cp $outdir/vpoxdev ~/config/add-ons/kernel/drivers/bin/
ln -sf ../../bin/vpoxdev ~/config/add-ons/kernel/drivers/dev/misc

# VPoxMouse
cp $outdir/VPoxMouse        ~/config/add-ons/input_server/devices/
cp $outdir/VPoxMouseFilter  ~/config/add-ons/input_server/filters/

# Services
mkdir -p $instdir
cp $outdir/VPoxService $instdir/
cp $outdir/VPoxTray    $instdir/
cp $outdir/VPoxControl $instdir/
ln -sf $instdir/VPoxService ~/config/boot/launch

