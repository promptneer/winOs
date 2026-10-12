# user6.S -- 用户态文件系统演示（入口 0x630000，栈顶 0x751000）
# 系统调用（S4-4）：eax=5 open(ebx=name, ecx=flags)
#                 eax=6 write(ebx=fd, ecx=buf, edx=len)
#                 eax=7 read(ebx=fd, ecx=buf, edx=len)
#                 eax=8 close(ebx=fd)
# 行为：创建 hello.txt -> 写入 16 字节 -> 关闭 -> 重开读取 -> 串口打印读回内容
#       （每轮重复：数据存在内核内存盘里，用户程序只能经系统调用读写）

.section .data
fname:  .ascii "hello.txt\0"
data:   .ascii "Hello WinPP FS!\r\n"    # 16B
buf:    .space 32

.section .text
.global _start
_start:
    movl $0x751000, %esp
    xorl %ebp, %ebp

loop:
    # open("hello.txt", 1)  创建/写
    movl $5, %eax
    leal fname, %ebx
    movl $1, %ecx
    int $0x80
    movl %eax, %esi          # fd

    # write(fd, data, 16)
    movl $6, %eax
    movl %esi, %ebx
    leal data, %ecx
    movl $16, %edx
    int $0x80

    # close(fd)
    movl $8, %eax
    movl %esi, %ebx
    int $0x80

    # open("hello.txt", 0)  读
    movl $5, %eax
    leal fname, %ebx
    xorl %ecx, %ecx
    int $0x80
    movl %eax, %esi

    # read(fd, buf, 16)
    movl $7, %eax
    movl %esi, %ebx
    leal buf, %ecx
    movl $16, %edx
    int $0x80

    # write(2, buf, 16)  串口打印读回内容
    movl $1, %eax
    movl $2, %ebx
    leal buf, %ecx
    movl $16, %edx
    int $0x80

    # close(fd)
    movl $8, %eax
    movl %esi, %ebx
    int $0x80

    movl $1500000, %ecx
wait:
    loop wait
    jmp loop
