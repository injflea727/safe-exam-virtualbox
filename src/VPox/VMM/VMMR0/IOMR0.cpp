/* $Id: IOMR0.cpp $ */
/** @file
 * IOM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_IOM
#include <VPox/vmm/iom.h>
#include "IOMInternal.h"
#include <VPox/vmm/vmcc.h>
#include <VPox/log.h>
#include <iprt/assertcompile.h>



/**
 * Initializes the per-VM data for the IOM.
 *
 * This is called from under the GVMM lock, so it should only initialize the
 * data so IOMR0CleanupVM and others will work smoothly.
 *
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(void) IOMR0InitPerVMData(PGVM pGVM)
{
    AssertCompile(sizeof(pGVM->iom.s) <= sizeof(pGVM->iom.padding));
    AssertCompile(sizeof(pGVM->iomr0.s) <= sizeof(pGVM->iomr0.padding));

    iomR0IoPortInitPerVMData(pGVM);
    iomR0MmioInitPerVMData(pGVM);
}


/**
 * Cleans up any loose ends before the GVM structure is destroyed.
 */
VMMR0_INT_DECL(void) IOMR0CleanupVM(PGVM pGVM)
{
    iomR0IoPortCleanupVM(pGVM);
    iomR0MmioCleanupVM(pGVM);
}

