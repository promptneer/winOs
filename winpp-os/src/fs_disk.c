/* WinPP S17: 磁盘文件系统实现（极简，单扇区文件，32 目录项） */
#include "fs_disk.h"
#include "ata.h"
#include "serial.h"
#include "vga.h"

static fs_super_t  g_super;
static int         g_fs_ready = 0;

/* 目录项内存镜像（2 扇区，最多 32 项） */
static u8 g_dir_raw[512 * FS_DIR_SECS];

static void fs_zero(u8 *p, int n) { int i; for (i = 0; i < n; i++) p[i] = 0; }
static int  fs_strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int  fs_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}

/* 读目录区到内存 */
static void fs_dir_load(void)
{
    int i;
    for (i = 0; i < FS_DIR_SECS; i++)
        ata_read_sector(FS_DIR_LBA + (u32)i, g_dir_raw + i * 512);
}

/* 写目录区回硬盘 */
static void fs_dir_save(void)
{
    int i;
    for (i = 0; i < FS_DIR_SECS; i++)
        ata_write_sector(FS_DIR_LBA + (u32)i, g_dir_raw + i * 512);
}

int fs_disk_init(void)
{
    u8 sec[512];
    if (!g_ata.present) return -1;
    if (ata_read_sector(1, sec) != 0) return -2;
    g_super.magic      = (u16)(sec[0] | (sec[1] << 8));
    g_super.version    = (u16)(sec[2] | (sec[3] << 8));
    g_super.nfiles     = (u32)(sec[4] | (sec[5] << 8) | (sec[6] << 16) | ((u32)sec[7] << 24));
    if (g_super.magic != FS_DISK_MAGIC) {
        serial_puts("[fs] no filesystem (run 'fs init')\r\n");
        g_fs_ready = 0;
        return 1;   /* 未格式化 */
    }
    g_fs_ready = 1;
    fs_dir_load();
    serial_printf("[fs] mounted: %u files, data from LBA %u\r\n",
                  (unsigned)g_super.nfiles, (unsigned)g_super.data_start);
    return 0;
}

int fs_disk_format(void)
{
    u8 sec[512];
    int i;
    if (!g_ata.present) return -1;

    fs_zero(sec, sizeof(sec));
    sec[0] = (u8)(FS_DISK_MAGIC & 0xFF);        sec[1] = (u8)((FS_DISK_MAGIC >> 8) & 0xFF);
    sec[2] = (u8)(FS_DISK_VERSION & 0xFF);      sec[3] = (u8)((FS_DISK_VERSION >> 8) & 0xFF);
    sec[4] = 0; sec[5] = 0; sec[6] = 0; sec[7] = 0;                    /* nfiles=0 */
    sec[8] = FS_DIR_LBA;                   sec[9] = 0;
    sec[10] = FS_DIR_SECS;                 sec[11] = 0;
    sec[12] = (u8)(FS_DATA_START & 0xFF);  sec[13] = (u8)((FS_DATA_START >> 8) & 0xFF);
    sec[14] = 0; sec[15] = 0;
    if (ata_write_sector(1, sec) != 0) return -3;

    /* 清目录区 */
    for (i = 0; i < FS_DIR_SECS; i++) {
        fs_zero(g_dir_raw + i * 512, 512);
        if (ata_write_sector(FS_DIR_LBA + (u32)i, g_dir_raw + i * 512) != 0) return -4;
    }
    g_super.magic = FS_DISK_MAGIC;
    g_super.version = FS_DISK_VERSION;
    g_super.nfiles = 0;
    g_super.data_start = FS_DATA_START;
    g_fs_ready = 1;
    serial_puts("[fs] formatted: superblock + 32 dir slots\r\n");
    return 0;
}

static fs_dir_entry_t *fs_ent(int i)
{
    return (fs_dir_entry_t *)(g_dir_raw + i * 32);
}

/* 找第一个空数据扇区（从数据区起始扫描） */
static u32 fs_find_free_data(void)
{
    u32 lba;
    fs_dir_entry_t *e;
    for (lba = FS_DATA_START; lba < g_ata.total_sectors; lba++) {
        int taken = 0, i;
        for (i = 0; i < FS_DISK_MAX_FILES; i++) {
            e = fs_ent(i);
            if (e->used && e->start_lba == lba) { taken = 1; break; }
        }
        if (!taken) return lba;
    }
    return 0;
}

