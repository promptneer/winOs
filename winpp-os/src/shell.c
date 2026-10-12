#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "rtc.h"
#include "demo.h"
#include "io.h"
#include "serial.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "sched.h"

extern u32 g_mem_kb;
extern char g_cpu_vendor[13];
extern void kernel_redraw_shell(void);
extern void kernel_reboot(void);
extern void kernel_poweroff(void);

/* 内嵌用户程序（build 时由 Python 从 userN.bin 生成 users_inc.c/.h 数组） */
#include "users_inc.h"
#include "fs.h"

/* ---------------- 极简 libc ---------------- */

static int k_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)((u8)*a - (u8)*b);
}

static int k_atoi(const char *s)
{
    int v = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

/* ---------------- 命令实现 ---------------- */

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("WinPP shell commands:\n");
    vga_printf("  help      show this list\n");
    vga_printf("  about     kernel version info\n");
    vga_printf("  echo <t>  print text\n");
    vga_printf("  clear     clear screen\n");
    vga_printf("  time      show CMOS clock\n");
    vga_printf("  uptime    show system uptime\n");
    vga_printf("  sysinfo   CPU / memory / ticks\n");
    vga_printf("  calc a o b  integer math (+ - * / %%)\n");
    vga_printf("  color     show VGA palette\n");
    vga_printf("  ls        list virtual filesystem\n");
    vga_printf("  demo      window UI demo\n");
    vga_printf("  beep      PC speaker beep\n");
    vga_printf("  paging    page table / fault stats\n");
    vga_printf("  meminfo   physical memory + heap usage\n");
    vga_printf("  alloc     kmalloc/kfree stress test\n");
    vga_printf("  pfault    trigger page fault + demand map\n");
    vga_printf("  run       start 3 user procs (round-robin)\n");
    vga_printf("  ps        list process table\n");
    vga_printf("  kill <pid>  terminate a user process\n");
    vga_printf("  stop      stop scheduler, back to shell\n");
    vga_printf("  reboot    restart machine\n");
    vga_printf("  poweroff  ACPI power off (QEMU)\n");
}

static void cmd_about(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("WinPP OS v0.1 -- a tiny unix-like kernel for i386\n");
    vga_printf("cpu: %s\n", g_cpu_vendor);
    vga_printf("compiled freestanding, multiboot v1, VGA text mode\n");
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) vga_putc(' ');
        vga_puts(argv[i]);
    }
    vga_putc('\n');
}

static void cmd_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    kernel_redraw_shell();
    vga_putc('\n');
}

static void cmd_time(int argc, char **argv)
{
    (void)argc; (void)argv;
    u16 yr; u8 mo, da, hh, mi, ss;
    rtc_get_datetime(&yr, &mo, &da, &hh, &mi, &ss);
    vga_printf("CMOS clock: %04u-%02u-%02u %02u:%02u:%02u\n", yr, mo, da, hh, mi, ss);
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 up = timer_uptime_sec();
    vga_printf("uptime: %02u:%02u:%02u (ticks=%u)\n",
               up / 3600, (up / 60) % 60, up % 60, timer_ticks());
}

static void cmd_sysinfo(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("CPU vendor : %s\n", g_cpu_vendor);
    vga_printf("RAM (mb)   : %u KB\n", g_mem_kb);
    vga_printf("ticks      : %u (%u Hz)\n", timer_ticks(), TIMER_HZ);
    vga_printf("video      : 80x25 VGA text, 16 colors\n");
}

static void cmd_calc(int argc, char **argv)
{
    if (argc != 4) {
        vga_printf("usage: calc <a> <op> <b>   (op = + - * / %%)\n");
        return;
    }
    int a = k_atoi(argv[1]);
    int b = k_atoi(argv[3]);
    char op = argv[2][0];
    int r = 0;
    switch (op) {
    case '+': r = a + b; break;
    case '-': r = a - b; break;
    case '*': r = a * b; break;
    case '/':
        if (b == 0) { vga_printf("calc: division by zero\n"); return; }
        r = a / b; break;
    case '%':
        if (b == 0) { vga_printf("calc: modulo by zero\n"); return; }
        r = a % b; break;
    default:
        vga_printf("calc: unknown operator '%c'\n", op);
        return;
    }
    vga_printf("%d %c %d = %d\n", a, op, b, r);
}

