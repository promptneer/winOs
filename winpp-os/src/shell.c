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
#include "device.h"
#include "user.h"
#include "gui.h"
#include "ne2k.h"
#include "net.h"
#include "tcp.h"
#include "vt.h"

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

/* S8: 八进制解析（chmod 用，如 644/400/755） */
static int k_octal(const char *s)
{
    int v = 0;
    while (*s >= '0' && *s <= '7') { v = v * 8 + (*s - '0'); s++; }
    return v;
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
    vga_printf("  ls <path>  list directory contents\n");
    vga_printf("  mkdir <path>  create directory\n");
    vga_printf("  cd <path>  change current directory\n");
    vga_printf("  pwd        print working directory\n");
    vga_printf("  login <user>  switch identity (root/guest)\n");
    vga_printf("  whoami     print current user\n");
    vga_printf("  id         print uid/gid\n");
    vga_printf("  chmod <oct> <path>  change file mode\n");
    vga_printf("  demo      window UI demo\n");
    vga_printf("  beep      PC speaker beep\n");
    vga_printf("  paging    page table / fault stats\n");
    vga_printf("  meminfo   physical memory + heap usage\n");
    vga_printf("  alloc     kmalloc/kfree stress test\n");
    vga_printf("  pfault    trigger page fault + demand map\n");
    vga_printf("  run       start 3 user procs (round-robin)\n");
    vga_printf("  pipe      pipe demo (user7)\n");
    vga_printf("  fifo      named-pipe demo (user8)\n");
    vga_printf("  mq        message-queue demo (user9)\n");
    vga_printf("  ps        list process table\n");
    vga_printf("  kill <pid>  terminate a user process\n");
    vga_printf("  stop      stop scheduler, back to shell\n");
    vga_printf("  net       NIC info + registers\n");
    vga_printf("  ip <ip>   set/show local IP (+gw/host port)\n");
    vga_printf("  gw <ip>   set/show gateway (=host side)\n");
    vga_printf("  arp       show/add ARP table\n");
    vga_printf("  ping <ip> ICMP echo (waits 1s for reply)\n");
    vga_printf("  udpsend <ip> <port> <text>  send UDP\n");
    vga_printf("  host <text>  send UDP to host (WinPP console)\n");
    vga_printf("  hostport [n]  host console UDP port\n");
    vga_printf("  udptest   UDP loopback self-test\n");
    vga_printf("  chatd <port> [echo|cmd]  UDP chat/cmd server\n");
    vga_printf("  netstat   addr/ARP/endpoints/stats\n");
    vga_printf("  vt [id]   list / switch virtual terminals\n");
    vga_printf("  msg <id> <text>  send msg to terminal\n");
    vga_printf("  msgs      show my terminal inbox\n");
    vga_printf("  ts        terminal status table\n");
    vga_printf("  tkill <id>  reset terminal session\n");
    vga_printf("  tcp listen <port> / connect <ip> <port>\n");
    vga_printf("  tcp send <text> / tcp recv\n");
    vga_printf("  tcpstat   TCP connection state table\n");
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
    fs_ls((argc >= 2) ? argv[1] : 0);
}

/* S6-1：mkdir <path> —— 创建目录 */
static void cmd_mkdir(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: mkdir <path>\n"); return; }
    int r = fs_mkdir(argv[1]);
    if (r == 0) vga_printf("mkdir ok: %s\n", argv[1]);
    else vga_printf("mkdir failed: %s\n", argv[1]);
}

/* S6-2：cd <path> —— 切换当前目录 */
static void cmd_cd(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: cd <path>\n"); return; }
    int r = fs_chdir(argv[1]);
    if (r == 0) {
        vga_printf("cd ok: %s\n", fs_cwd);
        vt_save_cwd();              /* S12-1: 把新 cwd 存回当前终端 */
    }
    else vga_printf("cd failed: %s\n", argv[1]);
}

/* S12-1：vt [id] —— 列出虚拟终端 / 切换终端 */
static void cmd_vt(int argc, char **argv)
{
    if (argc >= 2) {
        int id = k_atoi(argv[1]);
        if (vt_switch((u8)id) == 0)
            serial_printf("[vt] cmd switch -> VT%d\r\n", id);
        else
            vga_printf("vt: bad id (1..%d)\n", VT_MAX);
        return;
    }
    vt_list();
}

/* S12-3：msg <id> <text> —— 发消息到另一个终端 */
static void cmd_msg(int argc, char **argv)
{
    if (argc < 3) { vga_printf("usage: msg <id> <text>\n"); return; }
    int id = k_atoi(argv[1]);
    char text[VT_MSG_LEN];
    int n = 0;
    for (int a = 2; a < argc && n < VT_MSG_LEN - 1; a++) {
        for (const char *p = argv[a]; *p && n < VT_MSG_LEN - 1; p++) text[n++] = *p;
        if (a + 1 < argc && n < VT_MSG_LEN - 1) text[n++] = ' ';
    }
    text[n] = 0;
    if (vt_msg_send((u8)id, text) == 0) {
        vga_printf("msg sent to VT%d: %s\n", id, text);
    } else {
        vga_printf("msg: bad id (1..%d)\n", VT_MAX);
    }
}

