# user12.S -- S9 信号与同步演示（入口 0x690000，栈顶 0x7B1000）
# 系统调用：
#   eax=18 sem_create(ebx=value)  -> eax=信号量 id
#   eax=3  fork()                 -> eax=子 pid（父）；0（子）
#   eax=17 sleep(ebx=ticks)       -> 进程睡眠，调度让出
#   eax=19 sem_wait(ebx=id)       -> P 操作（可阻塞；阻塞后唤醒返回 0）
#   eax=20 sem_signal(ebx=id)     -> V 操作
#   eax=2  exit()
#   eax=1  write(ebx=2, ecx=buf, edx=len)  （fd=2 串口取证）
# 行为：
#   main：sem_create(1) -> fork 两次 -> 父 exit，得两个子进程 C1/C2
#   C1：先 sleep(60) 证明"睡眠让出 CPU"（期间 C2 独占互斥区多轮）
#   C1/C2：互斥循环 8 次：sem_wait -> 打印 "Cx IN" -> sleep 占用 ->
#         打印 "Cx OUT" -> sem_signal。信号量保证同刻只有一个进程在临界区。

.section .data
msg_c1w:  .ascii "C1 slept\r\n"      # 10B：C1 睡醒报告
msg_in1:  .ascii "C1 IN \r\n"       # 8B
msg_in2:  .ascii "C2 IN \r\n"       # 8B
msg_out1: .ascii "C1 OUT\r\n"       # 8B
msg_out2: .ascii "C2 OUT\r\n"       # 8B
loopcnt:  .long 8                   # 互斥循环次数（内存计数，避免 ecx 被 syscall 破坏）

.section .text
.global _start
_start:
    movl $0x7B1000, %esp
    xorl %ebp, %ebp

    # ---- sem_create(1)：互斥锁 = 初值 1 的信号量 ----
    movl $18, %eax
    movl $1, %ebx
    int $0x80
    movl %eax, %esi                 # esi = sem id

    # ---- fork() 第一次：父 vs 子1 ----
    movl $3, %eax
    int $0x80
    cmpl $0, %eax
    jne parent1
    movl $1, %edi                   # 我是 C1
    jmp child_body

parent1:
    # ---- 父：fork() 第二次 ----
    movl $3, %eax
    int $0x80
    cmpl $0, %eax
    jne parent_exit
    movl $2, %edi                   # 我是 C2
    jmp child_body

parent_exit:
    movl $2, %eax                   # 父进程退出
    int $0x80
1:  jmp 1b                          # 保险

child_body:
    # ---- C1 专用：sleep(60) 让出，证明睡眠期间不运行 ----
    cmpl $1, %edi
    jne skip_sleep
    movl $17, %eax
    movl $60, %ebx
    int $0x80
    movl $1, %eax                   # 睡醒：打印 "C1 slept"
    movl $2, %ebx
    leal msg_c1w, %ecx
    movl $10, %edx
    int $0x80
skip_sleep:

    # ---- 互斥循环 8 次 ----
loop_i:
    movl $19, %eax                  # sem_wait(sem)
    movl %esi, %ebx
    int $0x80

    # 打印 "Cx IN"（进入临界区）
    movl $1, %eax
    movl $2, %ebx
    cmpl $1, %edi
    je use_in1
    leal msg_in2, %ecx
    jmp in_ok
use_in1:
    leal msg_in1, %ecx
in_ok:
    movl $8, %edx
    int $0x80

    # sleep(20) 占用临界区（C1 首次还额外睡了 60）
    movl $17, %eax
    movl $20, %ebx
    int $0x80

    # 打印 "Cx OUT"（离开临界区）
    movl $1, %eax
    movl $2, %ebx
    cmpl $1, %edi
    je use_out1
    leal msg_out2, %ecx
    jmp out_ok
use_out1:
    leal msg_out1, %ecx
out_ok:
    movl $8, %edx
    int $0x80

    movl $20, %eax                  # sem_signal(sem)
    movl %esi, %ebx
    int $0x80

    decl loopcnt
    jnz loop_i

    movl $2, %eax                   # 循环完毕，退出
    int $0x80
2:  jmp 2b
