#include "gdt.h"
#include "tss.h"
#include "io.h"

typedef struct {
    u16 limit_lo;
    u16 base_lo;
    u8  base_mid;
    u8  access;
    u8  gran;
    u8  base_hi;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[6];   /* 0:null 1:内核代码 2:内核数据 3:用户代码 4:用户数据 5:TSS */
static gdt_ptr_t gdtp;

void gdt_set_entry(int i, u32 base, u32 limit, u8 access, u8 gran)
{
    gdt[i].limit_lo = (u16)(limit & 0xFFFF);
    gdt[i].base_lo  = (u16)(base & 0xFFFF);
    gdt[i].base_mid = (u8)((base >> 16) & 0xFF);
    gdt[i].access   = access;
    gdt[i].gran     = (u8)(gran | ((limit >> 16) & 0x0F));
    gdt[i].base_hi  = (u8)((base >> 24) & 0xFF);
}

void gdt_init(void)
{
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);   /* 内核代码段 0x08 */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);   /* 内核数据段 0x10 */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);   /* 用户代码段 0x18 DPL3 */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);   /* 用户数据段 0x20 DPL3 */
    gdt_set_entry(5, (u32)tss_ptr(), sizeof(tss_t) - 1, 0x89, 0x00);  /* TSS 0x28 */

    gdtp.limit = (u16)(sizeof(gdt) - 1);
    gdtp.base  = (u32)&gdt;

    asm volatile("lgdt %0" : : "m"(gdtp));
    asm volatile("ljmp $0x08, $1f\n1:" ::: "memory");
    asm volatile(
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        ::: "ax", "memory");
    /* 加载任务寄存器（此后中断可从用户态经 TSS.esp0 切到内核栈） */
    asm volatile("ltr %0" : : "r"((u16)TSS_SEL));
}
