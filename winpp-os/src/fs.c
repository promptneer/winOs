#include "fs.h"
#include "serial.h"
#include "vga.h"

/* S4-4 用户态文件系统：极简内存盘（tmpfs）。
 * 布局：16 个文件槽 x 4KB 数据区（静态数组，BSS 段）+ 每槽元数据表。
 * 语义：open(创建/读) -> write/read 从槽内 offset 推进 -> close 重置 offset。
 * 数据区位于内核内存（1MB+），用户程序只能经 int 0x80 系统调用访问，
 * 用户态无法直接触碰文件内容 —— 这正是"内核替用户管理数据"的隔离体现。 */

#define FS_MAX_FILES 16
#define FS_FILE_SIZE 4096

typedef struct {
    char name[16];
    u32  size;            /* 已写入字节数 */
    u32  offset;          /* 当前读写偏移 */
    int  used;
} fs_ent_t;

static fs_ent_t fstab[FS_MAX_FILES];
static u8 fsdata[FS_MAX_FILES][FS_FILE_SIZE];

static void f_memset(void *d, int v, u32 n)
{
    u8 *p = d;
    for (u32 i = 0; i < n; i++) p[i] = (u8)v;
}

static void f_strcpy(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
}

static int f_streq(const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

void fs_init(void)
{
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        fstab[i].used = 0;
        fstab[i].size = 0;
        fstab[i].offset = 0;
        fstab[i].name[0] = 0;
    }
    for (u32 i = 0; i < FS_MAX_FILES; i++)
        f_memset(fsdata[i], 0, FS_FILE_SIZE);
    serial_puts("[fs] init: ramdisk 16 files x 4KB ready\r\n");
}

int fs_open(const char *name, int flags)
{
    if (!name) return -1;
    /* 已存在：重置偏移后返回（读/写都行） */
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used && f_streq(fstab[i].name, name)) {
            fstab[i].offset = 0;
            serial_printf("[fs] open '%s' fd=%u (exist)\r\n", name, i);
            return (int)i;
        }
    }
    if (!(flags & 1)) return -1;             /* 不存在且非创建模式 */
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used) continue;
        f_strcpy(fstab[i].name, name);
        fstab[i].size = 0;
        fstab[i].offset = 0;
        fstab[i].used = 1;
        serial_printf("[fs] open created '%s' fd=%u\r\n", name, i);
        return (int)i;
    }
    return -1;                               /* 表满 */
}

int fs_write(int fd, const char *buf, int len)
{
    if (fd < 0 || fd >= FS_MAX_FILES || !fstab[fd].used || !buf) return -1;
    if (len < 0) len = 0;
    if (fstab[fd].offset + (u32)len > FS_FILE_SIZE)
        len = FS_FILE_SIZE - (int)fstab[fd].offset;
    if (len <= 0) return 0;
    for (int i = 0; i < len; i++)
        fsdata[fd][fstab[fd].offset + (u32)i] = buf[i];
    fstab[fd].offset += (u32)len;
    if (fstab[fd].offset > fstab[fd].size)
        fstab[fd].size = fstab[fd].offset;
    return len;
}

int fs_read(int fd, char *buf, int len)
{
    if (fd < 0 || fd >= FS_MAX_FILES || !fstab[fd].used || !buf) return -1;
    if (len < 0) len = 0;
    if (fstab[fd].offset + (u32)len > fstab[fd].size)
        len = (int)(fstab[fd].size - fstab[fd].offset);
    if (len <= 0) return 0;
    for (int i = 0; i < len; i++)
        buf[i] = fsdata[fd][fstab[fd].offset + (u32)i];
    fstab[fd].offset += (u32)len;
    return len;
}

int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_FILES || !fstab[fd].used) return -1;
    fstab[fd].offset = 0;
    return 0;
}

void fs_list(void)
{
    u32 n = 0;
    vga_printf("NAME            SIZE   FD\n");
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!fstab[i].used) continue;
        vga_printf("%-14s %5u  %u\n", fstab[i].name, fstab[i].size, i);
        n++;
    }
    serial_printf("[fs] list: %u files\r\n", n);
}
