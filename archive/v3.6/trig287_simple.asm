;
; TM5000 GPIB Control System - Simplified 287 Math Functions
; Target: 80287 Math Coprocessor (Compatible Instructions Only)
; 
; Provides basic math optimizations using only 287-compatible instructions
;

.MODEL SMALL
.286
.287

.DATA
; Mathematical constants
pi_value        DQ      3.141592653589793
two_pi_value    DQ      6.283185307179586
temp_storage    DQ      ?

.CODE

PUBLIC fast_sqrt_287_simple_
PUBLIC fast_mult_287_
PUBLIC fast_div_287_

;-----------------------------------------------------------------------------
; Fast square root using 287 FSQRT (available on 287)
; double fast_sqrt_287_simple(double x);
;-----------------------------------------------------------------------------
fast_sqrt_287_simple_ PROC NEAR
    push    bp
    mov     bp, sp
    
    fld     QWORD PTR [bp+4]    ; Load x
    
    ; Check if negative
    ftst                        ; Test against zero
    fstsw   ax
    sahf
    jae     sqrt_positive
    
    ; Handle negative - return 0
    fstp    st(0)               ; Pop value
    fldz                        ; Return 0
    jmp     sqrt_done
    
sqrt_positive:
    fsqrt                       ; Calculate square root (287 instruction)
    
sqrt_done:
    pop     bp
    ret
fast_sqrt_287_simple_ ENDP

;-----------------------------------------------------------------------------
; Fast multiplication using 287
; double fast_mult_287(double a, double b);
;-----------------------------------------------------------------------------
fast_mult_287_ PROC NEAR
    push    bp
    mov     bp, sp
    
    fld     QWORD PTR [bp+4]    ; Load a
    fld     QWORD PTR [bp+12]   ; Load b
    fmul                        ; Multiply (a * b)
    
    pop     bp
    ret
fast_mult_287_ ENDP

;-----------------------------------------------------------------------------
; Fast division using 287
; double fast_div_287(double a, double b);
;-----------------------------------------------------------------------------
fast_div_287_ PROC NEAR
    push    bp
    mov     bp, sp
    
    fld     QWORD PTR [bp+4]    ; Load a
    fld     QWORD PTR [bp+12]   ; Load b
    
    ; Check for division by zero
    ftst
    fstsw   ax
    sahf
    jne     div_ok
    
    ; Division by zero - return large value
    fstp    st(0)               ; Pop b
    fstp    st(0)               ; Pop a
    fld1
    fldz
    fdiv                        ; 1/0 = infinity
    jmp     div_done
    
div_ok:
    fdiv                        ; Divide (a / b)
    
div_done:
    pop     bp
    ret
fast_div_287_ ENDP

END