int fs_disk_mkfile(const char *name, const u8 *data, u32 size)
{
    int i, slot = -1;
    u32 lba;
    u8 sec[512];
    fs_dir_entry_t *e;

    if (!g_fs_ready) return -10;
    if (fs_strlen(name) == 0 || fs_strlen(name) > FS_NAME_MAX) return -11;
    if (size > 512) size = 512;
    if (g_super.nfiles >= FS_DISK_MAX_FILES) return -12;

    /* 重名检查 + 找空槽 */
    for (i = 0; i < FS_DISK_MAX_FILES; i++) {
        e = fs_ent(i);
        if (e->used && fs_strcmp(e->name, name) == 0) return -13;
        if (!e->used && slot < 0) slot = i;
    }
    if (slot < 0) return -12;

    lba = fs_find_free_data();
    if (lba == 0) return -14;

    /* 写数据扇区 */
    fs_zero(sec, sizeof(sec));
    for (i = 0; i < (int)size; i++) sec[i] = data[i];
    if (ata_write_sector(lba, sec) != 0) return -15;

    /* 写目录项 */
    e = fs_ent(slot);
    fs_zero((u8 *)e, 32);
    for (i = 0; name[i] && i <= FS_NAME_MAX; i++) e->name[i] = name[i];
    e->used = 1;
    e->type = 0;
    e->size = size;
    e->start_lba = lba;
    g_super.nfiles++;
    fs_dir_save();

    /* 回写超级块 nfiles */
    {
        u8 sb[512];
        ata_read_sector(1, sb);
        sb[4] = (u8)(g_super.nfiles & 0xFF);
        sb[5] = (u8)((g_super.nfiles >> 8) & 0xFF);
        sb[6] = (u8)((g_super.nfiles >> 16) & 0xFF);
        sb[7] = (u8)((g_super.nfiles >> 24) & 0xFF);
        ata_write_sector(1, sb);
    }
    serial_printf("[fs] created '%s' size=%u lba=%u (files=%u)\r\n",
                  name, (unsigned)size, (unsigned)lba, (unsigned)g_super.nfiles);
    return 0;
}

int fs_disk_cat(const char *name, u8 *buf)
{
    int i;
    u8 sec[512];
    fs_dir_entry_t *e;
    if (!g_fs_ready) return -10;
    for (i = 0; i < FS_DISK_MAX_FILES; i++) {
        e = fs_ent(i);
        if (e->used && fs_strcmp(e->name, name) == 0) {
            if (ata_read_sector(e->start_lba, sec) != 0) return -15;
            for (i = 0; i < (int)e->size; i++) buf[i] = sec[i];
            return (int)e->size;
        }
    }
    return -13;   /* 不存在 */
}

int fs_disk_ls(void)
{
    int i, n = 0;
    fs_dir_entry_t *e;
    if (!g_fs_ready) return -10;
    vga_printf("disk files (%u):\n", (unsigned)g_super.nfiles);
    for (i = 0; i < FS_DISK_MAX_FILES; i++) {
        e = fs_ent(i);
        if (e->used) {
            vga_printf("  %-15s  %5u B  LBA %u\n", e->name, (unsigned)e->size, (unsigned)e->start_lba);
            n++;
        }
    }
    if (n == 0) vga_printf("  (empty)\n");
    return n;
}

int fs_disk_rm(const char *name)
{
    int i;
    fs_dir_entry_t *e;
    if (!g_fs_ready) return -10;
    for (i = 0; i < FS_DISK_MAX_FILES; i++) {
        e = fs_ent(i);
        if (e->used && fs_strcmp(e->name, name) == 0) {
            fs_zero((u8 *)e, 32);
            e->used = 0;
            if (g_super.nfiles > 0) g_super.nfiles--;
            fs_dir_save();
            /* 回写超级块 nfiles */
            {
                u8 sb[512];
                ata_read_sector(1, sb);
                sb[4] = (u8)(g_super.nfiles & 0xFF);
                sb[5] = (u8)((g_super.nfiles >> 8) & 0xFF);
                sb[6] = (u8)((g_super.nfiles >> 16) & 0xFF);
                sb[7] = (u8)((g_super.nfiles >> 24) & 0xFF);
                ata_write_sector(1, sb);
            }
            serial_printf("[fs] removed '%s' (files=%u)\r\n", name, (unsigned)g_super.nfiles);
            return 0;
        }
    }
    return -13;
}
