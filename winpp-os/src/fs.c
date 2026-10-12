#include "fs.h"
#include "ipc.h"
#include "serial.h"
#include "vga.h"
#include "user.h"

/* S4-4 用户态文件系统：极简内存盘（tmpfs）。
 * 布局：16 个文件槽 x 4KB 数据区（静态数组，BSS 段）+ 每槽元数据表。
 * 语义：open(创建/读) -> write/read 从槽内 offset 推进 -> close 重置 offset。
 * 数据区位于内核内存（1MB+），用户程序只能经 int 0x80 系统调用访问，
 * 用户态无法直接触碰文件内容 —— 这正是"内核替用户管理数据"的隔离体现。
 * S5：槽的 type!=0 时 obj 指向 IPC 环形缓冲，read/write 分发到管道/FIFO。 */

#define FS_FILE_SIZE 4096

fs_ent_t fstab[FS_MAX_FILES];
static u8 fsdata[FS_MAX_FILES][FS_FILE_SIZE];
char fs_cwd[16] = "/";           /* S6: 内核当前目录（根为 "/"） */

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

static int f_strncmp_pre(const char *s, const char *pre)
{
    while (*pre) { if (*s != *pre) return 1; s++; pre++; }
    return 0;
}

/* S8: 权限检查 —— want: 4=读, 2=写, 1=可进入。
 * root(uid=0) 恒通过；否则按属主/他人位判断。 */
static int fs_can(u32 uid, u32 owner, u32 mode, int want)
{
    if (uid == 0) return 1;
    u32 bits = (uid == owner) ? ((mode >> 3) & 7) : (mode & 7);
    return (bits & (u32)want) != 0;
}

/* ---------------- S6：路径解析 ---------------- */

/* 相对路径拼接 fs_cwd -> 绝对路径（去掉尾部 '/'），写入 out。
 * name 以 '/' 开头视为绝对路径；"/" 本身保持 "/"。 */
int fs_resolve(const char *name, char *out, int outsz)
{
    char tmp[40];
    int ti = 0, i;
    if (!name || !*name) return -1;
    if (name[0] == '/') {
        for (i = 0; name[i] && ti < 39; i++) tmp[ti++] = name[i];
    } else {
        for (i = 0; fs_cwd[i] && ti < 39; i++) tmp[ti++] = fs_cwd[i];
        if (fs_cwd[1] && ti < 39) tmp[ti++] = '/';   /* cwd != "/" 时补分隔 */
        for (i = 0; name[i] && ti < 39; i++) tmp[ti++] = name[i];
    }
    while (ti > 1 && tmp[ti - 1] == '/') ti--;       /* 去尾部 '/' */
    if (ti <= 0) tmp[ti++] = '/';
    if (ti >= outsz) return -1;
    for (i = 0; i < ti; i++) out[i] = tmp[i];
    out[ti] = 0;
    return 0;
}

/* 取父目录路径："/docs/a.txt" -> "/docs"；"/a.txt" 或 "/" -> "/" */
static int fs_parent(const char *path, char *out, int outsz)
{
    int i, last = -1;
    for (i = 0; path[i]; i++) if (path[i] == '/') last = i;
    if (last <= 0) { out[0] = '/'; out[1] = 0; return 0; }
    if (last >= outsz) return -1;
    for (i = 0; i < last; i++) out[i] = path[i];
    out[last] = 0;
    return 0;
}

/* 判断某路径是否为目录（根目录恒为目录，不占槽） */
static int fs_is_dir(const char *path)
{
    if (f_streq(path, "/")) return 1;
    for (u32 i = 0; i < FS_MAX_FILES; i++)
        if (fstab[i].used && fstab[i].type == FS_TYPE_DIR &&
            f_streq(fstab[i].name, path))
            return 1;
    return 0;
}

