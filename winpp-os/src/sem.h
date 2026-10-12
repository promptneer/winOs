#ifndef SEM_H
#define SEM_H

#include "io.h"

/* S9 同步原语：信号量（semaphore）+ 互斥锁（mutex=初值 1 的信号量）。
 * 单 CPU 协作式调度（切换只发生在时钟中断/syscall 让出），P/V 无需关中断。
 * P（sem_wait）：value>0 则减一返回；否则当前进程挂到等待队列（PROC_SLEEPING）
 *               并在 syscall 内立即让出 CPU，直到被 V 唤醒。
 * V（sem_signal）：value 加一，唤醒等待队列队首（FIFO）。
 * 互斥锁：mutex_create()=sem_create(1)；lock=wait；unlock=signal。 */

#define SEM_MAX   8
#define SEM_WAITERS 8

typedef struct {
    u32  id;
    int  value;
    int  inuse;
    u32  waiters[SEM_WAITERS];   /* 等待进程 pid 队列（FIFO） */
    int  nwait;
} sem_t;

void sem_init(void);
int  sem_create(int value);              /* 返回 id（0..SEM_MAX-1）；满返回 -1 */
int  sem_wait(int id);                   /* P：成功立即返回 0；阻塞由调度让出 */
int  sem_signal(int id);                 /* V：返回 0 */
int  sem_peek(int id);                   /* 调试：当前 value */
void sem_dump(void);                     /* 调试：打印全部信号量 */

#endif
