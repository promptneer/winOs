# user8.S -- 命名管道演示（S5-2：fifo_create("fifo0") 创建命名管道，写读回打印）
# 系统调用：eax=10 fifo_create(ebx=name) -> eax=读端 fd，写端=fd+1
# 行为：每轮创建/复用 "fifo0" -> 写 "Hello FIFO!" -> 读回 -> 串口打印，无限循环

.section .data
fname:  .ascii "fifo0\0"
msg:    .ascii "Hello FIFO!\r\n"   # 13B
buf:    .space 32

.section .text
.global _start
_start:
    movl $0x771000, %esp
    xorl %ebp, %ebp

    # fifo_create("fifo0") 只创建一次（命名管道是持久对象）
    movl $10, %eax
    leal fname, %ebx
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
