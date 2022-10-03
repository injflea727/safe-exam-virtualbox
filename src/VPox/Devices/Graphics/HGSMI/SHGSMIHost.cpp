/* $Id: SHGSMIHost.cpp $ */
/** @file
 * Missing description.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SHGSMIHost.h"
#include <VPoxVideo.h>

/*
 * VPOXSHGSMI made on top HGSMI and allows receiving notifications
 * about G->H command completion
 */

static int vpoxSHGSMICommandCompleteAsynch (PHGSMIINSTANCE pIns, VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *pHdr)
{
    bool fDoIrq = !!(pHdr->fFlags & VPOXSHGSMI_FLAG_GH_ASYNCH_IRQ)
                || !!(pHdr->fFlags & VPOXSHGSMI_FLAG_GH_ASYNCH_IRQ_FORCE);
    return HGSMICompleteGuestCommand(pIns, pHdr, fDoIrq);
}

void VPoxSHGSMICommandMarkAsynchCompletion(void RT_UNTRUSTED_VOLATILE_GUEST *pvData)
{
    VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *pHdr = VPoxSHGSMIBufferHeader(pvData);
    Assert(!(pHdr->fFlags & VPOXSHGSMI_FLAG_HG_ASYNCH));
    pHdr->fFlags |= VPOXSHGSMI_FLAG_HG_ASYNCH;
}

int VPoxSHGSMICommandComplete(PHGSMIINSTANCE pIns, void RT_UNTRUSTED_VOLATILE_GUEST *pvData)
{
    VPOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *pHdr = VPoxSHGSMIBufferHeader(pvData);
    uint32_t fFlags = pHdr->fFlags;
    ASMCompilerBarrier();
    if (   !(fFlags & VPOXSHGSMI_FLAG_HG_ASYNCH)        /* <- check if synchronous completion */
        && !(fFlags & VPOXSHGSMI_FLAG_GH_ASYNCH_FORCE)) /* <- check if can complete synchronously */
        return VINF_SUCCESS;

    pHdr->fFlags = fFlags | VPOXSHGSMI_FLAG_HG_ASYNCH;
    return vpoxSHGSMICommandCompleteAsynch(pIns, pHdr);
}
