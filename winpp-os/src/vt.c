#include "vt.h"
#include "vga.h"
#include "serial.h"
#include "fs.h"

/* ============ S12-1 虚拟终端实现 ============
 * 方案：每个终端保存一份 25x80 屏幕快照 + 光标 + 当前目录。
 * 切换 = 保存当前屏到旧终端 → 载入新终端屏 → 恢复 fs_cwd 与光标。
 * 各终端显示互相独立，互不串扰。 */

static vt_t g_vts[VT_MAX];
static u8   g_active = 1;   /* 当前活动终端 id（1..VT_MAX） */

u8 vt_active(void) { return g_active; }
vt_t *vt_get(u8 id) { return (id >= 1 && id <= VT_MAX) ? &g_vts[id - 1] : 0; }
vt_t *vt_active_ptr(void) { return &g_vts[g_active - 1]; }

/* 0xB8000 -> v->screen */
static void vt_screen_save(vt_t *v)
{
    volatile u16 *vbuf = (volatile u16 *)0xB8000;
    for (int i = 0; i < VT_SCREEN_SIZE; i++) v->screen[i] = vbuf[i];
    vga_get_pos(&v->cur_x, &v->cur_y);
}

/* v->screen -> 0xB8000 */
static void vt_screen_restore(vt_t *v)
{
    volatile u16 *vbuf = (volatile u16 *)0xB8000;
    for (int i = 0; i < VT_SCREEN_SIZE; i++) vbuf[i] = v->screen[i];
    vga_gotoxy(v->cur_x, v->cur_y);
    vga_update_cursor();
}

/* 空白终端：全空格填充 + 顶部一行标识 */
static void vt_blank(vt_t *v)
{
    for (int i = 0; i < VT_SCREEN_SIZE; i++) v->screen[i] = 0x0700 | ' ';
    for (int c = 0; c < 80; c++) v->screen[c] = 0x0A00 | (u16)' ';
    for (int c = 0; c < 8 && v->name[c]; c++) v->screen[c] = 0x0A00 | (u16)v->name[c];
    v->cur_x = 0;
    v->cur_y = 1;
}

void vt_init(void)
{
    for (int i = 0; i < VT_MAX; i++) {
        vt_t *v = &g_vts[i];
        v->used = 1;
        v->id = (u8)(i + 1);
        v->name[0] = 'V'; v->name[1] = 'T';
        v->name[2] = (char)('1' + i); v->name[3] = 0;
        v->cwd[0] = '/'; v->cwd[1] = 0;
        v->msgs = 0;
        if (i == 0) {
            vt_screen_save(v);          /* VT1 = 当前屏幕（引导后 shell 屏） */
        } else {
            vt_blank(v);
        }
    }
    g_active = 1;
    serial_printf("[vt] init: 3 virtual terminals (VT1..VT3), active=VT1\r\n");
}

void vt_save_cwd(void)
{
    vt_t *v = vt_active_ptr();
    for (int i = 0; i < 15 && fs_cwd[i]; i++) v->cwd[i] = fs_cwd[i];
    v->cwd[15] = 0;
}

int vt_switch(u8 id)
{
    if (id < 1 || id > VT_MAX) return -1;
    if (id == g_active) return 0;

    vt_t *old = vt_active_ptr();
    vt_screen_save(old);                /* 1. 保存当前屏到旧终端 */
    vt_save_cwd();                      /* 2. 保存 cwd 到旧终端 */

    vt_t *new = &g_vts[id - 1];
    vt_screen_restore(new);             /* 3. 载入新终端屏 + 光标 */
    for (int i = 0; i < 15 && new->cwd[i]; i++) fs_cwd[i] = new->cwd[i];  /* 4. 恢复 cwd */
    fs_cwd[15] = 0;
    g_active = id;

    serial_printf("[vt] switch -> VT%d (cwd=%s)\r\n", id, fs_cwd);
    return 0;
}

void vt_list(void)
{
    vga_printf("id  name  status   cwd        msgs\r\n");
    for (int i = 0; i < VT_MAX; i++) {
        vt_t *v = &g_vts[i];
        vga_printf("%d   VT%d   %s   %s      %u\r\n",
                   v->id, v->id,
                   (v->id == g_active) ? "ACTIVE" : "idle  ",
                   v->cwd, (unsigned)v->msgs);
    }
    serial_printf("[vt] list: active=VT%d\r\n", g_active);
}
