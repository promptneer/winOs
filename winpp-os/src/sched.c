#include "sched.h"
#include "vga.h"
#include "serial.h"
#include "pmm.h"
#include "paging.h"
#include "tss.h"
#include "gdt.h"

/* 用户代码/栈映射标志：P + W + U（用户态可访问；内核页无 U 位 → 用户访问 #PF） */
#define USER_PAGE_FLAGS 0x7

static pcb_t procs[PROC_MAX];
static u32   nprocs = 1;          /* pid0 = shell 常驻 */
static int   sched_on = 0;
static volatile int need_resched = 0;

pcb_t *sched_current = 0;

/* 汇编：切换内核栈到目标上下文帧并跳回中断恢复段（不返回） */
extern void sched_do_switch(u32 to_ctx);

/* ---------------- 极简 memcpy / 字符串 ---------------- */

static void k_memcpy(void *dst, const void *src, u32 n)
{
    u8 *d = dst; const u8 *s = src;
    for (u32 i = 0; i < n; i++) d[i] = s[i];
}

static void k_strcpy(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
}

/* ---------------- 初始化 ---------------- */

void sched_init(void)
{
    for (u32 i = 0; i < PROC_MAX; i++) {
        procs[i].state = PROC_EMPTY;
        procs[i].pid = 0;
        procs[i].ctx = 0;
        procs[i].reaped = 0;
        procs[i].code_page = procs[i].stack_page = procs[i].kstack_page = 0;
        procs[i].ticks_run = 0;
    }
    procs[0].pid = 0;
    procs[0].state = PROC_RUNNING;    /* shell 初始即运行 */
    procs[0].kstack_top = 0;          /* 内核态中断不换栈 */
    procs[0].code_vaddr = 0;
    procs[0].stack_vaddr = 0;
    k_strcpy(procs[0].name, "shell");
    sched_current = &procs[0];
    sched_on = 0;
    need_resched = 0;
}

int sched_running(void) { return sched_on; }
void sched_need_resched(void) { need_resched = 1; }
int sched_should_switch(void) { return sched_on && need_resched; }
u32 sched_user_count(void) { return nprocs > 0 ? nprocs - 1 : 0; }

/* ---------------- 进程创建 ---------------- */

static void proc_init_frame(pcb_t *p, u32 entry, u32 stack_vaddr)
{
    regs_t *f = (regs_t *)(p->kstack_top - sizeof(regs_t));
    f->gs = f->fs = f->es = f->ds = USER_DS | 3;   /* 0x23 */
    f->edi = f->esi = f->ebp = f->ebx = f->edx = f->ecx = f->eax = 0;
    f->esp = 0;                                    /* oesp 占位 */
    f->int_no = 0; f->err_code = 0;
    f->eip = entry;
    f->cs = USER_CS | 3;                           /* 0x1B */
    f->eflags = 0x202;                             /* IF=1, 保留位 1 */
    f->user_esp = stack_vaddr;                     /* 用户栈顶 */
    f->ss = USER_DS | 3;                           /* 0x23 */
    p->ctx = (u32)f;
}

int sched_create(const char *name, u32 entry, u32 stack_vaddr,
                 const u8 *code, u32 code_len)
{
    if (nprocs >= PROC_MAX) return -1;
    if (code_len > PAGE_SIZE) return -1;

    u32 kp = pmm_alloc_page();
    if (!kp) return -1;
    u32 cp = pmm_alloc_page();
    if (!cp) { pmm_free_page(kp); return -1; }
    u32 sp = pmm_alloc_page();
    if (!sp) { pmm_free_page(kp); pmm_free_page(cp); return -1; }

    /* 恒等映射下物理地址可直接读写：拷代码到用户物理页 */
    k_memcpy((void *)cp, code, code_len);

    /* 用户代码页 + 用户栈页（栈顶在页面顶部） */
    map_page(entry, cp, USER_PAGE_FLAGS);
    map_page(stack_vaddr & ~0xFFFu, sp, USER_PAGE_FLAGS);
    serial_printf("[dbg] src=%x code=%x%x%x%x%x%x%x%x\r\n",
                  (u32)code, code[0], code[1], code[2], code[3],
                  code[4], code[5], code[6], code[7]);
    serial_printf("[dbg] map code %x->%x pd=%x pte=%x | stack %x->%x pd=%x pte=%x\r\n",
        entry, cp,
        ((volatile u32 *)0xFFFFF000u)[entry >> 22],
        ((volatile u32 *)(0xFFC00000u + (entry >> 22) * 0x1000))[(entry >> 12) & 0x3FF],
        stack_vaddr & ~0xFFFu, sp,
        ((volatile u32 *)0xFFFFF000u)[(stack_vaddr >> 22) & 0x3FF],
        ((volatile u32 *)(0xFFC00000u + ((stack_vaddr >> 22) & 0x3FF) * 0x1000))[((stack_vaddr >> 12) & 0x3FF)]);

    pcb_t *p = &procs[nprocs];
    p->pid = nprocs;               /* 1.. */
    p->state = PROC_READY;
    p->kstack_page = kp;
    p->kstack_top = kp + 0x1000;
    p->code_page = cp;
    p->code_vaddr = entry;
    p->stack_page = sp;
    p->stack_vaddr = stack_vaddr & ~0xFFFu;
    p->ticks_run = 0;
    p->reaped = 0;
    k_strcpy(p->name, name);
    proc_init_frame(p, entry, stack_vaddr);

    nprocs++;
    return p->pid;
}

/* ---------------- 资源回收 ---------------- */

