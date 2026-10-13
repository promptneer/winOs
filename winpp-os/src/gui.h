#ifndef WINPP_GUI_H
#define WINPP_GUI_H

#include "io.h"

/* S10 图形界面窗口系统：
 *   VGA Mode 13h（320x200x256）+ 帧缓冲绘制 + 5x7 字体
 *   + 窗口管理器（多窗口/激活/关闭/新建）+ 键盘交互 + PS/2 鼠标光标
 * shell 命令 gui 进入；'q' 退出回文本模式。 */

#define GUI_W 320
#define GUI_H 200

extern int gui_active;   /* 图形模式标志（irq_handler 据此跳过文本状态栏重绘） */

void gui_run(void);      /* 进入图形模式主循环，直到 'q' 退出 */

#endif
