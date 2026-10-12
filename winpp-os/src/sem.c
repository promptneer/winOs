#include "sem.h"
#include "sched.h"
#include "serial.h"
#include "vga.h"

/* S9 同步原语实现。阻塞通过进程状态 PROC_SLEEPING + pcb.blk_sem 表达；
 * 让出由 syscall 处理器调用 sched_tick 完成（见 sched.c syscall_handler）。
 * 进程唤醒：sched.c 提供 sched_wake_by_sem(id, pid)，由 sem_signal 调用。 */

extern int sched_wake_by_sem(int id, u32 pid);

static sem_t sems[SEM_MAX];

void sem_init(void)
{
    for (u32 i = 0; i < SEM_MAX; i++) {
        sems[i].id = i;
        sems[i].value = 0;
        sems[i].inuse = 0;
        sems[i].nwait = 0;
    }
    serial_puts("[11] sem ... 8 slots ready\r\n");
    vga_printf("[11] sem ... 8 slots ready\n");
}

int sem_create(int value)
{
    if (value < 0) value = 0;
    for (u32 i = 0; i < SEM_MAX; i++) {
        if (sems[i].inuse) continue;
        sems[i].inuse = 1;
        sems[i].value = value;
        sems[i].nwait = 0;
        serial_printf("[sem] create sem%d value=%d\r\n", i, value);
        return (int)i;
    }
    return -1;
}

/* P 操作：value>0 减一返回 0；否则把当前进程阻塞到该信号量等待队列
 * （状态 PROC_SLEEPING + blk_sem=id），返回 -1 表示"本次已阻塞"，
 * 由 syscall 处理器立即让出；唤醒后进程从 int 0x80 返回处继续，拿到锁。 */
int sem_wait(int id)
{
    if (id < 0 || (u32)id >= SEM_MAX || !sems[id].inuse) return -1;
    sem_t *s = &sems[id];
    if (s->value > 0) {
        s->value--;
        serial_printf("[sem] wait sem%d pid%d take (v=%d)\r\n",
                      id, sched_current ? sched_current->pid : 0, s->value);
        return 0;
    }
    if (sched_current && sched_current->pid != 0) {
        if (s->nwait < SEM_WAITERS) {
            s->waiters[s->nwait++] = sched_current->pid;
            sched_current->blk_sem = (int)id;
            sched_current->state = PROC_SLEEPING;
            serial_printf("[sem] wait sem%d pid%d block (q=%d)\r\n",
                          id, sched_current->pid, s->nwait);
        } else {
            /* 队列满：进程数<=8 不应发生，保守视为拿到 */
            s->value--;
            return 0;
        }
    }
    return -1;
}

/* V 操作：无等待者时 value 加一；有等待者时"直接交接"——
 * 不增 v（防第三方抢占），唤醒队首进程并让它返回 0（已拿到锁）。 */
int sem_signal(int id)
{
    if (id < 0 || (u32)id >= SEM_MAX || !sems[id].inuse) return -1;
    sem_t *s = &sems[id];
    if (s->nwait > 0) {
        u32 wpid = s->waiters[0];
        for (int k = 1; k < s->nwait; k++) s->waiters[k - 1] = s->waiters[k];
        s->nwait--;
        sched_wake_by_sem(id, wpid);   /* 队首直接拿到锁（返回 0），v 保持不变 */
        serial_printf("[sem] signal sem%d pid%d handoff wake pid%d (q=%d)\r\n",
                      id, sched_current ? sched_current->pid : 0,
                      wpid, s->nwait);
    } else {
        s->value++;
        serial_printf("[sem] signal sem%d pid%d (v=%d) no waiter\r\n",
                      id, sched_current ? sched_current->pid : 0, s->value);
    }
    return 0;
}

int sem_peek(int id)
{
    if (id < 0 || (u32)id >= SEM_MAX || !sems[id].inuse) return -1;
    return sems[id].value;
}

void sem_dump(void)
{
    vga_printf("SEM  VALUE  WAITERS\n");
    for (u32 i = 0; i < SEM_MAX; i++) {
        if (!sems[i].inuse) continue;
        vga_printf("%3u  %5d  %d\n", sems[i].id, sems[i].value, sems[i].nwait);
    }
}
