/* $Id: vmexit-reason-aggregation-1.d $ */
/** @file
 * DTracing VPox - vmexit reason aggregation test \#1.
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


vpoxvmm:::r0-hmsvm-vmexit
{
    @g_aSvmExits[args[2]] = count();
}

vpoxvmm:::r0-hmvmx-vmexit-noctx
{
    @g_aVmxExits[args[2]] = count();
}

END
{
    printa(" svmexit=%#04llx   %@10u times\n", @g_aSvmExits);
    printa(" vmxexit=%#04llx   %@10u times\n", @g_aVmxExits);
}

