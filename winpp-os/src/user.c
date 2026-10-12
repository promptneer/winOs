#include "user.h"
#include "serial.h"
#include "vga.h"

/* S8：极简用户表 —— 预置 root（UID=0，管理员）+ guest（UID=1，普通用户）。
 * 初始身份：root（启动即管理员，Linux root 惯例）。
 * login 命令切换 user_current_uid_val；进程创建继承它。 */

user_t users[USER_MAX];
u32    user_current_uid_val = 0;   /* 启动默认 root */

void user_init(void)
{
    for (u32 i = 0; i < USER_MAX; i++) users[i].used = 0;

    users[0].uid = 0;  users[0].used = 1;  users[0].name[0] = 'r';
    users[0].name[1] = 'o'; users[0].name[2] = 'o'; users[0].name[3] = 't';
    users[0].name[4] = 0;

    users[1].uid = 1;  users[1].used = 1;  users[1].name[0] = 'g';
    users[1].name[1] = 'u'; users[1].name[2] = 'e'; users[1].name[3] = 's';
    users[1].name[4] = 't'; users[1].name[5] = 0;

    user_current_uid_val = 0;
    serial_printf("[user] init: root(uid=0) guest(uid=1), current=root\r\n");
}

int user_lookup(const char *name)
{
    for (u32 i = 0; i < USER_MAX; i++) {
        if (!users[i].used) continue;
        const char *p = name, *q = users[i].name;
        while (*p && *q && *p == *q) { p++; q++; }
        if (*p == *q) return (int)users[i].uid;
    }
    return -1;
}

const char *user_name_of(u32 uid)
{
    for (u32 i = 0; i < USER_MAX; i++)
        if (users[i].used && users[i].uid == uid) return users[i].name;
    return "?";
}

u32 user_current_uid(void) { return user_current_uid_val; }

int user_login(const char *name)
{
    int uid = user_lookup(name);
    if (uid < 0) { serial_printf("[user] login '%s' denied: no such user\r\n", name); return -1; }
    user_current_uid_val = (u32)uid;
    serial_printf("[user] login %s -> uid=%d\r\n", user_name_of((u32)uid), uid);
    return 0;
}