static void proc_reap(pcb_t *p)
{
    if (p->reaped) return;
    p->reaped = 1;
    if (p->code_page) {
        unmap_page(p->code_vaddr);
        pmm_free_page(p->code_page);
    }
    if (p->stack_page) {
        unmap_page(p->stack_vaddr);
        pmm_free_page(p->stack_page);
    }
    if (p->kstack_page)
        pmm_free_page(p->kstack_page);
    p->state = PROC_EMPTY;
    p->ctx = 0;
}

int sched_kill(int pid)
{
    if (pid == 0) return -1;                       /* 不许杀 shell */
    for (u32 i = 1; i < nprocs; i++) {
        if (procs[i].pid != (u32)pid) continue;
        if (procs[i].state == PROC_EMPTY) return -1;
        if (&procs[i] == sched_current) {
            procs[i].state = PROC_ZOMBIE;          /* 切走后由调度器回收 */
            sched_need_resched();
        } else {
            proc_reap(&procs[i]);
        }
        return 0;
    }
    return -1;
}

void sched_stop(void)
{
    /* 杀全部用户进程（若当前是用户进程，先切回 shell 再清） */
    for (u32 i = 1; i < nprocs; i++) {
        if (procs[i].state != PROC_EMPTY)
            sched_kill((int)procs[i].pid);
    }
    sched_on = 0;
    need_resched = 0;
    if (sched_current->pid != 0)
        sched_current = &procs[0];
}

void sched_start(void)
{
    if (nprocs <= 1) return;
    sched_on = 1;
    need_resched = 1;   /* 下一个时钟中断即切到第一个用户进程 */
}

/* ---------------- 调度切换 ---------------- */

static pcb_t *pick_next(void)
{
    if (!sched_current) return 0;
    int cur_i = -1;
    for (u32 i = 0; i < nprocs; i++)
        if (&procs[i] == sched_current) { cur_i = (int)i; break; }
    /* round-robin：从当前之后开始找 READY，顺带回收僵尸 */
    for (u32 k = 1; k <= nprocs; k++) {
        u32 i = (u32)((cur_i + (int)k) % nprocs);
        if (procs[i].state == PROC_ZOMBIE && !procs[i].reaped)
            proc_reap(&procs[i]);                 /* 回收被 kill 的当前进程 */
        if (procs[i].state == PROC_READY)
            return &procs[i];
    }
    return 0;
}

void sched_tick(regs_t *r)
{
    if (!sched_on || !need_resched) return;
    need_resched = 0;
    if (!sched_current) return;

    pcb_t *cur = sched_current;
    cur->ctx = (u32)r;               /* 保存当前上下文帧（位于当前内核栈） */
    cur->state = PROC_READY;
    cur->ticks_run++;

    pcb_t *next = pick_next();
    if (!next) {
        cur->state = PROC_RUNNING;   /* 没有其他就绪进程：留在原地 */
        cur->ctx = 0;
        return;
    }
    next->state = PROC_RUNNING;
    next->ticks_run++;
    tss_set_esp0(next->kstack_top);
    sched_current = next;
    serial_printf("[sched] pid%d -> pid%d (cr3=%x esp0=%x)\r\n",
                  cur->pid, next->pid, read_cr3(), next->kstack_top);
    sched_do_switch(next->ctx);      /* 不返回：换栈 + iret 到 next 用户态 */
    for (;;) { }                     /* 编译器保险（实际不会执行） */
}

void sched_ps(void)
{
    static const char *st[4] = { "empty", "ready", "run", "zombie" };
    vga_printf("PID  STATE   NAME       TICKS  CODE\n");
    for (u32 i = 0; i < nprocs; i++) {
        if (procs[i].state == PROC_EMPTY) continue;
        vga_printf("%3u  %-7s %-10s %5u  %x\n",
                   procs[i].pid, st[procs[i].state], procs[i].name,
                   procs[i].ticks_run, procs[i].code_vaddr);
    }
}

/* ---------------- 系统调用（int 0x80） ---------------- */

void syscall_handler(regs_t *r)
{
    u32 nr = r->eax;

    if (nr == 0) {                  /* yield：让出剩余时间片 */
        sched_need_resched();
        r->eax = 0;
    }
    else if (nr == 1) {             /* write(fd, buf, len) */
        u32 fd = r->ebx, len = r->edx;
        char *buf = (char *)r->ecx;
        if (len > 200) len = 200;

        if (fd == 1) {              /* VGA：输出到屏幕底部两行，不打扰 shell */
            u8 sx, sy;
            vga_get_pos(&sx, &sy);
            static u8 vrow = 23;
            vga_gotoxy(0, vrow);
            vga_set_attr(0x0B);
            for (u32 i = 0; i < len; i++) {
                if (buf[i] == '\n') vga_putc(' ');
                else vga_putc(buf[i]);
            }
            vga_gotoxy(sx, sy);     /* 恢复 shell 光标 */
            vrow = (vrow == 23) ? 24 : 23;
        }
        else if (fd == 2) {         /* 串口：机器侧取证 */
            if (sched_current)
                serial_printf("[pid%d] ", sched_current->pid);
            for (u32 i = 0; i < len; i++) serial_putc(buf[i]);
        }
        r->eax = len;
    }
    else if (nr == 2) {             /* exit()：仅用户进程可用 */
        if (sched_current && sched_current->pid != 0) {
            serial_printf("[sched] pid%d exit\r\n", sched_current->pid);
            sched_current->state = PROC_ZOMBIE;
            sched_need_resched();
        }
        r->eax = 0;
    }
    else {
        r->eax = -1;
    }
}
