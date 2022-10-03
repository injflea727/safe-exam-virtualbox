/* $Id: mount.vpoxsf.cpp $ */
/** @file
 * VPoxSF - Darwin Shared Folders, Mount Utility.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>

#include "VPoxSFMount.h"
#include <iprt/string.h>


static RTEXITCODE usage(const char *pszArg0)
{
    fprintf(stderr, "usage: %s [OPTIONS] <shared folder name> <mount point>\n", pszArg0);
    return RTEXITCODE_SYNTAX;
}


int main(int argc, char **argv)
{
    /*
     * Skip past parameters.
     */
    int c;
    while ((c = getopt(argc, argv, "o:")) != -1)
    {
        switch (c)
        {
            case 'o':
                break;
            default:
                return usage(argv[0]);
        }
    }

    /* Two arguments are rquired: <share name> and <mount point> */
    if (argc - optind != 2)
        return usage(argv[0]);
    const char * const pszFolder     = argv[optind++];
    const char * const pszMountPoint = argv[optind];

    /*
     * Check that the folder is within bounds and doesn't include any shady characters.
     */
    size_t cchFolder = strlen(pszFolder);
    if (   cchFolder < 1
        || cchFolder >= RT_SIZEOFMEMB(VPOXSFDRWNMOUNTINFO, szFolder)
        || strpbrk(pszFolder, "\\/") != NULL)
    {
        fprintf(stderr, "Invalid shared folder name '%s'!\n", pszFolder);
        return RTEXITCODE_FAILURE;
    }

    /*
     * Do the mounting.
     */
    VPOXSFDRWNMOUNTINFO MntInfo;
    RT_ZERO(MntInfo);
    MntInfo.u32Magic = VPOXSFDRWNMOUNTINFO_MAGIC;
    memcpy(MntInfo.szFolder, pszFolder, cchFolder);
    int rc = mount(VPOXSF_DARWIN_FS_NAME, pszMountPoint, 0, &MntInfo);
    if (rc == 0)
        return 0;

    fprintf(stderr, "error mounting '%s' at '%s': %s (%d)\n", pszFolder, pszMountPoint, strerror(errno), errno);
    return RTEXITCODE_FAILURE;
}
