# user4.S -- fork 演示进程（入口 0x610000，栈顶 0x731000）
# 系统调用：eax=3 fork；返回 eax=0 为子进程，非 0 为父进程（返回子 pid）
# 行为：fork 后父子各自递增计数 -> "fork: child/parent 000000\r\n" -> 串口+VGAT 循环
# 内核串口带 [pidN] 前缀：父子 pid 不同 => 可证明两个进程同时存在、独立运行

.section .data
bufc:   .ascii "fork: child  000000\r\n"   # 21B, 数字区 @+13
bufp:   .ascii "fork: parent 000000\r\n"   # 21B, 数字区 @+13
buf_end:

.section .text
.global _start
_start:
    movl $0x731000, %esp
    xorl %ebp, %ebp
    movl $3, %eax                # fork()
    int $0x80
    testl %eax, %eax
    jnz parent                   # 父：eax=子pid != 0
child:
    leal bufc, %esi
    jmp runloop
parent:
    leal bufp, %esi
runloop:
    xorl %edi, %edi              # 计数器 n（每进程独立内存 -> 独立计数）
loop:
    leal 19(%esi), %ecx          # 数字区(13-18)末尾+1
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

    movl $1, %eax                # write
    movl $1, %ebx                # fd=1 VGA
    movl %esi, %ecx
    movl $21, %edx
    int $0x80

    movl $1, %eax
    movl $2, %ebx                # fd=2 串口
    movl %esi, %ecx
    movl $21, %edx
    int $0x80

    incl %edi
    movl $1500000, %ecx
wait:
    loop wait
    jmp loop
