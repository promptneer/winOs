#ifndef MOUSE_H
#define MOUSE_H

#include "io.h"
#include "device.h"

/* S10 PS/2 鼠标驱动（IRQ12）。3 字节包：flags + dx + dy。
 * Bochs 需 bochsrc 开 mouse: enabled=1, type=ps2。 */

#define MOUSE_MAJOR 4

void mouse_init(void);
void mouse_handle_irq(void);
int  mouse_buttons(void);      /* bit0=左键 bit1=右键 */
void mouse_pos(int *x, int *y); /* 当前光标位置（framebuffer 坐标） */

/* S7 设备框架注册用 */
device_t *mouse_device(void);

#endif
