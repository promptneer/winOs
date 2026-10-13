#include "mouse.h"
#include "device.h"
#include "serial.h"

/* PS/2 鼠标：8042 辅助设备，IRQ12，3 字节包（flags, dx, dy）。
 * 初始化：启用辅助端口 -> 写命令 0xF4（enable data reporting）-> 等 ACK。
 * 包格式：flags[7]=Y溢出 [6]=X溢出 [5]=Y符号 [4]=X符号 [3]=1 [2]=中 [1]=右 [0]=左 */

#define AUX_CMD_PORT 0x64
#define AUX_DATA_PORT 0x60

static volatile int m_buttons = 0;
static volatile int m_x = 160, m_y = 100;   /* 屏幕中心开始 */

static void aux_wait_write(void)
{
    for (u32 i = 0; i < 100000; i++) {
        if (!(inb(AUX_CMD_PORT) & 0x02)) return;   /* 输入缓冲空 */
    }
}

static void aux_wait_read(void)
{
    for (u32 i = 0; i < 100000; i++) {
        if (inb(AUX_CMD_PORT) & 0x01) return;      /* 输出缓冲有数据 */
    }
}

void mouse_init(void)
{
    aux_wait_write();
    outb(AUX_CMD_PORT, 0xA8);          /* 启用辅助设备 */
    aux_wait_write();
    outb(AUX_CMD_PORT, 0xD4);          /* 修复：下一字节写给鼠标（原 0x60 是写输出端口，会关 A20 破坏键盘） */
    aux_wait_write();
    outb(AUX_DATA_PORT, 0xF4);         /* enable data reporting */
    /* 等 ACK（0xFA）并丢弃（可能存在旧包） */
    aux_wait_read();
    (void)inb(AUX_DATA_PORT);
    m_x = 160; m_y = 100; m_buttons = 0;
    serial_puts("[gui] mouse init: PS/2 enabled\r\n");
}

void mouse_handle_irq(void)
{
    /* 关键：先看 8042 状态，只有 bit5（aux 数据）置位才读 0x60；
     * 否则该字节是键盘扫描码，必须留给键盘 IRQ1 驱动。 */
    u8 st = inb(AUX_CMD_PORT);
    if (!(st & 0x20)) return;
    u8 data = inb(AUX_DATA_PORT);
    static u8 pkt[3];
    static int phase = 0;
    pkt[phase++] = data;
    if (phase < 3) return;
    phase = 0;
    if (!(pkt[0] & 0x08)) return;      /* 同步位丢失：丢包 */

    int dx = (int)((signed char)pkt[1]);
    int dy = (int)((signed char)pkt[2]);
    m_x += dx; if (m_x < 0) m_x = 0; if (m_x > 319) m_x = 319;
    m_y -= dy; if (m_y < 0) m_y = 0; if (m_y > 199) m_y = 199;
    m_buttons = (pkt[0] & 0x03);
}

int mouse_buttons(void) { return m_buttons; }
void mouse_pos(int *x, int *y) { *x = m_x; *y = m_y; }

/* S7 设备框架：鼠标作为字符设备（major 4, irq 12） */
device_t *mouse_device(void)
{
    static device_t d = {
        "mouse", DEV_CHAR, MOUSE_MAJOR, 12,   /* name/type/major/irqno=IRQ12 */
        0, 0, 0, 0, mouse_handle_irq
    };
    return &d;
}
