SECTION .data
error_message: db "Invalid input.";错误消息
line: db 0Ah

;用于维护未初始化或初始值为0的全局变量
SECTION .bss
input: resb 203
left: resb 103
right: resb 103
left_len: resb 1
right_len: resb 1
result: resb 103 
blank: resb 2
var: resb 1
temp_len: resb 1

SECTION .text
global main ;使用gcc编译
main:
    mov ebp, esp; for correct debugging
    .read_and_deal:
   	mov      eax, input
   	mov      ebx, 203
   	call     getline      ;读取一行
    call     splitNumbers ;分割两个数字


    mov   esi, result
    mov   BYTE[esi], 0
    mov   esi, blank
    mov   BYTE[esi], ' '
    
    ;处理异常
    mov   edi, right
    cmp   byte[edi], '0'
    je    .error

    ;处理空
    mov      eax, left_len
   	cmp      BYTE[eax], 0
   	je       .error
   	mov      eax, right_len
   	cmp      BYTE[eax], 0
   	je       .error

    ;处理前导0
    mov      eax, left_len
   	cmp      BYTE[eax], 1
    je       .start
    
    mov      eax, left
    cmp      BYTE[eax], '0'
    je       .error
    mov      eax, right
    cmp      BYTE[eax], '0'
    je       .error
    
   
    ;开始计算
    .start:
    mov   esi, temp_len
	mov   edi, right_len
    mov   bl, BYTE[edi]
    mov   BYTE[esi], bl

    call  compare
    mov   edi, var
    cmp   BYTE[edi], '0'
    je    .cal_finished
    ;简单的情况已经处理完毕

    ;首先对齐
    mov   esi, temp_len
	mov   edi, left_len
    mov   bl, BYTE[edi]
    mov   BYTE[esi], bl

    ;不小于就sub1,即左对齐减
    call  compare
    mov   edi, var
    cmp   BYTE[edi], '0'
    je    .goto_sub2
    call  sub1
    jmp   .merge
    .goto_sub2:

    ;小于就右移,sub2，也就是对齐之后右移一位减
    dec   BYTE[esi]
    call  sub2

    ;将left左移，更改len, 这里还需要注意全0的情况，全0直接结束
    .merge:
    mov   al,  0
    mov   ebx, left
    mov   esi, left_len
    mov   ah,  byte[esi]

    ;先计算出需要移动多少位，保存在al中
    .merge_first:
    cmp   BYTE[ebx], '0'
    jne   .merge_second
   
    inc   al
    inc   ebx
    cmp   al, ah
    je    .minus_finished
    jmp   .merge_first

    ;更新长度
    .merge_second:
    mov   edi, left_len
    sub   byte[edi], al




    ;开始移动，al进行计数，将ecx放入eax
    mov   ecx, left
    mov   ah, 0
    .merge_move:
    mov   al, byte[ebx]
    mov   byte[ecx], al
    inc   ah
    inc   ebx
    inc   ecx
    cmp   ah, byte[edi]
    jb    .merge_move

    ;判断是否结束，循环重复
    jmp   .start

    .minus_finished:
    mov  byte[esi], 0
    jmp  .start

    ;计算完成，结束调用
    .cal_finished:
    call     print_result
    mov   esi, blank
    mov   BYTE[esi], ' '
    mov      eax, blank
    call     printStr_s
    mov      eax, left
    call     printStr_1
    jmp      .exit
   

   	;程序结束，退出
   	.exit:
   	mov ebx, 0
   	mov eax, 1 ;
   	int 0x80

   	;输出异常
   	.error:
    push        eax
    mov         eax, error_message
    call        printStr
    pop         eax
    jmp         .exit

getline:
    pushad
    mov     edx, ebx
    mov     ecx, eax
    mov     ebx, 0
    mov     eax, 3 
    int     80h
    popad
    ret

