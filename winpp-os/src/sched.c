#include "sched.h"
#include "vga.h"
#include "serial.h"
#include "pmm.h"
#include "paging.h"
#include "tss.h"
#include "gdt.h"
#include "fs.h"
#include "ipc.h"
#include "user.h"
#include "users_inc.h"
#include "timer.h"
#include "sem.h"

/* 用户代码/栈映射标志：P + W + U（用户态可访问；内核页无 U 位 → 用户访问 #PF） */
#define USER_PAGE_FLAGS 0x7

/* S4 独立地址空间：每个用户进程拥有自己的 PD + PT1。
 * 新 PD 复制 src_pd（内核恒等 + 自映射骨架），PD[1] 指向自己的 PT1；
 * 新 PT1 复制 src_pt1（内核栈/页表页/堆等恒等项，换 CR3 后内核仍可访问），
 * 清掉旧用户残留项后写入本进程的代码页 + 栈页（P+W+U）。
 * 创建进程时 src_pd=src_pt1=boot 的；fork 时传入父进程的 pd/pt1 继承布局。
 * 返回 PD 物理页；失败返回 0。 */
static u32 proc_make_addrspace(u32 entry, u32 stack_vaddr,
                               u32 code_phys, u32 stack_phys,
                               u32 src_pd, u32 src_pt1, u32 *out_pt1)
{
    u32 pd = pmm_alloc_page();
    if (!pd) return 0;
    u32 pt1 = pmm_alloc_page();
    if (!pt1) { pmm_free_page(pd); return 0; }

    volatile u32 *pdv  = (volatile u32 *)pd;
    volatile u32 *pt1v = (volatile u32 *)pt1;
    for (u32 i = 0; i < 1024; i++) pdv[i]  = ((volatile u32 *)src_pd)[i];
    for (u32 i = 0; i < 1024; i++) pt1v[i] = ((volatile u32 *)src_pt1)[i];

    /* 清旧共享用户项残留（第三阶段固定映射），防跨进程越权访问 */
    static const u32 legacy[6] = { 0x400000, 0x500000, 0x600000,
                                   0x700000, 0x710000, 0x720000 };
    for (u32 k = 0; k < 6; k++) pt1v[PT_INDEX(legacy[k])] = 0;

    /* 本进程用户代码页 + 用户栈页 */
    pt1v[PT_INDEX(entry)]        = (code_phys  & 0xFFFFF000u) | USER_PAGE_FLAGS;
    pt1v[PT_INDEX(stack_vaddr)]  = (stack_phys & 0xFFFFF000u) | USER_PAGE_FLAGS;

    pdv[1] = pt1 | USER_PAGE_FLAGS;          /* 用户 PDE 也要 U 位 */
    pdv[1023] = pd | 0x3;                    /* 自映射指向新 PD 自身 */
    *out_pt1 = pt1;
    return pd;
}

static void set_cr3(u32 pd) { asm volatile("mov %0, %%cr3" :: "r"(pd)); }

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

/* S4-3 exec：进程"变身"——同一进程换程序。
 * 释放旧代码页/栈页映射，载入目标程序（内嵌 userN.bin），重设入口与栈。
 * 调用方必须是用户进程（shell 不允许）。返回 0 成功 / -1 失败。 */
typedef struct {
    const char *name;
    u32  entry;
    u32  stack_vaddr;
    const u8 *code;
    u32  len;
} prog_t;

static const prog_t progtab[] = {
    { "user1", 0x400000, 0x701000, _user1_code, USER1_LEN },
    { "user2", 0x500000, 0x711000, _user2_code, USER2_LEN },
    { "user3", 0x600000, 0x721000, _user3_code, USER3_LEN },
    { "user4", 0x610000, 0x731000, _user4_code, USER4_LEN },
};
#define NPROGS ((int)(sizeof(progtab) / sizeof(progtab[0])))

