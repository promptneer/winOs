#include "gui.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "rtc.h"
#include "serial.h"
#include "pmm.h"
#include "mouse.h"
#include "sched.h"

extern void kernel_redraw_shell(void);   /* kernel.c：回文本模式后重绘 shell */

/* ============ S10 图形界面窗口系统 ============
 * 1) VGA Mode 13h 切换（寄存器编程，320x200x256，帧缓冲 0xA0000）
 * 2) 帧缓冲绘制原语（fill/rect/pixel）
 * 3) 内置 5x7 点阵字体（A-Z 0-9 及常用符号）
 * 4) 窗口管理器：多窗口、标题栏、激活/关闭/新建、内容窗口（时钟/系统信息/动画）
 * 5) 键盘 GUI 交互 + PS/2 鼠标光标（设备表 major=4 irq=12）
 * shell 命令 gui 进入；'q' 退出回文本模式。 */

/* ---------------- VGA Mode 13h / Mode 03 ---------------- */

static void vga_set_mode13h(void)
{
    /* 关闭输入 + 标准 320x200x256 编程序列（OSDev VGA 参考） */
    outb(0x3C2, 0x63);                    /* Misc Output */
    outb(0x3C4, 0x01); outb(0x3C5, 0x01); /* SR1 */
    outb(0x3C4, 0x04); outb(0x3C5, 0x06); /* SR4: 0xA0000 */
    outb(0x3CE, 0x05); outb(0x3CF, 0x40); /* GC5: read map 0 */
    outb(0x3CE, 0x06); outb(0x3CF, 0x05); /* GC6: 图形模式 */
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F); /* SR2: 全平面 */
    outb(0x3D4, 0x14); outb(0x3D5, 0x00); /* CRTC: 解除保护 */

    static const u8 crtc[32] = {
        0x5F,0x3F,0x3F,0x00,0x41,0x9C,0x0E,0x8F,
        0x28,0x40,0x9C,0x8F,0x28,0x00,0x3F,0xA3,
        0x9C,0x8F,0x28,0x40,0x9C,0x0E,0x8F,0x28,
        0x1F,0x03,0x0D,0x0B,0x0E
    };
    for (u8 i = 0; i < 29; i++) {
        outb(0x3D4, i); outb(0x3D5, crtc[i]);
    }
    /* 回读诊断：确认 VGA 寄存器写入生效（Mode13h: SR4=06, GC6=05, CR14=00） */
    outb(0x3C4, 0x04); u8 vsr4 = inb(0x3C5);
    outb(0x3CE, 0x06); u8 vgc6 = inb(0x3CF);
    outb(0x3D4, 0x14); u8 vcr14 = inb(0x3D5);
    serial_printf("[gui] vga verify SR4=%02x GC6=%02x CR14=%02x\r\n",
                  (unsigned)vsr4, (unsigned)vgc6, (unsigned)vcr14);
}

static void vga_set_mode03(void)
{
    /* 回 80x25 文本模式 */
    outb(0x3C2, 0x63);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01); /* SR1 */
    outb(0x3C4, 0x04); outb(0x3C5, 0x02); /* SR4: 文本 */
    outb(0x3CE, 0x06); outb(0x3CF, 0x04); /* GC6: 文本 */
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3D4, 0x14); outb(0x3D5, 0x00); /* 解除保护 */

    static const u8 crtc[32] = {
        0x5F,0x3F,0x3F,0x00,0x41,0x9C,0x0E,0x8F,
        0x28,0x0F,0x9C,0x8F,0x28,0x00,0x3F,0xA3,
        0x9C,0x8F,0x28,0x1F,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00
    };
    for (u8 i = 0; i < 29; i++) {
        outb(0x3D4, i); outb(0x3D5, crtc[i]);
    }
}

/* ---------------- 帧缓冲绘制 ---------------- */

#define FB ((volatile u8 *)0xA0000)

static void gui_pixel(int x, int y, u8 c)
{
    if (x < 0 || x >= GUI_W || y < 0 || y >= GUI_H) return;
    FB[y * GUI_W + x] = c;
}

static void gui_fill(int x, int y, int w, int h, u8 c)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > GUI_W) w = GUI_W - x;
    if (y + h > GUI_H) h = GUI_H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            FB[(y + j) * GUI_W + (x + i)] = c;
}

static void gui_rect(int x, int y, int w, int h, u8 c)
{
    gui_fill(x, y, w, 1, c);
    gui_fill(x, y + h - 1, w, 1, c);
    gui_fill(x, y, 1, h, c);
    gui_fill(x + w - 1, y, 1, h, c);
}

/* ---------------- 5x7 点阵字体（row-major，bit4-0） ---------------- */

