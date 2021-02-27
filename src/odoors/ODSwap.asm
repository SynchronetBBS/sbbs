;  OpenDoors Online Software Programming Toolkit
;  (C) Copyright 1991 - 1999 by Brian Pirie.
; 
;  This library is free software; you can redistribute it and/or
;  modify it under the terms of the GNU Lesser General Public
;  License as published by the Free Software Foundation; either
;  version 2 of the License, or (at your option) any later version.
; 
;  This library is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  Lesser General Public License for more details.
; 
;  You should have received a copy of the GNU Lesser General Public
;  License along with this library; if not, write to the Free Software
;  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;
;
;         File: ODSwap.asm
;
;  Description: Performs EMS/disk swapping and low level spawning
;               activities. This file should only be included when building
;               the MS-DOS version of OpenDoors.
;
;    Revisions: Date          Ver   Who  Change
;               ---------------------------------------------------------------
;               Oct 13, 1994  6.00  BP   New file header format.
;               Feb 19, 1996  6.00  BP   Changed version number to 6.00.
;               Mar 03, 1996  6.10  BP   Begin version 6.10.


; If you have increased the file handle table size so that more than 20 files
; may be open in the parent process, set FHTSZ to the new size.

FHTSZ           EQU     20

IFDEF LCODE
ARG_1           EQU     6
ELSE
ARG_1           EQU     4
ENDIF

arena           struc                           ; arena header
sig             db      0                       ; 'M' or 'Z' if last block
own             dw      0                       ; PSP of owner or 0 if free
siz             dw      0                       ; size not including header
arena           ends

vector          struc
number          db      0                       ; vector number
flag            db      0                       ; 0-CURRENT,1-IRET,2-free,3-end
vseg            dw      0                       ; vector segment
voff            dw      0                       ; vector offset
vector          ends

_TEXT           SEGMENT word public 'CODE'
                ASSUME  cs:_TEXT
                ASSUME  ds:nothing
                ASSUME  es:nothing
                ASSUME  ss:nothing

; The code between slidetop and slidebot constitutes the spawn kernel.  The
; kernel is copied to the front of the parent process immediately following the
; parent's PSP.  The environment passed to the child is copied to immediately
; following the kernel.

slidetop:

path            db      79 dup (0)              ; program to execute
command         db      128 dup (0)             ; command-line
file            db      79 dup (0)              ; swap file

parmblk         label   byte                    ; parameter block
environ         dw      0                       ; environment block
cmd             dw      0,0                     ; command-line tail
fcb1            dw      0,0                     ; first file control block
fcb2            dw      0,0                     ; second file control block

fcb5c           db      10h dup (0)             ; first file control block
fcb6c           db      10h dup (0)             ; second file control block

cntsvl          dw      0                       ; count save low
cntsvh          dw      0                       ; count save high
tmpcode         dw      0                       ; temporary return code
env             dw      0                       ; environment segment
envlen          dw      0                       ; environment length
parsz           dw      0                       ; parent size
ttlsz           dw      0                       ; total size
oldsz           dw      0                       ; old size
newsz           dw      0                       ; new size
emsseg          dw      0                       ; EMS page frame segment
handle          dw      0                       ; EMS handle
useems          db      0                       ; if 0, use EMS
save            db      4 dup (0)               ; save 4 bytes at DS:[2Eh]
f1add           dd      0                       ; fnish1 address
last            db      0                       ; if 0, last block swap
IF FHTSZ - 20
fhtsv           db      FHTSZ dup (0)           ; file handle table save
ENDIF

errmsg          db      'spawn error',0Dh,0Ah
msglen          EQU     $-errmsg

                EVEN
lclstk          dw      64 dup (0)              ; local stack
stktop          label   word                    ; stack top

slide1:         mov     ax,cs                   ; install local stack
                cli
                mov     ss,ax
                mov     sp,offset stktop - offset slidetop + 100h
                sti

; copy environment

                mov     bx,offset slidebot - offset slidetop + 15 + 100h
                mov     cl,4
                shr     bx,cl                   ; convert to paragraphs
                add     bx,ax                   ; add CS (actually PSP)
index = offset environ - offset slidetop + 100h
                mov     cs:[index],bx           ; parameter block
                mov     es,bx
                xor     di,di
index = offset env - offset slidetop + 100h
                mov     ds,cs:[index]
                xor     si,si
index = offset envlen - offset slidetop + 100h
                mov     cx,cs:[index]
                shr     cx,1                    ; translate to word count
                rep     movsw                   ; CF set if one byte left over
                adc     cx,cx                   ; CX = 1 or 0, depending CF
                rep     movsb                   ; possible final byte

                dec     ax                      ; PSP segment
                mov     es,ax                   ; program arena header
                mov     bx,es:[siz]
index = offset oldsz - offset slidetop + 100h
                mov     cs:[index],bx           ; old size
                mov     byte ptr es:[sig],'M'   ; not last
index = offset newsz - offset slidetop + 100h
                mov     bx,cs:[index]           ; new size
                mov     es:[siz],bx

                inc     ax                      ; PSP segment
                add     ax,bx                   ; add new size
                mov     es,ax                   ; new last arena header
                mov     byte ptr es:[sig],'Z'   ; last
index = offset last - offset slidetop + 100h
                cmp     byte ptr cs:[index],0
                je      slide2                  ; jump if last block swap
                mov     byte ptr es:[sig],'M'   ; not last