/* S6: 创建目录（幂等：已存在返回 0）。父目录必须存在且是目录。 */
int fs_mkdir(const char *name)
{
    char path[16], par[16];
    u32 uid = user_current_uid();
    if (fs_resolve(name, path, 16) < 0) return -1;
    if (fs_is_dir(path)) { serial_printf("[fs] mkdir '%s' (exists)\r\n", path); return 0; }
    for (u32 i = 0; i < FS_MAX_FILES; i++)
        if (fstab[i].used && f_streq(fstab[i].name, path)) return -1;  /* 同名文件 */
    if (fs_parent(path, par, 16) < 0) return -1;
    if (!fs_is_dir(par)) { serial_printf("[fs] mkdir '%s' parent not dir\r\n", path); return -1; }
    /* S8: 父目录需写权限 */
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!fstab[i].used || fstab[i].type != FS_TYPE_DIR) continue;
        if (f_streq(fstab[i].name, par)) {
            if (!fs_can(uid, fstab[i].owner_uid, fstab[i].mode, 2)) {
                serial_printf("[fs] mkdir '%s' EACCES (parent not writable)\r\n", path);
                return -1;
            }
            break;
        }
    }
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used) continue;
        f_strcpy(fstab[i].name, path);
        fstab[i].size = 0;
        fstab[i].offset = 0;
        fstab[i].used = 1;
        fstab[i].type = FS_TYPE_DIR;
        fstab[i].obj = 0;
        fstab[i].owner_uid = uid;                          /* S8 */
        fstab[i].mode = MODE_W_RWX | MODE_O_RX;            /* 0755 */
        serial_printf("[fs] mkdir '%s' fd=%u uid=%u\r\n", path, i, uid);
        return 0;
    }
    return -1;
}

/* S6: 切换当前目录。目标必须存在且是目录。 */
int fs_chdir(const char *name)
{
    char path[16];
    u32 uid = user_current_uid();
    if (fs_resolve(name, path, 16) < 0) return -1;
    if (!fs_is_dir(path)) { serial_printf("[fs] chdir '%s' not a dir\r\n", path); return -1; }
    /* S8: 进入目录需 x 权限（根目录恒可进） */
    if (!f_streq(path, "/")) {
        for (u32 i = 0; i < FS_MAX_FILES; i++) {
            if (!fstab[i].used || fstab[i].type != FS_TYPE_DIR) continue;
            if (f_streq(fstab[i].name, path)) {
                if (!fs_can(uid, fstab[i].owner_uid, fstab[i].mode, 1)) {
                    serial_printf("[fs] chdir '%s' EACCES (no x)\r\n", path);
                    return -1;
                }
                break;
            }
        }
    }
    f_strcpy(fs_cwd, path);
    serial_printf("[fs] chdir '%s'\r\n", path);
    return 0;
}

static void f_puts_ram(char *dst, const char *s)
{
    while (*s) *dst++ = *s++;
    *dst = 0;
}

/* S8: 预置演示文件（root 所有）：
 *   /secret.txt  0600  root 私有：只有 root 能读
 *   /public.txt  0644  公开：任何用户可读
 *   /rootonly    0700  私有目录：只有 root 能进入 */
static void fs_seed(void)
{
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used) continue;
        f_puts_ram(fstab[i].name, "/secret.txt");
        fstab[i].size = 0; fstab[i].offset = 0; fstab[i].used = 1;
        fstab[i].type = 0; fstab[i].obj = 0;
        fstab[i].owner_uid = 0; fstab[i].mode = MODE_W_RW | MODE_O_NONE; /* 0600 */
        static const char sec[] = "TOP SECRET!\r\n";   /* 13B */
        for (u32 k = 0; k < 13; k++) fsdata[i][k] = (u8)sec[k];
        fstab[i].size = 13;
        serial_printf("[fs] seed /secret.txt (0600 root)\r\n");
        break;
    }
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used) continue;
        f_puts_ram(fstab[i].name, "/public.txt");
        fstab[i].size = 0; fstab[i].offset = 0; fstab[i].used = 1;
        fstab[i].type = 0; fstab[i].obj = 0;
        fstab[i].owner_uid = 0; fstab[i].mode = MODE_W_RW | MODE_O_R;   /* 0644 */
        static const char pub[] = "hello public!\r\n";  /* 15B */
        for (u32 k = 0; k < 15; k++) fsdata[i][k] = (u8)pub[k];
        fstab[i].size = 15;
        serial_printf("[fs] seed /public.txt (0644 root)\r\n");
        break;
    }
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used) continue;
        f_puts_ram(fstab[i].name, "/rootonly");
        fstab[i].size = 0; fstab[i].offset = 0; fstab[i].used = 1;
        fstab[i].type = FS_TYPE_DIR; fstab[i].obj = 0;
        fstab[i].owner_uid = 0; fstab[i].mode = MODE_W_RWX | MODE_O_NONE; /* 0700 */
        serial_printf("[fs] seed /rootonly (0700 root dir)\r\n");
        break;
    }
}