static const u8 font5x7[][7] = {
    /* 0-9 */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},  /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},  /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},  /* 2 */
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},  /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},  /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},  /* 5 */
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},  /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},  /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},  /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E},  /* 9 */
    /* A-Z */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},  /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},  /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},  /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},  /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},  /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},  /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},  /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},  /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},  /* I */
    {0x01,0x01,0x01,0x01,0x01,0x11,0x0E},  /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},  /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},  /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},  /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},  /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},  /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},  /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},  /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},  /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},  /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},  /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},  /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},  /* V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},  /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},  /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},  /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},  /* Z */
    /* 符号 */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},  /* . */
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},  /* : */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},  /* - */
    {0x01,0x02,0x04,0x08,0x10,0x10,0x00},  /* / */
    {0x19,0x19,0x02,0x04,0x08,0x13,0x13},  /* % */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02},  /* ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08},  /* ) */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},  /* = */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},  /* + */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00},  /* space(45) */
};

/* 字符 -> 字体索引：0-9=A(10)... 具体查表 */
static int font_index(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    switch (c) {
    case '.': return 36;
    case ':': return 37;
    case '-': return 38;
    case '/': return 39;
    case '%': return 40;
    case '(': return 41;
    case ')': return 42;
    case '=': return 43;
    case '+': return 44;
    case ' ': return 45;
    default:  return 45;   /* 未知字符显示为空格 */
    }
}

static void gui_char(int x, int y, u8 c, u8 color)
{
    int idx = font_index(c);
    for (int row = 0; row < 7; row++) {
        u8 bits = font5x7[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col))
                gui_pixel(x + col, y + row, color);
        }
    }
}

static void gui_text(int x, int y, u8 color, const char *s)
{
    while (*s) {
        gui_char(x, y, *s, color);
        x += 6;
        s++;
    }
}

/* ---------------- 窗口管理器 ---------------- */

#define WIN_MAX 6
#define TITLE_H 9

enum { WT_CLOCK = 0, WT_SYSINFO, WT_DEMO, WT_LOGO };

typedef struct {
    int x, y, w, h;
    int visible;
    u8 bg;
    int type;
    char title[16];
} gw_t;

static gw_t wins[WIN_MAX];
static int nwins = 0;
static int active = -1;

/* 状态栏反馈：激活窗口 + 窗口数 + 最近动作（让按键"看得见"） */
static char status_txt[48];
static char action_txt[20];

static void status_update(void)
{
    int i = 0;
    const char *pre = "ACT:";
    const char *a = (active >= 0 && wins[active].visible) ? wins[active].title : "NONE";
    while (*pre && i < 44) status_txt[i++] = *pre++;
    while (*a && i < 44) status_txt[i++] = *a++;
    while (i < 12) status_txt[i++] = ' ';
    const char *w = "WINS:";
    while (*w && i < 44) status_txt[i++] = *w++;
    status_txt[i++] = (char)('0' + nwins / 10);
    status_txt[i++] = (char)('0' + nwins % 10);
    status_txt[i++] = '/';
    status_txt[i++] = (char)('0' + WIN_MAX / 10);
    status_txt[i++] = (char)('0' + WIN_MAX % 10);
    status_txt[i] = 0;
    /* 动作文字清零后拼到状态栏 */
    if (action_txt[0]) {
        while (i < 30) status_txt[i++] = ' ';
        int j = 0;
        while (action_txt[j] && i < 47) status_txt[i++] = action_txt[j++];
        status_txt[i] = 0;
    }
}

static void status_act(const char *a)
{
    int i = 0;
    while (a[i] && i < 19) { action_txt[i] = a[i]; i++; }
    action_txt[i] = 0;
    status_update();
}

int gui_active = 0;

static int gw_add(int x, int y, int w, int h, u8 bg, int type, const char *title)
{
    if (nwins >= WIN_MAX) return -1;
    gw_t *g = &wins[nwins];
    g->x = x; g->y = y; g->w = w; g->h = h;
    g->visible = 1;
    g->bg = bg;
    g->type = type;
    for (int i = 0; i < 15 && title[i]; i++) g->title[i] = title[i];
    g->title[15] = 0;
    active = nwins;
    nwins++;
    return nwins - 1;
}

