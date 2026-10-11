#include "vga.h"

static vu16 *const VGA = (vu16 *)0xB8000;

static u8 cur_x = 0, cur_y = 0;
static u8 cur_attr = 0x07;          /* 默认：浅灰字黑底 */
static u8 area_top = 1, area_bottom = 24;

static inline u16 entry(u8 c, u8 a) { return (u16)(((u16)a << 8) | c); }

static inline u16 idx(u8 x, u8 y) { return (u16)((u16)y * VGA_COLS + x); }

void vga_set_attr(u8 a)     { cur_attr = a; }
u8   vga_get_attr(void)     { return cur_attr; }
void vga_gotoxy(u8 x, u8 y) { cur_x = x; cur_y = y; }
void vga_get_pos(u8 *x, u8 *y) { *x = cur_x; *y = cur_y; }
void vga_set_area(u8 top, u8 bottom) { if (bottom > top) { area_top = top; area_bottom = bottom; } }

void vga_clear(void)
{
    for (u32 i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA[i] = entry(' ', cur_attr);
    cur_x = 0; cur_y = 0;
}

void vga_scroll_area(u8 top, u8 bottom)
{
    if (bottom <= top) return;
    for (u32 row = top; row < bottom; row++)
        for (u32 col = 0; col < VGA_COLS; col++)
            VGA[idx((u8)col, (u8)row)] = VGA[idx((u8)col, (u8)(row + 1))];
    for (u32 col = 0; col < VGA_COLS; col++)
        VGA[idx((u8)col, bottom)] = entry(' ', cur_attr);
}

void vga_putc(char c)
{
    switch (c) {
    case '\n':
        cur_x = 0; cur_y++;
        break;
    case '\r':
        cur_x = 0;
        break;
    case '\b':
        if (cur_x > 0) {
            cur_x--;
            VGA[idx(cur_x, cur_y)] = entry(' ', cur_attr);
        }
        break;
    case '\t':
        do {
            VGA[idx(cur_x, cur_y)] = entry(' ', cur_attr);
            cur_x++;
        } while (cur_x % 4 != 0 && cur_x < VGA_COLS);
        break;
    default:
        if (cur_x >= VGA_COLS) { cur_x = 0; cur_y++; }
        VGA[idx(cur_x, cur_y)] = entry(c, cur_attr);
        cur_x++;
        break;
    }
    if (cur_y > area_bottom) {
        vga_scroll_area(area_top, area_bottom);
        cur_y = area_bottom;
    }
    vga_update_cursor();
}

void vga_puts(const char *s)
{
    while (*s) vga_putc(*s++);
}

void vga_putc_at(u8 x, u8 y, char c, u8 attr) { VGA[idx(x, y)] = entry(c, attr); }
void vga_puts_at(u8 x, u8 y, const char *s, u8 attr)
{
    while (*s && x < VGA_COLS) { VGA[idx(x, y)] = entry(*s, attr); x++; s++; }
}

void vga_fill(u8 x, u8 y, u8 w, u8 h, char ch, u8 attr)
{
    for (u8 yy = y; yy < y + h && yy < VGA_ROWS; yy++)
        for (u8 xx = x; xx < x + w && xx < VGA_COLS; xx++)
            VGA[idx(xx, yy)] = entry(ch, attr);
}

void vga_draw_box(u8 x, u8 y, u8 w, u8 h, const char *title,
                  u8 border_attr, u8 title_attr, u8 fill_attr)
{
    if (w < 3 || h < 2) return;
    if (x + w > VGA_COLS) w = (u8)(VGA_COLS - x);
    if (y + h > VGA_ROWS) h = (u8)(VGA_ROWS - y);
    /* 边框 */
    vga_putc_at(x, y, 0xDA, border_attr);            /* ┌ */
    vga_putc_at(x + w - 1, y, 0xBF, border_attr);    /* ┐ */
    vga_putc_at(x, y + h - 1, 0xC0, border_attr);    /* └ */
    vga_putc_at(x + w - 1, y + h - 1, 0xD9, border_attr); /* ┘ */
    for (u8 i = 1; i < w - 1; i++) {
        vga_putc_at(x + i, y, 0xC4, border_attr);        /* ─ */
        vga_putc_at(x + i, y + h - 1, 0xC4, border_attr);
    }
    for (u8 j = 1; j < h - 1; j++) {
        vga_putc_at(x, y + j, 0xB3, border_attr);        /* │ */
        vga_putc_at(x + w - 1, y + j, 0xB3, border_attr);
    }
    /* 标题栏 */
    vga_fill(x + 1, y, w - 2, 1, ' ', title_attr);
    u8 tw = (u8)0;
    const char *t = title;
    while (*t && tw < w - 3) { t++; tw++; }
    vga_puts_at(x + 1 + (w - 2 - tw) / 2, y, title, title_attr);
    /* 内容区 */
    vga_fill(x + 1, y + 1, w - 2, h - 2, ' ', fill_attr);
}

void vga_update_cursor(void)
{
    u16 pos = (u16)(cur_y * VGA_COLS + cur_x);
    outb(0x3D4, 14);
    outb(0x3D5, (u8)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (u8)(pos & 0xFF));
}

/* ---------- 极简 printf：%s %c %d %i %u %x %X %p %%，支持左补零/宽度 ---------- */

void vga_printf(const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { vga_putc(*p); continue; }
        p++;
        int zpad = 0, width = 0;
        if (*p == '0') { zpad = 1; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        char spec = *p;
        char buf[40];
        int bi = 0, neg = 0;
        unsigned long uv;

        switch (spec) {
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) vga_putc(*s++);
            break;
        }
        case 'c':
            vga_putc((char)__builtin_va_arg(ap, int));
            break;
        case 'd':
        case 'i': {
            long v = __builtin_va_arg(ap, int);
            if (v < 0) { neg = 1; uv = (unsigned long)(-v); }
            else uv = (unsigned long)v;
            if (uv == 0) buf[bi++] = '0';
            while (uv > 0) { buf[bi++] = (char)('0' + (uv % 10)); uv /= 10; }
            if (neg) buf[bi++] = '-';
            break;
        }
        case 'u': {
            uv = __builtin_va_arg(ap, unsigned int);
            if (uv == 0) buf[bi++] = '0';
            while (uv > 0) { buf[bi++] = (char)('0' + (uv % 10)); uv /= 10; }
            break;
        }
        case 'x':
        case 'X': {
            uv = __builtin_va_arg(ap, unsigned int);
            if (uv == 0) buf[bi++] = '0';
            while (uv > 0) {
                int d = (int)(uv % 16);
                buf[bi++] = (char)(d < 10 ? '0' + d : (spec == 'X' ? 'A' + d - 10 : 'a' + d - 10));
                uv /= 16;
            }
            break;
        }
        case 'p': {
            uv = (unsigned long)__builtin_va_arg(ap, void *);
            if (uv == 0) buf[bi++] = '0';
            while (uv > 0) {
                int d = (int)(uv % 16);
                buf[bi++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
                uv /= 16;
            }
            buf[bi++] = 'x'; buf[bi++] = '0';
            break;
        }
        case '%':
            vga_putc('%');
            break;
        default:
            vga_putc('%');
            if (*p) vga_putc(*p);
            break;
        }
        /* 左补零（在缓冲区里补，避免任何歧义循环） */
        if (zpad && !neg && bi < width)
            while (bi < width && bi < (int)sizeof(buf)) buf[bi++] = '0';
        while (bi > 0) vga_putc(buf[--bi]);
    }
    __builtin_va_end(ap);
}