slide2:         mov     word ptr es:[own],0     ; free
index = offset ttlsz - offset slidetop + 100h
                mov     ax,cs:[index]           ; total size
                sub     ax,bx                   ; subtract new size
                dec     ax                      ; account for arena header
                mov     es:[siz],ax

; save 4 bytes destroyed by DOS 2.0 at DS:2Eh

                mov     ax,cs                   ; PSP segment
                mov     es,ax
                mov     bx,es:[2Eh]
index = offset save - offset slidetop + 100h
                mov     cs:[index],bx
                mov     bx,es:[30h]
index = offset save - offset slidetop + 102h
                mov     cs:[index],bx

                mov     bx,offset parmblk - offset slidetop + 100h
                mov     ds,ax                   ; PSP segment
                mov     dx,100h                 ; offset path
                mov     ax,4B00h                ; load and execute program
                int     21h
                jnc     slide3                  ; jump if no error
index = offset tmpcode - offset slidetop + 100h
                mov     cs:[index],ax           ; temporary return code

slide3:         mov     ax,cs                   ; install local stack
                cli
                mov     ss,ax
                mov     sp,offset stktop - offset slidetop + 100h
                sti

; restore 4 bytes destroyed by DOS 2.0 at DS:2Eh

                mov     es,ax                   ; PSP segment
index = offset save - offset slidetop + 100h
                mov     bx,cs:[index]
                mov     es:[2Eh],bx
index = offset save - offset slidetop + 102h
                mov     bx,cs:[index]
                mov     es:[30h],bx

index = offset oldsz - offset slidetop + 100h
                mov     bx,cs:[index]           ; old size
                mov     ah,4Ah                  ; resize memory block
                int     21h
                jnc     slide7

index = offset useems - offset slidetop + 100h
slide4:         cmp     byte ptr cs:[index],0
                jne     slide6                  ; jump if don't use EMS
index = offset handle - offset slidetop + 100h
                mov     dx,cs:[index]           ; EMS handle

slide5:         mov     ah,45h                  ; release handle and memory
                int     67h
                cmp     ah,82h                  ; memory manager busy?
                je      slide5                  ; jump if busy

slide6:         jmp     slide18                 ; exit

index = offset parsz - offset slidetop + 100h
slide7:         mov     bx,cs:[index]           ; parent size
index = offset ttlsz - offset slidetop + 100h
                mov     ax,cs:[index]           ; total size
                sub     ax,bx                   ; subtract parent size
                or      ax,ax
                jz      slide9
                mov     dx,cs                   ; PSP segment
                add     dx,bx                   ; add parent size
                mov     es,dx                   ; new last arena header
                mov     byte ptr es:[sig],'Z'   ; last
index = offset last - offset slidetop + 100h
                cmp     byte ptr cs:[index],0
                je      slide8                  ; jump if last block swap
                mov     byte ptr es:[sig],'M'   ; not last

slide8:         mov     word ptr es:[own],0     ; free
                dec     ax                      ; account for arena header
                mov     es:[siz],ax

slide9:         push    cs                      ; PSP segment
index = offset useems - offset slidetop + 100h
                cmp     byte ptr cs:[index],0
                jne     slide14                 ; jump if don't use EMS
                pop     es                      ; PSP segment
                mov     di,offset slidebot - offset slidetop + 100h
index = offset emsseg - offset slidetop + 100h
                mov     ds,cs:[index]           ; EMS page frame segment
                mov     si,offset slidebot - offset slidetop
index = offset handle - offset slidetop + 100h
                mov     dx,cs:[index]           ; EMS handle
                xor     bx,bx                   ; logical page number
                mov     cx,16384 - ( offset slidebot - offset slidetop )
                jmp     short slide13

index = offset cntsvl - offset slidetop + 100h
slide10:        sub     cs:[index],cx
index = offset cntsvh - offset slidetop + 100h
                sbb     word ptr cs:[index],0
                xor     al,al                   ; physical page number

slide11:        mov     ah,44h                  ; map memory
                int     67h
                or      ah,ah
                jz      slide12
                cmp     ah,82h                  ; memory manager busy?
                je      slide11                 ; jump if busy
                jmp     slide4                  ; exit

slide12:        shr     cx,1                    ; translate to word count
                rep     movsw                   ; CF set if one byte left over
                adc     cx,cx                   ; CX = 1 or 0, depending CF
                rep     movsb                   ; possible final byte
                xor     si,si
                mov     cx,16384                ; assume copy full block
                inc     bx                      ; logical page number
                cmp     bx,1
                je      slide13
                mov     ax,es
                add     ax,1024                 ; 16384 bytes
                mov     es,ax
                mov     di,16640                ; 16384 + 100h

index = offset cntsvh - offset slidetop + 100h
slide13:        cmp     word ptr cs:[index],0
                jne     slide10                 ; jump if more than full block
index = offset cntsvl - offset slidetop + 100h
                cmp     cs:[index],cx
                jae     slide10                 ; jump if at least full block
                mov     cx,cs:[index]           ; CX = cntsvl
                cmp     cx,0
                jne     slide10                 ; jump if more left to copy
                jmp     short slide17

slide14:        pop     ds                      ; PSP segment
IF FHTSZ - 20

