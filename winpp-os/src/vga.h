#ifndef VGA_H
#define VGA_H

#include "io.h"

#define VGA_COLS 80
#define VGA_ROWS 25

enum vga_color {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3,
    RED = 4, MAGENTA = 5, BROWN = 6, LIGHT_GRAY = 7,
    DARK_GRAY = 8, LIGHT_BLUE = 9, LIGHT_GREEN = 10, LIGHT_CYAN = 11,
    LIGHT_RED = 12, LIGHT_MAGENTA = 13, YELLOW = 14, WHITE = 15
};

void vga_clear(void);
void vga_set_attr(u8 attr);
u8   vga_get_attr(void);
void vga_gotoxy(u8 x, u8 y);
void vga_get_pos(u8 *x, u8 *y);
void vga_set_area(u8 top, u8 bottom);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_printf(const char *fmt, ...);
void vga_putc_at(u8 x, u8 y, char c, u8 attr);
void vga_puts_at(u8 x, u8 y, const char *s, u8 attr);
void vga_fill(u8 x, u8 y, u8 w, u8 h, char ch, u8 attr);
void vga_draw_box(u8 x, u8 y, u8 w, u8 h, const char *title,
                  u8 border_attr, u8 title_attr, u8 fill_attr);
void vga_update_cursor(void);

#endif
