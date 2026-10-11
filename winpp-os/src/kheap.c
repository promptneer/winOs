#include "kheap.h"
#include "pmm.h"
#include "serial.h"

/* 固定堆区（物理 = 虚拟，恒等映射已覆盖）：
 * 8MB 起 8MB 大小；pmm 把该区间标记为已用，避免双重分配。 */
#define HEAP_START 0x00800000u
#define HEAP_SIZE  (8u * 1024 * 1024)

#define HDR_ALIGN 8
typedef struct blk_hdr {
    u32 size;      /* 块总大小（含头部） */
    u32 used;
} blk_hdr_t;
#define HDR_SIZE ((u32)sizeof(blk_hdr_t))
#define MIN_BLK  (HDR_SIZE + 16)

static u8 *heap_base = 0;
static u32 heap_bytes = 0;
static u32 heap_used_bytes = 0;

void kheap_init(void)
{
    /* 把固定堆区页标记为已用 */
    u32 p0 = HEAP_START / 0x1000;
    u32 p1 = (HEAP_START + HEAP_SIZE) / 0x1000;
    for (u32 p = p0; p < p1; p++) pmm_mark_used(p);

    heap_base = (u8 *)HEAP_START;
    heap_bytes = HEAP_SIZE;
    blk_hdr_t *h = (blk_hdr_t *)heap_base;
    h->size = HEAP_SIZE;
    h->used = 0;
    heap_used_bytes = HDR_SIZE;
    serial_printf("[kheap] base=%x size=%u KB\r\n", HEAP_START, HEAP_SIZE / 1024);
}

void *kmalloc(u32 size)
{
    if (!size) size = 1;
    size = (size + HDR_ALIGN - 1) & ~(u32)(HDR_ALIGN - 1);

    blk_hdr_t *h = (blk_hdr_t *)heap_base;
    while ((u8 *)h + h->size <= heap_base + heap_bytes) {
        if (!h->used && h->size >= size + HDR_SIZE) {
            /* 分裂：剩余空间够一个新块头 + 最小块 */
            if (h->size >= size + HDR_SIZE + MIN_BLK) {
                blk_hdr_t *nh = (blk_hdr_t *)((u8 *)h + HDR_SIZE + size);
                nh->size = h->size - HDR_SIZE - size;
                nh->used = 0;
                h->size = size + HDR_SIZE;
            }
            h->used = 1;
            heap_used_bytes += h->size;
            return (void *)((u8 *)h + HDR_SIZE);
        }
        h = (blk_hdr_t *)((u8 *)h + h->size);
    }
    serial_puts("[kheap] kmalloc OOM!\r\n");
    return 0;
}

void kfree(void *p)
{
    if (!p) return;
    blk_hdr_t *h = (blk_hdr_t *)((u8 *)p - HDR_SIZE);
    if (h < (blk_hdr_t *)heap_base ||
        (u8 *)h + sizeof(blk_hdr_t) > heap_base + heap_bytes) {
        serial_printf("[kheap] kfree bad ptr %x!\r\n", (u32)p);
        return;
    }
    if (!h->used) { serial_puts("[kheap] double free!\r\n"); return; }
    h->used = 0;
    heap_used_bytes -= h->size;

    /* 向后合并（加边界检查，防垃圾 size 越界读） */
    blk_hdr_t *n = (blk_hdr_t *)((u8 *)h + h->size);
    while ((u8 *)n + sizeof(blk_hdr_t) <= heap_base + heap_bytes &&
           (u8 *)n + n->size <= heap_base + heap_bytes && !n->used) {
        h->size += n->size;
        n = (blk_hdr_t *)((u8 *)h + h->size);
    }
    /* 向前合并：从堆头走链表找前驱；垃圾 size 时 nn>=h 立即停止 */
    blk_hdr_t *prev = (blk_hdr_t *)heap_base;
    while ((u8 *)prev < (u8 *)h) {
        blk_hdr_t *nn = (blk_hdr_t *)((u8 *)prev + prev->size);
        if (nn >= h) break;
        prev = nn;
    }
    if ((u8 *)prev + prev->size == (u8 *)h && !prev->used) {
        prev->size += h->size;
    }
}

u32 kheap_total(void) { return heap_bytes; }
u32 kheap_used(void)  { return heap_used_bytes; }