; restore the file handle table from the kernel

                mov     si,offset fhtsv - offset slidetop + 100h
                mov     es,ds:[36h]             ; file handle table segment
                mov     di,ds:[34h]             ; file handle table offset
                mov     cx,FHTSZ                ; file handle table size
                rep     movsb
ENDIF
                mov     dx,offset file - offset slidetop + 100h
                mov     ax,3D00h                ; open file read only
                int     21h
                jc      slide18                 ; exit if error
                mov     bx,ax                   ; handle

                xor     cx,cx
                mov     dx,offset slidebot - offset slidetop
                mov     ax,4200h                ; move file pointer
                int     21h                     ;  from beginning of file

                mov     dx,offset slidebot - offset slidetop + 100h
                mov     cx,65520                ; assume read full block
                jmp     short slide16

index = offset cntsvl - offset slidetop + 100h
slide15:        sub     cs:[index],cx
index = offset cntsvh - offset slidetop + 100h
                sbb     word ptr cs:[index],0
                mov     ah,3Fh                  ; read file
                int     21h
                jc      slide18                 ; exit if error
                cmp     ax,cx
                jne     slide18                 ; exit if not all read

                mov     ax,ds
                add     ax,4095                 ; 65520 bytes
                mov     ds,ax

index = offset cntsvh - offset slidetop + 100h
slide16:        cmp     word ptr cs:[index],0
                jne     slide15                 ; jump if more than full block
index = offset cntsvl - offset slidetop + 100h
                cmp     word ptr cs:[index],65520
                jae     slide15                 ; jump if at least full block
                mov     cx,cs:[index]           ; CX = cntsvl
                cmp     cx,0
                jne     slide15                 ; jump if more left to read

index = offset tmpcode - offset slidetop + 100h
slide17:        mov     ax,cs:[index]           ; temporary return code
index = offset f1add - offset slidetop + 100h
                jmp     dword ptr cs:[index]

slide18:        push    cs                      ; PSP segment
                pop     ds
                mov     dx,offset errmsg - offset slidetop + 100h
                mov     cx,msglen               ; errmsg length
                mov     bx,2                    ; standard error device handle
                mov     ah,40h                  ; write error message
                int     21h
                mov     ax,4C01h                ; terminate with return code
                int     21h

handler:        iret                            ; interrupt handler

slidebot:

cntl            dw      0                       ; count low
cnth            dw      0                       ; count high
stks            dw      0                       ; original SS contents
stkp            dw      0                       ; original SP contents
psp             dw      0                       ; PSP segment
s1add           dd      0                       ; slide1 address
rcode           dw      0                       ; return code
useems2         db      0                       ; if 0, use EMS
vtabseg         dw      0                       ; vectab1 segment
vtaboff         dw      0                       ; vectab1 offset

errmsg2         db      'spawn error',0Dh,0Ah
msglen2         EQU     $-errmsg2

;
; int _xspawn( char *, char *, char *, VECTOR *, int, int, char *, int );
;

                PUBLIC  __xspawn
IFDEF LCODE
__xspawn        PROC    far
ELSE
__xspawn        PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    di                      ; preserve register variables
                push    si
                push    ds

IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1]
ELSE
                mov     si,word ptr [bp+ARG_1]
ENDIF
                mov     di,offset path

start1:         mov     al,ds:[si]              ; copy path string
                mov     cs:[di],al              ;  to code segment
                inc     si
                inc     di
                or      al,al                   ; null char?
                jnz     start1                  ; no, get next char

IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+4]
ELSE
                mov     si,word ptr [bp+ARG_1+2]
ENDIF
                mov     bx,si                   ; preserve si
                mov     di,offset command
                mov     cx,2                    ; account for count and '\r'
                add     cl,byte ptr ds:[bx]     ; add count byte

start2:         mov     al,ds:[bx]              ; copy command
                mov     cs:[di],al              ;  to code segment
                inc     bx
                inc     di
                loop    start2                  ; get next char

                inc     si                      ; skip count byte
                push    cs
                pop     es
                mov     di,offset fcb5c
                mov     ax,2901h                ; parse filename
                int     21h                     ;  skip leading separators
                mov     di,offset fcb6c
                mov     al,1                    ; parse filename
                int     21h                     ;  skip leading separators

IFDEF LDATA
                mov     ax,word ptr [bp+ARG_1+8]
ELSE
                mov     ax,word ptr [bp+ARG_1+4]
ENDIF
                mov     cl,4
                shr     ax,cl                   ; convert to paragraphs
IFDEF LDATA
                mov     bx,word ptr [bp+ARG_1+10]
ELSE
                mov     bx,ds
ENDIF
                add     ax,bx
                mov     cs:[env],ax             ; environment segment

IFDEF LDATA
                lds     bx,dword ptr [bp+ARG_1+12] ; vectab1
ELSE
                mov     bx,word ptr [bp+ARG_1+6] ; vectab1
ENDIF
                mov     cs:[vtabseg],ds         ; vectab1 segment
                mov     cs:[vtaboff],bx         ; vectab1 offset

                mov     cs:[stks],ss            ; original SS contents
                mov     cs:[stkp],sp            ; original SP contents
                mov     cs:[rcode],0            ; assume success

IFDEF LDATA
                mov     ax,word ptr [bp+ARG_1+16]
ELSE
                mov     ax,word ptr [bp+ARG_1+8]
