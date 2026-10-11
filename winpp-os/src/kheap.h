#ifndef KHEAP_H
#define KHEAP_H

#include "io.h"

/* 内核堆：first-fit 边界标签分配器，建立在页级映射之上 */
void   kheap_init(void);
void  *kmalloc(u32 size);
void   kfree(void *p);
u32    kheap_total(void);   /* 堆总大小（字节） */
u32    kheap_used(void);    /* 已分配字节（含块头） */

#endif
