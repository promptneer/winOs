#include "sock.h"
#include "net.h"
#include "tcp.h"
#include "serial.h"
#include "vga.h"

static sock_t g_sock[SOCK_MAX];

void sock_init(void)
{
    for (int i = 0; i < SOCK_MAX; i++) {
        g_sock[i].used = 0;
        g_sock[i].tcp_idx = -1;
    }
    serial_puts("[sock] layer ready (UDP/TCP unified)\r\n");
}

sock_t *sock_first(void) { return &g_sock[0]; }
sock_t *sock_next(const sock_t *cur) { return (sock_t *)cur + 1; }

sock_t *sock_active(void)
{
    for (int i = 0; i < SOCK_MAX; i++)
        if (g_sock[i].used) return &g_sock[i];
    return 0;
}

/* ---- UDP 收包分发：dport 被 sock 占用则进缓冲 ---- */
void sock_udp_rx(u16 dport, const u8 *src_ip, u16 sport, const u8 *buf, u16 len)
{
    for (int i = 0; i < SOCK_MAX; i++) {
        sock_t *s = &g_sock[i];
        if (!s->used || s->type != SOCK_UDP) continue;
        if (s->lport == dport) {
            if (len > SOCK_BUF_MAX) len = SOCK_BUF_MAX;
            for (u16 j = 0; j < len; j++) s->rbuf[j] = buf[j];
            s->rlen = len;
            for (int k = 0; k < 4; k++) s->rip[k] = src_ip[k];
            s->rport = sport;
            s->state = 2;                      /* 收到过对端数据 → connected */
            serial_printf("[sock] udp rx :%u <- ", (unsigned)dport);
            ip_print(src_ip);
            serial_printf(":%u len=%u\r\n", (unsigned)sport, (unsigned)len);
            vga_printf("\n[sock] ");
            for (u16 j = 0; j < len && j < 60; j++) vga_putc((char)buf[j]);
            vga_puts("\r\n");
            return;
        }
    }
}

/* ---- open ---- */
int sock_open_udp(u16 port)
{
    sock_t *s = sock_active();
    if (s) { serial_puts("[sock] err: close current socket first\r\n"); return -1; }
    udp_ep_t *ep = udp_bind(port);
    if (!ep) { serial_puts("[sock] err: udp bind failed (port busy?)\r\n"); return -1; }
    ep->owner = 1;
    for (int i = 0; i < SOCK_MAX; i++) {
        if (!g_sock[i].used) {
            g_sock[i].used = 1;
            g_sock[i].type = SOCK_UDP;
            g_sock[i].state = 1;               /* bound */
            g_sock[i].lport = port;
            g_sock[i].rlen = 0;
            g_sock[i].tcp_idx = -1;
            serial_printf("[sock] udp socket %d bound :%u\r\n", i, (unsigned)port);
            return 0;
        }
    }
    return -1;
}

int sock_open_tcp(u16 port)
{
    sock_t *s = sock_active();
    if (s) { serial_puts("[sock] err: close current socket first\r\n"); return -1; }
    tcp_sock_t *t = tcp_listen(port);
    if (!t) { serial_puts("[sock] err: tcp listen failed\r\n"); return -1; }
    int idx = (int)(t - tcp_first());
    for (int i = 0; i < SOCK_MAX; i++) {
        if (!g_sock[i].used) {
            g_sock[i].used = 1;
            g_sock[i].type = SOCK_TCP;
            g_sock[i].state = 1;               /* listening */
            g_sock[i].lport = port;
            g_sock[i].rlen = 0;
            g_sock[i].tcp_idx = idx;
            serial_printf("[sock] tcp socket %d listen :%u\r\n", i, (unsigned)port);
            return 0;
        }
    }
    return -1;
}

/* ---- connect ---- */
int sock_connect(const u8 *ip, u16 port)
{
    sock_t *s = sock_active();
    if (!s) { serial_puts("[sock] err: no socket open\r\n"); return -1; }
    if (s->type == SOCK_UDP) {
        for (int i = 0; i < 4; i++) s->rip[i] = ip[i];
        s->rport = port;
        s->state = 2;
        serial_puts("[sock] udp peer set to ");
        ip_print(ip);
        serial_printf(":%u\r\n", (unsigned)port);
        return 0;
    }
    /* TCP 主动连接 */
    tcp_sock_t *t = tcp_connect(ip, port);
    if (!t) { serial_puts("[sock] err: tcp connect failed\r\n"); return -1; }
    s->tcp_idx = (int)(t - tcp_first());
    s->state = 3;                       /* syn_sent */
    serial_puts("[sock] tcp connect initiated\r\n");
    return 0;
}

