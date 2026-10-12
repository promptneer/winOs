# user11.S -- S8 用户与权限演示（入口 0x680000，栈顶 0x7A1000）
# 系统调用：
#   eax=15 getuid()                     -> eax = 本进程 UID
#   eax=5  open(ebx=name, ecx=flags)    (flags: 0=读, 1=创建/写)
#   eax=7  read(ebx=fd, ecx=buf, edx=len)
#   eax=1  write(ebx=fd, ecx=buf, edx=len)  (fd=2 串口)
#   eax=8  close(ebx=fd)
# 行为（权限闭环演示，root 与 guest 各跑一轮对比）：
#   1) getuid 打印本进程身份（十进制）
#   2) 读 /public.txt（0644 root）-> 任何用户可读
#   3) 读 /secret.txt（0600 root）-> 仅 root；guest 收到 EACCES
#   4) 写 /guest.txt：root 创建成功（属主=root）；guest 对 root 的文件写被拒
#   5) 回读 /guest.txt 验证写读闭环

.section .data
uidp:   .ascii "user11 uid=\0"      # 11B
pubf:   .ascii "/public.txt\0"
secf:   .ascii "/secret.txt\0"
guestf: .ascii "/guest.txt\0"
data:   .ascii "perm!\r\n"          # 7B
msg_pub:   .ascii " pub-read-OK\r\n"        # 15B
msg_pub_d: .ascii " pub-read-DENIED\r\n"    # 19B
msg_sec:   .ascii " sec-read-OK\r\n"        # 15B
msg_sec_d: .ascii " sec-read-DENIED\r\n"    # 19B
msg_w:     .ascii " guest-write-OK\r\n"     # 18B
msg_w_d:   .ascii " guest-write-DENIED\r\n" # 22B
buf:    .space 16
digit:  .space 8

.section .text
.global _start
_start:
    movl $0x7A1000, %esp
    xorl %ebp, %ebp

    # ---- 1) getuid ----
    movl $15, %eax
    int $0x80
    movl %eax, %esi            # esi = uid

    # 串口打印 "user11 uid="（fd=2, 11B）
    movl $1, %eax
    movl $2, %ebx
    leal uidp, %ecx
    movl $11, %edx
    int $0x80

    # 打印 uid 十进制（0-999）
    movl %esi, %eax
    leal digit, %edi
    movl $100, %ecx
    xorl %edx, %edx
    divl %ecx                # eax=百位, edx=余
    addl $0x30, %eax
    movb %al, 0(%edi)
    movl %edx, %eax
    movl $10, %ecx
    xorl %edx, %edx
    divl %ecx                # eax=十位, edx=个位
    addl $0x30, %eax
    movb %al, 1(%edi)
    movl %edx, %eax
    addl $0x30, %eax
    movb %al, 2(%edi)
    movb $13, 3(%edi)
    movb $10, 4(%edi)
    movb $0, 5(%edi)
    movl $1, %eax
    movl $2, %ebx
    leal digit, %ecx
    movl $5, %edx
    int $0x80

    # ---- 2) 读 /public.txt（0644：人人可读）----
    movl $5, %eax
    leal pubf, %ebx
    xorl %ecx, %ecx
    int $0x80
    movl %eax, %edi
    cmpl $-1, %edi
    je pub_denied
    movl $7, %eax
    movl %edi, %ebx
    leal buf, %ecx
    movl $16, %edx
    int $0x80
    movl $1, %eax
    movl $2, %ebx
    leal buf, %ecx
    movl $16, %edx
    int $0x80
    movl $1, %eax
    movl $2, %ebx
    leal msg_pub, %ecx
    movl $15, %edx
    int $0x80
    movl $8, %eax
    movl %edi, %ebx
    int $0x80
    jmp sec_next
pub_denied:
    movl $1, %eax
    movl $2, %ebx
    leal msg_pub_d, %ecx
    movl $19, %edx
    int $0x80

    # ---- 3) 读 /secret.txt（0600：仅 root）----
sec_next:
    movl $5, %eax
    leal secf, %ebx
    xorl %ecx, %ecx
    int $0x80
    movl %eax, %edi
    cmpl $-1, %edi
    je sec_denied
    movl $7, %eax
    movl %edi, %ebx
    leal buf, %ecx
    movl $16, %edx
    int $0x80
    movl $1, %eax
    movl $2, %ebx
    leal buf, %ecx
    movl $16, %edx
    int $0x80
    movl $1, %eax
    movl $2, %ebx
    leal msg_sec, %ecx
    movl $15, %edx
    int $0x80
    movl $8, %eax
    movl %edi, %ebx
    int $0x80
    jmp guest_write
sec_denied:
    movl $1, %eax
    movl $2, %ebx
    leal msg_sec_d, %ecx
    movl $19, %edx
    int $0x80

    # ---- 4) 写 /guest.txt（root 首次创建；guest 对 root 文件写被拒）----
guest_write:
    movl $5, %eax
    leal guestf, %ebx
    movl $1, %ecx
    int $0x80
    movl %eax, %edi
    cmpl $-1, %edi
    je w_denied
    movl $6, %eax
    movl %edi, %ebx
    leal data, %ecx
    movl $7, %edx
    int $0x80
    movl $8, %eax
    movl %edi, %ebx
    int $0x80
    movl $1, %eax
    movl $2, %ebx
    leal msg_w, %ecx
    movl $18, %edx
    int $0x80

    # ---- 5) 回读 /guest.txt ----
    movl $5, %eax
    leal guestf, %ebx
    xorl %ecx, %ecx
    int $0x80
    movl %eax, %edi
    cmpl $-1, %edi
    je rb_done
    movl $7, %eax
    movl %edi, %ebx
    leal buf, %ecx
    movl $8, %edx
    int $0x80
    movl $1, %eax
    movl $2, %ebx
    leal buf, %ecx
    movl $8, %edx
    int $0x80
    movl $8, %eax
    movl %edi, %ebx
    int $0x80
    jmp rb_done
w_denied:
    movl $1, %eax
    movl $2, %ebx
    leal msg_w_d, %ecx
    movl $22, %edx
    int $0x80

rb_done:
    movl $1500000, %ecx
wait:
    loop wait
    jmp rb_done
