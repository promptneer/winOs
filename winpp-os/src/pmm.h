#ifndef PMM_H
#define PMM_H

#include "io.h"

/* 物理内存管理器：位图跟踪 4KB 物理页 */
void pmm_init(u32 mem_kb);
u32  pmm_alloc_page(void);          /* 返回物理地址；失败返回 0 */
void pmm_free_page(u32 phys);
void pmm_mark_used(u32 page);       /* 把某物理页标记为已占用 */
u32  pmm_total_pages(void);
u32  pmm_used_pages(void);

#endif
