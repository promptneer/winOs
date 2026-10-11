#include "gdt.h"
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

static gdt_entry_t gdt[3];
static gdt_ptr_t gdtp;

static void gdt_set(int i, u32 base, u32 limit, u8 access, u8 gran)
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
    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  /* 代码段 0x08 */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  /* 数据段 0x10 */

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
}
