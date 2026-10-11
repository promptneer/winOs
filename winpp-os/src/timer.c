#include "timer.h"

static volatile u32 ticks = 0;

void timer_init(void)
{
    u32 divisor = 1193182 / TIMER_HZ;   /* 0x40 通道，~100Hz */
    outb(0x43, 0x36);
    outb(0x40, (u8)(divisor & 0xFF));
    outb(0x40, (u8)((divisor >> 8) & 0xFF));
    ticks = 0;
}

void timer_tick(void)
{
    ticks++;
}

u32 timer_ticks(void)        { return ticks; }
u32 timer_uptime_sec(void)   { return ticks / TIMER_HZ; }
