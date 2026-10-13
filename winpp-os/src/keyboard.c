#include "keyboard.h"
#include "device.h"
#include "serial.h"

#define BUF_SIZE 128

/* 普通按键扫描码表（make code） */
static const char scancode_map[58] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h',
    'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

/* Shift 组合 */
static const char scancode_map_shift[58] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H',
    'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

static volatile u8 buf[BUF_SIZE];
static volatile u8 head = 0, tail = 0;
static volatile u8 shift = 0, e0 = 0, caps = 0;

static void push(u8 c)
{
    u8 next = (u8)((head + 1) % BUF_SIZE);
    if (next == tail) return;   /* 满则丢弃 */
    buf[head] = c;
    head = next;
}

void keyboard_init(void)
{
    head = tail = 0;
    shift = caps = 0;
    e0 = 0;
}

void keyboard_handle_irq(void)
{
    /* S9 原版直接读 0x60（Bochs 下 0x64 状态位时序与真实 8042 不同，
       门卫会误丢真实按键）；鼠标防吞键由 mouse_handle_irq 的 bit5 检查负责 */
    u8 sc = inb(0x60);
    if (sc == 0xE0) { e0 = 1; return; }

    u8 make = (sc & 0x80) == 0;
    u8 code = sc & 0x7F;

    if (e0) {
        e0 = 0;
        if (!make) return;
        switch (code) {
        case 0x48: push((u8)KEY_UP);    break;
        case 0x50: push((u8)KEY_DOWN);  break;
        case 0x4B: push((u8)KEY_LEFT);  break;
        case 0x4D: push((u8)KEY_RIGHT); break;
        default: break;
        }
        return;
    }

    switch (code) {
    case 0x2A: case 0x36: shift = make; return;  /* Shift */
    case 0x3A: if (make) caps = !caps;  return;  /* CapsLock */
    case 0x3B: if (make) push((u8)KEY_F1); return;   /* S12: F1 切 VT1 */
    case 0x3C: if (make) push((u8)KEY_F2); return;   /* S12: F2 切 VT2 */
    case 0x3D: if (make) push((u8)KEY_F3); return;   /* S12: F3 切 VT3 */
    default: break;
    }

    if (!make || code >= 58) return;

    char c;
    if (code == 0x39) { c = ' '; }
    else {
        c = (shift ^ caps) ? scancode_map_shift[code] : scancode_map[code];
    }
    if (c) push((u8)c);
}

static char get_raw(void)
{
    for (;;) {
        asm volatile("sti");
        asm volatile("hlt");
        if (head != tail) break;
    }
    char c = (char)buf[tail];
    tail = (u8)((tail + 1) % BUF_SIZE);
    return c;
}

char keyboard_getchar(void)
{
    return get_raw();
}

int keyboard_getkey(void)
{
    int c = get_raw();
    return c;
}

/* S10: 非阻塞读，无键返回 0（GUI 事件循环用） */
int keyboard_poll(void)
{
    if (head == tail) return 0;
    char c = (char)buf[tail];
    tail = (u8)((tail + 1) % BUF_SIZE);
    return (int)c;
}

/* ============ S7 设备驱动：键盘作为字符设备 ============ */

static int kbd_read(char *buf, int len)
{
    if (len < 1) return 0;
    buf[0] = (char)keyboard_getchar();   /* 统一接口：阻塞取一个字符 */
    return 1;
}

device_t *keyboard_device(void)
{
    static device_t d = {
        "kbd", DEV_CHAR, DEV_MAJOR_KBD, 1,   /* name/type/major/irqno=IRQ1 */
        0, 0, 0, 0, keyboard_handle_irq       /* init/read/write/ioctl/irq */
    };
    d.read = kbd_read;
    return &d;
}
