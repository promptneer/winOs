/* WinPP S16: ATA PIO 硬盘驱动（LBA28 读写 + IDENTIFY）
 * 寄存器组 0x1F0-0x1F7（主通道），LBA28 寻址，PIO 模式 */
#ifndef WINPP_ATA_H
#define WINPP_ATA_H

#include "io.h"

/* 磁盘参数（IDENTIFY 解析结果） */
typedef struct {
    int  present;        /* 探测到硬盘 */
    u16  words[256];     /* IDENTIFY 原始数据 */
    u32  cylinders;      /* 柱面数 */
    u32  heads;          /* 磁头数 */
    u32  sectors;        /* 每磁道扇区数 */
    u32  total_sectors;  /* LBA28 总扇区数 */
    u32  capacity_mb;    /* 字节容量/1MB（取整） */
    char model[41];      /* 型号字符串 */
    char serial[21];     /* 序列号 */
} ata_disk_t;

extern ata_disk_t g_ata;

/* 初始化：探测磁盘并执行 IDENTIFY */
void ata_init(void);

/* 读一个扇区（LBA28），成功返回 0 */
int ata_read_sector(u32 lba, u8 *buf);

/* 写一个扇区（LBA28），成功返回 0 */
int ata_write_sector(u32 lba, const u8 *buf);

#endif
