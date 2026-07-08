; dumps VGA 8x8 font to "r.bin"
; written in nasm syntax
; nasm -o fontdump.com fontdump.asm
    org 100h    ; code starts at offset 100h
    use16       ; use 16-bit code
    ; get pointer to ROM font
    mov ax,1130h
    mov bh,03h          ; 8x8 font
    int 10h             ; es:bp -> ROM font
    ; save ROM font to file
    push es
    mov dx,filename     ; ds:dx -> filename
    xor cx,cx           ; normal attributes
    mov ah,3ch          ; create file
    int 21h
    jc fail             ; cf = 1 = error
    mov bx,ax           ; bx = output file handle
    mov cx,2048         ; cx = output size
    mov dx,bp
    pop ds              ; ds:dx -> data
    mov ah,40h          ; write to file
    int 21h
    jc fail             ; cf = 1 = error
    cmp ax,cx           ; ax = cx -> write ok
    jne fail
    mov ah,3eh          ; close file, bx = outputfile handle
    int 21h
    mov al,0
    jnc exit            ; cf = 1 = error
fail:
    mov al,1
exit:
    mov ah,4ch
    int 21h

; data
filename db 'r.bin',0