ENDIF
                or      ax,ax                   ; do swap?
                jz      start3                  ; yes, jump
                jmp     noswap1

IFDEF LDATA
start3:         mov     ax,word ptr [bp+ARG_1+18]
ELSE
start3:         mov     ax,word ptr [bp+ARG_1+10]
ENDIF
                mov     cs:[envlen],ax          ; environment length
                add     ax,offset slidebot - offset slidetop + 30 + 100h
                mov     cl,4
                shr     ax,cl                   ; convert to paragraphs
                mov     cs:[newsz],ax           ; new size

IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+20]
ELSE
                mov     si,word ptr [bp+ARG_1+12]
ENDIF
                mov     di,offset file
                mov     cs:[useems2],1          ; assume don't use EMS
                cmp     byte ptr ds:[si],0
                jne     start4
                mov     cs:[useems2],0          ; use EMS

start4:         mov     al,ds:[si]              ; copy file string
                mov     cs:[di],al              ;  to code segment
                inc     si
                inc     di
                or      al,al                   ; null char?
                jnz     start4                  ; no, get next char

; save fnish1 address

                mov     word ptr cs:[f1add+2],cs
                mov     word ptr cs:[f1add],offset fnish1

; initialize parameter block

                mov     ax,cs:[psp]             ; PSP segment
                mov     cs:[cmd],offset command - offset slidetop + 100h
                mov     cs:[cmd+2],ax
                mov     cs:[fcb1],offset fcb5c - offset slidetop + 100h
                mov     cs:[fcb1+2],ax
                mov     cs:[fcb2],offset fcb6c - offset slidetop + 100h
                mov     cs:[fcb2+2],ax

                cld                             ; left to right direction

                mov     ds,ax                   ; PSP segment
IFDEF LDATA
                mov     dx,word ptr [bp+ARG_1+24]
ELSE
                mov     dx,word ptr [bp+ARG_1+14]
ENDIF
IF FHTSZ - 20
                cmp     word ptr ds:[32h],FHTSZ ; file handle table size
                je      start5                  ; jump if OK
                mov     cs:[rcode],5
                jmp     short start6
ENDIF

start5:         mov     ax,cs:[newsz]           ; new size
                cmp     ax,cs:[ttlsz]           ; new size < total size?
                jb      start8                  ; yes, jump
                mov     cs:[rcode],7            ; extremely unlikely

start6:         cmp     cs:[useems2],0
                jne     start7                  ; jump if don't use EMS
                jmp     fnish2

start7:         mov     bx,dx                   ; file handle
                jmp     fnish6

start8:         cmp     cs:[useems2],0
                jne     start12                 ; jump if don't use EMS
                mov     cs:[useems],0           ; use EMS
                mov     cs:[handle],dx          ; EMS handle
                mov     es,cs:[emsseg]          ; EMS page frame segment
                xor     bx,bx                   ; logical page number
                jmp     short start11

start9:         sub     cs:[cntl],cx
                sbb     cs:[cnth],0
                call    mapems
                or      ah,ah
                jz      start10                 ; jump if map succeeded
                jmp     fnish2

start10:        mov     si,100h
                xor     di,di
                shr     cx,1                    ; translate to word count
                rep     movsw                   ; CF set if one byte left over
                adc     cx,cx                   ; CX = 1 or 0, depending CF
                rep     movsb                   ; possible final byte
                inc     bx                      ; logical page number
                mov     ax,ds
                add     ax,1024                 ; 16384 bytes
                mov     ds,ax

start11:        mov     cx,16384                ; assume copy full block
                cmp     cs:[cnth],0
                jne     start9                  ; jump if more than full block
                cmp     cs:[cntl],16384
                jae     start9                  ; jump if at least full block
                mov     cx,cs:[cntl]
                cmp     cx,0
                jne     start9                  ; jump if more left to copy
                jmp     short start17

start12:        mov     cs:[useems],1           ; don't use EMS
                mov     bx,dx                   ; handle
                mov     dx,100h                 ; DS:DX segment:offset buffer
                mov     cx,65520                ; assume write full block
                jmp     short start16

start13:        sub     cs:[cntl],cx
                sbb     cs:[cnth],0
                mov     ah,40h                  ; write file
                int     21h
                jc      start14                 ; jump if error
                cmp     ax,cx
                je      start15                 ; jump if all written

start14:        mov     ah,3Eh                  ; close file
                int     21h
                mov     cs:[rcode],5
                jmp     fnish7

start15:        mov     ax,ds
                add     ax,4095                 ; 65520 bytes
                mov     ds,ax

start16:        cmp     cs:[cnth],0
                jne     start13                 ; jump if more than full block
                cmp     cs:[cntl],65520
                jae     start13                 ; jump if at least full block
                mov     cx,cs:[cntl]
                cmp     cx,0
                jne     start13                 ; jump if more left to write

                mov     ah,3Eh                  ; close file
                int     21h
IF FHTSZ - 20

; save the file handle table in the kernel

                mov     es,cs:[psp]             ; PSP segment
                mov     ds,es:[36h]             ; file handle table segment
                mov     si,es:[34h]             ; file handle table offset
                push    cs
                pop     es
                mov     di,offset fhtsv         ; file handle table save
                mov     cx,FHTSZ                ; file handle table size
                rep     movsb
ENDIF