/* S12-3：msgs —— 查看当前终端收件箱 */
static void cmd_msgs(int argc, char **argv)
{
    (void)argc; (void)argv;
    vt_msg_show();
}

/* S12-4：ts —— 终端状态表 */
static void cmd_ts(int argc, char **argv)
{
    (void)argc; (void)argv;
    vt_list();
}

/* S12-4：tkill <id> —— 重置（关闭）非活动终端会话 */
static void cmd_tkill(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: tkill <id>\n"); return; }
    int id = k_atoi(argv[1]);
    if (!vt_killable((u8)id)) {
        vga_printf("tkill: cannot kill active terminal (or bad id)\n");
        return;
    }
    vt_reset((u8)id);
    vga_printf("VT%d reset (clean session)\n", id);
}

/* ---------- S13 TCP ---------- */

static int parse_ip(const char *s, u8 *out);   /* S11 网络命令区定义 */

static tcp_sock_t *tcp_first_established(void)
{
    tcp_sock_t *s = tcp_first();
    while (s && s->state != TCP_ESTABLISHED) s = tcp_next(s);
    return s;
}

/* tcp listen <port> —— 监听端口（收到 SYN 自动完成握手） */
static void cmd_tcp_listen(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: tcp listen <port>\n"); return; }
    int port = k_atoi(argv[1]);
    if (port < 1 || port > 65535) { vga_printf("tcp: bad port\n"); return; }
    if (tcp_listen((u16)port))
        vga_printf("tcp: listening on :%d\n", port);
    else
        vga_printf("tcp: listen failed (port busy or table full)\n");
}

/* tcp connect <ip> <port> —— 主动连接（三次握手） */
static void cmd_tcp_connect(int argc, char **argv)
{
    if (argc != 3) { vga_printf("usage: tcp connect <a.b.c.d> <port>\n"); return; }
    u8 ip[4];
    if (parse_ip(argv[1], ip) != 0) { vga_printf("tcp: bad ip\n"); return; }
    int port = k_atoi(argv[2]);
    if (tcp_connect(ip, (u16)port))
        vga_printf("tcp: connecting to %u.%u.%u.%u:%d (SYN sent)\n",
                   ip[0], ip[1], ip[2], ip[3], port);
    else
        vga_printf("tcp: connect failed (table full)\n");
}

/* tcp send <text> —— 在第一条已建立连接上发数据 */
static void cmd_tcp_send(int argc, char **argv)
{
    if (argc < 2) { vga_printf("usage: tcp send <text>\n"); return; }
    tcp_sock_t *s = tcp_first_established();
    if (!s) { vga_printf("tcp: no established connection\n"); return; }
    char text[TCP_BUF_MAX];
    int n = 0;
    for (int a = 1; a < argc && n < TCP_BUF_MAX - 1; a++) {
        for (const char *p = argv[a]; *p && n < TCP_BUF_MAX - 1; p++) text[n++] = *p;
        if (a + 1 < argc && n < TCP_BUF_MAX - 1) text[n++] = ' ';
    }
    text[n] = 0;
    if (tcp_send(s, text, (u16)n) == 0)
        vga_printf("tcp: sent %d bytes\n", n);
    else
        vga_printf("tcp: send failed (busy or not established)\n");
}

/* tcp recv —— 打印接收缓冲 */
static void cmd_tcp_recv(int argc, char **argv)
{
    (void)argc; (void)argv;
    tcp_sock_t *s = tcp_first_established();
    if (!s) { vga_printf("tcp: no established connection\n"); return; }
    if (s->rlen == 0) { vga_printf("tcp: (no data received)\n"); return; }
    vga_printf("tcp: %u bytes:\n", (unsigned)s->rlen);
    for (u16 i = 0; i < s->rlen; i++) vga_putc((char)s->rbuf[i]);
    vga_putc('\n');
    s->rlen = 0;
}

/* tcp <subcmd> —— 分发 */
static void cmd_tcp(int argc, char **argv)
{
    if (argc < 2) {
        vga_printf("usage: tcp listen <port> | connect <ip> <port> | send <text> | recv\n");
        return;
    }
    if (k_strcmp(argv[1], "listen") == 0)      { cmd_tcp_listen(argc - 1, argv + 1); return; }
    if (k_strcmp(argv[1], "connect") == 0)     { cmd_tcp_connect(argc - 1, argv + 1); return; }
    if (k_strcmp(argv[1], "send") == 0)        { cmd_tcp_send(argc - 1, argv + 1); return; }
    if (k_strcmp(argv[1], "recv") == 0)        { cmd_tcp_recv(argc - 1, argv + 1); return; }
    vga_printf("tcp: unknown subcommand '%s'\n", argv[1]);
}

