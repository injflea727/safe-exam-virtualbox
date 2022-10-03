/* $Id: VPoxDispD3DCmn.h $ */
/** @file
 * VPoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3DCmn_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3DCmn_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VPoxDispD3DBase.h"

#include <iprt/asm.h>
#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VPox/Log.h>

#include <VPox/VPoxGuestLib.h>

#include "VPoxDispDbg.h"
#include "VPoxDispD3DIf.h"
#include "../../common/wddm/VPoxMPIf.h"
#include "VPoxDispMpInternal.h"
#include <VPoxDispKmt.h>
#include "VPoxDispD3D.h"
#include "VPoxD3DIf.h"

# ifdef VPOXWDDMDISP
#  define VPOXWDDMDISP_DECL(_type) DECLEXPORT(_type)
# else
#  define VPOXWDDMDISP_DECL(_type) DECLIMPORT(_type)
# endif

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VPoxDispD3DCmn_h */