int sched_exec(pcb_t *p, regs_t *r, int idx)
{
    if (idx < 0 || idx >= NPROGS) return -1;
    const prog_t *g = &progtab[idx];
    if (!p->code_page || !p->pt1_page) return -1;   /* shell 无地址空间 */

    volatile u32 *pt1v = (volatile u32 *)p->pt1_page;
    /* 1) 解除旧用户代码页 + 旧栈页映射（物理直写本进程 PT1） */
    pt1v[PT_INDEX(p->code_vaddr)]  = 0;
    pt1v[PT_INDEX(p->stack_vaddr)] = 0;
    /* 2) 载入新程序：拷代码、清空栈页 */
    for (u32 i = 0; i < PAGE_SIZE; i++) ((volatile u8 *)p->code_page)[i] = 0;
    for (u32 i = 0; i < g->len; i++)
        ((volatile u8 *)p->code_page)[i] = g->code[i];
    for (u32 i = 0; i < PAGE_SIZE; i++) ((volatile u8 *)p->stack_page)[i] = 0;
    /* 3) 新入口 + 新栈地址映射 */
    p->code_vaddr  = g->entry;
    p->stack_vaddr = g->stack_vaddr & ~0xFFFu;
    pt1v[PT_INDEX(p->code_vaddr)]  = p->code_page  | USER_PAGE_FLAGS;
    pt1v[PT_INDEX(p->stack_vaddr)] = p->stack_page | USER_PAGE_FLAGS;
    /* 4) 用户现场：入口 = 新程序，eax=0 表示成功，用户栈 = 新栈顶 */
    r->eip = g->entry;
    r->eax = 0;
    r->user_esp = g->stack_vaddr;
    k_strcpy(p->name, g->name);
    serial_printf("[exec] pid%d -> program %s (entry=%x pd=%x)\r\n",
                  p->pid, g->name, g->entry, p->pd_page);
    return 0;
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
        procs[i].pd_page = procs[i].pt1_page = 0;
        procs[i].ticks_run = 0;
        procs[i].uid = 0;
        procs[i].wake_tick = 0;
        procs[i].blk_sem = -1;
    }
    procs[0].pid = 0;
    procs[0].state = PROC_RUNNING;    /* shell 初始即运行 */
    procs[0].kstack_top = 0;          /* 内核态中断不换栈 */
    procs[0].code_vaddr = 0;
    procs[0].stack_vaddr = 0;
    procs[0].pd_page = read_cr3();    /* shell 沿用 boot PD */
    procs[0].pt1_page = 0;
    procs[0].uid = user_current_uid();   /* S8: shell 初始身份（root） */
    procs[0].wake_tick = 0;
    procs[0].blk_sem = -1;
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

    /* S4：为进程建立独立地址空间（PD + PT1），映射代码页/栈页。
     * 新进程继承 boot PD/PT1 的内核恒等布局。 */
    u32 boot_pd  = read_cr3();
    u32 boot_pt1 = ((volatile u32 *)boot_pd)[1] & 0xFFFFF000u;
    u32 pt1 = 0;
    u32 pd = proc_make_addrspace(entry, stack_vaddr & ~0xFFFu, cp, sp,
                                 boot_pd, boot_pt1, &pt1);
    if (!pd) {
        pmm_free_page(kp); pmm_free_page(cp); pmm_free_page(sp);
        return -1;
    }
    serial_printf("[dbg] src=%x code=%x%x%x%x%x%x%x%x\r\n",
                  (u32)code, code[0], code[1], code[2], code[3],
                  code[4], code[5], code[6], code[7]);
    serial_printf("[dbg] addrspace pd=%x pt1=%x code %x->%x stack %x->%x\r\n",
                  pd, pt1, entry, cp, stack_vaddr & ~0xFFFu, sp);

    pcb_t *p = &procs[nprocs];
    p->pid = nprocs;               /* 1.. */
    p->state = PROC_READY;
    p->kstack_page = kp;
    p->kstack_top = kp + 0x1000;
    p->code_page = cp;
    p->code_vaddr = entry;
    p->stack_page = sp;
    p->stack_vaddr = stack_vaddr & ~0xFFFu;
    p->pd_page = pd;
    p->pt1_page = pt1;
    p->ticks_run = 0;
    p->reaped = 0;
    p->uid = user_current_uid();       /* S8: 进程继承创建者身份 */
    p->wake_tick = 0;
    p->blk_sem = -1;
    k_strcpy(p->name, name);
    proc_init_frame(p, entry, stack_vaddr);

    nprocs++;
    return p->pid;
}

/* ---------------- 资源回收 ---------------- */

/* S4-2 fork：复制当前进程为子进程。
 * 子进程获得：新内核栈、新 PD/PT1（继承父布局）、用户代码页/栈页内容副本；
 * 用户上下文帧复制父现场，仅 eax=0（父返回子 pid，子返回 0）。
 * 返回子 pid；失败 -1。 */
