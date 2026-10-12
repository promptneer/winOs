#ifndef FS_H
#define FS_H

#include "io.h"

/* S4-4 用户态文件系统：内存盘（tmpfs）。
 * 16 个文件槽 x 4KB，open/read/write/close 四个系统调用。
 * 数据区在内核内存（1MB+ 恒等区），用户程序只能通过 syscall 访问。 */

void fs_init(void);
int  fs_open(const char *name, int flags);   /* flags: 0=读, 1=创建/写；返回 fd 或 -1 */
int  fs_write(int fd, const char *buf, int len);
int  fs_read(int fd, char *buf, int len);
int  fs_close(int fd);
void fs_list(void);                          /* 打印文件表 */

#endif
