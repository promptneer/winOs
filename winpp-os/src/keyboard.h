#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "io.h"

#define KEY_NONE    0
#define KEY_UP      1
#define KEY_DOWN    2
#define KEY_LEFT    3
#define KEY_RIGHT   4
#define KEY_ESC     27
#define KEY_TAB     9
#define KEY_ENTER   '\n'    /* 键盘驱动把 Enter 扫描码 0x1C 映射为 '\n'(10) */
#define KEY_BACKSP  '\b'

void keyboard_init(void);
void keyboard_handle_irq(void);
char keyboard_getchar(void);   /* 阻塞读取普通字符 */
int  keyboard_getkey(void);    /* 阻塞读取：普通字符或 KEY_* 特殊键 */
int  keyboard_poll(void);      /* S10: 非阻塞读（0=无键），GUI 用 */

#endif
