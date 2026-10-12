#include "idt.h"
#include "io.h"

typedef struct {
    u16 lo;      /* 偏移低 16 位 */
    u16 sel;     /* 段选择子 */
    u8  zero;    /* 恒 0 */
    u8  flags;   /* P/DPL/type */
    u16 hi;      /* 偏移高 16 位 */
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idtp;

/* 32 个 CPU 异常存根 */
#define DECL_ISR(n) extern void isr##n(void);
DECL_ISR(0)  DECL_ISR(1)  DECL_ISR(2)  DECL_ISR(3)
DECL_ISR(4)  DECL_ISR(5)  DECL_ISR(6)  DECL_ISR(7)
DECL_ISR(8)  DECL_ISR(9)  DECL_ISR(10) DECL_ISR(11)
DECL_ISR(12) DECL_ISR(13) DECL_ISR(14) DECL_ISR(15)
DECL_ISR(16) DECL_ISR(17) DECL_ISR(18) DECL_ISR(19)
DECL_ISR(20) DECL_ISR(21) DECL_ISR(22) DECL_ISR(23)
DECL_ISR(24) DECL_ISR(25) DECL_ISR(26) DECL_ISR(27)
DECL_ISR(28) DECL_ISR(29) DECL_ISR(30) DECL_ISR(31)

/* 16 个 IRQ 存根 */
#define DECL_IRQ(n) extern void irq##n(void);
DECL_IRQ(0)  DECL_IRQ(1)  DECL_IRQ(2)  DECL_IRQ(3)
DECL_IRQ(4)  DECL_IRQ(5)  DECL_IRQ(6)  DECL_IRQ(7)
DECL_IRQ(8)  DECL_IRQ(9)  DECL_IRQ(10) DECL_IRQ(11)
DECL_IRQ(12) DECL_IRQ(13) DECL_IRQ(14) DECL_IRQ(15)

/* 系统调用存根 */
extern void syscall_isr(void);

static void idt_set_gate(u8 n, u32 base, u16 sel, u8 flags)
{
    idt[n].lo    = (u16)(base & 0xFFFF);
    idt[n].hi    = (u16)((base >> 16) & 0xFFFF);
    idt[n].sel   = sel;
    idt[n].zero  = 0;
    idt[n].flags = flags;
}

void pic_remap(void)
{
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();   /* 主片 IRQ0-7 -> 0x20-0x27 */
    outb(0xA1, 0x28); io_wait();   /* 从片 IRQ8-15 -> 0x28-0x2F */
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();
    outb(0x21, 0x00); io_wait();   /* 全部开中断 */
    outb(0xA1, 0x00); io_wait();
}

void idt_init(void)
{
    idtp.limit = (u16)(sizeof(idt) - 1);
    idtp.base  = (u32)&idt;

    for (int i = 0; i < 256; i++)
        idt_set_gate((u8)i, 0, 0x08, 0x8E);

#define SET_ISR(n) idt_set_gate((u8)n, (u32)isr##n, 0x08, 0x8E);
    SET_ISR(0)  SET_ISR(1)  SET_ISR(2)  SET_ISR(3)
    SET_ISR(4)  SET_ISR(5)  SET_ISR(6)  SET_ISR(7)
    SET_ISR(8)  SET_ISR(9)  SET_ISR(10) SET_ISR(11)
    SET_ISR(12) SET_ISR(13) SET_ISR(14) SET_ISR(15)
    SET_ISR(16) SET_ISR(17) SET_ISR(18) SET_ISR(19)
    SET_ISR(20) SET_ISR(21) SET_ISR(22) SET_ISR(23)
    SET_ISR(24) SET_ISR(25) SET_ISR(26) SET_ISR(27)
    SET_ISR(28) SET_ISR(29) SET_ISR(30) SET_ISR(31)

#define SET_IRQ(n) idt_set_gate((u8)(32 + n), (u32)irq##n, 0x08, 0x8E);
    SET_IRQ(0)  SET_IRQ(1)  SET_IRQ(2)  SET_IRQ(3)
    SET_IRQ(4)  SET_IRQ(5)  SET_IRQ(6)  SET_IRQ(7)
    SET_IRQ(8)  SET_IRQ(9)  SET_IRQ(10) SET_IRQ(11)
    SET_IRQ(12) SET_IRQ(13) SET_IRQ(14) SET_IRQ(15)

    /* 系统调用门 int 0x80：P=1 DPL=3（用户态可触发） interrupt gate */
    idt_set_gate(0x80, (u32)syscall_isr, 0x08, 0xEE);

    asm volatile("lidt %0" : : "m"(idtp));
}