static void cmd_color(int argc, char **argv)
{
    (void)argc; (void)argv;
    u8 px, py;
    vga_get_pos(&px, &py);
    vga_printf("VGA palette (bg x fg):\n");
    for (u8 bg = 0; bg < 8; bg++) {
        vga_gotoxy(2, (u8)(py + 1 + bg));
        for (u8 fg = 0; fg < 8; fg++) {
            vga_putc_at((u8)(2 + fg), (u8)(py + 1 + bg), 0xDB, (u8)((bg << 4) | fg));
        }
    }
    vga_gotoxy(0, (u8)(py + 9));
}

static void cmd_ls(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("virtual root:\n");
    vga_printf("  bin/  dev/  etc/  proc/  usr/\n");
    vga_printf("  (no storage driver yet -- this is a demo vfs)\n");
}

static void cmd_demo(int argc, char **argv)
{
    (void)argc; (void)argv;
    demo_run();
    kernel_redraw_shell();   /* demo 退出后恢复 Shell 界面 */
    vga_putc('\n');
}

static void cmd_beep(int argc, char **argv)
{
    (void)argc; (void)argv;
    outb(0x43, 0xB6);
    u32 div = 1193182 / 880;
    outb(0x42, (u8)(div & 0xFF));
    outb(0x42, (u8)((div >> 8) & 0xFF));
    u8 t = inb(0x61);
    outb(0x61, (u8)(t | 3));
    u32 start = timer_ticks();
    while (timer_ticks() - start < TIMER_HZ / 10) { }
    outb(0x61, (u8)(t & ~3));
    vga_printf("beep.\n");
}

static void cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("rebooting...\n");
    kernel_reboot();
}

static void cmd_poweroff(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("powering off...\n");
    kernel_poweroff();
}

/* ---------------- 第二阶段：内存管理命令 ---------------- */

static void cmd_paging(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("paging enabled : %s\n", paging_enabled() ? "YES (CR0.PG)" : "no");
    vga_printf("cr3 (PD phys)  : %x\n", read_cr3());
    vga_printf("PD entries used: %u / 1024\n", pd_used_entries());
    vga_printf("self-map       : %x (PD[1023])\n", 0xFFC00000u);
    vga_printf("page faults    : %u (recoverable)\n", pf_count());
    vga_printf("4KB pages, identity map 1MB+; NULL unmapped\n");
}

static void cmd_meminfo(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 tp = pmm_total_pages(), up = pmm_used_pages();
    vga_printf("physical total : %u KB (%u pages)\n", tp * 4, tp);
    vga_printf("physical used  : %u pages, free: %u pages\n", up, tp - up);
    vga_printf("kernel heap    : %u KB total, %u KB used\n",
               kheap_total() / 1024, kheap_used() / 1024);
}

static void cmd_alloc(int argc, char **argv)
{
    (void)argc; (void)argv;
    static const u32 sizes[6] = { 16, 128, 1024, 7, 4096, 3 };
    u8 *ptrs[6];
    u32 before = kheap_used();

    vga_printf("kmalloc stress: heap used %u -> ", before);
    for (int i = 0; i < 6; i++) {
        ptrs[i] = (u8 *)kmalloc(sizes[i]);
        if (!ptrs[i]) { vga_printf("OOM at block %d!\n", i); return; }
        /* 写模式并回读校验 */
        for (u32 j = 0; j < sizes[i]; j++) ptrs[i][j] = (u8)(0xAA + i);
        int ok = 1;
        for (u32 j = 0; j < sizes[i]; j++) {
            if (ptrs[i][j] != (u8)(0xAA + i)) { ok = 0; break; }
        }
        vga_printf("[%d] %uB@%x%s ", i, sizes[i], (u32)ptrs[i], ok ? "ok" : "CORRUPT");
        serial_printf("[alloc] [%d] %uB@%x%s\r\n", i, sizes[i], (u32)ptrs[i], ok ? "ok" : "CORRUPT");
    }
    vga_printf("\n");
    serial_printf("[alloc] kfree pass\r\n");
    for (int i = 0; i < 6; i++) {
        serial_printf("[alloc] kfree(%d)=%x ...\r\n", i, (u32)ptrs[i]);
        kfree(ptrs[i]);
        serial_printf("[alloc] kfree(%d) done, used=%u\r\n", i, kheap_used());
    }
    vga_printf("all freed: heap used %u -> %u (leak=%u)\n",
               kheap_used(), before, kheap_used() - before);
    serial_printf("[alloc] all freed used=%u leak=%u\r\n", kheap_used(), kheap_used() - before);
}

