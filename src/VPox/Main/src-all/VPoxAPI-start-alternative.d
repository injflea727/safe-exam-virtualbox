/* $Id: VPoxAPI-start-alternative.d $ */
/** @file
 * VPoxAPI - Static dtrace probes.
 */

/*
 * Copyright (C) 2015-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*#pragma D attributes Evolving/Evolving/Common provider vpoxapi provider
#pragma D attributes Private/Private/Unknown  provider vpoxapi module
#pragma D attributes Private/Private/Unknown  provider vpoxapi function
#pragma D attributes Evolving/Evolving/Common provider vpoxapi name
#pragma D attributes Evolving/Evolving/Common provider vpoxapi args*/

provider vpoxapi
{
    /* Manually defined probes: */
    probe machine__state__changed(void *a_pMachine, int a_enmNewState, int a_enmOldState, const char *pszMachineUuid);

    /* The following probes are automatically generated and changes with the API: */
