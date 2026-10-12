#ifndef SCHED_H
#define SCHED_H

#include "io.h"

/* 寄存器帧：与 isr.S 存根布局完全一致（中断/系统调用进入时压栈顺序） */
typedef struct {
    u32 gs, fs, es, ds;          /* 存根压入 */
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* pusha */
    u32 int_no, err_code;        /* 存根压入 */
    u32 eip, cs, eflags, user_esp, ss;  /* CPU 压入（用户态中断含 ss/esp；内核态无） */
} regs_t;

#define PROC_MAX   8
#define PROC_EMPTY   0
#define PROC_READY   1
#define PROC_RUNNING 2
#define PROC_ZOMBIE  3
#define PROC_SLEEPING 4   /* S9：睡眠/等待信号量中（wake_tick 或 blk_sem 决定唤醒方式） */

#define SCHED_QUANTUM 10   /* 每 10 个 tick（0.1s）强制切换 */

typedef struct {
    u32 pid;
    int  state;
    char name[16];
    u32  kstack_top;     /* 内核栈顶（TSS.esp0 目标） */
    u32  kstack_page;    /* 内核栈物理页（0=静态栈不回收） */
    u32  ctx;            /* 用户上下文帧（regs_t *）在内核栈上 */
    u32  ticks_run;      /* 累计运行 tick */
    u32  code_page;      /* 用户代码物理页 */
    u32  code_vaddr;
    u32  stack_page;     /* 用户栈物理页 */
    u32  stack_vaddr;
    u32  pd_page;        /* 进程独立页目录（S4：0=沿用 boot PD，仅 shell） */
    u32  pt1_page;       /* 进程独立 PT1（4-8MB 用户区页表，含内核栈恒等项） */
    u32  reaped;         /* 资源已回收 */
    u32  uid;            /* S8: 进程属主 UID（创建时继承当前身份） */
    u32  wake_tick;      /* S9: sleep 唤醒时刻（timer_ticks 值；blk_sem>=0 时无效） */
    int  blk_sem;        /* S9: 阻塞等待的信号量 id；-1=非信号量阻塞（sleep） */
} pcb_t;

/* 调度器 */
extern pcb_t *sched_current;
void   sched_init(void);
int    sched_running(void);
void   sched_start(void);        /* 启动多任务轮转（run） */
void   sched_stop(void);         /* 停止调度并清理用户进程（stop） */
int    sched_create(const char *name, u32 entry, u32 stack_vaddr,
                    const u8 *code, u32 code_len);
int    sched_fork(pcb_t *parent, const regs_t *r);   /* S4-2：进程分身 */
int    sched_exec(pcb_t *p, regs_t *r, int idx);      /* S4-3：进程变身 */
void   sched_need_resched(void);
int    sched_should_switch(void);
void   sched_tick(regs_t *r);    /* irq_handler 末尾调用；可能不返回 */
int    sched_kill(int pid);
void   sched_ps(void);
u32    sched_user_count(void);

/* S9 同步：每 tick 唤醒到期睡眠进程；信号量唤醒；立即让出（阻塞 syscall 用） */
void   sched_wakeup_scan(void);
int    sched_wake_by_sem(int id, u32 pid);
void   sched_yield_now(regs_t *r);   /* 保存当前帧并强制切换（阻塞后调用，不返回） */

/* 系统调用分发（int 0x80 → syscall_common_stub → 此处） */
void syscall_handler(regs_t *r);

#endif
