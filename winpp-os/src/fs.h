#ifndef FS_H
#define FS_H

#include "io.h"

/* S4-4 用户态文件系统：内存盘（tmpfs）。
 * 16 个文件槽 x 4KB，open/read/write/close 四个系统调用。
 * 数据区在内核内存（1MB+ 恒等区），用户程序只能通过 syscall 访问。
 * S5：fd 槽同时承载管道/FIFO（type 1/2 = 管道读/写端，3/4 = FIFO 读/写端）。
 * S6：目录支持 —— type 5 = 目录；name 存完整路径（如 "/docs/a.txt"）；
 *     fs_cwd 为内核当前目录，相对路径经 fs_resolve 拼接为绝对路径。
 * S8：权限 —— 每槽 owner_uid + mode（rwx：bit5-3=owner，bit2-0=other）；
 *     uid=0（root）绕过一切检查；open/write/read/mkdir/chdir 均检查。 */

#define FS_MAX_FILES 16
#define FS_TYPE_DIR  5               /* S6: 目录项（size=0，无数据区） */

/* S8: 权限位（3 位一组：r=4 w=2 x=1） */
#define MODE_O_RX   0x5              /* other: r-x */
#define MODE_O_R    0x4              /* other: r-- */
#define MODE_O_NONE 0x0
#define MODE_W_RW   0x30             /* owner: rw- */
#define MODE_W_RWX  0x38             /* owner: rwx */
#define MODE_W_R    0x20             /* owner: r-- */

typedef struct {
    char name[16];
    u32  size;            /* 已写入字节数 */
    u32  offset;          /* 当前读写偏移 */
    int  used;
    int  type;            /* 0=文件, 1=pipe读, 2=pipe写, 3=fifo读, 4=fifo写, 5=目录 */
    void *obj;            /* S5: 环形缓冲对象（ipc_buf_t*） */
    u32  owner_uid;       /* S8: 属主 UID */
    u32  mode;            /* S8: 权限位（owner bit5-3 / other bit2-0） */
} fs_ent_t;

void fs_init(void);
int  fs_open(const char *name, int flags);   /* flags: 0=读, 1=创建/写；返回 fd 或 -1 */
int  fs_write(int fd, const char *buf, int len);
int  fs_read(int fd, char *buf, int len);
int  fs_close(int fd);
void fs_list(void);                          /* 打印文件全表（fslist 命令） */
void fs_ls(const char *path);                /* S6: 列出目录内容（ls 命令） */
int  fs_mkdir(const char *name);             /* S6: 创建目录，返回 0 或 -1 */
int  fs_chdir(const char *name);             /* S6: 切换当前目录，返回 0 或 -1 */
int  fs_resolve(const char *name, char *out, int outsz); /* S6: 相对->绝对路径 */
int  fs_chmod(const char *name, u32 mode);   /* S8: 改权限（仅 root/属主） */

extern fs_ent_t fstab[];                     /* ipc.c 需要分配/标记 fd 槽 */
extern char fs_cwd[16];                      /* S6: 内核当前目录 */

#endif

