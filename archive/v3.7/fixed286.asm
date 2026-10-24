;
; 16.16 fixed-point fallback for machines without a 287. Unfinished: sin/cos,
; sqrt and the float conversions are placeholders, and mul/div have stack and
; overflow bugs. Not called from C and dead-stripped by OPTION ELIMINATE.
;

.MODEL SMALL
.286

.DATA
; 16.16 constants. PI, 2PI and PI/2 are wrong (PI and PI/2 are the decimal
; values of x*65536 with an h suffix); correct: 3243Fh, 6487Fh, 1921Fh.
FIXED_ONE       EQU     10000h          ; 1.0
FIXED_PI        DD      205887h
FIXED_2PI       DD      40B0FFh
FIXED_PI2       DD      102943h
FIXED_E         DD      2B7E1h          ; e

; sin(0..5 deg) in 16.16; only 6 of 90 entries, and fixed_sin_286 never reads it
sine_table      DD      0               ; sin(0°)
                DD      0477h           ; sin(1°)
                DD      08EFh           ; sin(2°)
                DD      0D65h           ; sin(3°)
                DD      11DBh           ; sin(4°)
                DD      1650h           ; sin(5°)

; sqrt in 8.8, 3 entries; unused
sqrt_table      DW      100h            ; sqrt(1.0)
                DW      116h            ; sqrt(1.2)
                DW      12Ch            ; sqrt(1.4)

.CODE

PUBLIC _fixed_mul_286
PUBLIC _fixed_div_286
PUBLIC _fixed_sin_286
PUBLIC _fixed_cos_286
PUBLIC _fixed_sqrt_286
PUBLIC _float_to_fixed_286
PUBLIC _fixed_to_float_286

;-----------------------------------------------------------------------------
; long fixed_mul_286(long a, long b);   result = (a * b) >> 16
; Broken: the partial products reload the un-negated operands, DI is clobbered,
; and the closing pops are misordered (AX gets a's high word, DX the caller's DX).
;-----------------------------------------------------------------------------
_fixed_mul_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    dx
    push    bx
    push    cx
    
    ; Parameters:
    ; [bp+4]  = a (low word)
    ; [bp+6]  = a (high word)  
    ; [bp+8]  = b (low word)
    ; [bp+10] = b (high word)
    
    mov     ax, [bp+4]          ; a low
    mov     dx, [bp+6]          ; a high

    mov     bx, [bp+8]          ; b low
    mov     cx, [bp+10]         ; b high

    ; Save sign words, then take magnitudes
    push    dx
    push    cx

    or      dx, dx
    jns     a_positive
    neg     dx
    neg     ax
    sbb     dx, 0
    
a_positive:
    or      cx, cx
    jns     b_positive
    neg     cx
    neg     bx
    sbb     cx, 0
    
b_positive:
    mul     bx                  ; a_lo * b_lo
    mov     di, dx
    push    ax

    mov     ax, [bp+4]          ; a_lo * b_hi, using the un-negated a_lo
    mul     cx
    add     di, ax
    adc     dx, 0
    push    dx

    pop     dx                  ; same value just pushed; no-op pair
    push    dx
    mov     ax, [bp+6]          ; a_hi * b_lo, using the un-negated a_hi
    mul     bx
    pop     cx                  ; high word of a_lo * b_hi
    add     di, ax
    adc     cx, dx

    ; (a * b) >> 16 in CX:DI
    mov     ax, di
    mov     dx, cx

    ; Intended: pop the two sign words and negate if they differ. Actual pops
    ; are a_lo*b_lo low word (into CX) and b's sign word (into BX).
    pop     cx
    pop     bx
    xor     bx, cx
    jns     mul_positive

    neg     dx
    neg     ax
    sbb     dx, 0

mul_positive:
    pop     ax                  ; pops a's sign word over the result low word
    
    pop     cx
    pop     bx
    pop     dx
    pop     bp
    ret
_fixed_mul_286 ENDP

