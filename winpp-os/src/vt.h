#ifndef VT_H
#define VT_H

#include "io.h"

/* S12-1 虚拟终端框架：内核内开 VT_MAX 个"虚拟终端"，
 * 每个终端有自己独立的屏幕快照(25x80)、光标、当前目录，
 * 切换终端 = 保存当前屏到旧终端 + 载入新终端屏。 */

#define VT_MAX          3
#define VT_SCREEN_SIZE  (25 * 80)

typedef struct {
    u8  used;
    u8  id;                 /* 1..VT_MAX */
    char name[8];
    u16 screen[VT_SCREEN_SIZE];  /* 字符+属性快照（0xB8000 布局） */
    u8  cur_x, cur_y;            /* 终端自己的光标位置 */
    char cwd[16];                /* 终端自己的当前目录 */
    u32 msgs;                    /* S12-3: 收到的消息计数 */
} vt_t;

void vt_init(void);          /* 开机：VT1=当前屏，VT2/VT3=空白终端 */
u8   vt_active(void);        /* 当前活动终端 id（1..3） */
int  vt_switch(u8 id);       /* 切换终端；0=成功，-1=非法 id */
void vt_save_cwd(void);      /* 把 fs_cwd 存回当前终端 */
void vt_list(void);          /* 打印终端表 */
vt_t *vt_get(u8 id);         /* 取终端（id 1..3），非法返回 0 */
vt_t *vt_active_ptr(void);   /* 当前活动终端指针 */

#endif