start17:        mov     cx,cs
                mov     dx,offset handler       ; interrupt handler offset
                call    safevect                ; set vectors in vectab1

; time to copy the kernel

                mov     es,cs:[psp]             ; PSP segment
                mov     di,100h
                mov     ds,cx                   ; DS = CS
                mov     si,offset slidetop
                mov     cx,offset slidebot - offset slidetop
                shr     cx,1                    ; translate to word count
                rep     movsw                   ; CF set if one byte left over
                adc     cx,cx                   ; CX = 1 or 0, depending CF
                rep     movsb                   ; possible final byte

                mov     word ptr cs:[s1add+2],es ; PSP segment
index = offset slide1 - offset slidetop + 100h
                mov     word ptr cs:[s1add],index ; slide1 offset

                mov     cx,es                   ; PSP segment
index = offset handler - offset slidetop + 100h
                mov     dx,index                ; interrupt handler offset
                call    safevect                ; set vectors in vectab1

                jmp     dword ptr cs:[s1add]    ; jump to the kernel

; If all goes well, this is where we come back to from the kernel.

fnish1:         mov     cs:[rcode],ax           ; return code

                cli                             ; restore original stack
                mov     ss,cs:[stks]
                mov     sp,cs:[stkp]
                sti

                push    ds                      ; maybe EMS page frame segment
                push    dx                      ; maybe EMS handle
                push    bx                      ; maybe swap file handle
                mov     cx,cs
                mov     dx,offset handler       ; interrupt handler offset
                call    safevect                ; set vectors in vectab1
                pop     bx
                pop     dx
                pop     ds

                cmp     cs:[useems2],0
                jne     fnish4                  ; jump if don't use EMS

; DS = EMS page frame segment
; DX = EMS handle

                mov     cx,offset slidebot - offset slidetop
                mov     es,cs:[psp]             ; PSP segment
                mov     di,100h
                xor     si,si
                xor     bx,bx                   ; logical page number
                call    mapems
                or      ah,ah
                jnz     fnish3                  ; jump if map failed
                shr     cx,1                    ; translate to word count
                rep     movsw                   ; CF set if one byte left over
                adc     cx,cx                   ; CX = 1 or 0, depending CF
                rep     movsb                   ; possible final byte

fnish2:         mov     ah,45h                  ; release handle and memory
                int     67h
                cmp     ah,82h                  ; memory manager busy?
                je      fnish2                  ; jump if busy
                jmp     short fnish7

fnish3:         mov     ah,45h                  ; release handle and memory
                int     67h
                cmp     ah,82h                  ; memory manager busy?
                je      fnish3                  ; jump if busy
                jmp     short fnish5            ; exit

; BX = swap file handle

fnish4:         xor     cx,cx                   ; offset 0
                xor     dx,dx                   ; offset 0
                mov     ax,4200h                ; move file pointer
                int     21h                     ;  from beginning of file

                mov     cx,offset slidebot - offset slidetop
                mov     ds,cs:[psp]             ; PSP segment
                mov     dx,100h
                mov     ah,3Fh                  ; read file
                int     21h
                jc      fnish5                  ; exit if error
                cmp     ax,cx
                je      fnish6

fnish5:         push    cs
                pop     ds
                mov     dx,offset errmsg2
                mov     cx,msglen2              ; errmsg2 length
                mov     bx,2                    ; standard error device handle
                mov     ah,40h                  ; write error message
                int     21h
                mov     ax,4C01h                ; terminate with return code
                int     21h

fnish6:         mov     ah,3Eh                  ; close file
                int     21h
                push    cs
                pop     ds
                mov     dx,offset file
                mov     ah,41h                  ; delete file
                int     21h

fnish7:         pop     ds
                pop     si                      ; restore register variables
                pop     di
                pop     bp

                mov     ax,cs:[rcode]           ; return code
                or      ax,ax
                jz      fnish11
                push    ax
                mov     ax,3000h                ; get DOS version number
                int     21h
                cmp     al,3                    ; major version number
                pop     ax
                jb      fnish8
                cmp     al,34                   ; unknown error - 3.0
                jae     fnish9
                cmp     al,32                   ; sharing violation
                jb      fnish8
                mov     al,5                    ; access denied
                jmp     short fnish10

fnish8:         cmp     al,19                   ; unknown error - 2.0
                jbe     fnish10

fnish9:         mov     al,19                   ; unknown error - 2.0

fnish10:        xor     ah,ah

fnish11:        ret

; If we are not swapping, we jump here.

noswap1:        mov     ax,cs

; initialize parameter block

                mov     bx,cs:[env]
                mov     cs:[environ],bx
                mov     cs:[cmd],offset command
                mov     cs:[cmd+2],ax
                mov     cs:[fcb1],offset fcb5c
                mov     cs:[fcb1+2],ax
                mov     cs:[fcb2],offset fcb6c
                mov     cs:[fcb2+2],ax

; save 4 bytes destroyed by DOS 2.0 at DS:2Eh

                mov     si,cs:[2Eh]
                mov     word ptr cs:[save],si
                mov     si,cs:[30h]
                mov     word ptr cs:[save+2],si

                mov     cx,cs
                mov     dx,offset handler       ; interrupt handler offset
                call    safevect                ; set vectors in vectab1

                mov     es,cx                   ; ES = CS
                mov     bx,offset parmblk
                mov     ds,cx                   ; DS = CS
                mov     dx,offset path
                mov     ax,4B00h                ; load and execute program
                int     21h
                jnc     noswap2                 ; jump if no error
                mov     cs:[rcode],ax           ; return code

