# user9.S -- 消息队列演示（S5-3：msgsend 3 条不同长度消息，msgrecv 依次收，串口打印）
# 系统调用：eax=11 msgsend(ebx=src, ecx=len)
#           eax=12 msgrecv(edx=dst)      -> eax=收到长度
#           eax=1  write(2,buf,len)      串口打印
# 行为：每轮发 "AB" / "msg12" / "msg-789" 三条 -> 收三条 -> 依次打印，无限循环

.section .data
ma:     .ascii "AB\0"
mb:     .ascii "msg12\0"
mc:     .ascii "msg-789\0"
buf:    .space 40

.section .text
.global _start
_start:
    movl $0x781000, %esp
    xorl %ebp, %ebp

loop:
    # msgsend(ma, 2)
    movl $11, %eax
    leal ma, %ebx
    movl $2, %ecx
    int $0x80
    # msgsend(mb, 5)
    movl $11, %eax
    leal mb, %ebx
    movl $5, %ecx
    int $0x80
    # msgsend(mc, 7)
    movl $11, %eax
    leal mc, %ebx
    movl $7, %ecx
    int $0x80

    # 收 3 条并打印（ebp = 剩余条数）
    movl $3, %ebp
recv_loop:
    movl $12, %eax
    leal buf, %edx
    int $0x80
    testl %eax, %eax
    js  recv_loop            # 队列空则重试（<0）
    movl %eax, %edx          # edx = len
    movl $2, %ebx            # fd = 2（串口）
    leal buf, %ecx
    movl $1, %eax            # syscall write
    int $0x80
    decl %ebp
    jnz recv_loop

    movl $1500000, %ecx
wait:
    loop wait
    jmp loop
