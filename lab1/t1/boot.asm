    org 07c00h    ; 告诉编译器加载到07c00h
    mov ax, cs
    mov ds, ax
    call DispStr  ; 调用显示字符串例程
    jmp $         ; 无限循环

DispStr:
    mov ax, BootMessage
    mov bp, ax           ; ES:BP = 串地址
    mov cx, 10           ; CX = 串长度
    mov ax, 01301h       ;
    mov bx, 000ch        ;
    mov dl, 0
    int 10h              ; 10h号中断
    ret

BootMessage: db "Hello, OS!"
times 510-($-$$) db 0    ; 填充剩下的空间，使得生成的二进制代码恰好为512字节
dw 0xaa55                ; 结束标志