void fs_init(void)
{
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        fstab[i].used = 0;
        fstab[i].size = 0;
        fstab[i].offset = 0;
        fstab[i].name[0] = 0;
        fstab[i].type = 0;
        fstab[i].obj = 0;
        fstab[i].owner_uid = 0;
        fstab[i].mode = 0;
    }
    for (u32 i = 0; i < FS_MAX_FILES; i++)
        f_memset(fsdata[i], 0, FS_FILE_SIZE);
    fs_cwd[0] = '/'; fs_cwd[1] = 0;
    fs_seed();
    serial_puts("[fs] init: ramdisk 16 files x 4KB ready, cwd=/\r\n");
}

int fs_open(const char *name, int flags)
{
    char path[16], par[16];
    u32 uid = user_current_uid();
    if (!name) return -1;
    if (fs_resolve(name, path, 16) < 0) {
        serial_printf("[fs] open bad path '%s'\r\n", name);
        return -1;
    }
    /* 已存在：S8 权限检查（读模式查 r 位，写模式查 w 位），通过后重置偏移 */
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used && f_streq(fstab[i].name, path)) {
            if (fstab[i].type >= 1 && fstab[i].type <= 4) {  /* IPC 槽不设权限 */
                fstab[i].offset = 0;
                return (int)i;
            }
            int want = (flags & 1) ? 2 : 4;
            if (!fs_can(uid, fstab[i].owner_uid, fstab[i].mode, want)) {
                serial_printf("[fs] open '%s' EACCES uid=%u\r\n", path, uid);
                return -1;
            }
            fstab[i].offset = 0;
            serial_printf("[fs] open '%s' fd=%u (exist)\r\n", path, i);
            return (int)i;
        }
    }
    if (!(flags & 1)) return -1;             /* 不存在且非创建模式 */
    if (fs_parent(path, par, 16) < 0) return -1;
    if (!fs_is_dir(par)) {
        serial_printf("[fs] open '%s' parent not dir\r\n", path);
        return -1;
    }
    /* S8: 在父目录里创建文件需要父目录写权限 */
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!fstab[i].used || fstab[i].type != FS_TYPE_DIR) continue;
        if (f_streq(fstab[i].name, par)) {
            if (!fs_can(uid, fstab[i].owner_uid, fstab[i].mode, 2)) {
                serial_printf("[fs] open '%s' EACCES (parent not writable) uid=%u\r\n", path, uid);
                return -1;
            }
            break;
        }
    }
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (fstab[i].used) continue;
        f_strcpy(fstab[i].name, path);
        fstab[i].size = 0;
        fstab[i].offset = 0;
        fstab[i].used = 1;
        fstab[i].type = 0;                   /* S5: 文件槽 */
        fstab[i].obj = 0;
        fstab[i].owner_uid = uid;            /* S8: 创建者即属主 */
        fstab[i].mode = MODE_W_RW | MODE_O_R; /* 0644 */
        serial_printf("[fs] open created '%s' fd=%u uid=%u\r\n", path, i, uid);
        return (int)i;
    }
    return -1;                               /* 表满 */
}

int fs_write(int fd, const char *buf, int len)
{
    if (fd < 0 || fd >= FS_MAX_FILES || !fstab[fd].used || !buf) return -1;
    /* S5: 管道/FIFO 写端（type 2/4）-> 环形缓冲 */
    if (fstab[fd].type == 2 || fstab[fd].type == 4) {
        if (!fstab[fd].obj) return -1;
        return ipc_write((ipc_buf_t *)fstab[fd].obj, buf, len);
    }
    /* S8: 普通文件写权限检查（uid==0 root 恒通过） */
    if (fstab[fd].type == 0 &&
        !fs_can(user_current_uid(), fstab[fd].owner_uid, fstab[fd].mode, 2)) {
        serial_printf("[fs] write fd=%d EACCES uid=%u owner=%u mode=%x\r\n",
                      fd, user_current_uid(), fstab[fd].owner_uid, fstab[fd].mode);
        return -1;
    }
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
    /* S5: 管道/FIFO 读端（type 1/3）-> 环形缓冲 */
    if (fstab[fd].type == 1 || fstab[fd].type == 3) {
        if (!fstab[fd].obj) return -1;
        return ipc_read((ipc_buf_t *)fstab[fd].obj, buf, len);
    }
    /* S8: 普通文件读权限检查 */
    if (fstab[fd].type == 0 &&
        !fs_can(user_current_uid(), fstab[fd].owner_uid, fstab[fd].mode, 4)) {
        serial_printf("[fs] read fd=%d EACCES uid=%u owner=%u mode=%x\r\n",
                      fd, user_current_uid(), fstab[fd].owner_uid, fstab[fd].mode);
        return -1;
    }
    if (len < 0) len = 0;
    if (fstab[fd].offset + (u32)len > fstab[fd].size)
        len = (int)(fstab[fd].size - fstab[fd].offset);
    if (len <= 0) return 0;
    for (int i = 0; i < len; i++)
        buf[i] = fsdata[fd][fstab[fd].offset + (u32)i];
    fstab[fd].offset += (u32)len;
    return len;
}

