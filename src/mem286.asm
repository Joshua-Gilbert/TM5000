;
; TM5000 GPIB Control System - 286 Optimized Memory Operations
; Target: 80286 CPU
; Memory Model: Small (near code, far data support)
;
; This module provides optimized memory operations for 286 processors,
; taking advantage of 286-specific instructions and addressing modes.
;

.MODEL SMALL
.286

.DATA
; Alignment check flags
align_temp      DW      ?

.CODE

PUBLIC _fast_memcpy_286
PUBLIC _fast_memset_286
PUBLIC _fast_memcmp_286
PUBLIC _fast_memmove_286
PUBLIC _fast_fmemcpy_286

;-----------------------------------------------------------------------------
; Fast memory copy optimized for 286
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
    
    ; Set up for string operations
    cld                     ; Forward direction
    
    ; Load pointers
    mov     di, [bp+4]      ; Destination
    mov     si, [bp+6]      ; Source
    mov     cx, [bp+8]      ; Count
    
    ; Use DS=ES for near copy
    push    ds
    pop     es
    
    ; Check if we can align to word boundary
    mov     ax, si
    or      ax, di
    test    ax, 1           ; Check if either is odd
    jnz     byte_copy       ; If misaligned, use byte copy
    
    ; Aligned word copy
    shr     cx, 1           ; Convert to word count
    rep     movsw           ; 286 optimized word move
    jnc     copy_done       ; If no carry, we're done
    movsb                   ; Copy odd byte
    jmp     copy_done
    
byte_copy:
    ; Unaligned byte copy
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
; Fast far memory copy for large data
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
    
    cld                     ; Forward direction
    
    ; Load far pointers
    les     di, [bp+4]      ; ES:DI = destination
    lds     si, [bp+8]      ; DS:SI = source
    mov     cx, [bp+12]     ; Count
    
    ; Check alignment
    mov     ax, si
    or      ax, di
    test    ax, 1
    jnz     far_byte_copy
    
    ; Aligned word copy
    shr     cx, 1           ; Convert to words
    
    ; For large copies, use 286 block move optimization
    cmp     cx, 1000h       ; If more than 4K words
    jb      far_word_copy
    
    ; Large block copy - minimize loop overhead
large_block:
    push    cx
    mov     cx, 1000h       ; Copy 4K words at a time
    rep     movsw
    pop     cx
    sub     cx, 1000h
    cmp     cx, 1000h
    jae     large_block
    
far_word_copy:
    rep     movsw           ; Copy remaining words
    jnc     far_done
    movsb                   ; Copy odd byte
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
; Fast memory set optimized for 286
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
    ; [bp+6] = value (byte value)
    ; [bp+8] = count
    
    cld                     ; Forward direction
    
    ; Set up destination
    push    ds
    pop     es
    mov     di, [bp+4]      ; Destination
    mov     cx, [bp+8]      ; Count
    
    ; Prepare value
    mov     al, [bp+6]      ; Get byte value
    mov     ah, al          ; Duplicate for word operations
    
    ; Check alignment
    test    di, 1
    jz      aligned_set
    
    ; Handle unaligned start
    stosb                   ; Store first byte
    dec     cx
    jz      set_done
    
aligned_set:
    ; Fast word fill
    push    cx
    shr     cx, 1           ; Convert to words
    rep     stosw           ; Fast word store
    pop     cx
    
    ; Handle odd byte
    test    cx, 1
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
; Fast memory compare optimized for 286
; int fast_memcmp_286(void *s1, void *s2, unsigned int count);
; Returns: 0 if equal, <0 if s1<s2, >0 if s1>s2
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
    
    cld                     ; Forward direction
    
    ; Set up pointers
    mov     si, [bp+4]      ; s1
    mov     di, [bp+6]      ; s2
    mov     cx, [bp+8]      ; count
    push    ds
    pop     es              ; ES = DS
    
    ; Check if count is zero
    or      cx, cx
    jz      equal
    
    ; Check alignment
    mov     ax, si
    or      ax, di
    test    ax, 1
    jnz     byte_compare
    
    ; Word compare
    shr     cx, 1
    repe    cmpsw           ; Compare words
    je      check_odd_byte  ; If all words equal
    
    ; Words differ - back up to find exact byte
    dec     si
    dec     si
    dec     di
    dec     di
    mov     al, [si+1]      ; High byte of word
    cmp     al, [di+1]
    jne     got_diff
    mov     al, [si]        ; Low byte
    cmp     al, [di]
    jmp     got_diff
    
check_odd_byte:
    ; Check if there was an odd byte
    test    BYTE PTR [bp+8], 1
    jz      equal
    cmpsb                   ; Compare last byte
    je      equal
    dec     si
    dec     di
    mov     al, [si]
    cmp     al, [di]
    jmp     got_diff
    
byte_compare:
    repe    cmpsb           ; Byte-by-byte compare
    je      equal
    dec     si
    dec     di
    mov     al, [si]
    cmp     al, [di]
    
got_diff:
    ; Calculate return value
    sbb     ax, ax          ; AX = 0 if above/equal, -1 if below
    or      ax, 1           ; AX = 1 if above/equal, -1 if below
    jmp     cmp_done
    
equal:
    xor     ax, ax          ; Return 0 for equal
    
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
; Fast memory move with overlap handling
; void fast_memmove_286(void *dest, void *src, unsigned int count);
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
    
    ; Load parameters
    mov     di, [bp+4]      ; Destination
    mov     si, [bp+6]      ; Source
    mov     cx, [bp+8]      ; Count
    
    ; Set up segments
    push    ds
    pop     es
    
    ; Check for overlap
    cmp     di, si
    je      move_done       ; Same address, nothing to do
    ja      backward_copy   ; dest > src, copy backward
    
    ; Forward copy (dest < src or no overlap)
    cld
    
    ; Check alignment for forward copy
    mov     ax, si
    or      ax, di
    test    ax, 1
    jnz     forward_bytes
    
    ; Word-aligned forward copy
    shr     cx, 1
    rep     movsw
    jnc     move_done
    movsb
    jmp     move_done
    
forward_bytes:
    rep     movsb
    jmp     move_done
    
backward_copy:
    ; Set up for backward copy
    std                     ; Backward direction
    add     si, cx
    add     di, cx
    dec     si
    dec     di
    
    ; Check if we can do word copies
    mov     ax, cx
    and     ax, 1
    jz      backward_even
    
    ; Copy odd byte first
    movsb
    dec     cx
    
backward_even:
    ; Check alignment for backward copy
    mov     ax, si
    or      ax, di
    test    ax, 1
    jz      backward_words
    
    ; Unaligned backward byte copy
    rep     movsb
    jmp     move_done
    
backward_words:
    ; Aligned backward word copy
    dec     si              ; Adjust for word access
    dec     di
    shr     cx, 1
    rep     movsw
    
move_done:
    cld                     ; Restore forward direction
    popf
    pop     di
    pop     si
    pop     ds
    pop     es
    pop     bp
    ret
_fast_memmove_286 ENDP

END