static void gw_draw(gw_t *g, u32 tick)
{
    /* 窗口外框：激活=亮黄(14)，非激活=灰(7) */
    u8 frame = (g == &wins[active]) ? 14 : 7;
    gui_rect(g->x, g->y, g->w, g->h, frame);
    /* 激活窗口加白色内框线（更醒目） */
    if (g == &wins[active])
        gui_rect(g->x + 1, g->y + 1, g->w - 2, g->h - 2, 15);
    /* 标题栏：激活=黄底黑字 + '>' 标记，非激活=灰底白字 */
    gui_fill(g->x + 1, g->y + 1, g->w - 2, TITLE_H, (g == &wins[active]) ? 14 : 8);
    if (g == &wins[active]) {
        gui_char(g->x + 3, g->y + 2, '>', 0);
        gui_text(g->x + 8, g->y + 2, 0, g->title);
    } else {
        gui_text(g->x + 4, g->y + 2, 15, g->title);
    }
    /* 客户区背景 */
    gui_fill(g->x + 1, g->y + 1 + TITLE_H, g->w - 2, g->h - 2 - TITLE_H, g->bg);

    int cx = g->x + 4, cy = g->y + TITLE_H + 4;
    char buf[48];

    switch (g->type) {
    case WT_CLOCK: {
        u16 yr; u8 mo, da, hh, mi, ss;
        rtc_get_datetime(&yr, &mo, &da, &hh, &mi, &ss);
        buf[0] = (char)('0' + hh / 10); buf[1] = (char)('0' + hh % 10);
        buf[2] = ':';
        buf[3] = (char)('0' + mi / 10); buf[4] = (char)('0' + mi % 10);
        buf[5] = ':';
        buf[6] = (char)('0' + ss / 10); buf[7] = (char)('0' + ss % 10);
        buf[8] = 0;
        gui_text(cx, cy, 11, buf);
        gui_text(cx, cy + 9, 3, "REAL-TIME");
        break;
    }
    case WT_SYSINFO: {
        gui_text(cx, cy, 15, "CPU GENUINEINTEL");
        gui_text(cx, cy + 9, 7, "RAM KB MEM OK");
        /* 格式化数字 */
        u32 up = timer_uptime_sec();
        buf[0] = (char)('0' + (up / 3600) % 10);
        buf[1] = ':';
        buf[2] = (char)('0' + ((up / 60) % 60) / 10);
        buf[3] = (char)('0' + ((up / 60) % 60) % 10);
        buf[4] = ':';
        buf[5] = (char)('0' + (up % 60) / 10);
        buf[6] = (char)('0' + (up % 60) % 10);
        buf[7] = 0;
        gui_text(cx, cy + 18, 11, "UP ");
        gui_text(cx + 16, cy + 18, 11, buf);
        gui_text(cx, cy + 27, 7, "TICKS ");
        u32 t = timer_ticks();
        buf[0] = (char)('0' + (t / 1000) % 10);
        buf[1] = (char)('0' + (t / 100) % 10);
        buf[2] = (char)('0' + (t / 10) % 10);
        buf[3] = (char)('0' + t % 10);
        buf[4] = 0;
        gui_text(cx + 34, cy + 27, 11, buf);
        break;
    }
    case WT_DEMO: {
        /* 进度条动画 */
        u32 p = (tick / 4) % (g->w - 20);
        gui_rect(cx, cy, g->w - 14, 8, 7);
        gui_fill(cx + 1, cy + 1, (int)p, 6, 10);
        gui_text(cx, cy + 10, 14, "LOADING DEMO");
        /* 滚动方块 */
        int bx = (int)(tick / 2) % (g->w - 14);
        gui_fill(cx + bx, cy + 24, 6, 6, 13);
        gui_text(cx, cy + 33, 7, "WINDOW OK");
        break;
    }
    case WT_LOGO:
    default: {
        gui_text(cx, cy, 14, "WINPP GUI");
        gui_text(cx, cy + 9, 11, "I386 KERNEL");
        gui_text(cx, cy + 18, 3, "STAGE 10");
        break;
    }
    }
}

static void gui_draw_all(void)
{
    /* 桌面背景：深蓝渐变（下深上浅） */
    for (int y = 0; y < GUI_H; y++)
        gui_fill(0, y, GUI_W, 1, (u8)((y / 8) % 16 == 0 ? 1 : 9));
    /* 状态栏：蓝底白字，实时反馈激活窗口/窗口数/最近动作 */
    gui_fill(0, GUI_H - 22, GUI_W, 11, 1);
    gui_text(3, GUI_H - 20, 15, status_txt);
    /* 底部帮助栏 */
    gui_fill(0, GUI_H - 11, GUI_W, 11, 0);
    gui_text(3, GUI_H - 9, 15, "1-5 ACT");
    gui_text(58, GUI_H - 9, 7, "X CLOSE");
    gui_text(108, GUI_H - 9, 7, "N NEW");
    gui_text(148, GUI_H - 9, 7, "Q QUIT");
    gui_text(194, GUI_H - 9, 3, "MOUSE PS/2 OK");
    /* 窗口按创建顺序画 */
    for (int i = 0; i < nwins; i++)
        if (wins[i].visible)
            gw_draw(&wins[i], timer_ticks());
}