static void cmd_pfault(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 before = pf_count();
    vga_printf("faults before: %u. writing 0x1234 to NULL...\n", before);
    *(volatile u32 *)0 = 0x1234;          /* 触发 #PF -> handler demand-map -> 重试 */
    u32 v = *(volatile u32 *)0;           /* 重试成功后读回 */
    vga_printf("faults after : %u. value read back: %x\n", pf_count(), v);
    vga_printf("%s\n", (v == 0x1234) ? "demand paging OK" : "MISMATCH!");
}

/* ---------------- 第三阶段：进程与调度命令 ---------------- */

static void cmd_run(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p1 = sched_create("user1", 0x400000, 0x701000,
                          _user1_code, USER1_LEN);
    int p2 = sched_create("user2", 0x500000, 0x711000,
                          _user2_code, USER2_LEN);
    int p3 = sched_create("user3", 0x600000, 0x721000,
                          _user3_code, USER3_LEN);
    vga_printf("created user procs: pid %d %d %d\n", p1, p2, p3);
    serial_printf("[run] created pid %d %d %d\r\n", p1, p2, p3);
    if (p1 > 0 && p2 > 0 && p3 > 0) {
        sched_start();
        vga_printf("scheduler started: 3 procs round-robin (0.1s quantum)\n");
        serial_printf("[run] scheduler started\r\n");
    } else {
        vga_printf("failed to create processes\n");
    }
}

static void cmd_ps(int argc, char **argv)
{
    (void)argc; (void)argv;
    sched_ps();
}

static void cmd_kill(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: kill <pid>\n"); return; }
    int pid = k_atoi(argv[1]);
    int r = sched_kill(pid);
    if (r == 0) {
        vga_printf("killed pid %d\n", pid);
        serial_printf("[kill] pid %d terminated\r\n", pid);
    } else {
        vga_printf("kill: no such pid %d\n", pid);
    }
}

static void cmd_stop(int argc, char **argv)
{
    (void)argc; (void)argv;
    sched_stop();
    vga_printf("scheduler stopped, all user procs terminated\n");
    serial_printf("[stop] scheduler stopped\r\n");
}

/* S4-2：启动 fork 演示进程（user4：fork 后父子各自独立运行） */
static void cmd_fork(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user4", 0x610000, 0x731000,
                         _user4_code, USER4_LEN);
    if (p > 0) {
        vga_printf("created fork-demo proc: pid %d\n", p);
        serial_printf("[fork] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: fork demo (0.1s quantum)\n");
        serial_printf("[fork] scheduler started\r\n");
    } else {
        vga_printf("failed to create fork-demo process\n");
    }
}

/* S4-3：启动 exec 演示进程（user5：打印后 exec 变身成 user2） */
static void cmd_exec(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user5", 0x620000, 0x741000,
                         _user5_code, USER5_LEN);
    if (p > 0) {
        vga_printf("created exec-demo proc: pid %d\n", p);
        serial_printf("[exec] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: exec demo (0.1s quantum)\n");
        serial_printf("[exec] scheduler started\r\n");
    } else {
        vga_printf("failed to create exec-demo process\n");
    }
}

/* S4-4：启动文件系统演示进程（user6：创建/写/读 hello.txt） */
static void cmd_fs(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user6", 0x630000, 0x751000,
                         _user6_code, USER6_LEN);
    if (p > 0) {
        vga_printf("created fs-demo proc: pid %d\n", p);
        serial_printf("[fs] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: fs demo (0.1s quantum)\n");
        serial_printf("[fs] scheduler started\r\n");
    } else {
        vga_printf("failed to create fs-demo process\n");
    }
}

