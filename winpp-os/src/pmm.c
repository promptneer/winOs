#include "pmm.h"
#include "serial.h"

/* 位图：支持到 256MB（65536 页）。Bochs 64MB 只用到 16384 页。 */
#define PMM_MAX_PAGES (256u * 1024 * 1024 / 4096)
static u32 bitmap[PMM_MAX_PAGES / 32];

static u32 total_pages = 0;
static u32 used_pages  = 0;

/* 链接器符号：内核 .bss 末尾（低 1MB 以上） */
extern u8 _kernel_end[];

void pmm_init(u32 mem_kb)
{
    total_pages = mem_kb / 4;
    used_pages = 0;
    for (u32 i = 0; i < total_pages / 32 + 1; i++) bitmap[i] = 0;

    /* 保留：低 1MB（0..255 页）+ 内核占用（1MB 起到 _kernel_end） */
    u32 kernel_end_page = ((u32)_kernel_end + 0xFFF) / 0x1000;
    u32 keep = kernel_end_page;
    if (keep < 256) keep = 256;   /* 低 1MB 整段保留 */
    for (u32 p = 0; p < keep && p < total_pages; p++) {
        bitmap[p / 32] |= (1u << (p % 32));
        used_pages++;
    }
    serial_printf("[pmm] total=%u pages (%u KB) kernel_end=%x used=%u\r\n",
                  total_pages, mem_kb, (u32)_kernel_end, used_pages);
}

u32 pmm_alloc_page(void)
{
    for (u32 i = 0; i < total_pages; i++) {
        if (!(bitmap[i / 32] & (1u << (i % 32)))) {
            bitmap[i / 32] |= (1u << (i % 32));
            used_pages++;
            return i * 0x1000;
        }
    }
    serial_puts("[pmm] alloc failed: out of memory!\r\n");
    return 0;
}

void pmm_free_page(u32 phys)
{
    u32 p = phys / 0x1000;
    if (p >= total_pages) return;
    if (!(bitmap[p / 32] & (1u << (p % 32)))) {
        serial_printf("[pmm] double free %x!\r\n", phys);
        return;
    }
    bitmap[p / 32] &= ~(1u << (p % 32));
    used_pages--;
}

void pmm_mark_used(u32 page)
{
    if (page >= total_pages) return;
    if (!(bitmap[page / 32] & (1u << (page % 32)))) {
        bitmap[page / 32] |= (1u << (page % 32));
        used_pages++;
    }
}

u32 pmm_total_pages(void) { return total_pages; }
u32 pmm_used_pages(void)  { return used_pages; }
