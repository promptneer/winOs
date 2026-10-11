#ifndef TIMER_H
#define TIMER_H

#include "io.h"

#define TIMER_HZ 100

void timer_init(void);
void timer_tick(void);   /* 由 IRQ0 调用 */
u32  timer_ticks(void);
u32  timer_uptime_sec(void);

#endif