/* ---- send ---- */
int sock_send(const char *text)
{
    sock_t *s = sock_active();
    if (!s) { serial_puts("[sock] err: no socket open\r\n"); return -1; }
    int n = 0;
    while (text[n] && n < SOCK_BUF_MAX) n++;
    if (s->type == SOCK_UDP) {
        if (s->state < 2) { serial_puts("[sock] err: udp peer not set (sock connect first)\r\n"); return -1; }
        if (n == 0) return -1;
        int rc = net_udp_send(s->rip, s->lport, s->rport, (const u8 *)text, (u16)n);
        serial_printf("[sock] udp tx %d bytes -> ", n);
        ip_print(s->rip);
        serial_printf(":%u (rc=%d)\r\n", (unsigned)s->rport, rc);
        return 0;
    }
    /* TCP */
    tcp_sock_t *t = tcp_first() + s->tcp_idx;
    int rc = tcp_send(t, text, (u16)n);
    serial_printf("[sock] tcp tx %d bytes (rc=%d)\r\n", n, rc);
    return rc;
}

/* ---- recv ---- */
int sock_recv(void)
{
    sock_t *s = sock_active();
    if (!s) { serial_puts("[sock] err: no socket open\r\n"); return -1; }
    if (s->type == SOCK_UDP) {
        if (s->rlen == 0) { serial_puts("[sock] recv: (empty)\r\n"); return 0; }
        serial_printf("[sock] recv %u bytes: ", (unsigned)s->rlen);
        for (u16 i = 0; i < s->rlen && i < 100; i++) serial_putc((char)s->rbuf[i]);
        serial_puts("\r\n");
        vga_printf("[sock] recv %u bytes: ", (unsigned)s->rlen);
        for (u16 i = 0; i < s->rlen && i < 100; i++) vga_putc((char)s->rbuf[i]);
        vga_puts("\r\n");
        s->rlen = 0;                       /* 消费即清 */
        return 1;
    }
    /* TCP */
    tcp_sock_t *t = tcp_first() + s->tcp_idx;
    if (t->rlen == 0) { serial_puts("[sock] recv: (empty)\r\n"); return 0; }
    serial_printf("[sock] recv %u bytes: ", (unsigned)t->rlen);
    for (u16 i = 0; i < t->rlen && i < 100; i++) serial_putc((char)t->rbuf[i]);
    serial_puts("\r\n");
    vga_printf("[sock] recv %u bytes: ", (unsigned)t->rlen);
    for (u16 i = 0; i < t->rlen && i < 100; i++) vga_putc((char)t->rbuf[i]);
    vga_puts("\r\n");
    t->rlen = 0;
    return 1;
}

/* ---- close ---- */
int sock_close(void)
{
    sock_t *s = sock_active();
    if (!s) { serial_puts("[sock] err: no socket open\r\n"); return -1; }
    if (s->type == SOCK_UDP) {
        udp_ep_t *ep = udp_lookup(s->lport);
        if (ep) { ep->used = 0; ep->owner = 0; }
        serial_printf("[sock] udp socket closed (:port %u)\r\n", (unsigned)s->lport);
    } else {
        tcp_sock_t *t = tcp_first() + s->tcp_idx;
        if (t->used) { t->used = 0; t->state = TCP_CLOSED; }
        serial_printf("[sock] tcp socket closed (:port %u)\r\n", (unsigned)s->lport);
    }
    s->used = 0;
    return 0;
}

/* ---- list ---- */
int sock_list(void)
{
    vga_puts("\r\n#  type  state  lport  peer           buf\r\n");
    for (int i = 0; i < SOCK_MAX; i++) {
        sock_t *s = &g_sock[i];
        if (!s->used) continue;
        vga_printf("%d  %s  %u     :%-5u ", i,
                   s->type == SOCK_UDP ? "udp" : "tcp", (unsigned)s->state,
                   (unsigned)s->lport);
        for (int k = 0; k < 4; k++) vga_printf("%u.", (unsigned)s->rip[k]);
        vga_printf("\b %u  %u\r\n", (unsigned)s->rport, (unsigned)s->rlen);
    }
    serial_puts("[sock] list done\r\n");
    return 0;
}
