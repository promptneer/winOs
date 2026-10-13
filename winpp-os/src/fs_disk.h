/* WinPP S17: 磁盘文件系统（极简 FAT-like，教学级）
 * 硬盘布局（hd.img 20160 扇区）：
 *   LBA 0    : 引导区（保留，含 MBR 签名）
 *   LBA 1    : 超级块（魔数/版本/文件数/数据区起始）
 *   LBA 2-3  : 目录区（2 扇区 x 16 项/扇区 = 32 个目录项）
 *   LBA 4..  : 数据区（每个文件占 1 个扇区，最大 512 字节）
 */
#ifndef WINPP_FS_DISK_H
#define WINPP_FS_DISK_H

#include "io.h"

#define FS_DISK_MAGIC 0x5750    /* 'WP' */
#define FS_DISK_VERSION 1
#define FS_DIR_LBA    2         /* 目录区起始扇区 */
#define FS_DIR_SECS   2         /* 目录区扇区数 */
#define FS_DIR_ENTRIES_PER_SEC 16  /* 512/32 */
#define FS_DISK_MAX_FILES 32
#define FS_DATA_START 4         /* 数据区起始扇区 */
#define FS_NAME_MAX   15

/* 目录项（32 字节） */
typedef struct {
    char name[FS_NAME_MAX + 1]; /* 文件名（含结尾 0） */
    u8   used;                  /* 1=占用 */
    u8   type;                  /* 0=文件 1=目录（预留） */
    u8   pad[2];
    u32  size;                  /* 文件字节数 */
    u32  start_lba;             /* 数据扇区 */
} fs_dir_entry_t;

/* 超级块（512 字节，用前 16 字节） */
typedef struct {
    u16 magic;
    u16 version;
    u32 nfiles;                 /* 当前文件数 */
    u32 dir_lba;
    u32 dir_secs;
    u32 data_start;
} fs_super_t;

/* 初始化：读超级块；若未格式化则提示 */
int  fs_disk_init(void);

/* 格式化：写超级块 + 清空目录区 */
int  fs_disk_format(void);

/* 建文件：找空目录项 + 空数据扇区，写目录项 + 写数据 */
int  fs_disk_mkfile(const char *name, const u8 *data, u32 size);

/* 读文件内容到 buf（<=512B），返回字节数或负数 */
int  fs_disk_cat(const char *name, u8 *buf);

/* 列目录：打印所有文件 */
int  fs_disk_ls(void);

/* 删除文件：清目录项 */
int  fs_disk_rm(const char *name);

#endif