noswap2:        cli                             ; restore original stack
                mov     ss,cs:[stks]
                mov     sp,cs:[stkp]
                sti

; restore 4 bytes destroyed by DOS 2.0 at DS:2Eh

                mov     si,word ptr cs:[save]
                mov     cs:[2Eh],si
                mov     si,word ptr cs:[save+2]
                mov     cs:[30h],si

                jmp     fnish7

__xspawn        ENDP

mapems          PROC    near

; DX = handle
; BX = logical page number

                xor     al,al                   ; physical page number

map1:           mov     ah,44h                  ; map memory
                int     67h
                cmp     ah,82h                  ; memory manager busy?
                je      map1                    ; jump if busy

                ret

mapems          ENDP

setvectsub      PROC    near

; ES = vector table segment
; BX = vector table offset

                mov     ah,25h                  ; set interrupt vector

setvectsub1:    mov     al,es:[bx+flag]         ; 0-CURRENT,1-IRET,2-free,3-end
                cmp     al,3                    ; is it the end?
                je      setvectsub3             ; yes, jump
                cmp     al,2                    ; is it free?
                je      setvectsub2             ; yes, jump
                mov     al,es:[bx+number]       ; vector number
                mov     ds,es:[bx+vseg]         ; vector segment
                mov     dx,es:[bx+voff]         ; vector offset
                int     21h                     ; set interrupt vector

setvectsub2:    add     bx,6                    ; size of vector structure
                jmp     setvectsub1             ; next

setvectsub3:    ret

setvectsub      ENDP

safevect        PROC    near

; CX = handler segment
; DX = handler offset

                mov     es,cs:[vtabseg]         ; vectab1 segment
                mov     bx,cs:[vtaboff]         ; vectab1 offset

safevect1:      mov     al,es:[bx+flag]         ; 0-CURRENT,1-IRET,2-free,3-end
                cmp     al,3                    ; is it the end?
                je      safevect3               ; yes, jump
                cmp     al,1                    ; is it IRET?
                jne     safevect2               ; no, jump
                mov     es:[bx+vseg],cx         ; handler segment
                mov     es:[bx+voff],dx         ; handler offset

safevect2:      add     bx,6                    ; size of vector structure
                jmp     safevect1               ; next

safevect3:      mov     bx,cs:[vtaboff]         ; vectab1 offset
                call    setvectsub

                ret

safevect        ENDP

;
; int _xsize( unsigned int, long *, long * );
;

                PUBLIC  __xsize
IFDEF LCODE
__xsize         PROC    far
ELSE
__xsize         PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    di                      ; preserve register variables
                push    si
                push    ds

                mov     cs:[last],0             ; assume last block swap
                mov     bx,word ptr [bp+ARG_1]  ; PSP segment
                mov     cs:[psp],bx
                mov     dx,bx
                dec     bx                      ; program arena header

size1:          mov     es,bx                   ; current arena header
                mov     ax,es:[own]
                or      ax,ax                   ; is it free?
                jz      size2                   ; yes, count it
                cmp     ax,dx                   ; do we own it?
                jne     size4                   ; no, jump
                mov     cx,bx                   ; last owned block

size2:          inc     bx
                add     bx,es:[siz]             ; block size
                jc      size3                   ; carry, arena is trashed
                mov     al,es:[sig]             ; get arena signature
                cmp     al,'M'                  ; are we at end of memory?
                je      size1                   ; no, jump
                cmp     al,'Z'
                je      size5

size3:          mov     bx,-1                   ; request maximum memory
                mov     ah,48h                  ; allocate memory block
                int     21h
                mov     cs:[rcode],7
                jmp     fnish7

size4:          mov     cs:[last],1             ; not last block swap

size5:          sub     bx,dx                   ; subtract PSP segment
                mov     ax,cx                   ; last owned block
                mov     es,cx
                inc     ax
                add     ax,es:[siz]             ; block size
                sub     ax,dx                   ; subtract PSP segment
                mov     cs:[parsz],ax           ; parent size
                sub     ax,10h                  ; subtract PSP size
                xor     dx,dx                   ; convert to bytes
                mov     cx,4

size6:          shl     ax,1
                rcl     dx,1
                loop    size6
IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+2]
ELSE
                mov     si,word ptr [bp+ARG_1+2]
ENDIF
                mov     ds:[si],ax              ; swap size requirement
                mov     ds:[si+2],dx
                mov     cs:[cntl],ax            ; count low
                mov     cs:[cnth],dx            ; count high
                mov     cx,offset slidebot - offset slidetop
                sub     ax,cx
                sbb     dx,0
                mov     cs:[cntsvl],ax          ; count save low
                mov     cs:[cntsvh],dx          ; count save high
                mov     cs:[ttlsz],bx           ; total size
                xor     dx,dx                   ; convert to bytes
                mov     cx,4

size7:          shl     bx,1
                rcl     dx,1
                loop    size7
IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+6]
ELSE
                mov     si,word ptr [bp+ARG_1+4]
