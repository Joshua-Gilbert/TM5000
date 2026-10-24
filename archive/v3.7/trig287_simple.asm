;
; sqrt/mul/div on the 287. Despite the name there is no trig: FSIN/FCOS are
; 387-only. Not called from C and dead-stripped by OPTION ELIMINATE.
;

.MODEL SMALL
.286
.287

.DATA
; Unused
pi_value        DQ      3.141592653589793
two_pi_value    DQ      6.283185307179586
temp_storage    DQ      ?

.CODE

PUBLIC fast_sqrt_287_simple_
PUBLIC fast_mult_287_
PUBLIC fast_div_287_

;-----------------------------------------------------------------------------
; double fast_sqrt_287_simple(double x);   x < 0 returns 0
;-----------------------------------------------------------------------------
fast_sqrt_287_simple_ PROC NEAR
    push    bp
    mov     bp, sp

    fld     QWORD PTR [bp+4]

    ftst
    fstsw   ax                  ; FSTSW AX is 287+, not on the 8087
    sahf                        ; C0 -> CF, C3 -> ZF
    jae     sqrt_positive

    fstp    st(0)
    fldz
    jmp     sqrt_done

sqrt_positive:
    fsqrt
    
sqrt_done:
    pop     bp
    ret
fast_sqrt_287_simple_ ENDP

;-----------------------------------------------------------------------------
; double fast_mult_287(double a, double b);
;-----------------------------------------------------------------------------
fast_mult_287_ PROC NEAR
    push    bp
    mov     bp, sp

    fld     QWORD PTR [bp+4]    ; a
    fld     QWORD PTR [bp+12]   ; b
    fmul
    
    pop     bp
    ret
fast_mult_287_ ENDP

;-----------------------------------------------------------------------------
; double fast_div_287(double a, double b);
; b == 0 returns +inf (1/0, sign of a ignored); assumes zero-divide is masked.
;-----------------------------------------------------------------------------
fast_div_287_ PROC NEAR
    push    bp
    mov     bp, sp

    fld     QWORD PTR [bp+4]    ; a
    fld     QWORD PTR [bp+12]   ; b

    ftst
    fstsw   ax
    sahf
    jne     div_ok

    fstp    st(0)               ; pop b
    fstp    st(0)               ; pop a
    fld1
    fldz
    fdiv
    jmp     div_done

div_ok:
    fdiv                        ; a / b
    
div_done:
    pop     bp
    ret
fast_div_287_ ENDP

END