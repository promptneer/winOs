#include "dns.h"
#include "net.h"
#include "serial.h"
#include "vga.h"
#include "timer.h"

#define DNS_SERVER_IP0 10
#define DNS_SERVER_IP1 0
#define DNS_SERVER_IP2 2
#define DNS_SERVER_IP3 3

#define DNS_MAX_NAME 128
#define DNS_PKT_MAX  300

static u8  s_dns_buf[DNS_PKT_MAX];
static u16 s_dns_len;
static volatile u8 s_dns_pending;
static udp_ep_t *s_dns_ep;

void dns_init(void)
{
    s_dns_ep = udp_bind(DNS_LOCAL_PORT);
    if (s_dns_ep) {
        s_dns_ep->owner = 2;
        serial_puts("[dns] client bound :53535 (server 10.0.2.3:53)\r\n");
    } else {
        serial_puts("[dns] err: bind :53535 failed\r\n");
    }
}

/* UDP 收包分发：DNS 端口收到 → 存响应 + 置 pending（等命令上下文解析） */
void dns_rx(u16 dport, const u8 *src_ip, u16 sport, const u8 *buf, u16 len)
{
    (void)src_ip; (void)sport;
    if (dport != DNS_LOCAL_PORT) return;
    if (len > DNS_PKT_MAX) len = DNS_PKT_MAX;
    for (u16 i = 0; i < len; i++) s_dns_buf[i] = buf[i];
    s_dns_len = len;
    s_dns_pending = 1;
    serial_printf("[dns] response rx len=%u at tick=%u\r\n", (unsigned)len, (unsigned)timer_ticks());
}

/* 把域名编码进 DNS 报文（label 分段，末尾 0） */
static int dns_put_name(u8 *p, const char *name)
{
    int off = 0;
    const char *seg = name;
    while (*seg) {
        const char *dot = seg;
        int n = 0;
        while (*dot && *dot != '.') { dot++; n++; }
        if (n == 0 || n > 63) return -1;
        p[off++] = (u8)n;
        for (int i = 0; i < n; i++) p[off++] = (u8)seg[i];
        seg = dot;
        if (*seg == '.') seg++;
    }
    p[off++] = 0;
    return off;
}

int dns_query(const char *name)
{
    u8 pkt[DNS_PKT_MAX];
    int n = 0;
    static u16 s_qid = 0x1000;

    /* 头：ID + flags(0x0100 RD) + QD=1 + AN/NS/AR=0 */
    pkt[n++] = (u8)(s_qid >> 8); pkt[n++] = (u8)(s_qid & 0xFF);
    pkt[n++] = 0x01; pkt[n++] = 0x00;
    pkt[n++] = 0x00; pkt[n++] = 0x01;
    pkt[n++] = 0x00; pkt[n++] = 0x00;
    pkt[n++] = 0x00; pkt[n++] = 0x00;
    pkt[n++] = 0x00; pkt[n++] = 0x00;
    s_qid++;

    int qlen = dns_put_name(pkt + n, name);
    if (qlen < 0) { serial_puts("[dns] err: bad name\r\n"); return -1; }
    n += qlen;
    pkt[n++] = 0x00; pkt[n++] = 0x01;   /* QTYPE = A */
    pkt[n++] = 0x00; pkt[n++] = 0x01;   /* QCLASS = IN */

    ip4_t srv = { DNS_SERVER_IP0, DNS_SERVER_IP1, DNS_SERVER_IP2, DNS_SERVER_IP3 };

    /* 清零必须在发送之前：slirp 本地 DNS 代理毫秒级回包，
     * 若发送后才清零会把刚到的响应标记抹掉 → 永远 timeout */
    s_dns_pending = 0;
    s_dns_len = 0;
    int rc = net_udp_send(srv, DNS_LOCAL_PORT, 53, pkt, (u16)n);
    serial_printf("[dns] query %s -> 10.0.2.3:53 (pkt %d B, rc=%d)\r\n", name, n, rc);

    /* 等待响应（忙等轮询：最多 600 tick = 6 秒——slirp DNS 代理要递归到宿主 DNS，慢） */
    u32 t0 = timer_ticks();
    while (timer_ticks() - t0 < 600 && !s_dns_pending) { /* spin */ }
    serial_printf("[dns] wait end pending=%u len=%u dt=%u\r\n",
                  (unsigned)s_dns_pending, (unsigned)s_dns_len,
                  (unsigned)(timer_ticks() - t0));

    if (!s_dns_pending || s_dns_len < 12) {
        serial_puts("[dns] timeout: no response\r\n");
        vga_puts("\r\n[dns] timeout: no response\r\n");
        return -1;
    }

    /* 解析：跳头(12) + 问题（QNAME 到 0 + QTYPE/QCLASS 4） */
    int o = 12;
    /* 找问题区结束：允许压缩指针（0xC0 开头） */
    while (o < s_dns_len) {
        u8 l = s_dns_buf[o];
        if (l == 0) { o += 1 + 4; break; }          /* label 结束 + 4 */
        if ((l & 0xC0) == 0xC0) { o += 2 + 4; break; } /* 指针 + 4 */
        o += 1 + l;
    }
    if (o >= s_dns_len) { serial_puts("[dns] parse err: question\r\n"); return -1; }

    /* 遍历 Answer 区：跳过 CNAME(5)/NS(2) 等，找第一条 A 记录（type=1, rdlen=4） */
    u16 ancount = ((u16)s_dns_buf[6] << 8) | s_dns_buf[7];
    if (ancount > 8) ancount = 8;
    int pos = o;
    for (int a = 0; a < ancount && pos + 12 <= s_dns_len; a++) {
        int np = pos;
        if ((s_dns_buf[np] & 0xC0) == 0xC0) np += 2;              /* NAME 是指针 */
        else {
            while (np < s_dns_len && s_dns_buf[np]) {
                u8 l = s_dns_buf[np] & 0x3F;
                np += 1 + l;
            }
            np += 1;
        }
        if (np + 12 > s_dns_len) break;
        u16 type  = ((u16)s_dns_buf[np] << 8) | s_dns_buf[np + 1];
        u16 rdlen = ((u16)s_dns_buf[np + 8] << 8) | s_dns_buf[np + 9];
        /* RDATA 在 np+10（np 指向 TYPE：TYPE2+CLASS2+TTL4+RDLEN2） */
        if (type == 1 && rdlen == 4 && np + 10 + 4 <= s_dns_len) {
            u8 *ip = s_dns_buf + np + 10;
            serial_printf("[dns] %s -> %u.%u.%u.%u\r\n", name, (unsigned)ip[0],
                          (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
            vga_printf("\r\n[dns] %s -> %u.%u.%u.%u\r\n", name, (unsigned)ip[0],
                       (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
            return 0;
        }
        pos = np + 10 + rdlen;      /* 跳下一条（NAME 指针在 np-2 起） */
    }
    serial_printf("[dns] no A record (ancount=%u)\r\n", (unsigned)ancount);
    serial_puts("[dns] resp:");
    for (u16 i = 0; i < s_dns_len && i < 96; i++)
        serial_printf("%02x ", (unsigned)s_dns_buf[i]);
    serial_puts("\r\n");
    vga_printf("\r\n[dns] %s -> (no A record)\r\n", name);
    return -1;
}