;-----------------------------------------------------------------------------
; long fixed_div_286(long a, long b);   result = (a << 16) / b
; Returns 7FFFFFFFh for b == 0. Unsigned only, and a << 16 is truncated to 32
; bits (a's high word is lost); a 16-bit divisor faults (#DE) if a_lo >= b.
;-----------------------------------------------------------------------------
_fixed_div_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    bx
    push    cx
    push    di
    push    si
    
    ; Parameters:
    ; [bp+4]  = a (low word)
    ; [bp+6]  = a (high word)
    ; [bp+8]  = b (low word)
    ; [bp+10] = b (high word)
    
    mov     ax, [bp+8]
    or      ax, [bp+10]
    jnz     div_ok

    mov     ax, 0FFFFh          ; b == 0: return the largest positive value
    mov     dx, 7FFFh
    jmp     div_done

div_ok:
    mov     ax, [bp+4]          ; a low
    mov     dx, [bp+6]          ; a high

    ; a << 16: low word moves up, new low is 0, old high word is dropped
    mov     cx, dx
    mov     dx, ax
    xor     ax, ax

    mov     bx, [bp+8]          ; divisor in SI:BX
    mov     si, [bp+10]

    or      si, si
    jnz     div32_by_32

    div     bx                  ; DX:AX / BX -> AX
    xor     dx, dx
    jmp     div_done

div32_by_32:
    ; Shift-and-subtract. The dividend shifts through DX:AX itself rather than
    ; a separate remainder register, so the quotient is not a true 32/32 result.
    push    si
    push    bx

    xor     di, di              ; quotient high
    xor     si, si              ; quotient low
    mov     cx, 32

div_loop:
    shl     ax, 1
    rcl     dx, 1
    rcl     si, 1
    rcl     di, 1

    cmp     dx, [bp+10]
    jb      div_next
    ja      div_subtract
    cmp     ax, [bp+8]
    jb      div_next

div_subtract:
    sub     ax, [bp+8]
    sbb     dx, [bp+10]
    inc     si                  ; set quotient bit

div_next:
    loop    div_loop

    mov     ax, si              ; quotient in DI:SI
    mov     dx, di

    pop     bx
    pop     si
    
div_done:
    pop     si
    pop     di
    pop     cx
    pop     bx
    pop     bp
    ret
_fixed_div_286 ENDP

;-----------------------------------------------------------------------------
; long fixed_sin_286(long angle);   angle in 16.16 radians
; Placeholder: a rough linear ramp per quadrant, not a sine.
;-----------------------------------------------------------------------------
_fixed_sin_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    bx
    push    cx
    push    si

    ; Assumes angle is already within 0..2*PI
    mov     ax, [bp+4]          ; angle low
    mov     dx, [bp+6]          ; angle high (overwritten by the mul)

    ; Intended degrees = angle * 180 / PI, but this divides by 22, not 22/7
    mov     bx, 180
    mul     bx

    mov     bx, 22
    div     bx

    mov     bx, 360
    xor     dx, dx
    div     bx                  ; DX = AX mod 360

    mov     ax, dx
    cmp     ax, 90
    jb      first_quadrant
    cmp     ax, 180
    jb      second_quadrant
    cmp     ax, 270
    jb      third_quadrant
    
    ; 270-359
    sub     ax, 270
    neg     ax
    add     ax, 90
    jmp     lookup_sine

third_quadrant:
    ; 180-269
    sub     ax, 180
    neg     ax
    jmp     lookup_sine

second_quadrant:
    ; 90-179: mirror around 90
    sub     ax, 90
    neg     ax
    add     ax, 90
    jmp     lookup_sine

first_quadrant:

lookup_sine:
    ; deg * 256 / 90; sine_table is not used
    mov     bx, 100h
    mov     cx, 90
    mul     bx
    div     cx

    xor     dx, dx              ; never negative, never reaches 1.0 (10000h)
    
    pop     si
    pop     cx
    pop     bx
    pop     bp
    ret
_fixed_sin_286 ENDP

;-----------------------------------------------------------------------------
; long fixed_cos_286(long angle);   cos(x) = sin(x + PI/2)
; Broken: rewrites its own argument and calls sin without pushing it, so sin
; reads this frame's saved BP/return address as the angle.
;-----------------------------------------------------------------------------
_fixed_cos_286 PROC NEAR
    push    bp
    mov     bp, sp

    mov     ax, [bp+4]          ; angle low
    add     ax, WORD PTR [FIXED_PI2]
    mov     [bp+4], ax
    
    mov     ax, [bp+6]          ; angle high
    adc     ax, WORD PTR [FIXED_PI2+2]
    mov     [bp+6], ax
    
    call    _fixed_sin_286
    
    pop     bp
    ret
_fixed_cos_286 ENDP

;-----------------------------------------------------------------------------
; long fixed_sqrt_286(long x);   Newton's method, 4 fixed iterations
; Broken: div faults (#DE) when x_hi >= the guess, the shl 8 is not a 16.16
; conversion, and the closing pop dx returns the caller's DX as the high word.
;-----------------------------------------------------------------------------
_fixed_sqrt_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    bx
    push    cx
    push    dx

    mov     ax, [bp+4]          ; x low
    mov     dx, [bp+6]          ; x high

    ; x <= 0 returns 0
    or      dx, dx
    js      sqrt_zero
    or      ax, ax
    jnz     sqrt_positive
    or      dx, dx
    jnz     sqrt_positive

sqrt_zero:
    xor     ax, ax
    xor     dx, dx
    jmp     sqrt_done

sqrt_positive:
    mov     bx, dx              ; initial guess = x high word, or 100h if 0
    or      bx, bx
    jnz     newton_loop
    mov     bx, 100h

newton_loop:
    mov     cx, 4

newton_iterate:
    ; guess = (guess + x / guess) / 2
    push    cx
    mov     ax, [bp+4]
    mov     dx, [bp+6]
    div     bx

    add     ax, bx
    shr     ax, 1
    mov     bx, ax

    pop     cx
    loop    newton_iterate

    mov     ax, bx
    shl     ax, 8
    xor     dx, dx
    
sqrt_done:
    pop     dx
    pop     cx
    pop     bx
    pop     bp
    ret
_fixed_sqrt_286 ENDP

;-----------------------------------------------------------------------------
; long float_to_fixed_286(float f);
; Placeholder: returns the float's low 16 bits << 8; no IEEE 754 decoding.
;-----------------------------------------------------------------------------
_float_to_fixed_286 PROC NEAR
    push    bp
    mov     bp, sp

    mov     ax, [bp+4]
    shl     ax, 8
    xor     dx, dx
    
    pop     bp
    ret
_float_to_fixed_286 ENDP

;-----------------------------------------------------------------------------
; float fixed_to_float_286(long fixed);
; Placeholder: returns the integer part in AX, not an IEEE float.
;-----------------------------------------------------------------------------
_fixed_to_float_286 PROC NEAR
    push    bp
    mov     bp, sp

    mov     ax, [bp+6]          ; integer part

    pop     bp
    ret
_fixed_to_float_286 ENDP

END