/* S4-4：显示内存盘文件表 */
static void cmd_fslist(int argc, char **argv)
{
    (void)argc; (void)argv;
    fs_list();
}

/* ---------------- 命令表 ---------------- */

typedef struct {
    const char *name;
    const char *desc;
    void (*fn)(int argc, char **argv);
} cmd_t;

static const cmd_t cmds[] = {
    { "help",     "show this list",             cmd_help },
    { "about",    "kernel version info",        cmd_about },
    { "echo",     "print text",                 cmd_echo },
    { "clear",    "clear screen",               cmd_clear },
    { "time",     "show CMOS clock",            cmd_time },
    { "uptime",   "show system uptime",         cmd_uptime },
    { "sysinfo",  "CPU/memory/ticks",           cmd_sysinfo },
    { "calc",     "integer math",               cmd_calc },
    { "color",    "show VGA palette",           cmd_color },
    { "ls",       "list virtual fs",            cmd_ls },
    { "demo",     "window UI demo",             cmd_demo },
    { "beep",     "PC speaker beep",            cmd_beep },
    { "paging",   "page table / fault stats",   cmd_paging },
    { "meminfo",  "physical mem + heap",        cmd_meminfo },
    { "alloc",    "kmalloc/kfree stress",       cmd_alloc },
    { "pfault",   "page fault + demand map",    cmd_pfault },
    { "run",      "start 3 user procs",         cmd_run },
    { "fork",     "fork demo (user4)",          cmd_fork },
    { "exec",     "exec demo (user5->user2)",   cmd_exec },
    { "fs",       "fs demo (user6)",            cmd_fs },
    { "fslist",   "list ramdisk files",         cmd_fslist },
    { "ps",       "list processes",             cmd_ps },
    { "kill",     "kill <pid>",                 cmd_kill },
    { "stop",     "stop scheduler",             cmd_stop },
    { "reboot",   "restart machine",            cmd_reboot },
    { "poweroff", "ACPI power off (QEMU)",      cmd_poweroff },
};
#define NCMDS ((int)(sizeof(cmds) / sizeof(cmds[0])))

/* ---------------- 输入与分发 ---------------- */

static void shell_readline(char *buf, int max)
{
    int i = 0;
    for (;;) {
        int k = keyboard_getkey();
        serial_printf("[key %d]\r\n", k);
        if (k == KEY_ENTER) { vga_putc('\n'); buf[i] = 0; return; }
        if (k == KEY_BACKSP) {
            if (i > 0) { i--; vga_putc('\b'); }
            continue;
        }
        if (k == KEY_TAB || k == KEY_ESC) continue;
        if (k >= 32 && k < 127 && i < max - 1) {
            buf[i++] = (char)k;
            vga_putc((char)k);
        }
    }
}

/* ---------------- 屏幕转储（串口取证） ---------------- */

static void shell_dump_screen(const char *tag)
{
    volatile u16 *vbuf = (volatile u16 *)0xB8000;
    serial_printf("=== screen dump (%s) ===\r\n", tag);
    for (u32 row = 0; row < 25; row++) {
        serial_printf("r%02u|", row);
        for (u32 col = 0; col < 80; col++) {
            u8 ch = (u8)(vbuf[row * 80 + col] & 0xFF);
            if (ch >= 32 && ch < 127) serial_putc(ch);
            else serial_putc('.');
        }
        serial_puts("|\r\n");
    }
}

static void shell_dispatch(char *line)
{
    char *argv[8];
    int argc = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
        if (argc >= 8) break;
    }
    if (argc == 0) return;

    shell_dump_screen("before");
    for (int i = 0; i < NCMDS; i++) {
        if (k_strcmp(argv[0], cmds[i].name) == 0) {
            cmds[i].fn(argc, argv);
            shell_dump_screen("after");
            return;
        }
    }
    vga_printf("sh: command not found: %s\n", argv[0]);
    shell_dump_screen("after-unknown");
}

void shell_run(void)
{
    char line[128];
    for (;;) {
        vga_set_attr(0x0A);
        vga_printf("winpp@kernel:~$ ");
        vga_set_attr(0x07);
        shell_readline(line, sizeof(line));
        shell_dispatch(line);
    }
}
