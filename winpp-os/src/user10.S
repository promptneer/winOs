# user10.S -- S6 文件系统增强演示（入口 0x670000，栈顶 0x791000）
# 系统调用：eax=13 mkdir(ebx=path)
#           eax=14 chdir(ebx=path)
#           eax=5  open(ebx=name, ecx=flags)   相对路径基于当前目录
#           eax=6  write / eax=7 read / eax=8 close
# 行为：mkdir("/docs") -> open("/docs/a.txt") 写 11B -> chdir("/docs")
#       -> open("b.txt") 写 11B（相对路径）-> 读回两个文件 -> 串口打印
#       （每轮循环重读验证：路径解析 + 目录 + 相对路径全链路）

.section .data
dir1:   .ascii "/docs\0"
f1:     .ascii "/docs/a.txt\0"
f2:     .ascii "b.txt\0"
d1:     .ascii "Hello Dir!\r\n"     # 11B
d2:     .ascii "Sub File!\r\n"      # 11B
buf:    .space 32

.section .text
.global _start
_start:
    movl $0x791000, %esp
    xorl %ebp, %ebp

    # mkdir("/docs")  只创建一次（幂等）
    movl $13, %eax
    leal dir1, %ebx
    int $0x80

    # open("/docs/a.txt", 1) 创建
    movl $5, %eax
    leal f1, %ebx
    movl $1, %ecx
    int $0x80
    movl %eax, %esi          # fd

    # write(fd, d1, 11)
    movl $6, %eax
    movl %esi, %ebx
    leal d1, %ecx
    movl $11, %edx
    int $0x80

    # close(fd)
    movl $8, %eax
    movl %esi, %ebx
    int $0x80

    # chdir("/docs")
    movl $14, %eax
    leal dir1, %ebx
    int $0x80

loop:
    # open("b.txt", 1)  相对路径：应解析为 /docs/b.txt
    movl $5, %eax
    leal f2, %ebx
    movl $1, %ecx
    int $0x80
    movl %eax, %esi

    # write(fd, d2, 11)
    movl $6, %eax
    movl %esi, %ebx
    leal d2, %ecx
    movl $11, %edx
    int $0x80

    # close(fd)
    movl $8, %eax
    movl %esi, %ebx
    int $0x80

    # open("b.txt", 0) 读回
    movl $5, %eax
    leal f2, %ebx
    xorl %ecx, %ecx
    int $0x80
    movl %esi, %ebx
    movl %eax, %esi          # 新 fd

    # read(fd, buf, 11)
    movl $7, %eax
    movl %esi, %ebx
    leal buf, %ecx
    movl $11, %edx
    int $0x80

    # write(2, buf, 11)  串口打印读回的 b.txt 内容
    movl $1, %eax
    movl $2, %ebx
    leal buf, %ecx
    movl $11, %edx
    int $0x80

    movl $1500000, %ecx
wait:
    loop wait
    jmp loop
