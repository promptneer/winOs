# user1.S -- 用户态进程 1（链接于 0x400000，栈顶 0x701000）
# 系统调用 int 0x80: eax=1 write(ebx=fd, ecx=buf, edx=len); eax=2 exit()
# 行为：计数器递增 -> "user1: <n>\r\n" -> write 到 VGA(1) 与串口(2) -> 忙等

.section .data
buf:    .ascii "user1: 000000\r\n"
buf_end:
.set buf_len, buf_end - buf

.section .text
.global _start
_start:
    movl $0x701000, %esp
    xorl %ebp, %ebp
    xorl %edi, %edi              # 计数器 n

loop:
    leal buf+12, %ecx            # 数字区末尾（6 位，右对齐）
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
    leal buf, %ecx
    movl $buf_len, %edx
    int $0x80

    movl $1, %eax
    movl $2, %ebx                # fd=2 串口
    leal buf, %ecx
    movl $buf_len, %edx
    int $0x80

    incl %edi

    movl $1500000, %ecx          # 忙等：时间片内让出前被时钟中断切走
wait:
    loop wait

    jmp loop
