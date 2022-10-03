; $Id: vbdmismi.inc $
;; @file
; DMI and SMBIOS definitions for VPox.
;

;
; Copyright (C) 2006-2020 Oracle Corporation
;
; This file is part of VirtualPox Open Source Edition (OSE), as
; available from http://www.virtualpox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualPox OSE distribution. VirtualPox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

;; TODO: make dynamic

VPOX_DMI_TABLE_BASE	equ	0E1000h
VPOX_DMI_TABLE_VER	equ	25h
VPOX_DMI_TABLE_SIZE	equ	352

VPOX_SMBIOS_MAJOR_VER	equ	2
VPOX_SMBIOS_MINOR_VER	equ	5
VPOX_SMBIOS_MAXSS	equ	0FFh