;标准输出函数
printStr:
    cmp     BYTE[eax], 0
    jz     .print_ret

    push    edx
    push    ecx
    push    ebx
    push    eax

    call    strlen
 
    mov     edx, eax ;内容的长度
	
    pop     eax
    ;内容的首地址
    mov     ecx, eax 
    ;标准输出stdout
    mov     ebx, 1 
    ;调用 SYS_WRITE
    mov     eax, 4
    int     80h 
 
    pop     ebx
    pop     ecx
    pop     edx
    .print_ret:
    ret


printStr_s:
    pushad
    cmp     BYTE[eax], 0
    jz     .print_ret

    push    edx
    push    ecx
    push    ebx
    push    eax

    call    strlen
 
    mov     edx, 1 ;内容的长度
	
    pop     eax
    ;内容的首地址
    mov     ecx, eax 
    ;标准输出stdout
    mov     ebx, 1 
    ;调用 SYS_WRITE
    mov     eax, 4
    int     80h 
 
    pop     ebx
    pop     ecx
    pop     edx
    .print_ret:
    popad
    ret

printStr_1:
    pushad
    cmp     BYTE[eax], 0
    jz     .print_ret

    push    edx
    push    ecx
    push    ebx
    push    eax
    

    call    strlen

    mov     eax, left_len
    cmp     byte[eax], 0
    ;如果长度为0，代表余数为0，应该输出'0'
    je      .small_bug
    .back_here:
    mov     dl, byte[eax]
    movzx   edx, dl
    
    

	
    pop     eax
    ;内容的首地址
    mov     ecx, eax 
    ;标准输出stdout
    mov     ebx, 1 
    ;调用 SYS_WRITE
    mov     eax, 4
    int     80h 
 
    pop     ebx
    pop     ecx
    pop     edx
    .print_ret:
    popad
    ret
    
    .small_bug:
    mov     byte[eax], 1
    jmp     .back_here

;输出商的函数，需要先反转，找到输出的开始位置，再进行输出
;输出部分调用的是printStr
print_result:
    ;将right征用
    pushad
    mov   eax, result
    add   eax, 102
    mov   ebx, right
    mov   dl, 0

    .loop1:
    mov   dh, byte[eax]
    add   dh, '0'
    mov   byte[ebx], dh
    inc   dl
    inc   ebx
    dec   eax
    cmp   dl, 103
    jb    .loop1

    mov   ebx, right
    mov   eax, ebx
    add   eax, 103

    .loop2:
    cmp  byte[ebx], '0'
    jne  .to_print
    inc  ebx
    cmp  ebx, eax
    je   .easy_print
    jmp  .loop2

    .to_print:
    sub  eax, ebx
    mov  edx, eax
    mov  ecx, ebx
    ;标准输出stdout
    mov     ebx, 1 
    ;调用 SYS_WRITE
    mov     eax, 4 
    int     80h 
    jmp    .pt_res_ret


    .easy_print:
    mov   eax, blank
    mov   BYTE[eax], '0'
    call  printStr

    .pt_res_ret:
    popad
    ret


;计算字符串的长度用于输出
strlen:
    push	ebx ;暂存ebx中的值
    mov		ebx, eax
    .len_loop:
    cmp     byte[eax], 0
    je    	.len_finished 
    inc     eax 
    jmp     .len_loop
   .len_finished:
    sub     eax, ebx    
    pop     ebx 
    ret


splitNumbers:
	pushad
	mov   eax, input
	mov   ebx, eax
	mov   ecx, left
	mov   edx, right 
	mov   esi, left_len
	mov   edi, right_len
	mov   byte[esi], 0
	mov   byte[edi], 0

    ;第一个循环，用于获得被除数
    .getLeft:
  	cmp   byte[eax], ' '
  	je    .getBlank
   
  	mov   bl, byte[eax]
  	mov   byte[ecx], bl
  	inc   eax
  	inc   ecx
  	inc   byte[esi]
  	jmp   .getLeft

    ;读取空格，分割左右
    .getBlank:
  	inc   eax
   	jmp   .getRight
    
    ;第二个循环，获得除数
    .getRight:
  	cmp   byte[eax], 0
  	jz    .split_finished
  	mov   bl,byte[eax]
  	mov   byte[edx], bl
  	inc   eax
  	inc   edx
  	inc   byte[edi]
  	jmp   .getRight
    
    .split_finished:
   	dec   byte[edi]
  	popad
  	ret
  
