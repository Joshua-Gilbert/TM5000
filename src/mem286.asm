;
; memcpy/memset/memcmp/memmove using word string ops when both pointers are even.
; Small model (near pointers) except fast_fmemcpy_286. Not called from C and
; dead-stripped by OPTION ELIMINATE.
;

.MODEL SMALL
.286

.DATA
align_temp      DW      ?               ; unused

.CODE

PUBLIC _fast_memcpy_286
PUBLIC _fast_memset_286
PUBLIC _fast_memcmp_286
PUBLIC _fast_memmove_286
PUBLIC _fast_fmemcpy_286

;-----------------------------------------------------------------------------
; void fast_memcpy_286(void *dest, void *src, unsigned int count);
;-----------------------------------------------------------------------------
_fast_memcpy_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    ds
    push    si
    push    di
    pushf
    
    ; Parameters (near pointers):
    ; [bp+4] = dest
    ; [bp+6] = src
    ; [bp+8] = count
    
    cld

    mov     di, [bp+4]      ; dest
    mov     si, [bp+6]      ; src
    mov     cx, [bp+8]      ; count

    push    ds              ; ES = DS for a near copy
    pop     es

    mov     ax, si
    or      ax, di
    test    ax, 1           ; either pointer odd -> byte copy
    jnz     byte_copy

    shr     cx, 1           ; to words; CF = odd byte left over
    rep     movsw
    jnc     copy_done
    movsb
    jmp     copy_done

byte_copy:
    rep     movsb
    
copy_done:
    popf
    pop     di
    pop     si
    pop     ds
    pop     es
    pop     bp
    ret
_fast_memcpy_286 ENDP

;-----------------------------------------------------------------------------
; void fast_fmemcpy_286(void far *dest, void far *src, unsigned int count);
;-----------------------------------------------------------------------------
_fast_fmemcpy_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    ds
    push    si
    push    di
    pushf
    
    ; Parameters (far pointers):
    ; [bp+4]  = dest offset
    ; [bp+6]  = dest segment
    ; [bp+8]  = src offset
    ; [bp+10] = src segment
    ; [bp+12] = count
    
    cld

    les     di, [bp+4]      ; ES:DI = dest
    lds     si, [bp+8]      ; DS:SI = src (BP-relative reads still use SS)
    mov     cx, [bp+12]     ; count

    mov     ax, si
    or      ax, di
    test    ax, 1
    jnz     far_byte_copy

    shr     cx, 1           ; to words; CF = odd byte left over

    ; 4K-word chunks. The cmps clobber CF from the shr: CF is always 1 at
    ; far_word_copy, so an aligned copy always moves one extra byte.
    cmp     cx, 1000h
    jb      far_word_copy

large_block:
    push    cx
    mov     cx, 1000h
    rep     movsw
    pop     cx
    sub     cx, 1000h
    cmp     cx, 1000h
    jae     large_block

far_word_copy:
    rep     movsw
    jnc     far_done
    movsb
    jmp     far_done
    
far_byte_copy:
    rep     movsb
    
far_done:
    popf
    pop     di
    pop     si
    pop     ds
    pop     es
    pop     bp
    ret
_fast_fmemcpy_286 ENDP

;-----------------------------------------------------------------------------
; void fast_memset_286(void *dest, int value, unsigned int count);
;-----------------------------------------------------------------------------
_fast_memset_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    di
    pushf
    
    ; Parameters:
    ; [bp+4] = dest
    ; [bp+6] = value (low byte used)
    ; [bp+8] = count

    cld

    push    ds
    pop     es
    mov     di, [bp+4]
    mov     cx, [bp+8]

    mov     al, [bp+6]
    mov     ah, al          ; byte duplicated for stosw

    test    di, 1
    jz      aligned_set

    stosb                   ; odd dest: one byte to reach a word boundary
    dec     cx
    jz      set_done