/* ---------------- 光标（XOR 4x4 + 斜线） ---------------- */

static void gui_cursor(int x, int y, int erase)
{
    /* 简单箭头：斜线 + 方块，XOR 语义（erase=1 时用同色再画一次清除） */
    static const u8 spr[8][8] = {
        {1,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0},
        {1,0,1,0,0,0,0,0},
        {1,0,0,1,0,0,0,0},
        {1,0,0,0,1,0,0,0},
        {1,0,0,0,0,1,0,0},
        {1,0,0,0,0,0,1,0},
        {1,1,1,1,1,1,1,1},
    };
    (void)erase;
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++)
            if (spr[j][i])
                gui_pixel(x + i, y + j, 15);
}

/* ---------------- 键盘 GUI 交互 ---------------- */

static void gw_close_active(void)
{
    if (active < 0) return;
    wins[active].visible = 0;
    serial_printf("[gui] close win %d (%s)\r\n", active, wins[active].title);
    /* 激活移到下一个可见窗口 */
    int na = -1;
    for (int i = 0; i < nwins; i++)
        if (wins[i].visible) { na = i; break; }
    active = na;
    status_act("CLOSE WIN");
}

static void gw_activate(int idx)
{
    if (idx < 0 || idx >= nwins || !wins[idx].visible) return;
    active = idx;
    serial_printf("[gui] activate win %d (%s)\r\n", idx, wins[idx].title);
    {
        char ab[20];
        int i = 0;
        const char *pre = "ACT ";
        while (pre[i] && i < 6) { ab[i] = pre[i]; i++; }
        ab[i++] = (char)('1' + idx);
        ab[i] = 0;
        status_act(ab);
    }
}

/* ---------------- 主循环 ---------------- */

void gui_run(void)
{
    serial_puts("[gui] enter graphics mode (13h 320x200)\r\n");
    gui_active = 1;
    vga_set_mode13h();

    /* 初始化演示窗口 */
    nwins = 0; active = -1;
    gw_add(10, 8, 104, 52, 4, WT_CLOCK, "CLOCK");
    gw_add(124, 6, 118, 72, 4, WT_SYSINFO, "SYSINFO");
    gw_add(52, 70, 108, 62, 0, WT_DEMO, "DEMO");
    gw_add(170, 62, 96, 44, 1, WT_LOGO, "WINPP");
    active = 0;
    serial_printf("[gui] windows created: %d\r\n", nwins);
    action_txt[0] = 0;
    status_update();

    int last_redraw = -100;
    int mouse_ok = 0;

    for (;;) {
        /* 等待下一个 tick（约 10ms）：避免忙等占满 CPU。
         * Bochs 下无等待的 for(;;) 会吃满模拟 CPU，键盘事件被延迟到"无响应"。 */
        u32 wait_t = timer_ticks();
        while ((int)(timer_ticks() - (int)wait_t) < 1) {
            asm volatile("sti; hlt");
        }

        /* 键盘事件（含串口注入通道） */
        int c = keyboard_poll();
        if (!c) c = serial_getc();   /* S10 自动化测试：串口输入 */
        if (c) {
            serial_printf("[gui] key '%c'\r\n", c);
            if (c == 'q') break;
            if (c == 'x') gw_close_active();
            if (c == 'n' && nwins < WIN_MAX) {
                int id = gw_add(20 + (nwins % 4) * 14, 30 + (nwins % 3) * 14,
                                90, 46, (u8)(nwins % 6 + 2), WT_LOGO, "WIN");
                serial_printf("[gui] new win id=%d\r\n", id);
                status_act("NEW WIN");
            }
            if (c >= '1' && c <= '5') gw_activate(c - '1');
            last_redraw = -100;   /* 强制立即重绘 */
        }

        /* 周期性重绘（动画/时钟） */
        u32 t = timer_ticks();
        if ((int)(t - (u32)last_redraw) >= 3) {
            gui_draw_all();
            last_redraw = (int)t;
        }

        /* 鼠标光标：仅位置变化时擦旧画新（避免每圈重绘吃满 CPU） */
        static int last_mx = -1, last_my = -1;
        int mx, my;
        mouse_pos(&mx, &my);
        if (mx != last_mx || my != last_my || last_mx < 0) {
            if (last_mx >= 0 && last_my >= 0)
                gui_cursor(last_mx, last_my, 1);
            gui_cursor(mx, my, 0);
            last_mx = mx; last_my = my;
        }
    }

    /* 退出：回文本模式 */
    vga_set_mode03();
    gui_active = 0;
    serial_puts("[gui] exit to text mode\r\n");
    kernel_redraw_shell();
}
