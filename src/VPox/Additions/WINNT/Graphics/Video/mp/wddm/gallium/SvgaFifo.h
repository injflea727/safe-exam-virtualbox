/* $Id: SvgaFifo.h $ */
/** @file
 * VirtualPox Windows Guest Mesa3D - Gallium driver VMSVGA FIFO operations.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaFifo_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaFifo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Svga.h"

NTSTATUS SvgaFifoInit(PVPOXWDDM_EXT_VMSVGA pSvga);

void *SvgaFifoReserve(PVPOXWDDM_EXT_VMSVGA pSvga, uint32_t cbReserve);
void SvgaFifoCommit(PVPOXWDDM_EXT_VMSVGA pSvga, uint32_t cbActual);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaFifo_h */