/* tcpstat —— 连接状态表 */
static void cmd_tcpstat(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("state      lport  rip:rport       snd/una  rcv_nxt  buf\n");
    tcp_sock_t *s = tcp_first();
    while (s) {
        vga_printf("%-10s %-5u ", tcp_state_name(s->state), (unsigned)s->lport);
        if (s->rport)
            vga_printf("%u.%u.%u.%u:%u  ", s->rip[0], s->rip[1], s->rip[2], s->rip[3], (unsigned)s->rport);
        else
            vga_printf("(any)          ");
        vga_printf("%u/%u  %u  %u/%u\n",
                   s->snd_seq, s->snd_una,
                   s->rcv_nxt, (unsigned)s->slen, (unsigned)s->rlen);
        s = tcp_next(s);
    }
    serial_printf("[tcp] stat: rx=%u tx=%u retx=%u syn=%u\r\n",
                  g_tcp_rx, g_tcp_tx,
                  g_tcp_retx, g_tcp_syn);
}

/* S6-2：pwd —— 显示当前目录 */
static void cmd_pwd(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("%s\n", fs_cwd);
}

/* ---------------- S8：用户与权限命令 ---------------- */

/* S8: login <user> —— 切换当前身份（root/guest，无密码演示版） */
static void cmd_login(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: login <root|guest>\n"); return; }
    if (user_login(argv[1]) == 0) {
        vga_printf("logged in as %s (uid=%u)\n", user_name_of(user_current_uid()), user_current_uid());
        serial_printf("[login] ok -> %s uid=%u\r\n", user_name_of(user_current_uid()), user_current_uid());
    } else {
        vga_printf("login: no such user: %s\n", argv[1]);
        serial_printf("[login] denied '%s'\r\n", argv[1]);
    }
}

/* S8: whoami —— 当前用户名 */
static void cmd_whoami(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("%s\n", user_name_of(user_current_uid()));
    serial_printf("[whoami] %s (uid=%u)\r\n", user_name_of(user_current_uid()), user_current_uid());
}

/* S8: id —— 打印 uid/gid（gid=uid，简化版） */
static void cmd_id(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 uid = user_current_uid();
    vga_printf("uid=%u(%s) gid=%u(%s)\n", uid, user_name_of(uid), uid, user_name_of(uid));
    serial_printf("[id] uid=%u gid=%u user=%s\r\n", uid, uid, user_name_of(uid));
}

/* S8: chmod <oct> <path> —— 改权限（八进制，如 400/644/755） */
static void cmd_chmod(int argc, char **argv)
{
    if (argc != 3) { vga_printf("usage: chmod <oct> <path>\n"); return; }
    int m = k_octal(argv[1]);
    if (m < 0 || m > 0x1FF) { vga_printf("chmod: bad mode (octal 000-777)\n"); return; }
    int r = fs_chmod(argv[2], (u32)m);
    if (r == 0) vga_printf("chmod ok: %s mode=%s\n", argv[2], argv[1]);
    else vga_printf("chmod failed: %s (EACCES or no such file)\n", argv[2]);
}

/* S8: fs11 —— 权限演示进程（user11：getuid + 公开/私有文件读写） */
static void cmd_fs11(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user11", 0x680000, 0x7A1000,
                         _user11_code, USER11_LEN);
    if (p > 0) {
        vga_printf("created perm-demo proc: pid %d (uid=%u)\n", p, user_current_uid());
        serial_printf("[fs11] demo proc pid %d uid=%u\r\n", p, user_current_uid());
        sched_start();
        vga_printf("scheduler started: permission demo (0.1s quantum)\n");
        serial_printf("[fs11] scheduler started\r\n");
    } else {
        vga_printf("failed to create perm-demo process\n");
    }
}

/* S9：信号量互斥 + sleep 演示（user12：两个子进程争一把互斥锁） */
static void cmd_syncdemo(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user12", 0x690000, 0x7B1000,
                         _user12_code, USER12_LEN);
    if (p > 0) {
        vga_printf("created sync-demo proc: pid %d\n", p);
        serial_printf("[sync] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: semaphore/mutex + sleep demo\n");
        serial_printf("[sync] scheduler started\r\n");
    } else {
        vga_printf("failed to create sync-demo process\n");
    }
}

/* S10：进入图形界面窗口系统（VGA Mode13h；'q' 退出回文本） */
static void cmd_gui(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("stop scheduler first\n"); return; }
    vga_printf("entering GUI (VGA 13h 320x200) ... press q to exit\n");
    serial_puts("[gui] cmd gui\r\n");
    gui_run();
    vga_printf("back to text mode\n");
}

/* ============ S11 网络命令 ============ */

static int parse_ip(const char *s, u8 *out)
{
    int part[4];
    const char *p = s;
    for (int i = 0; i < 4; i++) {
        part[i] = k_atoi(p);
        if (part[i] < 0 || part[i] > 255) return -1;
        while (*p >= '0' && *p <= '9') p++;
        if (i < 3) { if (*p == '.') p++; else return -1; }
    }
    for (int i = 0; i < 4; i++) out[i] = (u8)part[i];
    return 0;
}

static int parse_mac(const char *s, u8 *out)
{
    for (int i = 0; i < 6; i++) {
        int hi = -1, lo = -1;
        char c = s[0];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
        c = s[1];
        if (c >= '0' && c <= '9') lo = c - '0';
        else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;
        if (hi < 0 || lo < 0) return -1;
        out[i] = (u8)((hi << 4) | lo);
        s += 2;
        if (i < 5) { if (*s == ':') s++; else return -1; }
    }
    return 0;
}

