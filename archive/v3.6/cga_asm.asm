;
; TM5000 GPIB Control System - CGA Graphics Assembly Module
; Target: CGA Graphics Adapter (320x200 4-color, 640x200 2-color)
; CPU: 80286
; Memory Model: Small
;
; CGA Memory Layout:
; - Text mode: B800:0000
; - Graphics mode: B800:0000 (even lines) and B800:2000 (odd lines)
;

.MODEL SMALL
.286

.DATA
; CGA color masks for 320x200 mode (2 bits per pixel)
pixel_masks     DB      0C0h, 30h, 0Ch, 03h     ; Masks for pixels 0-3
color_shifts    DB      6, 4, 2, 0              ; Shift amounts

; Current graphics mode
cga_mode        DB      ?

.CODE

PUBLIC cga_init_asm_
PUBLIC cga_plot_pixel_
PUBLIC cga_draw_hline_
PUBLIC cga_draw_vline_
PUBLIC cga_clear_screen_
PUBLIC cga_fast_copy_

;-----------------------------------------------------------------------------
; Initialize CGA graphics mode
; void cga_init_asm(int mode);
; mode: 4 = 320x200 4-color, 6 = 640x200 2-color
;-----------------------------------------------------------------------------
cga_init_asm_ PROC NEAR
    push    bp
    mov     bp, sp
    push    ax
    
    mov     al, [bp+4]      ; Get mode parameter
    mov     cga_mode, al    ; Save for later use
    
    ; Set video mode using BIOS
    mov     ah, 0           ; Set video mode function
    int     10h             ; BIOS video interrupt
    
    pop     ax
    pop     bp
    ret
cga_init_asm_ ENDP

;-----------------------------------------------------------------------------
; Plot a pixel in CGA 320x200 4-color mode
; void cga_plot_pixel(int x, int y, int color);
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
    
    ; Set up CGA segment
    mov     ax, 0B800h
    mov     es, ax
    
    ; Calculate byte offset
    mov     ax, [bp+6]      ; y coordinate
    mov     bx, ax
    shr     ax, 1           ; y/2 (80 bytes per line pair)
    mov     cx, 80
    mul     cx              ; AX = (y/2) * 80
    
    mov     cx, [bp+4]      ; x coordinate
    shr     cx, 2           ; x/4 (4 pixels per byte)
    add     ax, cx          ; Add x byte offset
    
    ; Check if odd or even scanline
    test    bx, 1           ; Test bit 0 of y
    jz      even_line
    add     ax, 2000h       ; Add 8K offset for odd lines
    
even_line:
    mov     di, ax          ; DI = byte offset
    
    ; Calculate pixel position within byte
    mov     cx, [bp+4]      ; x coordinate
    and     cx, 3           ; x mod 4 = pixel position (0-3)
    mov     si, cx          ; Save pixel position
    
    ; Get current byte value
    mov     al, es:[di]
    
    ; Clear current pixel bits
    mov     bl, pixel_masks[si]
    not     bl
    and     al, bl          ; Clear pixel bits
    
    ; Set new pixel color
    mov     bl, [bp+8]      ; Get color (0-3)
    mov     cl, color_shifts[si]
    shl     bl, cl          ; Shift color to correct position
    or      al, bl          ; Set pixel bits
    
    ; Write back to video memory
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
; Draw horizontal line optimized for CGA
; void cga_draw_hline(int x1, int x2, int y, int color);
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
    ; Set up CGA segment
    mov     ax, 0B800h
    mov     es, ax
    
    ; Calculate starting byte offset
    mov     ax, [bp+8]      ; y coordinate
    mov     dx, ax          ; Save y for odd/even test
    shr     ax, 1           ; y/2
    mov     cx, 80
    mul     cx              ; AX = (y/2) * 80
    
    mov     bx, [bp+4]      ; x1
    mov     cx, bx
    shr     bx, 2           ; x1/4
    add     ax, bx          ; Add x byte offset
    
    ; Check if odd scanline
    test    dx, 1
    jz      hline_even
    add     ax, 2000h       ; Add 8K offset for odd lines
    
