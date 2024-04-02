section .data
red:  db 1Bh, "[31m"
redSize: equ  $-red
normal:  db 1Bh, "[0m"
normalSize: equ $-normal


section .text

global normalPrint
global redPrint


normalPrint:
    mov	eax,4
	mov	ebx,1
	mov	ecx,normal
	mov	edx,normalSize
    int 80h
	;利用栈传递参数
	mov	eax,4
	mov	ebx,1
	mov	ecx,[esp+4]
	mov	edx,[esp+8]
	int	80h
	ret

redPrint:
    mov	eax,4
	mov	ebx,1
	mov	ecx,red
	mov	edx,redSize
    int 80h
	;利用栈传递参数
	mov	eax,4
	mov	ebx,1
	mov	ecx,[esp+4]
	mov	edx,[esp+8]
	int	80h
	ret