ENDIF
                mov     ds:[si],bx              ; parent and free memory
                mov     ds:[si+2],dx

                pop     ds
                pop     si                      ; restore register variables
                pop     di
                pop     bp
                xor     ax,ax
                ret

__xsize         ENDP

;
; int _chkems( char *, int * );
;

                PUBLIC  __chkems
IFDEF LCODE
__chkems        PROC    far
ELSE
__chkems        PROC    near
ENDIF

                push    bp
                mov     bp,sp

; determine whether expanded memory is available

IFDEF LDATA
                push    ds
                lds     dx,dword ptr [bp+ARG_1] ; EMM device driver name
ELSE
                mov     dx,word ptr [bp+ARG_1]  ; EMM device driver name
ENDIF
                mov     ax,3D00h                ; open file read only
                int     21h
IFDEF LDATA
                pop     ds
ENDIF
                jc      check2                  ; expanded memory unavailable

; determine whether we opened the EMM device driver or a file

                mov     bx,ax                   ; handle
                mov     ax,4400h                ; IOCTL - get device info
                int     21h
                jc      check1                  ; expanded memory unavailable
                test    dx,80h                  ; test bit 7
                jz      check1                  ; expanded memory unavailable

                mov     ax,4407h                ; IOCTL - get output status
                int     21h
                jc      check1                  ; expanded memory unavailable
                or      al,al
                jz      check1                  ; expanded memory unavailable

; close EMM device driver to reclaim handle

                mov     ah,3Eh                  ; close file
                int     21h

; determine whether the EMM is functional

                mov     ah,40h                  ; get manager status
                int     67h
                or      ah,ah
                jnz     check2                  ; expanded memory unavailable

; check EMM version

                mov     ah,46h                  ; get EMM version
                int     67h
                or      ah,ah
                jnz     check2                  ; expanded memory unavailable
                cmp     al,32h                  ; version 3.2
                jb      check2                  ; expanded memory unavailable

; get page frame segment

                mov     ah,41h                  ; get page frame segment
                int     67h
                or      ah,ah
                jnz     check2                  ; expanded memory unavailable
                mov     cs:[emsseg],bx          ; segment

; get size of page map information

                mov     ax,4E03h                ; get size of page map info
                int     67h
                or      ah,ah
                jnz     check2                  ; expanded memory unavailable
IFDEF LDATA
                les     bx,dword ptr [bp+ARG_1+4] ; mapsize address
                mov     es:[bx],ax
ELSE
                mov     bx,word ptr [bp+ARG_1+2] ; mapsize address
                mov     ds:[bx],ax
ENDIF
                xor     ax,ax                   ; expanded memory available
                pop     bp
                ret

; close EMM device driver or file to reclaim handle

check1:         mov     ah,3Eh                  ; close file
                int     21h

check2:         mov     ax,1                    ; expanded memory unavailable
                pop     bp
                ret

__chkems        ENDP

;
; int _savemap( char * );
;

                PUBLIC  __savemap
IFDEF LCODE
__savemap       PROC    far
ELSE
__savemap       PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    di                      ; preserve register variable

IFDEF LDATA
                les     di,dword ptr [bp+ARG_1] ; buffer address
ELSE
                mov     di,word ptr [bp+ARG_1]  ; buffer address
                push    ds
                pop     es
ENDIF
                mov     ax,4E00h                ; save page map
                int     67h

                pop     di                      ; restore register variable
                pop     bp
                ret

__savemap       ENDP

;
; int _restmap( char * );
;

                PUBLIC  __restmap
IFDEF LCODE
__restmap       PROC    far
ELSE
__restmap       PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    ds
                push    si                      ; preserve register variable

IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1] ; buffer address
ELSE
                mov     si,word ptr [bp+ARG_1]  ; buffer address
ENDIF
                mov     ax,4E01h                ; restore page map
                int     67h
                xor     al,al

                pop     si                      ; restore register variable
                pop     ds
                pop     bp
                ret

__restmap       ENDP

;
; int _getems( int, int * );
;

                PUBLIC  __getems
IFDEF LCODE
__getems        PROC    far
ELSE
__getems        PROC    near
ENDIF

                push    bp
                mov     bp,sp

                mov     bx,word ptr [bp+ARG_1]  ; number of pages
                mov     ah,43h                  ; get handle and allocate
                                                ;  memory
                int     67h
IFDEF LDATA
                les     bx,dword ptr [bp+ARG_1+2] ; handle address
                mov     es:[bx],dx              ; handle
ELSE
                mov     bx,word ptr [bp+ARG_1+2] ; handle address
                mov     ds:[bx],dx              ; handle
ENDIF
                xor     al,al
                pop     bp
                ret

__getems        ENDP

;
; int _dskspace( int, unsigned int *, unsigned int * );
;

                PUBLIC  __dskspace
IFDEF LCODE
__dskspace      PROC    far
ELSE
__dskspace      PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    di                      ; preserve register variable

                mov     ah,36h                  ; get free disk space
                mov     dl,byte ptr [bp+ARG_1]  ; drive code (0=default, 1=A)
                int     21h
                cmp     ax,0FFFFh               ; was drive invalid?
                je      space1                  ; yes, jump
                mul     cx                      ; bytes per sector *
                                                ;  sectors per cluster
IFDEF LDATA
                les     di,dword ptr [bp+ARG_1+2]
                mov     es:[di],ax              ; bytes per cluster
