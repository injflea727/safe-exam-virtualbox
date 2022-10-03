/* $Id: LoadVPoxDDU.cpp $ */
/** @file
 * VirtualPox Runtime - Try Load VPoxDDU to get VFS chain providers from storage.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/errcore.h>
#include <iprt/path.h>
#include <iprt/string.h>


/**
 * Class used for registering a VFS chain element provider.
 */
class LoadVPoxDDU
{
private:
    /** The VPoxDDU handle. */
    RTLDRMOD g_hLdrMod;

public:
    LoadVPoxDDU(void) : g_hLdrMod(NIL_RTLDRMOD)
    {
        int rc = RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
        if (RT_SUCCESS(rc))
        {
            char szPath[RTPATH_MAX];

            /* Try private arch dir first. */
            rc = RTPathAppPrivateArch(szPath, sizeof(szPath));
            if (RT_SUCCESS(rc))
                rc = RTPathAppend(szPath, sizeof(szPath), "VPoxDDU");
            if (RT_SUCCESS(rc))
                rc = RTStrCat(szPath, sizeof(szPath), RTLdrGetSuff());
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrLoad(szPath, &g_hLdrMod);
                if (RT_SUCCESS(rc))
                    return;
            }

            /* Try shared libs dir next. */
            rc = RTPathSharedLibs(szPath, sizeof(szPath));
            if (RT_SUCCESS(rc))
                rc = RTPathAppend(szPath, sizeof(szPath), "VPoxDDU");
            if (RT_SUCCESS(rc))
                rc = RTStrCat(szPath, sizeof(szPath), RTLdrGetSuff());
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrLoad(szPath, &g_hLdrMod);
                if (RT_SUCCESS(rc))
                    return;
            }

            /* Try exec dir after that. */
            rc = RTPathExecDir(szPath, sizeof(szPath));
            if (RT_SUCCESS(rc))
                rc = RTPathAppend(szPath, sizeof(szPath), "VPoxDDU");
            if (RT_SUCCESS(rc))
                rc = RTStrCat(szPath, sizeof(szPath), RTLdrGetSuff());
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrLoad(szPath, &g_hLdrMod);
                if (RT_SUCCESS(rc))
                    return;
            }

            /* Try exec dir parent after that. */
            rc = RTPathExecDir(szPath, sizeof(szPath));
            if (RT_SUCCESS(rc))
                rc = RTPathAppend(szPath, sizeof(szPath), ".." RTPATH_SLASH_STR "VPoxDDU");
            if (RT_SUCCESS(rc))
                rc = RTStrCat(szPath, sizeof(szPath), RTLdrGetSuff());
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrLoad(szPath, &g_hLdrMod);
                if (RT_SUCCESS(rc))
                    return;
            }

            /* Didn't work out, don't sweat it. */
            g_hLdrMod = NIL_RTLDRMOD;
        }
    }

    ~LoadVPoxDDU()
    {
        if (g_hLdrMod != NIL_RTLDRMOD)
        {
            RTLdrClose(g_hLdrMod);
            g_hLdrMod = NIL_RTLDRMOD;
        }
    }

    static LoadVPoxDDU s_LoadVPoxDDU;
};

/* static*/ LoadVPoxDDU LoadVPoxDDU::s_LoadVPoxDDU;

