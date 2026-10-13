#include "io.h"
#include "serial.h"
#include "vga.h"
#include "timer.h"
#include "net.h"
#include "tcp.h"
#include "httpd.h"

/* ============ S15 网络服务：HTTP 服务器 + 跨机 TCP 聊天 ============
 * 复用 S13 TCP 协议栈（停等式可靠传输、连接表 2 条）。
 * httpd：监听 8080，收 GET 请求 → 解析请求行 → 回内建页面（/ 主页、/info、404）。
 * chat ：监听 7784，客户端发消息 → 内核打印到屏幕并原样回显。
 * 注意：无 snprintf，字符串拼装全部手写（utoa + 逐段拷贝）。 */

#define HTTPD_PORT 8080
#define CHAT_PORT  7784

static tcp_sock_t *s_http = 0;
static tcp_sock_t *s_chat = 0;

extern u32 g_mem_kb;   /* kernel.c：物理内存 KB */
extern volatile u32 g_tcp_rx;
extern volatile u32 g_tcp_tx;

/* ---------------- 小工具 ---------------- */

static int k_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)((u8)*a - (u8)*b);
}

static char *k_utoa(u32 v, char *out)   /* 数字 → 十进制字符串 */
{
    char tmp[12]; int i = 0;
    if (v == 0) { out[0] = '0'; out[1] = 0; return out; }
    while (v) { tmp[i++] = (char)('0' + v % 10); v /= 10; }
    int j = 0;
    while (i) out[j++] = tmp[--i];
    out[j] = 0;
    return out;
}

static char *k_cat(char *dst, const char *src)  /* 拼接并返回新尾部 */
{
    while (*src) *dst++ = *src++;
    *dst = 0;
    return dst;
}

static void vga_ip_print(const u8 *ip)
{
    vga_printf("%u.%u.%u.%u", (unsigned)ip[0], (unsigned)ip[1],
               (unsigned)ip[2], (unsigned)ip[3]);
}

/* ---------------- HTTP 服务器 ---------------- */

void httpd_start(u16 port)
{
    if (s_http) { vga_printf("httpd: already running (:%u)\n", (unsigned)s_http->lport); return; }
    s_http = tcp_listen(port);
    if (!s_http) {
        vga_printf("httpd: listen failed (no free tcp slot)\n");
        serial_printf("[httpd] listen FAIL :%u (no slot)\r\n", (unsigned)port);
        return;
    }
    vga_printf("[httpd] server on :%u  (browser: http://127.0.0.1:%u/)\n",
               (unsigned)port, (unsigned)port);
    serial_printf("[httpd] listening :%u\r\n", (unsigned)port);
}

/* 找请求头结束 "\r\n\r\n"；返回其后的偏移，未收全返回 -1 */
static int find_hdr_end(const u8 *buf, u16 len)
{
    for (u16 i = 0; i + 3 < len; i++)
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n')
            return (int)i + 4;
    return -1;
}

/* 解析请求行第一行（GET <path> HTTP/x.y），只取 path */
static void parse_reqline(const u8 *buf, u16 len, char *path, u16 pcap)
{
    u16 e = 0;
    while (e < len && buf[e] != '\r') e++;
    u16 p = 0;
    if (e >= 5 && buf[0]=='G' && buf[1]=='E' && buf[2]=='T' && buf[3]==' ') p = 4;
    u16 i = p, j = 0;
    while (i < e && buf[i] != ' ' && j + 1 < pcap) path[j++] = (char)buf[i++];
    path[j] = 0;
}

/* 拼完整 HTTP 响应并单包发出（≤TCP_BUF_MAX） */
static void http_reply(tcp_sock_t *s, u16 code, const char *ct, const char *body)
{
    u32 blen = 0;
    while (body[blen]) blen++;
    static u8 resp[TCP_BUF_MAX];
    u16 n = 0;
    const char *reason = (code == 200) ? "OK" : "Not Found";
    char num[12];

    const char *p = "HTTP/1.0 ";
    while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    k_utoa(code, num); p = num;
    while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = " "; while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = reason; while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = "\r\nServer: WinPP-OS/0.1\r\nContent-Type: ";
    while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = ct; while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = "\r\nContent-Length: "; while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    k_utoa(blen, num); p = num;
    while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = "\r\nConnection: close\r\n\r\n";
    while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;
    p = body; while (*p && n < TCP_BUF_MAX) resp[n++] = (u8)*p++;

    int rc = tcp_send(s, (const char *)resp, n);
    serial_printf("[httpd] reply %u (%u B, rc=%d)\r\n",
                  (unsigned)code, (unsigned)n, rc);
}

/* 主页（带实时 uptime/mem/tcp 计数） */
static char *build_home(char *b)
{
    u32 up = timer_ticks() / TIMER_HZ;
    char num[12];
    b = k_cat(b, "<html><head><title>WinPP OS</title></head>"
                 "<body style=\"background:#0d1117;color:#e6edf3;"
                 "font-family:monospace;padding:24px\">"
                 "<h1>WinPP OS HTTP Server</h1>"
                 "<p>This page is served by a real kernel running from a floppy image.</p>"
                 "<p>Uptime: ");
    k_utoa(up, num); b = k_cat(b, num);
    b = k_cat(b, " s | Mem: ");
    k_utoa(g_mem_kb, num); b = k_cat(b, num);
    b = k_cat(b, " KB | TCP rx: ");
    k_utoa(g_tcp_rx, num); b = k_cat(b, num);
    b = k_cat(b, " | TCP tx: ");
    k_utoa(g_tcp_tx, num); b = k_cat(b, num);
    b = k_cat(b, "</p><hr><p>Routes: / (home) | /info (system info)</p></body></html>");
    return b;
}

