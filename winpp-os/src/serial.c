#include "serial.h"
#include <stdarg.h>

static int serial_ready = 0;

void serial_init(void)
{
    outb(0x3F8 + 1, 0x00);   /* 关中断 */
    outb(0x3F8 + 3, 0x80);   /* DLAB=1 */
    outb(0x3F8 + 0, 0x03);   /* 115200 */
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);   /* 8N1 */
    outb(0x3F8 + 2, 0xC7);   /* FIFO */
    outb(0x3F8 + 4, 0x0B);   /* IRQ 使能, OUT2 */
    serial_ready = 1;
}

static void serial_wait_tx(void)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(0x3F8 + 5) & 0x20) return;
    }
}

void serial_putc(char c)
{
    if (!serial_ready) return;
    serial_wait_tx();
    outb(0x3F8 + 0, (u8)c);
}

void serial_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
        s++;
    }
}

void serial_printf(const char *fmt, ...)
{
    /* 极简 %s %d %u %x 支持，与 vga_printf 思路一致但只服务调试 */
    va_list ap;
    va_start(ap, fmt);
    char buf[256];
    /* 复用 vga.c 的格式化是不必要的耦合；这里直接手写 */
    int bi = 0;
    for (; *fmt && bi < 250; fmt++) {
        if (*fmt != '%') { buf[bi++] = *fmt; continue; }
        fmt++;
        while (*fmt >= '0' && *fmt <= '9') fmt++;   /* 忽略宽度 */
        switch (*fmt) {
        case 's': {
            const char *p = va_arg(ap, const char *);
            while (*p && bi < 250) buf[bi++] = *p++;
            break;
        }
        case 'd': case 'u': {
            u32 v = va_arg(ap, u32);
            char tmp[12]; int ti = 0;
            if (v == 0) tmp[ti++] = '0';
            while (v) { tmp[ti++] = (char)('0' + v % 10); v /= 10; }
            while (ti) buf[bi++] = tmp[--ti];
            break;
        }
        case 'x': case 'X': {
            u32 v = va_arg(ap, u32);
            char tmp[12]; int ti = 0;
            if (v == 0) tmp[ti++] = '0';
            while (v) { int d = v % 16; tmp[ti++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= 16; }
            while (ti) buf[bi++] = tmp[--ti];
            break;
        }
        case '%': buf[bi++] = '%'; break;
        default: buf[bi++] = *fmt; break;
        }
    }
    buf[bi] = 0;
    va_end(ap);
    serial_puts(buf);
}
