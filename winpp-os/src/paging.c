#include "paging.h"
#include "pmm.h"
#include "serial.h"

#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PD_SELF_INDEX 1023
#define RECURSIVE_BASE 0xFFC00000u

static u32 pd_phys = 0;
static u32 page_faults = 0;

u32 read_cr3(void)
{
    u32 v;
    asm volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

int paging_enabled(void)
{
    u32 v;
    asm volatile("mov %%cr0, %0" : "=r"(v));
    return (v & 0x80000000u) != 0;
}

u32 pf_count(void)      { return page_faults; }
void pf_inc(void)       { page_faults++; }

/* 通过递归自映射访问 PD / PT 表项。
 * 自映射：PD[1023] = PD 自身。
 *  - 读 PD[i]：地址 0xFFFFF000 + i*4（PD 索引 1023、PT 索引 1023 -> PT[1023]=PD 页）
 *  - 读 PT[j][i]：地址 0xFFC00000 + j*0x1000 + i*4（PD 索引 1023、PT 索引 j）
 * 注意：0xFFC00000 + i*4 会翻译到 PT0 页（PT 索引 0），不是 PD 页 —— 这是早期 769 的根因。 */
static volatile u32 *pd_entry(u32 idx)
{
    return (volatile u32 *)(0xFFFFF000u + idx * 4);
}
static volatile u32 *pt_entry(u32 pd_idx, u32 pt_idx)
{
    return (volatile u32 *)(RECURSIVE_BASE + pd_idx * 0x1000 + pt_idx * 4);
}

u32 pd_used_entries(void)
{
    u32 n = 0;
    if (!paging_enabled()) return 0;
    for (u32 i = 0; i < 1024; i++) {
        if (*pd_entry(i) & PAGE_PRESENT) n++;
    }
    return n;
}

void paging_init(u32 mem_kb)
{
    serial_puts("[pg] step1 pd alloc\r\n");
    /* 1) 分配并清零 PD（此时未开分页，直接物理写） */
    pd_phys = pmm_alloc_page();
    if (!pd_phys) { serial_puts("[paging] no mem for PD!\r\n"); return; }
    for (u32 i = 0; i < 1024; i++) ((volatile u32 *)pd_phys)[i] = 0;
    serial_puts("[pg] step2 identity map\r\n");

    /* 2) 恒等映射 [1MB, mem_kb)，每 4MB 一个页表。
     *    v 按虚拟 4MB 边界步进：pte[j] 的物理地址必须等于虚拟地址 va，
     *    否则虚拟 1MB（内核代码）会被翻译到错误物理页 -> 开分页瞬间 crash。 */
    u32 top = mem_kb * 1024;
    for (u32 v = 0; v < top; v += 0x400000) {
        u32 pt = pmm_alloc_page();
        if (!pt) { serial_puts("[paging] no mem for PT!\r\n"); break; }
        for (u32 i = 0; i < 1024; i++) ((volatile u32 *)pt)[i] = 0;
        u32 pd_idx = PD_INDEX(v);
        ((volatile u32 *)pd_phys)[pd_idx] = pt | PAGE_PRESENT | PAGE_WRITE;
        volatile u32 *pte = (volatile u32 *)pt;
        for (u32 j = 0; j < 1024; j++) {
            u32 va = v + j * 0x1000;
            if (va >= 0x100000 && va < top) pte[j] = va | PAGE_PRESENT | PAGE_WRITE;
        }
    }
    serial_puts("[pg] step3 vga pt\r\n");

    /* 3) VGA 文本窗口 0xB8000-0xBFFFF（32KB，8 页）：文本缓冲 + plane 2 的 8KB 字体。
     *    S10 的 gui 退出时要存/装字体（0xB8000-0xB9FFF），只映射一页会在 0xB9000 #PF。
     *    低 1MB 其余保持不映射 -> 留给 #PF 演示（pfault 打地址 0）。 */
    u32 pd0_pt = ((volatile u32 *)pd_phys)[0] & 0xFFFFF000u;
    for (u32 pg = 0; pg < 8; pg++) {
        ((volatile u32 *)pd0_pt)[0xB8 + pg] = (0xB8000 + pg * 0x1000) | PAGE_PRESENT | PAGE_WRITE;
    }
    /* 3b) S10 VGA 图形帧缓冲 0xA0000-0xAFFFF（Mode13h 320x200 = 64KB，16 页）。
     *     补进同一恒等页表。 */
    for (u32 pg = 0; pg < 16; pg++) {
        ((volatile u32 *)pd0_pt)[0xA0 + pg] = (0xA0000 + pg * 0x1000) | PAGE_PRESENT | PAGE_WRITE;
    }
    serial_puts("[pg] step4 selfmap\r\n");

    /* 4) 自映射：PD[1023] = PD 自身 */
    ((volatile u32 *)pd_phys)[PD_SELF_INDEX] = pd_phys | PAGE_PRESENT | PAGE_WRITE;
    serial_puts("[pg] step5 cr3\r\n");

    /* 5) 载入 CR3 -> 开 CR0.PG（当前代码/栈/IDT 均在 1MB+，已映射） */
    asm volatile("mov %0, %%cr3" :: "r"(pd_phys));
    serial_puts("[pg] step6 cr0\r\n");
    u32 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
    serial_puts("[pg] step7 after cr0\r\n");

    serial_printf("[paging] ON cr3=%x identity=1MB..%uKB vga+selfmap ok\r\n",
                  pd_phys, mem_kb);
}

void map_page(u32 vaddr, u32 paddr, u32 flags)
{
    u32 pd_idx = PD_INDEX(vaddr), pt_idx = PT_INDEX(vaddr);
    u32 pdv = *pd_entry(pd_idx);
    if (!(pdv & PAGE_PRESENT)) {
        u32 pt = pmm_alloc_page();
        if (!pt) { serial_printf("[paging] map fail @%x\r\n", vaddr); return; }
        volatile u32 *pt_v = (volatile u32 *)(RECURSIVE_BASE + pd_idx * 0x1000);
        for (u32 i = 0; i < 1024; i++) pt_v[i] = 0;
        *pd_entry(pd_idx) = pt | PAGE_PRESENT | PAGE_WRITE;
    }
    /* 恒等映射的 PDE 只有 P|W（无 USER 位）：用户态访问 4MB+ 恒等区会因 PDE 无 U 报 err=5。
     * 用户映射请求 U 位时，同步给 PDE 补 USER 位（PTE 各自权限仍由 PTE 决定，隔离不破坏）。 */
    if (flags & 0x4) {
        u32 pdv2 = *pd_entry(pd_idx);
        if (!(pdv2 & 0x4))
            *pd_entry(pd_idx) = pdv2 | 0x4;
    }
    *pt_entry(pd_idx, pt_idx) = (paddr & 0xFFFFF000u) | flags | PAGE_PRESENT;
    /* 全量 TLB 刷新：PDE/PTE 均可能变更，重载 CR3 保证旧翻译条目全部失效 */
    u32 cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" :: "r"(cr3));
}

void unmap_page(u32 vaddr)
{
    u32 pd_idx = PD_INDEX(vaddr), pt_idx = PT_INDEX(vaddr);
    if (!(*pd_entry(pd_idx) & PAGE_PRESENT)) return;
    *pt_entry(pd_idx, pt_idx) = 0;
    asm volatile("invlpg (%0)" :: "r"(vaddr));
}
