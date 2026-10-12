# user5.S -- exec 演示进程（入口 0x620000，栈顶 0x741000）
# 行为：打印 "user5: before exec..." -> exec(1) 变身成 user2 程序（入口 0x500000）
# 变身成功后：同一 pid 继续输出 user2 的计数 => 证明"进程没变、程序换了"

.section .data
msg:    .ascii "user5: before exec...\r\n"
msg_end:
.set msg_len, msg_end - msg

.section .text
.global _start
_start:
    movl $0x741000, %esp
    xorl %ebp, %ebp

    movl $1, %eax                # write(fd=2 串口, msg, len)
    movl $2, %ebx
    leal msg, %ecx
    movl $msg_len, %edx
    int $0x80

    movl $4, %eax                # exec(1) -> 变成 user2
    movl $1, %ebx
    int $0x80

fail:                            # exec 成功后不会回到这里
    jmp fail