; 比较left 和 right，将结果保存在var中
; 0 小于， 1等于， 2大于
compare:
    pushad
    mov   esi, left_len
	mov   edi, temp_len
    mov   dh, BYTE[edi]
    cmp   BYTE[esi], dh
    jb    .lessThan
    ja    .moreThan

    ; al用于计数，超过长度意味着相等
    ; ebx 和 ecx为left、right的指针，从头走到尾
    mov   al, 0
    mov   ebx, left
    mov   ecx, right

    ;不断比较的循环
    .core_cmp:
    mov   dl, byte[ecx]
    cmp   byte[ebx], dl
    jb    .lessThan
    ja    .moreThan
    inc   ebx
    inc   ecx
    inc   al
    cmp   al, dh
    je    .equalTo
    jmp   .core_cmp

    .lessThan:
    mov   esi, var
    mov   BYTE[esi], '0'
    jmp   .com_end

    .moreThan:
    mov   esi, var
    mov   BYTE[esi], '2'
    jmp   .com_end

    .equalTo:
    mov   esi, var
    mov   BYTE[esi], '1'
    jmp   .com_end

    .com_end:
    popad
    ret

;这里设置好寄存器，做好前期准备工作，找到result的对应位置++，eax、ebx分别设为减法进行的最低位，也即开始位置，然后调用real_sub
sub1:
    pushad
    mov    eax, temp_len
    mov    ebx, right_len
    mov    cl, byte[eax]
    mov    ch, byte[ebx]
    sub    cl, ch
    movzx  ecx, cl
    mov    eax, result
    add    eax, ecx
    inc    byte[eax]
    

    mov    eax, left
    mov    ebx, right
    mov    edi, right_len
    mov    cl, byte[edi]
    movzx  ecx, cl
    sub    ecx, 1
    add    eax, ecx
    add    ebx, ecx
    call   real_sub
    popad
    ret

;dh用于计数，dl用于长度，cl用于进位
;sub1和sub2的公用部分，也是最核心的做减法部分
real_sub:
    mov   cl, 0
    mov   dh, 0
    mov   edi, right_len
    mov   dl, byte[edi]

    .minus:
    mov   ch, byte[ebx]
    
    sub   byte[eax], ch
    sub   byte[eax], cl
    add   byte[eax], 30h
    cmp   byte[eax], 30h
    jb    .real_sub_2
    jnb   .real_sub_1

    .minus_2:
    inc   dh
    dec   eax
    dec   ebx
    cmp   dh, dl
    je    .sub_exit
    jmp   .minus
    
    .real_sub_1:
    mov   cl, 0
    jmp   .minus_2

    .real_sub_2:
    mov   cl, 1
    add   byte[eax], 10
    jmp   .minus_2


    .sub_exit:
    ret

;这里设置好寄存器，做好前期准备工作，找到result的对应位置++，eax、ebx分别设为减法进行的最低位，也即开始位置，然后调用real_sub
sub2:
    pushad
    mov    eax, temp_len
    mov    ebx, right_len
    mov    cl, byte[eax]
    mov    ch, byte[ebx]
    sub    cl, ch
    movzx  ecx, cl
    mov    eax, result
    add    eax, ecx
    inc    byte[eax]
    

    mov    eax, left
    mov    ebx, right
    mov    edi, right_len
    mov    cl, byte[edi]
    movzx  ecx, cl
    sub    ecx, 1
    add    eax, ecx
    add    ebx, ecx
    add    eax, 1
    call   real_sub
    cmp    cl, 0
    je     .sub2_ret
    sub   byte[eax], cl
    .sub2_ret:
    popad
    ret