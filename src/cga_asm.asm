;
; CGA pixel/line/clear routines for 320x200 4-color mode.
; Graphics memory is interlaced: even scanlines at B800:0000, odd at B800:2000.
; Not called from C and dead-stripped by OPTION ELIMINATE.
;

.MODEL SMALL
.286

.DATA
; 2 bits per pixel, leftmost pixel in the high bits
pixel_masks     DB      0C0h, 30h, 0Ch, 03h     ; Masks for pixels 0-3
color_shifts    DB      6, 4, 2, 0

cga_mode        DB      ?

.CODE

PUBLIC cga_init_asm_
PUBLIC cga_plot_pixel_
PUBLIC cga_draw_hline_
PUBLIC cga_draw_vline_
PUBLIC cga_clear_screen_
PUBLIC cga_fast_copy_

;-----------------------------------------------------------------------------
; void cga_init_asm(int mode);
; mode: 4 = 320x200 4-color, 6 = 640x200 2-color
;-----------------------------------------------------------------------------
cga_init_asm_ PROC NEAR
    push    bp
    mov     bp, sp
    push    ax

    mov     al, [bp+4]      ; mode
    mov     cga_mode, al

    mov     ah, 0           ; INT 10h AH=0: set video mode
    int     10h
    
    pop     ax
    pop     bp
    ret
cga_init_asm_ ENDP

;-----------------------------------------------------------------------------
; void cga_plot_pixel(int x, int y, int color);   320x200 4-color mode
;-----------------------------------------------------------------------------
cga_plot_pixel_ PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    di
    push    si
    push    ax
    push    bx
    push    cx
    push    dx
    
    ; Parameters:
    ; [bp+4] = x (0-319)
    ; [bp+6] = y (0-199)
    ; [bp+8] = color (0-3)
    
    mov     ax, 0B800h
    mov     es, ax

    mov     ax, [bp+6]      ; y
    mov     bx, ax
    shr     ax, 1           ; y/2 = row within its bank, 80 bytes per row
    mov     cx, 80
    mul     cx

    mov     cx, [bp+4]      ; x
    shr     cx, 2           ; 4 pixels per byte
    add     ax, cx

    test    bx, 1
    jz      even_line
    add     ax, 2000h       ; odd scanlines live in the second 8K bank

even_line:
    mov     di, ax

    mov     cx, [bp+4]
    and     cx, 3           ; pixel within byte (0-3)
    mov     si, cx

    mov     al, es:[di]

    mov     bl, pixel_masks[si]
    not     bl
    and     al, bl

    mov     bl, [bp+8]      ; color (0-3)
    mov     cl, color_shifts[si]
    shl     bl, cl
    or      al, bl

    mov     es:[di], al
    
    pop     dx
    pop     cx
    pop     bx
    pop     ax
    pop     si
    pop     di
    pop     es
    pop     bp
    ret
cga_plot_pixel_ ENDP

;-----------------------------------------------------------------------------
; void cga_draw_hline(int x1, int x2, int y, int color);
; Plots the unaligned edges per pixel and fills whole bytes with rep stosb.
;-----------------------------------------------------------------------------
_cga_draw_hline PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    di
    push    ax
    push    bx
    push    cx
    push    dx
    
    ; Parameters:
    ; [bp+4] = x1
    ; [bp+6] = x2
    ; [bp+8] = y
    ; [bp+10] = color
    
    ; Ensure x1 <= x2
    mov     ax, [bp+4]
    mov     bx, [bp+6]
    cmp     ax, bx
    jle     x_ordered
    xchg    ax, bx
    mov     [bp+4], ax
    mov     [bp+6], bx

x_ordered:
    mov     ax, 0B800h
    mov     es, ax

    mov     ax, [bp+8]      ; y
    mov     dx, ax          ; keep y for the odd/even test
    shr     ax, 1
    mov     cx, 80
    mul     cx

    mov     bx, [bp+4]      ; x1
    mov     cx, bx
    shr     bx, 2
    add     ax, bx

    test    dx, 1
    jz      hline_even
    add     ax, 2000h       ; odd-scanline bank

hline_even:
    mov     di, ax

    ; Replicate the 2-bit color into all 4 pixel slots of a byte
    mov     al, [bp+10]
    mov     ah, al
    shl     ah, 2
    or      al, ah
    mov     ah, al
    shl     ah, 4
    or      al, ah

    mov     cx, [bp+6]
    sub     cx, [bp+4]
    inc     cx              ; pixel count, inclusive of x2

    mov     bx, [bp+4]
    and     bx, 3
    jz      full_bytes      ; x1 already byte-aligned

    ; Leading partial byte, one pixel at a time
    push    cx