int sched_fork(pcb_t *parent, const regs_t *r)
{
    if (nprocs >= PROC_MAX) return -1;

    u32 kp = pmm_alloc_page();
    u32 cp = pmm_alloc_page();
    u32 sp = pmm_alloc_page();
    if (!kp || !cp || !sp) {
        if (kp) pmm_free_page(kp);
        if (cp) pmm_free_page(cp);
        if (sp) pmm_free_page(sp);
        return -1;
    }
    /* 复制父用户代码页 + 栈页（当前 CR3=父 PD，恒等区可直读写） */
    k_memcpy((void *)cp, (void *)parent->code_page, PAGE_SIZE);
    k_memcpy((void *)sp, (void *)parent->stack_page, PAGE_SIZE);

    /* 子地址空间：继承父 PD/PT1 布局，改写 code/stack 项指向子页 */
    u32 pt1 = 0;
    u32 pd = proc_make_addrspace(parent->code_vaddr, parent->stack_vaddr,
                                 cp, sp, parent->pd_page, parent->pt1_page, &pt1);
    if (!pd) {
        pmm_free_page(kp); pmm_free_page(cp); pmm_free_page(sp);
        return -1;
    }

    pcb_t *c = &procs[nprocs];
    c->pid = nprocs;
    c->state = PROC_READY;
    c->kstack_page = kp;
    c->kstack_top = kp + 0x1000;
    c->code_page = cp;
    c->code_vaddr = parent->code_vaddr;
    c->stack_page = sp;
    c->stack_vaddr = parent->stack_vaddr;
    c->pd_page = pd;
    c->pt1_page = pt1;
    c->ticks_run = 0;
    c->reaped = 0;
    c->uid = parent->uid;              /* S8: fork 继承父身份 */
    c->wake_tick = 0;
    c->blk_sem = -1;
    k_strcpy(c->name, parent->name);
    c->name[0] = 'f';              /* "user1" -> "fuser1"（同 pid 前缀） */

    /* 复制父用户上下文帧到子内核栈；子进程 eax=0（fork 返回 0） */
    regs_t *cf = (regs_t *)(c->kstack_top - sizeof(regs_t));
    k_memcpy(cf, r, sizeof(regs_t));
    cf->eax = 0;
    c->ctx = (u32)cf;

    nprocs++;
    serial_printf("[fork] pid %d -> child pid %d (pd=%x esp0=%x)\r\n",
                  parent->pid, c->pid, pd, c->kstack_top);
    return (int)c->pid;
}

