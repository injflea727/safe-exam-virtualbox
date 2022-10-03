; $Id: bs3-cmn-SelFlatDataToRealMode.asm $
;; @file
; BS3Kit - Bs3SelFlatDataToRealMode
;

;
; Copyright (C) 2007-2020 Oracle Corporation
;
; This file is part of VirtualPox Open Source Edition (OSE), as
; available from http://www.virtualpox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualPox OSE distribution. VirtualPox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualPox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;


;*********************************************************************************************************************************
;*      Header Files                                                                                                             *
;*********************************************************************************************************************************
%include "bs3kit-template-header.mac"


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
%ifdef BS3_STRICT
BS3_EXTERN_CMN Bs3Panic
%endif
TMPL_BEGIN_TEXT
%if TMPL_BITS == 16
CPU 8086
%endif


;;
; @cproto   BS3_CMN_PROTO_NOSB(uint32_t, Bs3SelFlatDataToRealMode,(uint32_t uFlatAddr));
;
; @uses     Only return registers (ax:dx, eax, eax)
; @remarks  No 20h scratch area requirements.
;
BS3_PROC_BEGIN_CMN Bs3SelFlatDataToRealMode, BS3_PBC_NEAR      ; Far stub generated by the makefile/bs3kit.h.
        push    xBP
        mov     xBP, xSP

        ;
        ; Take the simplest approach possible (64KB tiled).
        ;
%if TMPL_BITS == 16
        mov     ax, cx                  ; save cx
        mov     dx, [xBP + xCB + cbCurRetAddr + 2]
 %ifdef BS3_STRICT
        cmp     dx, _1M >> 16
        jb      .address_ok
        call    Bs3Panic
.address_ok:
 %endif
        mov     cl, 12
        shl     dx, cl
        mov     ax, cx                  ; restore cx

        mov     ax, [xBP + xCB + cbCurRetAddr]

%else
 %if TMPL_BITS == 32
        mov     eax, [xBP + xCB + cbCurRetAddr]
 %else
        mov     rax, rcx
 %endif
 %ifdef BS3_STRICT
        cmp     xAX, _1M
        jb      .address_ok
        call    Bs3Panic
.address_ok:
 %endif
        ror     eax, 16
        shl     ax, 12
        rol     eax, 16
%endif

        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3SelFlatDataToRealMode

