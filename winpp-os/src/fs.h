#ifndef FS_H
#define FS_H

#include "io.h"

/* S4-4 用户态文件系统：内存盘（tmpfs）。
 * 16 个文件槽 x 4KB，open/read/write/close 四个系统调用。
 * 数据区在内核内存（1MB+ 恒等区），用户程序只能通过 syscall 访问。
 * S5：fd 槽同时承载管道/FIFO（type 1/2 = 管道读/写端，3/4 = FIFO 读/写端）。 */

#define FS_MAX_FILES 16

typedef struct {
    char name[16];
    u32  size;            /* 已写入字节数 */
    u32  offset;          /* 当前读写偏移 */
    int  used;
    int  type;            /* 0=文件, 1=pipe读, 2=pipe写, 3=fifo读, 4=fifo写 */
    void *obj;            /* S5: 环形缓冲对象（ipc_buf_t*） */
} fs_ent_t;

void fs_init(void);
int  fs_open(const char *name, int flags);   /* flags: 0=读, 1=创建/写；返回 fd 或 -1 */
int  fs_write(int fd, const char *buf, int len);
int  fs_read(int fd, char *buf, int len);
int  fs_close(int fd);
void fs_list(void);                          /* 打印文件表 */

extern fs_ent_t fstab[];                     /* ipc.c 需要分配/标记 fd 槽 */

#endif