/* net：网卡信息 + 寄存器诊断 */
static void cmd_net(int argc, char **argv)
{
    (void)argc; (void)argv;
    u8 isr, curr, bnry;
    u8 ready = ne2k_regs_diag(&isr, &curr, &bnry);
    vga_printf("NE2000 NIC %s\n", ready ? "READY" : "NOT READY");
    vga_printf("  IO=0x%X  IRQ=%d  MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
               NE2K_IO, NE2K_IRQ,
               g_local_mac[0], g_local_mac[1], g_local_mac[2],
               g_local_mac[3], g_local_mac[4], g_local_mac[5]);
    vga_printf("  ISR=%02X CURR=%02X BNRY=%02X\n", isr, curr, bnry);
    vga_printf("  IP=%u.%u.%u.%u\n",
               g_local_ip[0], g_local_ip[1], g_local_ip[2], g_local_ip[3]);
    serial_printf("[net] cmd net: ready=%u ISR=%02X CURR=%02X BNRY=%02X\r\n",
                  ready, isr, curr, bnry);
}

/* ip [a.b.c.d]：设置/显示本机 IP（连带显示网关与宿主通讯端口） */
static void cmd_ip(int argc, char **argv)
{
    if (argc == 2) {
        u8 ip[4];
        if (parse_ip(argv[1], ip)) { vga_printf("bad ip\n"); return; }
        net_set_ip(ip);
        vga_printf("ip set: %u.%u.%u.%u (ARP 表已清，下一包重新解析)\n",
                   ip[0], ip[1], ip[2], ip[3]);
        serial_printf("[net] ip set %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
        net_arp_announce();
    }
    vga_printf("IP:  %u.%u.%u.%u\n",
               g_local_ip[0], g_local_ip[1], g_local_ip[2], g_local_ip[3]);
    vga_printf("GW:  %u.%u.%u.%u   (host side)   host port: %u\n",
               g_local_gw[0], g_local_gw[1], g_local_gw[2], g_local_gw[3],
               (u32)g_host_port);
    vga_printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
               g_local_mac[0], g_local_mac[1], g_local_mac[2],
               g_local_mac[3], g_local_mac[4], g_local_mac[5]);
}

/* gw [a.b.c.d]：设置/显示网关（= 宿主侧地址） */
static void cmd_gw(int argc, char **argv)
{
    if (argc == 2) {
        u8 ip[4];
        if (parse_ip(argv[1], ip)) { vga_printf("bad ip\n"); return; }
        net_set_gw(ip);
        serial_printf("[net] gw set\r\n");
    }
    vga_printf("GW: %u.%u.%u.%u\n",
               g_local_gw[0], g_local_gw[1], g_local_gw[2], g_local_gw[3]);
}

/* arp：显示 ARP 表；arp add <ip> <mac> 手动加 */
static void cmd_arp(int argc, char **argv)
{
    if (argc >= 4 && k_strcmp(argv[1], "add") == 0) {
        u8 ip[4], mac[6];
        if (parse_ip(argv[2], ip) || parse_mac(argv[3], mac)) {
            vga_printf("usage: arp add <ip> <mac>\n"); return;
        }
        arp_add(ip, mac);
        vga_printf("arp: added\n");
        return;
    }
    vga_printf("ARP table (rx=%u):\n", (u32)g_arp_rx);
    arp_ent_t *e;
    u8 idx = 0;
    extern arp_ent_t *net_arp_table(int *n);
    int n = 0;
    e = net_arp_table(&n);
    for (int i = 0; i < n; i++) {
        if (!e[i].used) continue;
        vga_printf("  %u.%u.%u.%u -> %02X:%02X:%02X:%02X:%02X:%02X\n",
                   e[i].ip[0], e[i].ip[1], e[i].ip[2], e[i].ip[3],
                   e[i].mac[0], e[i].mac[1], e[i].mac[2],
                   e[i].mac[3], e[i].mac[4], e[i].mac[5]);
        idx++;
    }
    if (idx == 0) vga_printf("  (empty)\n");
}

/* ping <ip>：ICMP echo；发完真等 1 秒看有没有回包（中断收包），
 * 自环与真链路都会给出明确结论。 */
static void cmd_ping(int argc, char **argv)
{
    if (argc != 2) { vga_printf("usage: ping <ip>\n"); return; }
    u8 ip[4];
    if (parse_ip(argv[1], ip)) { vga_printf("bad ip\n"); return; }
    vga_printf("ping %u.%u.%u.%u ...\n", ip[0], ip[1], ip[2], ip[3]);
    g_ping_rx = 0;
    net_ping(ip);

    u32 t0 = timer_ticks();
    while (timer_ticks() - t0 < TIMER_HZ) {      /* 最多等 1 秒，IRQ 里收包 */
        if (g_ping_rx) break;
    }
    if (g_ping_rx > 0) {
        vga_printf("Reply from %u.%u.%u.%u: seq=1 OK (%u)\n",
                   ip[0], ip[1], ip[2], ip[3], (u32)g_ping_rx);
        serial_printf("[net] ping: REPLY OK from %u.%u.%u.%u\r\n",
                      ip[0], ip[1], ip[2], ip[3]);
    } else {
        vga_printf("no reply (arp_req=%u arp_rep=%u)\n",
                   (u32)g_arp_req, (u32)g_arp_rep);
        serial_printf("[net] ping: TIMEOUT arp_req=%u arp_rep=%u\r\n",
                      (u32)g_arp_req, (u32)g_arp_rep);
    }
}

