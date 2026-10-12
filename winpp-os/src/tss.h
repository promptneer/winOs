#ifndef TSS_H
#define TSS_H

#include "io.h"

/* i386 任务状态段（仅用内核栈切换 + 无 IO 位图） */
typedef struct {
    u32 prev;          /* 0x00 前一个 TSS（链） */
    u32 esp0;          /* 0x04 特权级 0 栈指针（中断从用户态进入时用） */
    u32 ss0;           /* 0x08 特权级 0 栈段 */
    u32 esp1, ss1;     /* 0x0C/0x10 特权级 1（未用） */
    u32 esp2, ss2;     /* 0x14/0x18 特权级 2（未用） */
    u32 cr3;           /* 0x1C 页目录物理地址 */
    u32 eip;           /* 0x20 */
    u32 eflags;        /* 0x24 */
    u32 eax, ecx, edx, ebx;  /* 0x28..0x34 */
    u32 esp, ebp, esi, edi;  /* 0x38..0x44 */
    u32 es, cs, ss, ds, fs, gs;  /* 0x48..0x5C 段寄存器 */
    u32 ldt;           /* 0x60 LDT 选择子 */
    u16 trap;          /* 0x64 调试陷阱 */
    u16 iomap;         /* 0x66 IO 位图基址（= sizeof 则无位图 → 用户 IO 全 #GP） */
} __attribute__((packed)) tss_t;

void tss_init(void);
void tss_set_esp0(u32 esp0);
tss_t *tss_ptr(void);

#endif