/* S8: chmod(path, mode) —— 仅 root 或属主可改 */
int fs_chmod(const char *name, u32 mode)
{
    char path[16];
    u32 uid = user_current_uid();
    if (fs_resolve(name, path, 16) < 0) return -1;
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!fstab[i].used) continue;
        if (f_streq(fstab[i].name, path)) {
            if (uid != 0 && uid != fstab[i].owner_uid) {
                serial_printf("[fs] chmod '%s' EACCES uid=%u\r\n", path, uid);
                return -1;
            }
            fstab[i].mode = mode & 0x3F;
            serial_printf("[fs] chmod '%s' mode=%x uid=%u\r\n", path, mode, uid);
            return 0;
        }
    }
    return -1;
}

int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_FILES || !fstab[fd].used) return -1;
    fstab[fd].offset = 0;
    return 0;
}

/* S8: 权限字符串 "rwxrwxrwx"（owner+other 6 位） */
static void mode_str(u32 mode, char *out)
{
    static const char bits[] = "rwxrwxrwx";
    for (int i = 0; i < 6; i++) out[i] = (mode & (0x20 >> i)) ? bits[i] : '-';
    out[6] = 0;
}

void fs_list(void)
{
    u32 n = 0;
    vga_printf("NAME            MODE      OWNER  SIZE   FD\n");
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!fstab[i].used) continue;
        char ms[7];
        mode_str(fstab[i].mode, ms);
        if (fstab[i].type == FS_TYPE_DIR)
            vga_printf("%-14s/ %-8s %-5u %5s  %u\n", fstab[i].name, ms,
                       fstab[i].owner_uid, "-", i);
        else
            vga_printf("%-14s  %-8s %-5u %5u  %u\n", fstab[i].name, ms,
                       fstab[i].owner_uid, fstab[i].size, i);
        n++;
    }
    serial_printf("[fs] list: %u entries (cwd=%s)\r\n", n, fs_cwd);
}

/* S6: 列目录 —— 列出 path 直属的条目（前缀匹配 + 不含更深分隔符）。
 * 目录显示 "name/"，文件显示 "name size"。 */
void fs_ls(const char *arg)
{
    char path[16], pref[16];
    int i, n = 0, pl;
    if (fs_resolve(arg ? arg : "/", path, 16) < 0) {
        vga_printf("ls: bad path\n");
        return;
    }
    if (!fs_is_dir(path)) {
        vga_printf("ls: not a directory: %s\n", path);
        return;
    }
    if (f_streq(path, "/")) { pref[0] = '/'; pref[1] = 0; }
    else {
        for (pl = 0; path[pl]; pl++) pref[pl] = path[pl];
        pref[pl] = '/'; pref[pl + 1] = 0;
    }
    if (f_streq(path, "/")) {
        vga_printf("/:\n");
        serial_printf("[ls] /:\r\n");
    } else {
        vga_printf("%s/:\n", path);
        serial_printf("[ls] %s/:\r\n", path);
    }
    for (u32 s = 0; s < FS_MAX_FILES; s++) {
        if (!fstab[s].used) continue;
        if (f_strncmp_pre(fstab[s].name, pref) != 0) continue;  /* 前缀不匹配 */
        const char *rest = fstab[s].name;
        for (i = 0; pref[i]; i++) rest++;
        int deeper = 0;
        for (i = 0; rest[i]; i++) if (rest[i] == '/') { deeper = 1; break; }
        if (deeper) continue;                                   /* 子目录里的 */
        char ms[7];
        mode_str(fstab[s].mode, ms);
        if (fstab[s].type == FS_TYPE_DIR) {
            vga_printf("  %-12s/ %s %s\n", rest, ms, user_name_of(fstab[s].owner_uid));
            serial_printf("  %s/ %s %s\r\n", rest, ms, user_name_of(fstab[s].owner_uid));
        } else {
            vga_printf("  %-12s %s %s %5u\n", rest, ms, user_name_of(fstab[s].owner_uid), fstab[s].size);
            serial_printf("  %s %s %s %u\r\n", rest, ms, user_name_of(fstab[s].owner_uid), fstab[s].size);
        }
        n++;
    }
    vga_printf("%d entries\n", n);
    serial_printf("[ls] %d entries\r\n", n);
}
