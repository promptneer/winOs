#include "tss.h"

/* 单个全局 TSS：多进程切换时只更新 esp0（内核栈），不换 CR3（共享页目录） */
static tss_t tss;

void tss_init(void)
{
    for (u32 *p = (u32 *)&tss; (u8 *)p < (u8 *)&tss + sizeof(tss_t); p++)
        *p = 0;
    tss.ss0   = 0x10;                 /* 内核数据段 */
    tss.iomap = (u16)sizeof(tss_t);   /* 0x68：无 IO 位图 → 用户态 in/out 触发 #GP */
}

void tss_set_esp0(u32 esp0)
{
    tss.esp0 = esp0;
}

tss_t *tss_ptr(void)
{
    return &tss;
}
