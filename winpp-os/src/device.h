#ifndef DEVICE_H
#define DEVICE_H

#include "io.h"

/* ============ S7 设备驱动层：设备表 + 统一接口 ============
 * 把散落的外设代码（键盘/定时器/串口）收进一张设备表，
 * 内核通过 major 号找设备、通过 irqno 分发中断，
 * 上层用统一接口 read/write/ioctl 访问设备。 */

#define DEV_MAX          8
#define DEV_CHAR         1   /* 字符设备：一次一个字符 */
#define DEV_CLOCK        2   /* 时钟设备：周期中断 */

/* 主设备号（内核与设备的一一对应，类似 Linux 的 major） */
#define DEV_MAJOR_KBD    1
#define DEV_MAJOR_TIMER  2
#define DEV_MAJOR_COM1   3

/* 设备 ioctl 命令号 */
#define DEV_IOCTL_TICKS      1   /* timer: 返回 tick 计数 */
#define DEV_IOCTL_UPTIME     2   /* timer: 返回运行秒数 */

typedef struct device {
    const char *name;            /* "kbd" / "timer" / "com1" */
    u8   type;                   /* DEV_CHAR / DEV_CLOCK */
    u8   major;                  /* 主设备号 */
    u8   irqno;                  /* 绑定的 IRQ（0 定时器 / 1 键盘 / 4 串口） */
    int (*init)(void);           /* 硬件初始化（可选） */
    int (*read)(char *buf, int len);    /* 读：键盘取一个字符 */
    int (*write)(const char *buf, int len); /* 写：串口输出 */
    int (*ioctl)(u32 cmd, u32 arg);     /* 控制通道：查状态/配置 */
    void (*irq)(void);           /* 中断处理：由 irq_handler 按 irqno 分发 */
} device_t;

void device_init_all(void);          /* 注册全部内建设备（kmain 调用） */
int  device_register(const device_t *d);
device_t *device_get(u8 major);      /* 按主设备号查设备 */
device_t *device_by_irq(u8 irq);     /* 按 IRQ 查设备（中断分发用） */
int  device_count(void);

/* 各驱动暴露的设备描述 */
device_t *keyboard_device(void);
device_t *timer_device(void);
device_t *serial_device(void);

#endif
