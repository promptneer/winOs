#include "demo.h"
#include "vga.h"
#include "keyboard.h"

typedef struct {
    u8 x, y, w, h;
    const char *title;
    const char *lines[4];
    int nlines;
} win_t;

static void draw_window(const win_t *w, int focus)
{
    u8 b = focus ? 0x7F : 0x17;   /* 边框：焦点=亮白/深灰，非焦点=白/蓝 */
    u8 t = focus ? 0x1F : 0x07;   /* 标题：焦点=白/蓝，非焦点=灰/黑 */
    u8 f = focus ? 0x70 : 0x17;   /* 内容 */
    vga_draw_box(w->x, w->y, w->w, w->h, w->title, b, t, f);
    for (int i = 0; i < w->nlines; i++) {
        vga_puts_at((u8)(w->x + 2), (u8)(w->y + 2 + i), w->lines[i], f);
    }
}

static void draw_desktop(const win_t *wins, int n, int focus)
{
    vga_fill(0, 1, VGA_COLS, VGA_ROWS - 1, ' ', 0x17);   /* 桌面底色 */
    for (int i = 0; i < n; i++)
        if (i != focus) draw_window(&wins[i], 0);
    draw_window(&wins[focus], 1);
    /* 底部提示条 */
    vga_fill(0, VGA_ROWS - 1, VGA_COLS, 1, ' ', 0x30);
    vga_puts_at(2, VGA_ROWS - 1, "Tab:switch window   Arrow:move   Esc:back to shell", 0x30);
}

void demo_run(void)
{
    static win_t wins[3];
    wins[0] = (win_t){ 2, 2, 32, 8, "Terminal",
        { "$ ps", "PID TTY TIME CMD", "  1 tty1 0:00 init", "  2 tty1 0:00 shell" }, 4 };
    wins[1] = (win_t){ 36, 5, 34, 9, "Files",
        { "bin/  dev/", "etc/  proc/", "usr/  winpp.ini", "type 'ls' in shell" }, 4 };
    wins[2] = (win_t){ 60, 1, 18, 6, "Settings",
        { "bg: blue", "fg: white", "vga: 80x25", "timer: 100Hz" }, 4 };

    int focus = 0;
    draw_desktop(wins, 3, focus);

    for (;;) {
        int k = keyboard_getkey();
        if (k == KEY_ESC) break;
        if (k == KEY_TAB) { focus = (focus + 1) % 3; }
        if (k == KEY_UP)   { if (wins[focus].y > 1)     wins[focus].y--; }
        if (k == KEY_DOWN) { if (wins[focus].y + wins[focus].h < VGA_ROWS - 1) wins[focus].y++; }
        if (k == KEY_LEFT) { if (wins[focus].x > 1)     wins[focus].x--; }
        if (k == KEY_RIGHT){ if (wins[focus].x + wins[focus].w < VGA_COLS - 1) wins[focus].x++; }
        draw_desktop(wins, 3, focus);
    }
}
