/** @file
 * tstShflSize - Testcase for shared folder structure sizes.
 * Run this on Linux and Windows, then compare.
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
#include <VPox/shflsvc.h>
#include <iprt/string.h>
#include <stdio.h>

#define STRUCT(t, size)   \
    do { \
        if (fPrintChecks) \
            printf("    STRUCT(" #t ", %d);\n", (int)sizeof(t)); \
        else if ((size) != sizeof(t)) \
        { \
            printf("%30s: %d expected %d!\n", #t, (int)sizeof(t), (size)); \
            cErrors++; \
        } \
        else if (!fQuiet)\
            printf("%30s: %d\n", #t, (int)sizeof(t)); \
    } while (0)


int main(int argc, char **argv)
{
    unsigned cErrors = 0;

    /*
     * Prints the code below if any argument was giving.
     */
    bool fQuiet = argc == 2 && !strcmp(argv[1], "quiet");
    bool fPrintChecks = !fQuiet && argc != 1;

    printf("tstShflSizes: TESTING\n");

    /*
     * The checks.
     */
    STRUCT(SHFLROOT, 4);
    STRUCT(SHFLHANDLE, 8);
    STRUCT(SHFLSTRING, 6);
    STRUCT(SHFLCREATERESULT, 4);
    STRUCT(SHFLCREATEPARMS, 108);
    STRUCT(SHFLMAPPING, 8);
    STRUCT(SHFLDIRINFO, 128);
    STRUCT(SHFLVOLINFO, 40);
    STRUCT(SHFLFSOBJATTR, 44);
    STRUCT(SHFLFSOBJINFO, 92);
#ifdef VPOX_WITH_64_BITS_GUESTS
/* The size of the guest structures depends on the current architecture bit count (ARCH_BITS)
 * because the HGCMFunctionParameter structure differs in 32 and 64 bit guests.
 * The host VMMDev device takes care about this.
 *
 * Therefore this testcase verifies whether structure sizes are correct for the current ARCH_BITS.
 */
# if ARCH_BITS == 64
    STRUCT(VPoxSFQueryMappings, 88);
    STRUCT(VPoxSFQueryMapName, 72);
    STRUCT(VPoxSFMapFolder_Old, 88);
    STRUCT(VPoxSFMapFolder, 104);
    STRUCT(VPoxSFUnmapFolder, 56);
    STRUCT(VPoxSFCreate, 88);
    STRUCT(VPoxSFClose, 72);
    STRUCT(VPoxSFRead, 120);
    STRUCT(VPoxSFWrite, 120);
    STRUCT(VPoxSFLock, 120);
    STRUCT(VPoxSFFlush, 72);
    STRUCT(VPoxSFList, 168);
    STRUCT(VPoxSFInformation, 120);
    STRUCT(VPoxSFRemove, 88);
    STRUCT(VPoxSFRename, 104);
# elif ARCH_BITS == 32
    STRUCT(VPoxSFQueryMappings, 24+52);
    STRUCT(VPoxSFQueryMapName, 24+40); /* this was changed from 52 in 21976 after VPox-1.4. */
    STRUCT(VPoxSFMapFolder_Old, 24+52);
    STRUCT(VPoxSFMapFolder, 24+64);
    STRUCT(VPoxSFUnmapFolder, 24+28);
    STRUCT(VPoxSFCreate, 24+52);
    STRUCT(VPoxSFClose, 24+40);
    STRUCT(VPoxSFRead, 24+76);
    STRUCT(VPoxSFWrite, 24+76);
    STRUCT(VPoxSFLock, 24+76);
    STRUCT(VPoxSFFlush, 24+40);
    STRUCT(VPoxSFList, 24+112);
    STRUCT(VPoxSFInformation, 24+76);
    STRUCT(VPoxSFRemove, 24+52);
    STRUCT(VPoxSFRename, 24+64);
# else
#  error "Unsupported ARCH_BITS"
# endif /* ARCH_BITS */
#else
    STRUCT(VPoxSFQueryMappings, 24+52);
    STRUCT(VPoxSFQueryMapName, 24+40); /* this was changed from 52 in 21976 after VPox-1.4. */
    STRUCT(VPoxSFMapFolder_Old, 24+52);
    STRUCT(VPoxSFMapFolder, 24+64);
    STRUCT(VPoxSFUnmapFolder, 24+28);
    STRUCT(VPoxSFCreate, 24+52);
    STRUCT(VPoxSFClose, 24+40);
    STRUCT(VPoxSFRead, 24+76);
    STRUCT(VPoxSFWrite, 24+76);
    STRUCT(VPoxSFLock, 24+76);
    STRUCT(VPoxSFFlush, 24+40);
    STRUCT(VPoxSFList, 24+112);
    STRUCT(VPoxSFInformation, 24+76);
    STRUCT(VPoxSFRemove, 24+52);
    STRUCT(VPoxSFRename, 24+64);
#endif /* VPOX_WITH_64_BITS_GUESTS */

    /*
     * The summary.
     */
    if (!cErrors)
        printf("tstShflSizes: SUCCESS\n");
    else
        printf("tstShflSizes: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

