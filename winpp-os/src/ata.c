/* WinPP S16: ATA PIO 硬盘驱动实现
 * 主通道 0x1F0-0x1F7，LBA28 + PIO 模式（教学级，无 IRQ/无 DMA） */
#include "ata.h"
#include "io.h"
#include "serial.h"
#include "vga.h"

/* ---- 寄存器端口 ---- */
#define ATA_DATA       0x1F0   /* 16 位数据口 */
#define ATA_ERR        0x1F1
#define ATA_NSECT      0x1F2   /* 扇区数 */
#define ATA_LBA_LO     0x1F3   /* LBA 低 8 位 */
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRV        0x1F6   /* 0xE0|(lba>>24) 主盘 */
#define ATA_STATUS     0x1F7   /* 读=状态，写=命令 */
#define ATA_ALT_STATUS 0x3F6

/* 状态位 */
#define ATA_SR_BSY     0x80    /* 忙 */
#define ATA_SR_DRQ     0x08    /* 数据请求（可读写数据口） */
#define ATA_SR_ERR     0x01

/* 命令 */
#define ATA_CMD_READ   0x20    /* PIO 读扇区 */
#define ATA_CMD_WRITE  0x30    /* PIO 写扇区 */
#define ATA_CMD_IDENT  0xEC    /* IDENTIFY */

ata_disk_t g_ata;

/* 等待磁盘不忙（BSY 清零），超时返回 0 */
static int ata_wait_bsy(void)
{
    int t;
    for (t = 0; t < 1000000; t++) {
        u8 s = inb(ATA_STATUS);
        if (!(s & ATA_SR_BSY)) return 1;
    }
    return 0;
}

/* 等待数据请求（DRQ 置位），超时返回 0 */
static int ata_wait_drq(void)
{
    int t;
    for (t = 0; t < 1000000; t++) {
        u8 s = inb(ATA_STATUS);
        if (s & ATA_SR_ERR) return 0;
        if (s & ATA_SR_DRQ) return 1;
    }
    return 0;
}

/* 软复位 + 探测：写 4 次 0 到错误口，等 BSY 清，再等 DRQ——典型探测法 */
static int ata_probe(void)
{
    int t;
    outb(ATA_ALT_STATUS, 0x04);   /* SRST 复位 */
    for (t = 0; t < 100000; t++) inb(ATA_ALT_STATUS); /* 小延时 */
    outb(ATA_ALT_STATUS, 0x00);   /* 释放复位 */
    return ata_wait_bsy();
}

/* 小延时（PIO 时序：读写之间等 400ns） */
static void ata_delay(void)
{
    int i;
    for (i = 0; i < 200; i++) inb(ATA_ALT_STATUS);
}

/* 从磁盘读 256 个 16 位字（1 扇区） */
static void ata_read_words(u16 *dst)
{
    int i;
    for (i = 0; i < 256; i++) dst[i] = inw(ATA_DATA);
}

/* 向磁盘写 256 个 16 位字（1 扇区） */
static void ata_write_words(const u16 *src)
{
    int i;
    for (i = 0; i < 256; i++) outw(ATA_DATA, src[i]);
}

/* LBA28 寻址 + 等待就绪后发命令 */
static void ata_setup_lba(u32 lba, u8 nsect, u8 cmd)
{
    outb(ATA_NSECT, nsect);
    outb(ATA_LBA_LO, (u8)(lba & 0xFF));
    outb(ATA_LBA_MID, (u8)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI, (u8)((lba >> 16) & 0xFF));
    outb(ATA_DRV, 0xE0 | ((u8)((lba >> 24) & 0x0F)));  /* 主盘 LBA 模式 */
    outb(ATA_STATUS, cmd);
}

void ata_init(void)
{
    int i;
    g_ata.present = 0;
    g_ata.total_sectors = 0;
    g_ata.capacity_mb = 0;

    if (!ata_probe()) {
        serial_puts("[ata] probe failed (no drive?)\r\n");
        return;
    }

    /* IDENTIFY */
    outb(ATA_DRV, 0xE0);
    ata_delay();
    outb(ATA_STATUS, ATA_CMD_IDENT);
    if (!ata_wait_bsy()) return;
    if (inb(ATA_STATUS) == 0) {  /* 主盘不存在 */
        serial_puts("[ata] no master drive\r\n");
        return;
    }
    if (!ata_wait_drq()) {
        serial_puts("[ata] identify: no DRQ (error?)\r\n");
        return;
    }
    ata_read_words(g_ata.words);

    /* 解析参数 */
    g_ata.cylinders = ((u32)g_ata.words[1] | ((u32)g_ata.words[2] << 16));
    g_ata.heads     = g_ata.words[3];
    g_ata.sectors   = g_ata.words[6];
    /* 容量优先用 LBA28 总数（words[60]|words[61]<<16），否则 CHS 推算 */
    g_ata.total_sectors = (u32)g_ata.words[60] | ((u32)g_ata.words[61] << 16);
    if (g_ata.total_sectors == 0)
        g_ata.total_sectors = g_ata.cylinders * g_ata.heads * g_ata.sectors;
    g_ata.capacity_mb = (u32)((u32)g_ata.total_sectors / 2048u);   /* 扇区数/2048 = MB */

    /* 型号（word 27-46，20 字节）与序列号（word 10-19） */
    for (i = 0; i < 20; i++) {
        u16 w = g_ata.words[27 + i];
        g_ata.model[i * 2]     = (char)(w >> 8);   /* ATA 字符串高位在前 */
        g_ata.model[i * 2 + 1] = (char)(w & 0xFF);
    }
    g_ata.model[40] = 0;
    for (i = 0; i < 10; i++) {
        u16 w = g_ata.words[10 + i];
        g_ata.serial[i * 2]     = (char)(w >> 8);
        g_ata.serial[i * 2 + 1] = (char)(w & 0xFF);
    }
    g_ata.serial[20] = 0;

    g_ata.present = 1;
    serial_printf("[ata] IDENTIFY ok: %u cyl %u head %u spt, %u sectors (%u MB)\r\n",
                  (unsigned)g_ata.cylinders, (unsigned)g_ata.heads, (unsigned)g_ata.sectors,
                  (unsigned)g_ata.total_sectors, (unsigned)g_ata.capacity_mb);
}

int ata_read_sector(u32 lba, u8 *buf)
{
    if (!g_ata.present) return -1;
    if (lba >= g_ata.total_sectors) return -2;
    ata_setup_lba(lba, 1, ATA_CMD_READ);
    if (!ata_wait_bsy()) return -3;
    if (!ata_wait_drq()) return -4;
    ata_read_words((u16 *)buf);
    ata_delay();
    return 0;
}

int ata_write_sector(u32 lba, const u8 *buf)
{
    if (!g_ata.present) return -1;
    if (lba >= g_ata.total_sectors) return -2;
    ata_setup_lba(lba, 1, ATA_CMD_WRITE);
    if (!ata_wait_bsy()) return -3;
    if (!ata_wait_drq()) return -4;
    ata_write_words((const u16 *)buf);
    ata_delay();
    if (!ata_wait_bsy()) return -5;   /* 写完后等磁盘落盘完成 */
    return 0;
}
