# user3.S -- 用户态进程 3（链接于 0x600000，栈顶 0x721000）
# 系统调用 int 0x80: eax=1 write(ebx=fd, ecx=buf, edx=len); eax=2 exit()

.section .data
buf:    .ascii "user3: 000000\r\n"
buf_end:
.set buf_len, buf_end - buf

.section .text
.global _start
_start:
    movl $0x721000, %esp
    xorl %ebp, %ebp
    xorl %edi, %edi

loop:
    leal buf+12, %ecx
    movl %edi, %eax
    movl $10, %ebx
conv:
    xorl %edx, %edx
    divl %ebx
    addb $'0', %dl
    decl %ecx
    movb %dl, (%ecx)
    testl %eax, %eax
    jnz conv

    movl $1, %eax
    movl $1, %ebx                # fd=1 VGA
    leal buf, %ecx
    movl $buf_len, %edx
    int $0x80

    movl $1, %eax
    movl $2, %ebx                # fd=2 串口
    leal buf, %ecx
    movl $buf_len, %edx
    int $0x80

    incl %edi

    movl $1500000, %ecx
wait:
    loop wait

    jmp loop
