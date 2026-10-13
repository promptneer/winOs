#include "vt.h"
#include "vga.h"
#include "serial.h"
#include "fs.h"
#include "user.h"

/* ============ S12 虚拟终端实现 ============ */

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
        v->nmsg = 0; v->msg_head = 0;
        v->ilen = 0;
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

    serial_printf("[vt] switch -> VT%d (cwd=%s, ibuf_len=%d, nmsg=%d)\r\n",
                  id, fs_cwd, (int)new->ilen, (int)new->nmsg);
    return 0;
}

void vt_list(void)
{
    vga_printf("id  name  status   cwd        msgs  input\r\n");
    for (int i = 0; i < VT_MAX; i++) {
        vt_t *v = &g_vts[i];
        vga_printf("%d   VT%d   %s   %s      %u    %d\r\n",
                   v->id, v->id,
                   (v->id == g_active) ? "ACTIVE" : "idle  ",
                   v->cwd, (unsigned)v->nmsg, (int)v->ilen);
    }
    serial_printf("[vt] list: active=VT%d\r\n", g_active);
}

/* ---------- S12-3 终端间消息 ---------- */

int vt_msg_send(u8 dst_id, const char *text)
{
    vt_t *v = vt_get(dst_id);
    if (!v) return -1;
    int n = 0;
    for (; text[n] && n < VT_MSG_LEN - 1; n++) v->msgs[v->msg_head][n] = text[n];
    v->msgs[v->msg_head][n] = 0;
    v->msg_head = (u8)((v->msg_head + 1) % VT_MSG_MAX);
    if (v->nmsg < VT_MSG_MAX) v->nmsg++;
    serial_printf("[vt] msg -> VT%d: %s\r\n", dst_id, text);

    /* 目标终端是当前活动终端：消息直接实时显示在屏幕 */
    if (v->id == g_active) {
        vga_printf("\n[msg] %s\n", text);
    } else {
        /* 非活动终端：在它的屏幕快照输入区上方打一行（切换过去即见） */
        for (int c = 0; c < 80; c++) v->screen[23 * 80 + c] = 0x0700 | ' ';
        int c = 0;
        const char *p = "[msg] ";
        while (*p && c < 79) v->screen[23 * 80 + c++] = 0x0E00 | (u8)*p++;
        p = text;
        while (*p && c < 79) v->screen[23 * 80 + c++] = 0x0E00 | (u8)*p++;
    }
    return 0;
}

void vt_msg_show(void)
{
    vt_t *v = vt_active_ptr();
    if (v->nmsg == 0) { vga_printf("(no messages)\n"); return; }
    vga_printf("VT%d inbox (%u):\r\n", v->id, (unsigned)v->nmsg);
    for (int k = 0; k < VT_MSG_MAX; k++) {
        int idx = (v->msg_head + VT_MSG_MAX - 1 - k) % VT_MSG_MAX;  /* 最新的在前 */
        if (v->msgs[idx][0])
            vga_printf("  [%d] %s\n", k + 1, v->msgs[idx]);
    }
    serial_printf("[vt] msgs shown: VT%d nmsg=%u\r\n", v->id, (unsigned)v->nmsg);
}

/* ---------- S12-4 会话管理 ---------- */

int vt_killable(u8 id) { return (id >= 1 && id <= VT_MAX && id != g_active); }

void vt_reset(u8 id)
{
    vt_t *v = vt_get(id);
    if (!v || id == g_active) return;
    vt_blank(v);
    v->cwd[0] = '/'; v->cwd[1] = 0;
    v->nmsg = 0; v->msg_head = 0;
    v->ilen = 0;
    /* 重置后打一个提示符快照（切过去就是干净的新终端） */
    int row = 1, col = 0;
    const char *prompt = "root@kernel:/# ";
    for (const char *p = prompt; *p; p++) v->screen[row * 80 + col++] = 0x0A00 | (u8)*p;
    v->cur_x = (u8)col;
    v->cur_y = (u8)row;
    serial_printf("[vt] reset VT%d (killed, back to clean state)\r\n", id);
}

/* readline 辅助：活动终端半行输入指针 */
void vt_buf_restore(void) { /* 占位：shell 直接读写 vt_active_ptr()->ibuf */ }