/* udpsend <ip> <port> <text>：发送 UDP 数据报 */
static void cmd_udpsend(int argc, char **argv)
{
    if (argc < 4) { vga_printf("usage: udpsend <ip> <port> <text>\n"); return; }
    u8 ip[4];
    int port = k_atoi(argv[2]);
    if (parse_ip(argv[1], ip) || port < 1 || port > 65535) {
        vga_printf("bad arg\n"); return;
    }
    const char *txt = argv[3];
    int len = 0;
    while (txt[len] && len < 500) len++;
    int rc = net_udp_send(ip, 40000, (u16)port, (const u8 *)txt, (u16)len);
    if (rc == 0) {
        vga_printf("udp: sent %d bytes to %u.%u.%u.%u:%d\n",
                   len, ip[0], ip[1], ip[2], ip[3], port);
        serial_printf("[net] udpsend ok len=%d\r\n", len);
    } else if (rc == -1) {
        vga_printf("udp: no ARP entry, ARP request sent (wait)\n");
    } else {
        vga_printf("udp: send failed rc=%d\n", rc);
    }
}

/* 把 argv[1..] 拼成一句（允许带空格） */
static char s_joinbuf[220];
static const char *join_args(int argc, char **argv, int from)
{
    int n = 0;
    for (int i = from; i < argc && n < (int)sizeof(s_joinbuf) - 2; i++) {
        if (i > from) s_joinbuf[n++] = ' ';
        for (const char *p = argv[i]; *p && n < (int)sizeof(s_joinbuf) - 2; p++)
            s_joinbuf[n++] = *p;
    }
    s_joinbuf[n] = 0;
    return s_joinbuf;
}

/* host <text>：把一句话发给"外面的 Windows"（UDP → 网关:宿主端口，默认 40001） */
static void cmd_host(int argc, char **argv)
{
    if (argc < 2) {
        vga_printf("usage: host <text>   ->  %u.%u.%u.%u:%u (host console)\n",
                   g_local_gw[0], g_local_gw[1], g_local_gw[2], g_local_gw[3],
                   (u32)g_host_port);
        return;
    }
    const char *txt = join_args(argc, argv, 1);
    int len = 0;
    while (txt[len] && len < 200) len++;
    int rc = net_udp_send(g_local_gw, 40000, g_host_port, (const u8 *)txt, (u16)len);
    if (rc == 0) {
        vga_printf("host: sent %d bytes -> %u.%u.%u.%u:%u\n", len,
                   g_local_gw[0], g_local_gw[1], g_local_gw[2], g_local_gw[3],
                   (u32)g_host_port);
        serial_printf("[net] host send ok len=%d port=%u\r\n", len, (u32)g_host_port);
    } else if (rc == -1) {
        vga_printf("host: queued, will go out after ARP reply\n");
        serial_puts("[net] host send pending (arp)\r\n");
    } else {
        vga_printf("host: send failed rc=%d\n", rc);
    }
}

/* hostport [n]：设置/显示宿主通讯台端口 */
static void cmd_hostport(int argc, char **argv)
{
    if (argc == 2) {
        int p = k_atoi(argv[1]);
        if (p < 1 || p > 65535) { vga_printf("bad port\n"); return; }
        g_host_port = (u16)p;
        serial_printf("[net] host port set %d\r\n", p);
    }
    vga_printf("host port: %u\n", (u32)g_host_port);
}

/* udptest：内核内 UDP 环回自测（两端口互发，验证协议栈收发） */
static void cmd_udptest(int argc, char **argv)
{
    (void)argc; (void)argv;
    udp_ep_t *a = udp_bind(9999);
    udp_ep_t *b = udp_bind(9998);
    if (!a || !b) { vga_printf("udptest: bind fail\n"); return; }
    u8 data[20];
    for (int i = 0; i < 20; i++) data[i] = (u8)('A' + (i % 26));
    a->len = 0; b->len = 0;
    int rc = net_udp_send(g_local_ip, 9999, 9998, data, 20);
    if (rc != 0) { vga_printf("udptest: send rc=%d\n", rc); return; }
    if (b->len == 20) {
        int ok = 1;
        for (int i = 0; i < 20; i++) if (b->buf[i] != data[i]) ok = 0;
        if (ok) {
            vga_printf("SEND 20 bytes -> RECV 20 bytes OK (data matches)\n");
            serial_puts("[net] udptest: LOOPBACK OK 20/20\r\n");
        } else {
            vga_printf("recv data mismatch\n");
        }
    } else {
        vga_printf("udptest: recv len=%u (expect 20)\n", b->len);
    }
}

