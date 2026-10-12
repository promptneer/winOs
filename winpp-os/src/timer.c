#include "timer.h"
#include "device.h"
#include "serial.h"

static volatile u32 ticks = 0;

void timer_init(void)
{
    u32 divisor = 1193182 / TIMER_HZ;   /* 0x40 通道，~100Hz */
    outb(0x43, 0x36);
    outb(0x40, (u8)(divisor & 0xFF));
    outb(0x40, (u8)((divisor >> 8) & 0xFF));
    ticks = 0;
    serial_printf("[timer] init divisor=%u expect %uHz\r\n", divisor, TIMER_HZ);
}

void timer_tick(void)
{
    ticks++;
}

u32 timer_ticks(void)        { return ticks; }
u32 timer_uptime_sec(void)   { return ticks / TIMER_HZ; }

/* ============ S7 设备驱动：定时器作为时钟设备 ============ */

static int tmr_ioctl(u32 cmd, u32 arg)
{
    (void)arg;
    if (cmd == DEV_IOCTL_TICKS)  return (int)timer_ticks();
    if (cmd == DEV_IOCTL_UPTIME) return (int)timer_uptime_sec();
    return -1;
}

device_t *timer_device(void)
{
    static device_t d = {
        "timer", DEV_CLOCK, DEV_MAJOR_TIMER, 0,  /* name/type/major/irqno=IRQ0 */
        0, 0, 0, tmr_ioctl, timer_tick            /* init/read/write/ioctl/irq */
    };
    return &d;
}
