/* $Id: vmexit-rip-aggregation-1.d $ */
/** @file
 * DTracing VPox - vmexit rip aggregation test \#1.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#pragma D option quiet


vpoxvmm:::r0-hmsvm-vmexit,vpoxvmm:::r0-hmvmx-vmexit
{
    /*printf("cs:rip=%02x:%08llx", args[1]->cs.Sel, args[1]->rip.rip);*/
    @g_aRips[args[1]->rip.rip] = count();
    /*@g_aRips[args[0]->cpum.s.Guest.rip.rip] = count(); - alternative access route */
}

END
{
    printa(" rip=%#018llx   %@4u times\n", @g_aRips);
}

