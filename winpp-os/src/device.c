#include "device.h"
#include "serial.h"
#include "mouse.h"

/* ============ 设备表实现 ============ */

static device_t devtab[DEV_MAX];
static int ndev = 0;

int device_register(const device_t *d)
{
    if (ndev >= DEV_MAX) return -1;
    if (!d || d->major == 0 || d->major >= DEV_MAX) return -2;
    /* 主设备号唯一 */
    for (int i = 0; i < ndev; i++) {
        if (devtab[i].major == d->major) return -3;
    }
    devtab[ndev] = *d;
    ndev++;
    return 0;
}

device_t *device_get(u8 major)
{
    if (major == 0 || major >= DEV_MAX) return 0;
    for (int i = 0; i < ndev; i++) {
        if (devtab[i].major == major) return &devtab[i];
    }
    return 0;
}

device_t *device_by_irq(u8 irq)
{
    for (int i = 0; i < ndev; i++) {
        if (devtab[i].irq && devtab[i].irqno == irq) return &devtab[i];
    }
    return 0;
}

int device_count(void)
{
    return ndev;
}

/* ============ 注册全部内建设备 ============
 * 每个设备一行：name / major / irqno / 能力标记。
 * 硬件初始化仍在各驱动自己的 init（kmain 分步调用），
 * 这里只把"谁是谁"登记进设备表，供上层统一访问。 */
void device_init_all(void)
{
    device_register(keyboard_device());   /* kbd  major=1 irq=1 */
    device_register(timer_device());      /* timer major=2 irq=0 */
    device_register(serial_device());     /* com1 major=3 irq=4(未开) */
    device_register(mouse_device());      /* mouse major=4 irq=12 (S10) */
    device_register(ne2k_device());       /* ne2k major=5 irq=10 (S11) */

    serial_printf("[dev] registered %d devices:\r\n", device_count());
    for (u8 m = 1; m < DEV_MAX; m++) {
        device_t *d = device_get(m);
        if (!d) continue;
        serial_printf("[dev]   %-6s major=%u type=%u irq=%u r=%d w=%d io=%d\r\n",
                      d->name, d->major, d->type, d->irqno,
                      d->read ? 1 : 0, d->write ? 1 : 0, d->ioctl ? 1 : 0);
    }
}
