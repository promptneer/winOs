#ifndef GDT_H
#define GDT_H

#include "io.h"

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x18   /* RPL3：实际选择子 0x1B */
#define USER_DS   0x20   /* RPL3：实际选择子 0x23 */
#define TSS_SEL   0x28

void gdt_init(void);
void gdt_set_entry(int i, u32 base, u32 limit, u8 access, u8 gran);

#endif
