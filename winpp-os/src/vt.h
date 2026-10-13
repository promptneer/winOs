#ifndef VT_H
#define VT_H

#include "io.h"

/* S12 虚拟终端系统：
 * S12-1 虚拟终端框架：VT_MAX 个虚拟终端，每终端独立屏幕快照/光标/当前目录。
 * S12-2 输入保留：每终端独立输入缓冲（切走再切回，半行输入不丢）。
 * S12-3 终端间互发消息：每终端消息队列（环形 4 条），msg/msgs 命令。
 * S12-4 会话管理：ts 列表 / tkill 重置非活动终端。 */

#define VT_MAX          3
#define VT_SCREEN_SIZE  (25 * 80)
#define VT_MSG_MAX      4               /* 每终端消息队列容量 */
#define VT_MSG_LEN      60              /* 单条消息最长 */
#define VT_IBUF_MAX     120             /* 输入缓冲长度 */

typedef struct {
    u8  used;
    u8  id;                 /* 1..VT_MAX */
    char name[8];
    u16 screen[VT_SCREEN_SIZE];  /* 字符+属性快照（0xB8000 布局） */
    u8  cur_x, cur_y;            /* 终端自己的光标位置 */
    char cwd[16];                /* 终端自己的当前目录 */
    /* S12-2: 输入缓冲（半行输入） */
    char ibuf[VT_IBUF_MAX];
    u8   ilen;
    /* S12-3: 消息队列 */
    u8   nmsg;                 /* 未读消息数 */
    u8   msg_head;             /* 下一写入位置（环形） */
    char msgs[VT_MSG_MAX][VT_MSG_LEN];
} vt_t;

void vt_init(void);          /* 开机：VT1=当前屏，VT2/VT3=空白终端 */
u8   vt_active(void);        /* 当前活动终端 id（1..3） */
int  vt_switch(u8 id);       /* 切换终端；0=成功，-1=非法 id */
void vt_save_cwd(void);      /* 把 fs_cwd 存回当前终端 */
void vt_list(void);          /* 打印终端表 */
vt_t *vt_get(u8 id);         /* 取终端（id 1..3），非法返回 0 */
vt_t *vt_active_ptr(void);   /* 当前活动终端指针 */
void vt_buf_restore(void);   /* 把当前终端的输入缓冲恢复到 ibuf 指针（readline 用） */

/* S12-3: 终端间消息 */
int  vt_msg_send(u8 dst_id, const char *text);  /* 发消息到终端；0=成功 */
void vt_msg_show(void);                          /* msgs 命令：打印当前终端全部消息 */

/* S12-4: 会话管理 */
void vt_reset(u8 id);        /* tkill：清空终端（不能是当前活动终端） */
int  vt_killable(u8 id);     /* 能否重置该终端（非活动=1） */

#endif
