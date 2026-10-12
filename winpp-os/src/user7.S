# user7.S -- 管道演示（S5-1：pipe() 创建匿名管道，写端写、读端读，串口打印）
# 系统调用：eax=9 pipe()               -> eax=读端 fd，写端=fd+1
#           eax=6 write(fd,buf,len)    （fs 分发到管道写端）
#           eax=7 read(fd,buf,len)     （fs 分发到管道读端）
#           eax=1 write(2,buf,len)     串口打印
# 行为：每轮 pipe() -> 写 "Hello Pipe!" -> 读回 -> 串口打印，无限循环

.section .data
msg:    .ascii "Hello Pipe!\r\n"    # 13B
buf:    .space 32

.section .text
.global _start
_start:
    movl $0x761000, %esp
    xorl %ebp, %ebp

    # pipe() 只创建一次（匿名管道是持久对象，循环里反复读写）
    movl $9, %eax
    int $0x80
    movl %eax, %esi          # esi = 读端 fd（写端 = esi+1）

loop:
    # write(写端=esi+1, msg, 13)
    movl $6, %eax
    leal 1(%esi), %ebx
    leal msg, %ecx
    movl $13, %edx
    int $0x80

    # read(读端=esi, buf, 13)
    movl $7, %eax
    movl %esi, %ebx
    leal buf, %ecx
    movl $13, %edx
    int $0x80

    # write(2, buf, 13)  串口打印读回内容
    movl $1, %eax
    movl $2, %ebx
    leal buf, %ecx
    movl $13, %edx
    int $0x80

    movl $1500000, %ecx
wait:
    loop wait
    jmp loop
