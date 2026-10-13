#include "net.h"
#include "ne2k.h"
#include "io.h"
#include "serial.h"

/* ============ 协议栈实现 ============ */

u8 g_local_ip[4] = { 192, 168, 0, 10 };
u8 g_local_mac[6];
u8 g_local_gw[4] = { 192, 168, 0, 1 };

static arp_ent_t g_arp[ARP_MAX];
static udp_ep_t  g_udp[UDP_EP_MAX];

volatile u32 g_arp_rx = 0;
volatile u32 g_ping_rx = 0;
volatile u32 g_udp_rx = 0;
static u32 s_udp_sent = 0;

static u8 s_eth_buf[1600];     /* 网卡收包缓冲 */
static u8 s_tx_buf[1600];      /* 发送缓冲（以太网帧） */
static u8 s_rx_ep_buf[1600];   /* 环回接收暂存 */

u8 *net_rx_buf(void) { return s_eth_buf; }

/* 前向声明（net_rx_ip 先于定义使用） */
void net_ping_send_reply(ip4_t dst, const u8 *icmp, u16 plen);

void ip_print(const u8 *ip)
{
    serial_printf("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

/* ---------- 校验和 ---------- */
static u16 net_checksum(const u8 *d, int len)
{
    u32 sum = 0;
    int i = 0;
    for (; i + 1 < len; i += 2) sum += ((u16)d[i] << 8) | d[i + 1];
    if (i < len) sum += (u16)d[i] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

/* ---------- ARP 表 ---------- */
static int ip_eq(const u8 *a, const u8 *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

arp_ent_t *arp_lookup(ip4_t ip)
{
    for (int i = 0; i < ARP_MAX; i++)
        if (g_arp[i].used && ip_eq(g_arp[i].ip, ip)) return &g_arp[i];
    return 0;
}

int arp_add(ip4_t ip, const u8 *mac)
{
    arp_ent_t *e = arp_lookup(ip);
    if (!e) {
        for (int i = 0; i < ARP_MAX; i++) {
            if (!g_arp[i].used) { e = &g_arp[i]; break; }
        }
        if (!e) e = &g_arp[0];   /* 覆盖最旧（简化） */
    }
    e->used = 1;
    for (int i = 0; i < 4; i++) e->ip[i] = ip[i];
    for (int i = 0; i < 6; i++) e->mac[i] = mac[i];
    return 0;
}

void arp_clear(void)
{
    for (int i = 0; i < ARP_MAX; i++) g_arp[i].used = 0;
}

/* ---------- UDP 端点 ---------- */
udp_ep_t *udp_bind(u16 port)
{
    for (int i = 0; i < UDP_EP_MAX; i++) {
        if (g_udp[i].used && g_udp[i].port == port) return &g_udp[i];
    }
    for (int i = 0; i < UDP_EP_MAX; i++) {
        if (!g_udp[i].used) {
            g_udp[i].used = 1;
            g_udp[i].port = port;
            g_udp[i].len  = 0;
            return &g_udp[i];
        }
    }
    return 0;
}

udp_ep_t *udp_lookup(u16 port)
{
    for (int i = 0; i < UDP_EP_MAX; i++)
        if (g_udp[i].used && g_udp[i].port == port) return &g_udp[i];
    return 0;
}

udp_ep_t *udp_first(void)
{
    for (int i = 0; i < UDP_EP_MAX; i++)
        if (g_udp[i].used) return &g_udp[i];
    return 0;
}

udp_ep_t *udp_next(const udp_ep_t *cur)
{
    int idx = (int)(cur - g_udp) + 1;
    for (; idx < UDP_EP_MAX; idx++)
        if (g_udp[idx].used) return &g_udp[idx];
    return 0;
}

arp_ent_t *net_arp_table(int *n)
{
    *n = ARP_MAX;
    return g_arp;
}

void udp_stats(u32 *sent, u32 *recv)
{
    *sent = s_udp_sent;
    *recv = (u32)g_udp_rx;
}

/* ---------- IP/UDP/ICMP 接收 ---------- */
static void net_rx_ip(const u8 *pkt, u16 len)
{
    if (len < 20) return;
    u8  ihl   = (pkt[0] & 0x0F) << 2;
    u8  proto = pkt[9];
    if (len < ihl + 8) return;
    const u8 *src = pkt + 12;
    const u8 *dst = pkt + 16;

    /* 只收发给自己或广播的包 */
    if (!ip_eq(dst, g_local_ip) && dst[0] != 255) return;

    if (proto == NET_IP_PROTO_ICMP) {
        const u8 *icmp = pkt + ihl;
        u8 type = icmp[0];
        if (type == NET_ICMP_ECHO) {
            /* 构造 echo reply 并发送（原样回数据） */
            u16 plen = len - ihl;
            u8 *r = s_tx_buf;
            for (int i = 0; i < plen && i < 60; i++) r[i] = icmp[i];
            r[0] = NET_ICMP_REPLY;
            r[2] = 0; r[3] = 0;   /* 清校验和 */
            u16 cs = net_checksum(r, plen);
            r[2] = cs >> 8; r[3] = cs & 0xFF;
            u8 src_copy[4];
            for (int i = 0; i < 4; i++) src_copy[i] = src[i];
            net_ping_send_reply(src_copy, r, plen);
        } else if (type == NET_ICMP_REPLY) {
            g_ping_rx++;
        }
    } else if (proto == NET_IP_PROTO_UDP) {
        const u8 *udp = pkt + ihl;
        u16 sport = ((u16)udp[0] << 8) | udp[1];
        u16 dport = ((u16)udp[2] << 8) | udp[3];
        u16 ulen  = ((u16)udp[4] << 8) | udp[5];
        if (ulen < 8 || len < ihl + ulen) return;
        udp_ep_t *ep = udp_lookup(dport);
        if (!ep) return;
        u16 dlen = ulen - 8;
        if (dlen > UDP_BUF_MAX) dlen = UDP_BUF_MAX;
        for (u16 i = 0; i < dlen; i++) ep->buf[i] = udp[8 + i];
        ep->len = dlen;
        for (int i = 0; i < 4; i++) ep->from[i] = src[i];
        ep->from_port = sport;
        g_udp_rx++;
    }
}

/* ---------- 以太网接收 ---------- */
void net_rx_eth(const u8 *frame, u16 len)
{
    if (len < 14) return;
    /* 目标 MAC：广播 FF:FF:FF:FF:FF:FF 或本机 */
    int mine = 1;
    for (int i = 0; i < 6; i++) {
        if (frame[i] != 0xFF && frame[i] != g_local_mac[i]) { mine = 0; break; }
    }
    if (!mine) return;
    u16 etype = ((u16)frame[12] << 8) | frame[13];

    if (etype == NET_ETH_TYPE_ARP) {
        if (len < 42) return;
        const u8 *a = frame + 14;
        u16 op = ((u16)a[6] << 8) | a[7];
        g_arp_rx++;
        if (op == 1) {
            /* ARP 请求：若目标 IP 是自己则回复 */
            const u8 *tpa = a + 24;
            if (ip_eq(tpa, g_local_ip)) {
                u8 *r = s_tx_buf;
                for (int i = 0; i < 6; i++) r[i] = a[8 + i];      /* dst = 请求方 MAC */
                for (int i = 0; i < 6; i++) r[i + 6] = g_local_mac[i];
                r[12] = 0x08; r[13] = 0x06;
                r[14] = 0x00; r[15] = 0x01; r[16] = 0x08; r[17] = 0x00;
                r[18] = 0x06; r[19] = 0x04; r[20] = 0x00; r[21] = 0x02;
                for (int i = 0; i < 6; i++) r[22 + i] = g_local_mac[i]; /* sha */
                for (int i = 0; i < 4; i++) r[28 + i] = g_local_ip[i];  /* spa */
                for (int i = 0; i < 6; i++) r[32 + i] = a[8 + i];       /* tha */
                for (int i = 0; i < 4; i++) r[38 + i] = a[14 + i];      /* tpa */
                ne2k_tx(r, 42);
            }
        } else if (op == 2) {
            arp_add((u8 *)a + 14, (u8 *)a + 8);
        }
    } else if (etype == NET_ETH_TYPE_IP) {
        net_rx_ip(frame + 14, len - 14);
    }
}

/* ---------- 构造并发送以太网帧 ---------- */
static int eth_send(const u8 *dstmac, u16 etype, const u8 *payload, u16 plen)
{
    u8 *f = s_tx_buf;
    for (int i = 0; i < 6; i++) f[i] = dstmac[i];
    for (int i = 0; i < 6; i++) f[i + 6] = g_local_mac[i];
    f[12] = etype >> 8; f[13] = etype & 0xFF;
    for (u16 i = 0; i < plen; i++) f[14 + i] = payload[i];
    return ne2k_tx(f, 14 + plen);
}

/* 发送 IPv4 包（校验和自动计算） */
static int ip_send(ip4_t dst, u8 proto, const u8 *payload, u16 plen)
{
    u8 *p = s_tx_buf + 14;
    u16 total = 20 + plen;
    p[0] = 0x45;
    p[1] = 0x00;
    p[2] = total >> 8; p[3] = total & 0xFF;
    p[4] = 0x12; p[5] = 0x34;          /* id */
    p[6] = 0x00; p[7] = 0x00;          /* frag */
    p[8] = 64;                         /* ttl */
    p[9] = proto;
    p[10] = 0; p[11] = 0;              /* checksum 待算 */
    for (int i = 0; i < 4; i++) p[12 + i] = g_local_ip[i];
    for (int i = 0; i < 4; i++) p[16 + i] = dst[i];
    for (u16 i = 0; i < plen; i++) p[20 + i] = payload[i];
    u16 cs = net_checksum(p, 20);
    p[10] = cs >> 8; p[11] = cs & 0xFF;

    /* 环回：目标是自己 → 直接走本机接收路径 */
    if (ip_eq(dst, g_local_ip)) {
        u8 *lf = s_rx_ep_buf;
        for (int i = 0; i < 6; i++) lf[i] = g_local_mac[i];
        for (int i = 0; i < 6; i++) lf[i + 6] = g_local_mac[i];
        lf[12] = 0x08; lf[13] = 0x00;
        for (u16 i = 0; i < total; i++) lf[14 + i] = p[i];
        net_rx_eth(lf, 14 + total);
        return 0;
    }

    /* 非环回：ARP 解析目标 MAC */
    arp_ent_t *e = arp_lookup(dst);
    if (e) {
        eth_send(e->mac, NET_ETH_TYPE_IP, p, total);
        return 0;
    }
    /* 无 ARP 条目：发 ARP 请求（广播），返回 -1 提示待回复 */
    u8 *a = s_tx_buf + 14;
    a[0] = 0x00; a[1] = 0x01; a[2] = 0x08; a[3] = 0x00;
    a[4] = 0x06; a[5] = 0x04; a[6] = 0x00; a[7] = 0x01;
    for (int i = 0; i < 6; i++) a[8 + i] = g_local_mac[i];
    for (int i = 0; i < 4; i++) a[14 + i] = g_local_ip[i];
    for (int i = 0; i < 6; i++) a[18 + i] = 0x00;
    for (int i = 0; i < 4; i++) a[24 + i] = dst[i];
    u8 bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    eth_send(bcast, NET_ETH_TYPE_ARP, a, 28);
    return -1;
}

/* ---------- 发送 ICMP echo ---------- */
static void icmp_echo_send(ip4_t dst)
{
    u8 *c = s_tx_buf + 34;   /* 14(eth)+20(ip) */
    u16 plen = 8 + 4;        /* ICMP 头 + 4 字节数据 */
    c[0] = NET_ICMP_ECHO;
    c[1] = 0;
    c[2] = 0; c[3] = 0;
    c[4] = 0xAB; c[5] = 0xCD;   /* id */
    c[6] = 0x00; c[7] = 0x01;   /* seq */
    c[8] = 'W'; c[9] = 'P'; c[10] = 'P'; c[11] = '!';
    u16 cs = net_checksum(c, plen);
    c[2] = cs >> 8; c[3] = cs & 0xFF;
    ip_send(dst, NET_IP_PROTO_ICMP, c, plen);
}

/* 回复 ICMP echo（ping 回复） */
void net_ping_send_reply(ip4_t dst, const u8 *icmp, u16 plen)
{
    ip_send(dst, NET_IP_PROTO_ICMP, icmp, plen);
}

int net_ping(ip4_t dst)
{
    icmp_echo_send(dst);
    return 0;
}

/* ---------- 发送 UDP ---------- */
int net_udp_send(ip4_t dst, u16 src_port, u16 dst_port, const u8 *data, u16 len)
{
    if (len > 500) return -2;
    u8 *u = s_tx_buf + 34;   /* 14(eth)+20(ip) */
    u16 ulen = 8 + len;
    u[0] = src_port >> 8; u[1] = src_port & 0xFF;
    u[2] = dst_port >> 8; u[3] = dst_port & 0xFF;
    u[4] = ulen >> 8;   u[5] = ulen & 0xFF;
    u[6] = 0; u[7] = 0;          /* 校验和可为 0（IPv4 UDP 允许） */
    for (u16 i = 0; i < len; i++) u[8 + i] = data[i];
    int rc = ip_send(dst, NET_IP_PROTO_UDP, u, ulen);
    if (rc == 0) s_udp_sent++;
    return rc;
}

/* ---------- 协议栈初始化 ---------- */
void net_init(void)
{
    ne2k_get_mac(g_local_mac);
    arp_clear();
    for (int i = 0; i < UDP_EP_MAX; i++) g_udp[i].used = 0;
    g_arp_rx = 0; g_ping_rx = 0; g_udp_rx = 0; s_udp_sent = 0;
    serial_printf("[net] stack init: IP=");
    ip_print(g_local_ip);
    serial_printf(" MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  g_local_mac[0], g_local_mac[1], g_local_mac[2],
                  g_local_mac[3], g_local_mac[4], g_local_mac[5]);
}