partial_first:
    push    [bp+10]         ; color
    push    [bp+8]          ; y
    push    [bp+4]          ; x
    call    cga_plot_pixel_
    add     sp, 6
    
    inc     WORD PTR [bp+4]
    dec     cx
    jz      hline_done      ; line ended inside the first byte

    mov     bx, [bp+4]
    and     bx, 3
    jnz     partial_first
    pop     cx
    sub     cx, 4
    add     cx, bx
    inc     di

full_bytes:
    push    cx
    shr     cx, 2           ; whole bytes (4 pixels each)
    jz      partial_last

    cld
    rep     stosb

partial_last:
    pop     cx
    and     cx, 3           ; trailing pixels
    jz      hline_done

partial_loop:
    push    cx
    mov     ax, [bp+6]
    sub     ax, cx
    inc     ax
    
    push    [bp+10]         ; color
    push    [bp+8]          ; y
    push    ax              ; x
    call    cga_plot_pixel_
    add     sp, 6
    
    pop     cx
    loop    partial_loop
    
hline_done:
    pop     dx
    pop     cx
    pop     bx
    pop     ax
    pop     di
    pop     es
    pop     bp
    ret
_cga_draw_hline ENDP

;-----------------------------------------------------------------------------
; void cga_draw_vline(int x, int y1, int y2, int color);
; One cga_plot_pixel call per scanline.
;-----------------------------------------------------------------------------
_cga_draw_vline PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    di
    push    ax
    push    bx
    push    cx
    
    ; Parameters:
    ; [bp+4] = x
    ; [bp+6] = y1
    ; [bp+8] = y2
    ; [bp+10] = color
    
    ; Ensure y1 <= y2
    mov     ax, [bp+6]
    mov     bx, [bp+8]
    cmp     ax, bx
    jle     y_ordered
    xchg    ax, bx
    mov     [bp+6], ax
    mov     [bp+8], bx
    
y_ordered:
    mov     cx, bx
    sub     cx, ax
    inc     cx              ; pixel count, inclusive of y2

vline_loop:
    push    cx
    
    push    [bp+10]         ; color
    push    [bp+6]          ; current y
    push    [bp+4]          ; x
    call    cga_plot_pixel_
    add     sp, 6
    
    inc     WORD PTR [bp+6]
    
    pop     cx
    loop    vline_loop
    
    pop     cx
    pop     bx
    pop     ax
    pop     di
    pop     es
    pop     bp
    ret
_cga_draw_vline ENDP

;-----------------------------------------------------------------------------
; void cga_clear_screen(int color);
;-----------------------------------------------------------------------------
_cga_clear_screen PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    di
    push    ax
    push    cx

    mov     ax, 0B800h
    mov     es, ax
    xor     di, di

    mov     al, [bp+4]      ; color, replicated into all 4 pixel slots
    mov     ah, al
    shl     ah, 2
    or      al, ah
    mov     ah, al
    shl     ah, 4
    or      al, ah
    mov     ah, al

    ; 4000h words = 32 KB, twice the 16 KB of CGA memory (both banks)
    mov     cx, 4000h
    cld
    rep     stosw
    
    pop     cx
    pop     ax
    pop     di
    pop     es
    pop     bp
    ret
_cga_clear_screen ENDP

;-----------------------------------------------------------------------------
; void cga_fast_copy(char far *source, int offset, int count);
; Starts the copy at vertical retrace to avoid CGA snow; a long copy outlasts it.
;-----------------------------------------------------------------------------
_cga_fast_copy PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    ds
    push    si
    push    di
    push    ax
    push    cx
    push    dx
    
    ; Parameters:
    ; [bp+4]  = source offset
    ; [bp+6]  = source segment
    ; [bp+8]  = CGA offset
    ; [bp+10] = byte count
    
    mov     si, [bp+4]
    mov     ds, [bp+6]

    mov     ax, 0B800h
    mov     es, ax
    mov     di, [bp+8]

    mov     cx, [bp+10]

    mov     dx, 3DAh        ; CGA status register
wait_retrace:
    in      al, dx
    test    al, 8           ; bit 3 = vertical retrace
    jz      wait_retrace

    shr     cx, 1           ; to words; CF = odd byte left over
    cld
    rep     movsw

    jnc     copy_done       ; rep movsw/cld leave CF from the shr
    movsb
    
copy_done:
    pop     dx
    pop     cx
    pop     ax
    pop     di
    pop     si
    pop     ds
    pop     es
    pop     bp
    ret
_cga_fast_copy ENDP

END