ELSE
                mov     di,word ptr [bp+ARG_1+2]
                mov     ds:[di],ax              ; bytes per cluster
ENDIF
IFDEF LDATA
                les     di,dword ptr [bp+ARG_1+6]
                mov     es:[di],bx              ; number of available clusters
ELSE
                mov     di,word ptr [bp+ARG_1+4]
                mov     ds:[di],bx              ; number of available clusters
ENDIF
                xor     ax,ax

space1:         pop     di                      ; restore register variable
                pop     bp
                ret

__dskspace      ENDP

;
; int _getrc( void );
;

                PUBLIC  __getrc
IFDEF LCODE
__getrc         PROC    far
ELSE
__getrc         PROC    near
ENDIF

                mov     ah,4Dh                  ; get child return code
                int     21h

                ret

__getrc         ENDP

;
; int _create( char *, int * );
;

                PUBLIC  __create
IFDEF LCODE
__create        PROC    far
ELSE
__create        PROC    near
ENDIF

                push    bp
                mov     bp,sp
IFDEF LDATA
                push    ds
ENDIF

                mov     ax,3000h                ; get DOS version number
                int     21h
IFDEF LDATA
                lds     dx,dword ptr [bp+ARG_1] ; file
ELSE
                mov     dx,word ptr [bp+ARG_1]  ; file
ENDIF
                xor     cx,cx                   ; normal attribute
                mov     ah,5Bh                  ; create new file
                cmp     al,3                    ; major version number
                jae     create1
                mov     ah,3Ch                  ; create file

create1:        int     21h
                jc      create2
IFDEF LDATA
                lds     bx,dword ptr [bp+ARG_1+4] ; handle address
ELSE
                mov     bx,word ptr [bp+ARG_1+2] ; handle address
ENDIF
                mov     ds:[bx],ax
                xor     ax,ax

create2:
IFDEF LDATA
                pop     ds
ENDIF
                pop     bp
                ret

__create        ENDP


;
; int _getcd( int, char * );
;


                PUBLIC  __getcd
IFDEF LCODE
__getcd         PROC    far
ELSE
__getcd         PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    si                      ; preserve register variable
IFDEF LDATA
                push    ds
ENDIF

                mov     dl,byte ptr [bp+ARG_1]  ; drive code (0=default, 1=A)
IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+2] ; buffer
ELSE
                mov     si,word ptr [bp+ARG_1+2] ; buffer
ENDIF
                mov     ah,47h                  ; get current directory
                int     21h
                jc      getcd1
                xor     ax,ax

getcd1:
IFDEF LDATA
                pop     ds
ENDIF
                pop     si                      ; restore register variable
                pop     bp
                ret

__getcd         ENDP

;
; int _getdrv( void );
;

                PUBLIC  __getdrv
IFDEF LCODE
__getdrv        PROC    far
ELSE
__getdrv        PROC    near
ENDIF

                mov     ah,19h                  ; get default disk drive
                int     21h
                xor     ah,ah

; AX = drive (0 = A, 1 = B, etc.)

                ret

__getdrv        ENDP

;
; void _getvect( int, unsigned int *, unsigned int * );
;

                PUBLIC  __getvect
IFDEF LCODE
__getvect       PROC    far
ELSE
__getvect       PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    ds
                push    si                      ; preserve register variable

                mov     ah,35h                  ; get interrupt vector
                mov     al,byte ptr [bp+ARG_1]  ; interrupt number
                int     21h
IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+2]
ELSE
                mov     si,word ptr [bp+ARG_1+2]
ENDIF
                mov     ds:[si],es              ; segment
IFDEF LDATA
                lds     si,dword ptr [bp+ARG_1+6]
ELSE
                mov     si,word ptr [bp+ARG_1+4]
ENDIF
                mov     ds:[si],bx              ; offset

                pop     si                      ; restore register variable
                pop     ds
                pop     bp
                ret

__getvect       ENDP

;
; void _setvect( VECTOR * );
;

                PUBLIC  __setvect
IFDEF LCODE
__setvect       PROC    far
ELSE
__setvect       PROC    near
ENDIF

                push    bp
                mov     bp,sp
                push    ds                      ; modified in setvectsub

IFDEF LDATA
                les     bx,dword ptr [bp+ARG_1] ; vector table
ELSE
                mov     bx,word ptr [bp+ARG_1]  ; vector table
                push    ds
                pop     es
ENDIF

                call    setvectsub

                pop     ds
                pop     bp
                ret

__setvect       ENDP


;
; void _setdrvcd(int drive, char * string);
;

                PUBLIC  __setdrvcd
IFDEF LCODE
__setdrvcd      PROC    far
ELSE
__setdrvcd      PROC    near
ENDIF
                push    bp
                mov     bp,sp
IFDEF LDATA
                push    ds
ENDIF
                mov     dl,byte ptr [bp+ARG_1]  ; drive code (0=A, 1=B)
                mov     ah,0eh
                int     21h
IFDEF LDATA
                lds     dx,dword ptr [bp+ARG_1+2] ; buffer
ELSE
                mov     dx,word ptr [bp+ARG_1+2] ; buffer
ENDIF
                mov     ah,3bh                  ; set current directory
                int     21h

IFDEF LDATA
                pop     ds
ENDIF
                pop     bp
                ret

__setdrvcd      ENDP

_TEXT           ENDS

                END