aligned_set:
    push    cx
    shr     cx, 1
    rep     stosw
    pop     cx

    test    cx, 1           ; trailing odd byte
    jz      set_done
    stosb
    
set_done:
    popf
    pop     di
    pop     es
    pop     bp
    ret
_fast_memset_286 ENDP

;-----------------------------------------------------------------------------
; int fast_memcmp_286(void *s1, void *s2, unsigned int count);
; Returns 0 if equal, -1 if s1<s2, 1 if s1>s2.
;-----------------------------------------------------------------------------
_fast_memcmp_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    ds
    push    si
    push    di
    pushf
    
    ; Parameters:
    ; [bp+4] = s1
    ; [bp+6] = s2
    ; [bp+8] = count
    
    cld

    mov     si, [bp+4]      ; s1
    mov     di, [bp+6]      ; s2
    mov     cx, [bp+8]      ; count
    push    ds
    pop     es

    or      cx, cx
    jz      equal

    mov     ax, si
    or      ax, di
    test    ax, 1
    jnz     byte_compare

    shr     cx, 1
    repe    cmpsw
    je      check_odd_byte

    ; Back up to the differing word and find the byte. This tests the high
    ; (later) byte first, so the sign can disagree with memcmp when both differ.
    dec     si
    dec     si
    dec     di
    dec     di
    mov     al, [si+1]
    cmp     al, [di+1]
    jne     got_diff
    mov     al, [si]
    cmp     al, [di]
    jmp     got_diff

check_odd_byte:
    test    BYTE PTR [bp+8], 1
    jz      equal
    cmpsb
    je      equal
    dec     si
    dec     di
    mov     al, [si]
    cmp     al, [di]
    jmp     got_diff
    
byte_compare:
    repe    cmpsb
    je      equal
    dec     si
    dec     di
    mov     al, [si]
    cmp     al, [di]

got_diff:
    sbb     ax, ax          ; AX = 0 if above/equal, -1 if below
    or      ax, 1           ; AX = 1 if above/equal, -1 if below
    jmp     cmp_done

equal:
    xor     ax, ax
    
cmp_done:
    popf
    pop     di
    pop     si
    pop     ds
    pop     es
    pop     bp
    ret
_fast_memcmp_286 ENDP

;-----------------------------------------------------------------------------
; void fast_memmove_286(void *dest, void *src, unsigned int count);
; Copies backward when dest > src so overlapping regions are safe.
;-----------------------------------------------------------------------------
_fast_memmove_286 PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    ds
    push    si
    push    di
    pushf
    
    ; Parameters:
    ; [bp+4] = dest
    ; [bp+6] = src
    ; [bp+8] = count
    
    mov     di, [bp+4]      ; dest
    mov     si, [bp+6]      ; src
    mov     cx, [bp+8]      ; count

    push    ds
    pop     es

    cmp     di, si
    je      move_done
    ja      backward_copy

    cld

    mov     ax, si
    or      ax, di
    test    ax, 1
    jnz     forward_bytes
    
    shr     cx, 1
    rep     movsw
    jnc     move_done
    movsb
    jmp     move_done

forward_bytes:
    rep     movsb
    jmp     move_done

backward_copy:
    std
    add     si, cx          ; point at the last byte of each block
    add     di, cx
    dec     si
    dec     di

    mov     ax, cx
    and     ax, 1
    jz      backward_even

    movsb                   ; odd count: peel one byte so the rest is whole words
    dec     cx

backward_even:
    mov     ax, si
    or      ax, di
    test    ax, 1
    jz      backward_words

    rep     movsb
    jmp     move_done

backward_words:
    dec     si              ; movsw addresses the word's low byte
    dec     di
    shr     cx, 1
    rep     movsw

move_done:
    cld
    popf
    pop     di
    pop     si
    pop     ds
    pop     es
    pop     bp
    ret
_fast_memmove_286 ENDP

END