static void proc_reap(pcb_t *p)
{
    if (p->reaped) return;
    p->reaped = 1;
    /* S4：进程有独立地址空间，直接物理写目标 PT1 清用户项
     * （unmap_page 操作当前 CR3 的 PD，不能用于清理非当前进程） */
    if (p->pt1_page) {
        volatile u32 *pt1v = (volatile u32 *)p->pt1_page;
        if (p->code_vaddr)  pt1v[PT_INDEX(p->code_vaddr)] = 0;
        if (p->stack_vaddr) pt1v[PT_INDEX(p->stack_vaddr)] = 0;
    }
    if (p->code_page)  pmm_free_page(p->code_page);
    if (p->stack_page) pmm_free_page(p->stack_page);
    if (p->kstack_page) pmm_free_page(p->kstack_page);
    if (p->pt1_page) pmm_free_page(p->pt1_page);
    if (p->pd_page && p->pd_page != procs[0].pd_page)
        pmm_free_page(p->pd_page);
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
    if (sched_current->pid != 0) {
        sched_current = &procs[0];
        set_cr3(procs[0].pd_page);   /* S4：切回 shell 恢复 boot 页目录 */
    }
    /* S4-fix: stop 后复位进程表，pid 从 1 重新分配（防槽位耗尽、演示 pid 漂移） */
    for (u32 i = 1; i < PROC_MAX; i++) {
        if (procs[i].state != PROC_EMPTY) proc_reap(&procs[i]);
        procs[i].pid = 0;
        procs[i].reaped = 0;
        procs[i].ticks_run = 0;
        procs[i].pd_page = procs[i].pt1_page = 0;
        procs[i].code_page = procs[i].stack_page = procs[i].kstack_page = 0;
    }
    nprocs = 1;
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

/* S9 唤醒扫描：每时钟 tick 调用一次，把 sleep 到期（blk_sem==-1）的睡眠进程置 READY */
void sched_wakeup_scan(void)
{
    if (!sched_on) return;
    u32 now = timer_ticks();
    for (u32 i = 0; i < nprocs; i++) {
        if (procs[i].state != PROC_SLEEPING) continue;
        if (procs[i].blk_sem >= 0) continue;      /* 信号量等待由 sem_signal 唤醒 */
        if ((u32)(now - procs[i].wake_tick) >= 0x80000000u) continue;  /* 未到期 */
        {
            procs[i].state = PROC_READY;
            serial_printf("[sched] pid%d wake (tick=%u)\r\n", procs[i].pid, now);
        }
    }
}

/* S9 信号量唤醒：V 操作调这里，把等待该信号量的 pid 置 READY（FIFO 队首） */
int sched_wake_by_sem(int id, u32 pid)
{
    for (u32 i = 0; i < nprocs; i++) {
        if (procs[i].pid != pid) continue;
        if (procs[i].state != PROC_SLEEPING || procs[i].blk_sem != id) continue;
        procs[i].state = PROC_READY;
        procs[i].blk_sem = -1;
        /* 唤醒后从阻塞的 int 0x80 返回：sem_wait 返回 0（已拿到锁） */
        if (procs[i].ctx) {
            regs_t *f = (regs_t *)procs[i].ctx;
            f->eax = 0;
        }
        return 0;
    }
    return -1;
}

/* S9 强制让出：阻塞 syscall 后调用，保存当前帧并立即切换（不返回）。
 * 若没有其他就绪进程，等待下一个 tick 唤醒扫描后再切换。 */
void sched_yield_now(regs_t *r)
{
    if (!sched_on) return;
    sched_need_resched();
    sched_tick(r);
    for (;;) asm volatile("hlt");   /* 保险：sched_tick 应已换栈不返回 */
}

void sched_tick(regs_t *r)
{
    if (!sched_on || !need_resched) return;
    need_resched = 0;
    if (!sched_current) return;

    pcb_t *cur = sched_current;
    cur->ctx = (u32)r;               /* 保存当前上下文帧（位于当前内核栈） */
    if (cur->state == PROC_RUNNING) cur->state = PROC_READY;  /* S9: 阻塞中保持 SLEEPING */
    cur->ticks_run++;

    pcb_t *next = pick_next();
    if (!next) {
        /* S9：当前若是阻塞（SLEEPING）则原地等待唤醒扫描；否则恢复运行 */
        if (cur->state == PROC_READY) cur->state = PROC_RUNNING;
        cur->ctx = 0;
        return;
    }
    next->state = PROC_RUNNING;
    next->ticks_run++;
    tss_set_esp0(next->kstack_top);
    /* S4：切换 CR3 到目标进程的独立页目录（不同才换；shell 沿用 boot PD） */
    if (next->pd_page != cur->pd_page) set_cr3(next->pd_page);
    sched_current = next;
    serial_printf("[sched] pid%d -> pid%d (pd=%x esp0=%x)\r\n",
                  cur->pid, next->pid, read_cr3(), next->kstack_top);
    sched_do_switch(next->ctx);      /* 不返回：换栈 + iret 到 next 用户态 */
    for (;;) { }                     /* 编译器保险（实际不会执行） */
}

void sched_ps(void)
{
    static const char *st[5] = { "empty", "ready", "run", "zombie", "sleep" };
    vga_printf("PID  STATE   NAME       UID  TICKS  CODE\n");
    for (u32 i = 0; i < nprocs; i++) {
        if (procs[i].state == PROC_EMPTY) continue;
        vga_printf("%3u  %-7s %-10s %3u %5u  %x\n",
                   procs[i].pid, st[procs[i].state], procs[i].name,
                   procs[i].uid, procs[i].ticks_run, procs[i].code_vaddr);
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
    else if (nr == 3) {             /* fork()：复制当前进程（S4-2） */
        if (sched_current && sched_current->pid != 0)
            r->eax = (u32)sched_fork(sched_current, r);
        else
            r->eax = 0xFFFFFFFFu;   /* shell 不允许 fork */
    }
    else if (nr == 4) {             /* exec(idx)：进程变身（S4-3） */
        if (sched_current && sched_current->pid != 0)
            r->eax = (u32)sched_exec(sched_current, r, (int)r->ebx);
        else
            r->eax = 0xFFFFFFFFu;   /* shell 不允许 exec */
    }
    else if (nr == 5) {             /* open(name, flags)：S4-4 文件系统 */
        r->eax = (u32)fs_open((const char *)r->ebx, (int)r->ecx);
    }
    else if (nr == 6) {             /* write(fd, buf, len) */
        r->eax = (u32)fs_write((int)r->ebx, (const char *)r->ecx, (int)r->edx);
    }
    else if (nr == 7) {             /* read(fd, buf, len) */
        r->eax = (u32)fs_read((int)r->ebx, (char *)r->ecx, (int)r->edx);
    }
    else if (nr == 8) {             /* close(fd) */
        r->eax = (u32)fs_close((int)r->ebx);
    }
    else if (nr == 9) {             /* pipe()：S5-1 匿名管道，返回读端 fd（写端=fd+1） */
        r->eax = (u32)ipc_pipe_create();
    }
    else if (nr == 10) {            /* fifo_create(name)：S5-2 命名管道 */
        r->eax = (u32)ipc_fifo_create((const char *)r->ebx);
    }
    else if (nr == 11) {            /* msgsend(src, len)：S5-3 消息队列 */
        r->eax = (u32)mq_send((const char *)r->ebx, (int)r->ecx);
    }
    else if (nr == 12) {            /* msgrecv(dst)：返回本消息长度 */
        r->eax = (u32)mq_recv((char *)r->edx);
    }
    else if (nr == 13) {            /* mkdir(path)：S6-1 创建目录 */
        r->eax = (u32)fs_mkdir((const char *)r->ebx);
    }
    else if (nr == 14) {            /* chdir(path)：S6-2 切换当前目录 */
        r->eax = (u32)fs_chdir((const char *)r->ebx);
    }
    else if (nr == 15) {            /* getuid()：S8 返回当前进程 UID */
        r->eax = (sched_current && sched_current->pid != 0)
                 ? sched_current->uid : user_current_uid();
    }
    else if (nr == 16) {            /* chmod(path, mode)：S8 改文件权限 */
        r->eax = (u32)fs_chmod((const char *)r->ebx, r->ecx);
    }
    else if (nr == 17) {            /* sleep(ticks)：S9 进程睡眠（仅用户进程） */
        if (sched_current && sched_current->pid != 0) {
            sched_current->wake_tick = timer_ticks() + r->ebx;
            sched_current->blk_sem = -1;
            sched_current->state = PROC_SLEEPING;
            serial_printf("[sched] pid%d sleep %u (tick=%u wake=%u)\r\n",
                          sched_current->pid, r->ebx, timer_ticks(),
                          sched_current->wake_tick);
            r->eax = 0;
            sched_yield_now(r);     /* 立即让出，不返回 */
        } else {
            r->eax = 0xFFFFFFFFu;   /* shell 不允许睡眠 */
        }
    }
    else if (nr == 18) {            /* sem_create(value)：S9 创建信号量 */
        r->eax = (u32)sem_create((int)r->ebx);
    }
    else if (nr == 19) {            /* sem_wait(id)：S9 P 操作（可阻塞） */
        if (sched_current && sched_current->pid != 0) {
            int rc = sem_wait((int)r->ebx);
            r->eax = (u32)rc;
            if (rc == -1 && sched_current->state == PROC_SLEEPING)
                sched_yield_now(r); /* 阻塞：立即让出，不返回 */
        } else {
            r->eax = 0xFFFFFFFFu;
        }
    }
    else if (nr == 20) {            /* sem_signal(id)：S9 V 操作 */
        r->eax = (u32)sem_signal((int)r->ebx);
    }
    else if (nr == 21) {            /* mutex_create()：S9 互斥锁（=初值 1 的信号量） */
        r->eax = (u32)sem_create(1);
    }
    else if (nr == 22) {            /* mutex_lock(id)：S9 加锁（=P，可阻塞） */
        if (sched_current && sched_current->pid != 0) {
            int rc = sem_wait((int)r->ebx);
            r->eax = (u32)rc;
            if (rc == -1 && sched_current->state == PROC_SLEEPING)
                sched_yield_now(r);
        } else {
            r->eax = 0xFFFFFFFFu;
        }
    }
    else if (nr == 23) {            /* mutex_unlock(id)：S9 解锁（=V） */
        r->eax = (u32)sem_signal((int)r->ebx);
    }
    else {
        r->eax = -1;
    }
}