/* chatd [port] [echo|cmd]：UDP 聊天/命令服务器
 *   chatd 7777        绑定端口等宿主发消息（无参数时查看最近一条）
 *   chatd 7777 echo   收到即自动回 ACK（宿主"发一句→收一句"闭环）
 *   chatd 7778 cmd    收到的整行当 shell 命令执行，并把整屏回传宿主（远程操作） */
static u16 s_chatd_port = 0;
static void cmd_chatd(int argc, char **argv)
{
    if (argc >= 2 && argv[1][0] >= '0' && argv[1][0] <= '9') {
        int port = k_atoi(argv[1]);
        if (port < 1 || port > 65535) { vga_printf("bad port\n"); return; }
        udp_ep_t *ep = udp_bind((u16)port);
        if (!ep) { vga_printf("chatd: bind fail\n"); return; }
        s_chatd_port = (u16)port;
        ep->auto_ack = (argc >= 3 && k_strcmp(argv[2], "echo") == 0) ? 1 : 0;
        ep->is_cmd   = (argc >= 3 && k_strcmp(argv[2], "cmd") == 0) ? 1 : 0;
        vga_printf("chatd: UDP server on port %d%s%s\n", port,
                   ep->auto_ack ? " (auto-ack)" : "",
                   ep->is_cmd ? " (remote cmd)" : "");
        serial_printf("[net] chatd bind %d echo=%u cmd=%u\r\n",
                      port, (u32)ep->auto_ack, (u32)ep->is_cmd);
        return;
    }
    if (argc == 1) {
        /* 无参数：把所有端点上待处理的消息都打出来（可能同时绑了消息口和命令口） */
        int shown = 0;
        udp_ep_t *e = udp_first();
        while (e) {
            if (e->len) {
                vga_printf("recv :%u from %u.%u.%u.%u:%u (%u bytes): ",
                           e->port, e->from[0], e->from[1], e->from[2], e->from[3],
                           e->from_port, e->len);
                for (u16 i = 0; i < e->len; i++) vga_putc((char)e->buf[i]);
                vga_putc('\n');
                serial_printf("[net] chatd recv :%u from ", e->port);
                ip_print(e->from);
                serial_printf(":%u len=%u\r\n", e->from_port, e->len);
                e->len = 0;
                shown++;
            }
            e = udp_next(e);
        }
        if (!shown) vga_printf("chatd: no message yet\n");
        return;
    }
    if (s_chatd_port == 0) { vga_printf("chatd: not started (chatd <port> [echo|cmd])\n"); return; }
    udp_ep_t *ep = udp_lookup(s_chatd_port);
    if (!ep) { vga_printf("chatd: endpoint lost\n"); return; }
    if (argc >= 2 && k_strcmp(argv[1], "echo") == 0) {
        ep->auto_ack = (argc >= 3 && k_strcmp(argv[2], "off") == 0) ? 0 : 1;
        vga_printf("chatd: auto-ack = %u\n", (u32)ep->auto_ack);
        return;
    }
    if (ep->len == 0) {
        vga_printf("chatd: no message yet (auto-ack=%u)\n", (u32)ep->auto_ack);
        return;
    }
    vga_printf("chatd recv from %u.%u.%u.%u:%u (%u bytes): ",
               ep->from[0], ep->from[1], ep->from[2], ep->from[3],
               ep->from_port, ep->len);
    for (u16 i = 0; i < ep->len; i++) vga_putc((char)ep->buf[i]);
    vga_putc('\n');
    serial_printf("[net] chatd recv from ");
    ip_print(ep->from);
    serial_printf(":%u len=%u\r\n", ep->from_port, ep->len);
    ep->len = 0;   /* 已读，清空 */
}

/* netstat：地址/ARP/端点/收发计数一览 */
static void cmd_netstat(int argc, char **argv)
{
    (void)argc; (void)argv;
    u32 sent, recv;
    udp_stats(&sent, &recv);
    vga_printf("IP %u.%u.%u.%u  GW(host) %u.%u.%u.%u  host port %u\n",
               g_local_ip[0], g_local_ip[1], g_local_ip[2], g_local_ip[3],
               g_local_gw[0], g_local_gw[1], g_local_gw[2], g_local_gw[3],
               (u32)g_host_port);
    vga_printf("ARP: rx=%u rep=%u req=%u | pending=%d sent_late=%u\n",
               (u32)g_arp_rx, (u32)g_arp_rep, (u32)g_arp_req,
               net_pending(), (u32)g_pend_tx);
    vga_printf("ICMP: echo replies received=%u\n", (u32)g_ping_rx);
    vga_printf("UDP: %u sent / %u recv | endpoints:\n", sent, recv);
    udp_ep_t *ep = udp_first();
    if (!ep) { vga_printf("  (none)\n"); return; }
    while (ep) {
        vga_printf("  :%u %s%s%s\n", ep->port,
                   ep->len ? "[pending]" : "[idle]",
                   ep->auto_ack ? " auto-ack" : "",
                   ep->len ? "" : "");
        ep = udp_next(ep);
    }
    serial_printf("[net] netstat: sent=%u recv=%u arp_rep=%u arp_req=%u\r\n",
                  (u32)sent, (u32)recv, (u32)g_arp_rep, (u32)g_arp_req);
}

