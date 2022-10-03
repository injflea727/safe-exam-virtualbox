/* $Id: PGMPhysRWTmpl.h $ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Access Template.
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


/**
 * Read physical memory. (one byte/word/dword)
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             The cross context VM structure.
 * @param   GCPhys          Physical address start reading from.
 * @param   enmOrigin       Who is calling.
 */
VMMDECL(PGMPHYS_DATATYPE) PGMPHYSFN_READNAME(PVM pVM, RTGCPHYS GCPhys, PGMACCESSORIGIN enmOrigin)
{
    Assert(VM_IS_EMT(pVM));
    PGMPHYS_DATATYPE val;
    VPOXSTRICTRC rcStrict = PGMPhysRead(pVM, GCPhys, &val, sizeof(val), enmOrigin);
    AssertMsg(rcStrict == VINF_SUCCESS, ("%Rrc\n", VPOXSTRICTRC_VAL(rcStrict))); NOREF(rcStrict);
    return val;
}


/**
 * Write to physical memory. (one byte/word/dword)
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             The cross context VM structure.
 * @param   GCPhys          Physical address to write to.
 * @param   val             What to write.
 * @param   enmOrigin       Who is calling.
 */
VMMDECL(void) PGMPHYSFN_WRITENAME(PVM pVM, RTGCPHYS GCPhys, PGMPHYS_DATATYPE val, PGMACCESSORIGIN enmOrigin)
{
    Assert(VM_IS_EMT(pVM));
    VPOXSTRICTRC rcStrict = PGMPhysWrite(pVM, GCPhys, &val, sizeof(val), enmOrigin);
    AssertMsg(rcStrict == VINF_SUCCESS, ("%Rrc\n", VPOXSTRICTRC_VAL(rcStrict))); NOREF(rcStrict);
}

#undef PGMPHYSFN_READNAME
#undef PGMPHYSFN_WRITENAME
#undef PGMPHYS_DATATYPE
#undef PGMPHYS_DATASIZE

