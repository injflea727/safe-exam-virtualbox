/* $Id: */
/** @file
 * VPoxServicePropCache - Guest property cache.
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

#ifndef GA_INCLUDED_SRC_common_VPoxService_VPoxServicePropCache_h
#define GA_INCLUDED_SRC_common_VPoxService_VPoxServicePropCache_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxServiceInternal.h"

#ifdef VPOX_WITH_GUEST_PROPS

/** @name VGSVCPROPCACHE_FLAG_XXX - Guest Property Cache Flags.
 * @{ */
/** Indicates wheter a guest property is temporary and either should
 *  - a) get a "reset" value assigned (via VPoxServicePropCacheUpdateEntry)
 *       as soon as the property cache gets destroyed, or
 *  - b) get deleted when no reset value is specified.
 */
# define VGSVCPROPCACHE_FLAGS_TEMPORARY             RT_BIT(1)
/** Indicates whether a property every time needs to be updated, regardless
 *  if its real value changed or not. */
# define VGSVCPROPCACHE_FLAGS_ALWAYS_UPDATE         RT_BIT(2)
/** The guest property gets deleted when
 *  - a) the property cache gets destroyed, or
 *  - b) the VM gets reset / shutdown / destroyed.
 */
# define VGSVCPROPCACHE_FLAGS_TRANSIENT             RT_BIT(3)
/** @}  */

int  VGSvcPropCacheCreate(PVPOXSERVICEVEPROPCACHE pCache, uint32_t uClientId);
int  VGSvcPropCacheUpdateEntry(PVPOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags, const char *pszValueReset);
int  VGSvcPropCacheUpdate(PVPOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValueFormat, ...);
int  VGSvcPropCacheUpdateByPath(PVPOXSERVICEVEPROPCACHE pCache, const char *pszValue, uint32_t fFlags,
                                const char *pszPathFormat, ...);
int  VGSvcPropCacheFlush(PVPOXSERVICEVEPROPCACHE pCache);
void VGSvcPropCacheDestroy(PVPOXSERVICEVEPROPCACHE pCache);
#endif /* VPOX_WITH_GUEST_PROPS */

#endif /* !GA_INCLUDED_SRC_common_VPoxService_VPoxServicePropCache_h */

