# WinPP OS -- 中断/异常存根 (GAS, i686)
# 0-31 为 CPU 异常，32-47 为 PIC 重映射后的 IRQ

.section .text

.macro ISR_NOERR n
.global isr\n
isr\n:
    pushl $0
    pushl $\n
    jmp isr_common_stub
.endm

.macro ISR_ERR n
.global isr\n
isr\n:
    pushl $\n
    jmp isr_common_stub
.endm

.macro IRQ n, vec
.global irq\n
irq\n:
    pushl $0
    pushl $\vec
    jmp irq_common_stub
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

IRQ 0 32
IRQ 1 33
IRQ 2 34
IRQ 3 35
IRQ 4 36
IRQ 5 37
IRQ 6 38
IRQ 7 39
IRQ 8 40
IRQ 9 41
IRQ 10 42
IRQ 11 43
IRQ 12 44
IRQ 13 45
IRQ 14 46
IRQ 15 47

.extern isr_handler
isr_common_stub:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movl %esp, %eax
    pushl %eax
    call isr_handler
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

.extern irq_handler
irq_common_stub:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movl %esp, %eax
    pushl %eax
    call irq_handler
    addl $4, %esp
irq_common_restore:
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

# 系统调用 int 0x80（DPL3 门，用户态可触发）
.global syscall_isr
syscall_isr:
    pushl $0
    pushl $0x80
    jmp syscall_common_stub

.extern syscall_handler
syscall_common_stub:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movl %esp, %eax
    pushl %eax
    call syscall_handler
    addl $4, %esp
    jmp irq_common_restore

# void sched_do_switch(u32 to_ctx) -- 把内核栈切到目标进程上下文帧
# 然后跳回中断恢复段 pop/iret，从目标进程的用户态现场继续。永不返回。
.global sched_do_switch
sched_do_switch:
    movl 4(%esp), %eax    # to_ctx
    movl %eax, %esp       # 换内核栈（丢弃当前 irq_handler 栈帧）
    jmp irq_common_restore
