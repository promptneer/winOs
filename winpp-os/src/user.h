#ifndef USER_H
#define USER_H

#include "io.h"

/* S8 用户与权限：软件级用户账户表。
 * 与 S3 的"硬件级用户态（CPL3）"不同 —— 这里是"这台机器给谁用"的账户层：
 * root（UID=0，拥有全部权限）、guest（UID=1，普通用户）。
 * shell 的当前身份 = user_current_uid()；新进程继承它（pcb.uid）。 */

#define USER_MAX  8

typedef struct {
    u32 uid;
    char name[16];
    int  used;
} user_t;

void user_init(void);
int  user_login(const char *name);       /* 切换当前身份，成功返回 0 */
u32  user_current_uid(void);
const char *user_name_of(u32 uid);       /* uid -> 用户名（未知名返回 "?"） */
int  user_lookup(const char *name);      /* 用户名 -> uid（不存在 -1） */

extern user_t users[USER_MAX];
extern u32    user_current_uid_val;

#endif