static char *build_info(char *b)
{
    u32 up = timer_ticks() / TIMER_HZ;
    char num[12];
    b = k_cat(b, "WinPP OS system info\n-------------------\n");
    b = k_cat(b, "uptime = "); k_utoa(up, num); b = k_cat(b, num);
    b = k_cat(b, " s\nmem    = "); k_utoa(g_mem_kb, num); b = k_cat(b, num);
    b = k_cat(b, " KB\ntcp_rx = "); k_utoa(g_tcp_rx, num); b = k_cat(b, num);
    b = k_cat(b, "\ntcp_tx = "); k_utoa(g_tcp_tx, num); b = k_cat(b, num);
    b = k_cat(b, "\nstack  = S13 TCP + S15 httpd/chat\n");
    return b;
}

void httpd_poll(void)
{
    /* 连接已结束（对端发了 FIN，进 CLOSE_WAIT/LAST_ACK/CLOSED）：释放槽并重新监听，
     * 否则槽被占死，后续请求的 SYN 全部被丢（实测 /info、/nope 超时即此原因） */
    if (s_http && s_http->state != TCP_LISTEN && s_http->state != TCP_ESTABLISHED) {
        serial_printf("[httpd] conn done (%s) -> re-listen :%u\r\n",
                      tcp_state_name(s_http->state), (unsigned)s_http->lport);
        s_http->used = 0;
        s_http = 0;
        httpd_start(HTTPD_PORT);
        return;
    }
    if (!s_http || s_http->state != TCP_ESTABLISHED || s_http->rlen == 0) return;
    if (s_http->rlen >= TCP_BUF_MAX) { s_http->rlen = 0; return; }  /* 防塞满 */
    int he = find_hdr_end(s_http->rbuf, s_http->rlen);
    if (he < 0) return;               /* 请求头没到齐，等下一段 */

    char path[64];
    parse_reqline(s_http->rbuf, (u16)s_http->rlen, path, sizeof path);
    serial_printf("[httpd] GET %s from ", path);
    ip_print(s_http->rip);
    serial_printf(":%u\r\n", (unsigned)s_http->rport);
    vga_printf("\n[httpd] GET %s\n", path);

    static char body[512];
    if (k_strcmp(path, "/info") == 0) {
        build_info(body);
        http_reply(s_http, 200, "text/plain", body);
    } else if (path[0] == '/' && path[1] == 0) {
        build_home(body);
        http_reply(s_http, 200, "text/html", body);
    } else {
        http_reply(s_http, 404, "text/html",
                   "<html><body><h1>404 Not Found</h1><p>WinPP OS</p></body></html>");
    }
    s_http->rlen = 0;   /* 已消费请求（发送尽力而为；-2=上一响应未 ACK，丢弃本请求） */
}

/* ---------------- 跨机 TCP 聊天服务 ---------------- */

void chat_start(u16 port)
{
    if (s_chat) { vga_printf("chat: already running (: %u)\n", (unsigned)s_chat->lport); return; }
    s_chat = tcp_listen(port);
    if (!s_chat) {
        vga_printf("chat: listen failed (no free tcp slot)\n");
        serial_printf("[chat] listen FAIL :%u (no slot)\r\n", (unsigned)port);
        return;
    }
    vga_printf("[chat] server on :%u  (connect 127.0.0.1:%u)\n",
               (unsigned)port, (unsigned)port);
    serial_printf("[chat] listening :%u\r\n", (unsigned)port);
}

void chat_poll(void)
{
    /* 同 httpd：连接结束后释放槽并重新监听 */
    if (s_chat && s_chat->state != TCP_LISTEN && s_chat->state != TCP_ESTABLISHED) {
        serial_printf("[chat] conn done (%s) -> re-listen :%u\r\n",
                      tcp_state_name(s_chat->state), (unsigned)s_chat->lport);
        s_chat->used = 0;
        s_chat = 0;
        chat_start(CHAT_PORT);
        return;
    }
    if (!s_chat || s_chat->state != TCP_ESTABLISHED || s_chat->rlen == 0) return;
    u16 n = s_chat->rlen;
    if (n > 256) n = 256;

    vga_printf("\n[chat] ");
    vga_ip_print(s_chat->rip);
    vga_printf(": ");
    for (u16 i = 0; i < n; i++) vga_putc((char)s_chat->rbuf[i]);
    vga_printf("\n");

    serial_printf("[chat] rx "); ip_print(s_chat->rip);
    serial_printf(":%u len=%u: ", (unsigned)s_chat->rport, (unsigned)n);
    for (u16 i = 0; i < n; i++) serial_putc((char)s_chat->rbuf[i]);
    serial_puts("\r\n");

    int rc = tcp_send(s_chat, (const char *)s_chat->rbuf, n);  /* 原样回显 */
    serial_printf("[chat] echo rc=%d\r\n", rc);
    s_chat->rlen = 0;
}