static void cmd_demo(int argc, char **argv)
{
    (void)argc; (void)argv;    demo_run();
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

/* S6：启动目录/路径演示进程（user10：mkdir /docs + 绝对/相对路径写读） */
static void cmd_fs2(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user10", 0x670000, 0x791000,
                         _user10_code, USER10_LEN);
    if (p > 0) {
        vga_printf("created dir-demo proc: pid %d\n", p);
        serial_printf("[fs2] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: dir demo (0.1s quantum)\n");
        serial_printf("[fs2] scheduler started\r\n");
    } else {
        vga_printf("failed to create dir-demo process\n");
    }
}

/* ---------------- S5：进程间通信演示命令 ---------------- */

/* S5-1：管道演示（user7：pipe() 创建匿名管道，写读回） */
static void cmd_pipe(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user7", 0x640000, 0x761000,
                         _user7_code, USER7_LEN);
    if (p > 0) {
        vga_printf("created pipe-demo proc: pid %d\n", p);
        serial_printf("[pipe] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: pipe demo (0.1s quantum)\n");
        serial_printf("[pipe] scheduler started\r\n");
    } else {
        vga_printf("failed to create pipe-demo process\n");
    }
}

/* S5-2：命名管道演示（user8：fifo_create("fifo0") 写读回） */
static void cmd_fifo(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user8", 0x650000, 0x771000,
                         _user8_code, USER8_LEN);
    if (p > 0) {
        vga_printf("created fifo-demo proc: pid %d\n", p);
        serial_printf("[fifo] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: fifo demo (0.1s quantum)\n");
        serial_printf("[fifo] scheduler started\r\n");
    } else {
        vga_printf("failed to create fifo-demo process\n");
    }
}

/* S5-3：消息队列演示（user9：msgsend 3 条 + msgrecv 3 条） */
static void cmd_mq(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (sched_running()) { vga_printf("scheduler already running\n"); return; }
    int p = sched_create("user9", 0x660000, 0x781000,
                         _user9_code, USER9_LEN);
    if (p > 0) {
        vga_printf("created mq-demo proc: pid %d\n", p);
        serial_printf("[mq] demo proc pid %d\r\n", p);
        sched_start();
        vga_printf("scheduler started: mq demo (0.1s quantum)\n");
        serial_printf("[mq] scheduler started\r\n");
    } else {
        vga_printf("failed to create mq-demo process\n");
    }
}

