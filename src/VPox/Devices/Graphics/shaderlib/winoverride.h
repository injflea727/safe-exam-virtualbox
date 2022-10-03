/* $Id: winoverride.h $ */
/** @file
 * DevVMWare/Shaderlib - Wine Function Portability Overrides
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

#ifndef VPOX_INCLUDED_SRC_Graphics_shaderlib_winoverride_h
#define VPOX_INCLUDED_SRC_Graphics_shaderlib_winoverride_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define GetProcessHeap()    ((HANDLE)0)
#define HeapAlloc           VPoxHeapAlloc
#define HeapFree            VPoxHeapFree
#define HeapReAlloc         VPoxHeapReAlloc
#define DebugBreak          VPoxDebugBreak

LPVOID      WINAPI VPoxHeapAlloc(HANDLE hHeap, DWORD heaptype, SIZE_T size);
BOOL        WINAPI VPoxHeapFree(HANDLE hHeap, DWORD heaptype, LPVOID ptr);
LPVOID      WINAPI VPoxHeapReAlloc(HANDLE hHeap,DWORD heaptype, LPVOID ptr, SIZE_T size);
void VPoxDebugBreak(void);

#endif /* !VPOX_INCLUDED_SRC_Graphics_shaderlib_winoverride_h */

