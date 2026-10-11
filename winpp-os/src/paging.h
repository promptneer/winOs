#ifndef PAGING_H
#define PAGING_H

#include "io.h"

#define PAGE_SIZE 0x1000
#define PD_INDEX(v) ((u32)(v) >> 22)
#define PT_INDEX(v) (((u32)(v) >> 12) & 0x3FF)

/* 分页：4KB 二级页表 + 递归自映射。
 * 布局：低 1MB 只映射 VGA 0xB8000 一页，其余不映射（NULL 访问触发 #PF）；
 *       1MB 到 mem_kb 恒等映射（内核/代码/栈所在）；
 *       PD[1023] 自映射 -> 递归地址 0xFFC00000。
 */
void paging_init(u32 mem_kb);
void map_page(u32 vaddr, u32 paddr, u32 flags);
void unmap_page(u32 vaddr);
u32  read_cr3(void);
int  paging_enabled(void);
u32  pf_count(void);
void pf_inc(void);
u32  pd_used_entries(void);

#endif
