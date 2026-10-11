#ifndef IO_H
#define IO_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed int     s32;
typedef volatile unsigned char  vu8;
typedef volatile unsigned short vu16;

static inline void outb(u16 port, u8 val)  { asm volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline u8   inb(u16 port)           { u8 r; asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static inline void outw(u16 port, u16 val) { asm volatile("outw %0, %1" : : "a"(val), "Nd"(port)); }
static inline u16  inw(u16 port)           { u16 r; asm volatile("inw %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static inline void io_wait(void)           { outb(0x80, 0); }

#endif
