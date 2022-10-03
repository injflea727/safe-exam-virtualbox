/* $Id: VPoxStubCertUtil.h $ */
/** @file
 * VPoxStub - VirtualPox's Windows installer stub (certificate manipulations).
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

#ifndef VPOX_INCLUDED_SRC_Stub_VPoxStubCertUtil_h
#define VPOX_INCLUDED_SRC_Stub_VPoxStubCertUtil_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

extern bool addCertToStore(DWORD dwDst, const char *pszStoreNm, const unsigned char kpCertBuf[], DWORD cbCertBuf);

#endif /* !VPOX_INCLUDED_SRC_Stub_VPoxStubCertUtil_h */