/* S7：设备表 + 统一接口演示（dev） */
static void cmd_dev(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("device table (%d registered):\n", device_count());
    for (u8 m = 1; m < DEV_MAX; m++) {
        device_t *d = device_get(m);
        if (!d) continue;
        vga_printf("  [%u] %-6s type=%s irq=%u  r=%s w=%s ioctl=%s\n",
                   d->major, d->name,
                   d->type == DEV_CLOCK ? "clock" : "char",
                   d->irqno,
                   d->read ? "yes" : "-", d->write ? "yes" : "-",
                   d->ioctl ? "yes" : "-");
    }

    /* 统一接口实测：read（键盘，阻塞等一键） */
    device_t *k = device_get(DEV_MAJOR_KBD);
    if (k && k->read) {
        vga_printf("press any key -> kbd.read() ...\n");
        char c;
        if (k->read(&c, 1) == 1) {
            vga_printf("kbd.read got '%c'\n", c);
            serial_printf("[dev] kbd read via device interface: '%c'\r\n", c);
        }
    }
    /* 统一接口实测：ioctl（定时器查状态） */
    device_t *t = device_get(DEV_MAJOR_TIMER);
    if (t && t->ioctl) {
        int tk = t->ioctl(DEV_IOCTL_TICKS, 0);
        int up = t->ioctl(DEV_IOCTL_UPTIME, 0);
        vga_printf("timer.ioctl: ticks=%d uptime=%ds\n", tk, up);
        serial_printf("[dev] timer ioctl ticks=%d uptime=%d\r\n", tk, up);
    }
    /* 统一接口实测：write（串口输出） */
    device_t *s = device_get(DEV_MAJOR_COM1);
    if (s && s->write) {
        const char *msg = "hello from com1.write() via device layer\n";
        int n = s->write(msg, 43);
        vga_printf("com1.write: %d bytes -> serial (see log)\n", n);
        serial_printf("[dev] com1 write returned %d bytes\r\n", n);
    }
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
    { "ls",       "list directory contents",   cmd_ls },
    { "mkdir",    "create directory",           cmd_mkdir },
    { "cd",       "change directory",           cmd_cd },
    { "pwd",      "print working directory",    cmd_pwd },
    { "login",    "switch identity",            cmd_login },
    { "whoami",   "print current user",         cmd_whoami },
    { "id",       "print uid/gid",              cmd_id },
    { "chmod",    "change file mode",           cmd_chmod },
    { "demo",     "window UI demo",             cmd_demo },
    { "beep",     "PC speaker beep",            cmd_beep },
    { "paging",   "page table / fault stats",   cmd_paging },
    { "meminfo",  "physical mem + heap",        cmd_meminfo },
    { "alloc",    "kmalloc/kfree stress",       cmd_alloc },
    { "pfault",   "page fault + demand map",    cmd_pfault },
    { "run",      "start 3 user procs",         cmd_run },
    { "pipe",     "pipe demo (user7)",          cmd_pipe },
    { "fifo",     "named-pipe demo (user8)",    cmd_fifo },
    { "mq",       "message-queue demo (user9)", cmd_mq },
    { "fork",     "fork demo (user4)",          cmd_fork },
    { "exec",     "exec demo (user5->user2)",   cmd_exec },
    { "fs",       "fs demo (user6)",            cmd_fs },
    { "fs2",      "dir/path demo (user10)",     cmd_fs2 },
    { "fs11",     "permission demo (user11)",   cmd_fs11 },
    { "syncdemo", "sem/mutex/sleep demo (user12)", cmd_syncdemo },
    { "gui",      "graphics window system (13h)",  cmd_gui },
    { "net",      "NIC info + regs",            cmd_net },
    { "ip",       "set/show local IP",          cmd_ip },
    { "gw",       "set/show gateway (=host)",   cmd_gw },
    { "arp",      "show/add ARP table",         cmd_arp },
    { "ping",     "ICMP echo (wait for reply)", cmd_ping },
    { "udpsend",  "send UDP datagram",          cmd_udpsend },
    { "host",     "send UDP to host console",   cmd_host },
    { "hostport", "host console UDP port",      cmd_hostport },
    { "udptest",  "UDP loopback self-test",     cmd_udptest },
    { "chatd",    "UDP chat server (bind/view/echo)", cmd_chatd },
    { "netstat",  "addr/ARP/endpoints/stats",   cmd_netstat },
    { "vt",       "list/switch virtual terminals", cmd_vt },
    { "msg",      "send msg to terminal <id>",     cmd_msg },
    { "msgs",     "show my terminal inbox",        cmd_msgs },
    { "ts",       "terminal status table",         cmd_ts },
    { "tkill",    "reset terminal <id> session",   cmd_tkill },
    { "tcp",      "tcp listen/connect/send/recv",  cmd_tcp },
    { "tcpstat",  "tcp connection state table",    cmd_tcpstat },
    { "fslist",   "list ramdisk files",         cmd_fslist },
    { "dev",      "device table + interface",   cmd_dev },
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
    /* S12-2: 输入缓冲挂在当前终端上——切走再切回，半行输入不丢 */
    vt_t *v = vt_active_ptr();
    int i = v->ilen;
    if (i >= max) i = 0;
    for (;;) {
        /* 远程命令通道（宿主 UDP 发来的整行命令）：直接当一行输入返回 */
        if (net_cmd_take(buf, max)) {
            vga_puts(buf);
            vga_putc('\n');
            v->ilen = 0;
            return;
        }
        /* 非阻塞轮询：键盘 + 串口（S10 自动化测试通道）双输入源 */
        int k = keyboard_poll();
        if (!k) k = serial_getc();
        if (k <= 0) {
            for (volatile int d = 0; d < 20000; d++);   /* 小忙等降频 */
            continue;
        }
        serial_printf("[key %d]\r\n", k);
        /* 串口/键盘统一：CR(13) 与 LF(10) 都当回车（宿主发的一般是 CR） */
        if (k == KEY_ENTER || k == '\r' || k == '\n') {
            vga_putc('\n');
            for (int j = 0; j < i; j++) buf[j] = v->ibuf[j];   /* 半行 -> 命令行 */
            buf[i] = 0;
            v->ilen = 0;
            return;
        }
        if (k == KEY_BACKSP || k == 127) {
            if (i > 0) { i--; vga_putc('\b'); v->ilen = (u8)i; }
            continue;
        }
        /* S12-1: F1/F2/F3 直接切换虚拟终端（半行输入随终端保留） */
        if (k == KEY_F1) { vt_switch(1); v = vt_active_ptr(); i = v->ilen; continue; }
        if (k == KEY_F2) { vt_switch(2); v = vt_active_ptr(); i = v->ilen; continue; }
        if (k == KEY_F3) { vt_switch(3); v = vt_active_ptr(); i = v->ilen; continue; }
        if (k == KEY_TAB || k == KEY_ESC) continue;
        if (k >= 32 && k < 127 && i < max - 1) {
            v->ibuf[i] = (char)k;
            v->ilen = (u8)(i + 1);
            vga_putc((char)k);
            i++;
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
        /* S8: 提示符显示当前用户名 + #(root)/$(普通用户) */
        vga_printf("%s@kernel:%s%c ",
                   user_name_of(user_current_uid()), fs_cwd,
                   user_current_uid() == 0 ? '#' : '$');
        vga_set_attr(0x07);
        shell_readline(line, sizeof(line));
        shell_dispatch(line);
        if (net_cmd_active()) net_send_dump();   /* 远程命令：把结果屏回传宿主 */
    }
}