hline_even:
    mov     di, ax          ; DI = starting byte offset
    
    ; Create color pattern byte (all 4 pixels same color)
    mov     al, [bp+10]     ; Get color
    mov     ah, al
    shl     ah, 2
    or      al, ah          ; Now AL = color in bits 1-0 and 3-2
    mov     ah, al
    shl     ah, 4
    or      al, ah          ; Now AL = color pattern for all 4 pixels
    
    ; Calculate number of pixels to draw
    mov     cx, [bp+6]      ; x2
    sub     cx, [bp+4]      ; x2 - x1
    inc     cx              ; Include end pixel
    
    ; Handle partial first byte
    mov     bx, [bp+4]
    and     bx, 3           ; Starting pixel within byte
    jz      full_bytes      ; If aligned, skip partial handling
    
    ; Draw partial first byte pixels
    push    cx
partial_first:
    push    [bp+10]         ; color
    push    [bp+8]          ; y
    push    [bp+4]          ; x
    call    cga_plot_pixel_
    add     sp, 6
    
    inc     WORD PTR [bp+4]
    dec     cx
    jz      hline_done      ; If that was the only pixel
    
    mov     bx, [bp+4]
    and     bx, 3
    jnz     partial_first
    pop     cx
    sub     cx, 4
    add     cx, bx          ; Adjust count
    inc     di              ; Move to next byte
    
full_bytes:
    ; Draw full bytes (4 pixels at a time)
    push    cx
    shr     cx, 2           ; Number of full bytes
    jz      partial_last
    
    ; Fast byte fill
    cld
    rep     stosb           ; Fill with color pattern
    
partial_last:
    pop     cx
    and     cx, 3           ; Remaining pixels
    jz      hline_done
    
    ; Draw remaining pixels
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
; Draw vertical line optimized for CGA interlaced memory
; void cga_draw_vline(int x, int y1, int y2, int color);
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
    ; Set up loop counter
    mov     cx, bx
    sub     cx, ax
    inc     cx              ; Number of pixels to draw
    
    ; Draw each pixel
vline_loop:
    push    cx
    
    push    [bp+10]         ; color
    push    [bp+6]          ; current y
    push    [bp+4]          ; x
    call    cga_plot_pixel_
    add     sp, 6
    
    inc     WORD PTR [bp+6] ; Next y coordinate
    
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
; Clear CGA screen quickly
; void cga_clear_screen(int color);
;-----------------------------------------------------------------------------
_cga_clear_screen PROC NEAR
    push    bp
    mov     bp, sp
    push    es
    push    di
    push    ax
    push    cx
    
    ; Set up CGA segment
    mov     ax, 0B800h
    mov     es, ax
    xor     di, di          ; Start at offset 0
    
    ; Create color pattern
    mov     al, [bp+4]      ; Get color
    mov     ah, al
    shl     ah, 2
    or      al, ah
    mov     ah, al
    shl     ah, 4
    or      al, ah          ; AL = color pattern for 4 pixels
    mov     ah, al          ; AX = color pattern word
    
    ; Clear 16K of video memory (both banks)
    mov     cx, 4000h       ; 16K words
    cld
    rep     stosw           ; Fast word fill
    
    pop     cx
    pop     ax
    pop     di
    pop     es
    pop     bp
    ret
_cga_clear_screen ENDP

;-----------------------------------------------------------------------------
; Fast memory copy to CGA during vertical retrace
; void cga_fast_copy(char far *source, int offset, int count);
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
    
    ; Set up source
    mov     si, [bp+4]
    mov     ds, [bp+6]
    
    ; Set up destination
    mov     ax, 0B800h
    mov     es, ax
    mov     di, [bp+8]
    
    ; Get count
    mov     cx, [bp+10]
    
    ; Wait for vertical retrace start
    mov     dx, 3DAh        ; CGA status register
wait_retrace:
    in      al, dx
    test    al, 8           ; Test vertical retrace bit
    jz      wait_retrace
    
    ; Fast copy during retrace
    shr     cx, 1           ; Convert to words
    cld
    rep     movsw           ; Fast word copy
    
    ; Handle odd byte
    jnc     copy_done
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