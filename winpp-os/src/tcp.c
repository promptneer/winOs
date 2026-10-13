#include "tcp.h"
#include "net.h"
#include "timer.h"
#include "serial.h"
#include "vga.h"

/* ============ S13 TCP 实现（教学级可靠传输） ============ */

static int ip_eq4(const u8 *a, const u8 *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static tcp_sock_t g_tcp[TCP_MAX];

volatile u32 g_tcp_rx   = 0;
volatile u32 g_tcp_tx   = 0;
volatile u32 g_tcp_retx = 0;
volatile u32 g_tcp_syn  = 0;

static u8 s_seg[1200];    /* 构造 TCP 段的缓冲（14 eth + 20 ip + 20 tcp + data） */

tcp_sock_t *tcp_first(void)
{
    for (int i = 0; i < TCP_MAX; i++) if (g_tcp[i].used) return &g_tcp[i];
    return 0;
}

tcp_sock_t *tcp_next(const tcp_sock_t *cur)
{
    int idx = (int)(cur - g_tcp) + 1;
    for (; idx < TCP_MAX; idx++) if (g_tcp[idx].used) return &g_tcp[idx];
    return 0;
}

const char *tcp_state_name(tcp_state_t st)
{
    switch (st) {
    case TCP_CLOSED: return "CLOSED";
    case TCP_LISTEN: return "LISTEN";
    case TCP_SYN_SENT: return "SYN_SENT";
    case TCP_ESTABLISHED: return "ESTABLISHED";
    case TCP_FIN_WAIT_1: return "FIN_WAIT_1";
    case TCP_FIN_WAIT_2: return "FIN_WAIT_2";
    case TCP_CLOSE_WAIT: return "CLOSE_WAIT";
    case TCP_LAST_ACK: return "LAST_ACK";
    default: return "CLOSING";
    }
}

void tcp_init(void)
{
    for (int i = 0; i < TCP_MAX; i++) {
        g_tcp[i].used = 0;
        g_tcp[i].state = TCP_CLOSED;
        g_tcp[i].slen = 0;
        g_tcp[i].rlen = 0;
    }
    g_tcp_rx = 0; g_tcp_tx = 0; g_tcp_retx = 0; g_tcp_syn = 0;
    serial_puts("[tcp] stack init: state machine + 2 sockets\r\n");
}

/* 本机端口是否已被占用（TCP） */
static tcp_sock_t *tcp_lookup_lport(u16 port)
{
    for (int i = 0; i < TCP_MAX; i++)
        if (g_tcp[i].used && g_tcp[i].lport == port) return &g_tcp[i];
    return 0;
}

/* 四元组匹配（对端 IP + 对端端口 + 本地端口） */
static tcp_sock_t *tcp_lookup4(const u8 *rip, u16 rport, u16 lport)
{
    for (int i = 0; i < TCP_MAX; i++) {
        tcp_sock_t *s = &g_tcp[i];
        if (!s->used) continue;
        if (s->lport != lport) continue;
        if (s->rport != rport) continue;
        int eq = 1;
        for (int j = 0; j < 4; j++) if (s->rip[j] != rip[j]) { eq = 0; break; }
        if (eq) return s;
    }
    return 0;
}

/* ---- 序号工具 ---- */
static u32 tcp_seq_add(u32 base, u32 n) { return base + n; }
static int tcp_seq_lt(u32 a, u32 b) { return (int)(a - b) < 0; }

/* ---- 发送一个 TCP 段 ----
 * flags 位：0x01 FIN 0x02 SYN 0x04 RST 0x08 PSH 0x10 ACK
 * 数据在 payload 之前（SYN/FIN 各占 1 个序号，ACK 不占）。 */
static int tcp_send_seg(tcp_sock_t *s, u8 flags, u32 seq, u32 ack,
                        const u8 *data, u16 dlen)
{
    u8 *t = s_seg + 34;   /* 14 eth + 20 ip */
    t[0] = s->lport >> 8;  t[1] = s->lport & 0xFF;
    t[2] = s->rport >> 8;  t[3] = s->rport & 0xFF;
    t[4] = seq >> 24; t[5] = seq >> 16; t[6] = seq >> 8; t[7] = seq;
    t[8] = ack >> 24; t[9] = ack >> 16; t[10] = ack >> 8; t[11] = ack;
    t[12] = 0x50;                       /* 20 字节头 */
    t[13] = flags;
    t[14] = 0x20; t[15] = 0x00;        /* window 8192 */
    t[16] = 0; t[17] = 0;              /* checksum 待算 */
    t[18] = 0; t[19] = 0;              /* urgent */
    for (u16 i = 0; i < dlen; i++) t[20 + i] = data[i];

    u16 tcp_len = 20 + dlen;
    /* TCP 校验和 = 伪头(12B) + TCP 段 */
    u8 ph[12];
    for (int i = 0; i < 4; i++) ph[i] = g_local_ip[i];
    for (int i = 0; i < 4; i++) ph[4 + i] = s->rip[i];
    ph[8] = 0; ph[9] = NET_IP_PROTO_TCP;
    ph[10] = tcp_len >> 8; ph[11] = tcp_len & 0xFF;

    /* 伪头 + 段 一起算：先把段拷到 34 偏移后的连续区，前面 12 字节放伪头 */
    u8 *tmp = s_seg + 22;    /* 22 = 14 eth + 8 ip 头前段（ip 头本身 20，留 12 伪头） */
    for (int i = 0; i < 12; i++) tmp[i] = ph[i];
    /* t 从 s_seg+34 开始，tmp+12 = s_seg+34，正好连续 */
    u16 cs = 0;
    {
        u32 sum = 0;
        int len = 12 + tcp_len;
        const u8 *d = tmp;
        for (int i = 0; i + 1 < len; i += 2) sum += ((u16)d[i] << 8) | d[i + 1];
        if (len & 1) sum += (u16)d[len - 1] << 8;
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        cs = (u16)~sum;
    }
    t[16] = cs >> 8; t[17] = cs & 0xFF;

    int rc = net_ip_send(s->rip, NET_IP_PROTO_TCP, t, tcp_len);
    if (rc == 0 || ip_eq4(s->rip, g_local_ip)) g_tcp_tx++;
    return rc;
}

/* 纯 ACK 段（不带数据/不带 SYN）——保留（后续关流/窗口用），防 -Wunused 用属性 */
static int tcp_send_ack(tcp_sock_t *s, u32 seq, u32 ack)
{
    (void)s; (void)seq; (void)ack;
    return -1;
}

/* 重发当前未确认数据（snd_una 起 slen 长） */
static void tcp_retransmit(tcp_sock_t *s)
{
    if (s->slen == 0) return;
    g_tcp_retx++;
    tcp_send_seg(s, 0x08 | 0x10, s->snd_una, s->rcv_nxt, s->sbuf, s->slen);
    serial_printf("[tcp] retx %u bytes (una=%u)\r\n", (unsigned)s->slen, s->snd_una);
}

/* ---- 监听 ---- */
tcp_sock_t *tcp_listen(u16 port)
{
    if (tcp_lookup_lport(port)) return 0;
    for (int i = 0; i < TCP_MAX; i++) {
        if (!g_tcp[i].used) {
            tcp_sock_t *s = &g_tcp[i];
            s->used = 1;
            s->state = TCP_LISTEN;
            s->lport = port;
            s->rport = 0;
            s->snd_seq = (u32)(timer_ticks() ^ 0x9E3779B9);  /* ISN 伪随机 */
            s->rcv_nxt = 0;
            s->slen = 0; s->rlen = 0;
            s->retries = 0;
            serial_printf("[tcp] listen :%u (socket %d)\r\n", (unsigned)port, i);
            return s;
        }
    }
    return 0;
}

/* ---- 主动连接 ---- */
tcp_sock_t *tcp_connect(const u8 *ip, u16 port)
{
    for (int i = 0; i < TCP_MAX; i++) {
        if (!g_tcp[i].used) {
            tcp_sock_t *s = &g_tcp[i];
            s->used = 1;
            s->state = TCP_SYN_SENT;
            s->lport = (u16)(50000 + (timer_ticks() % 1000));  /* 临时端口 */
            s->rport = port;
            for (int j = 0; j < 4; j++) s->rip[j] = ip[j];
            s->snd_seq = (u32)(timer_ticks() ^ 0x85EBCA6B);
            s->snd_una = s->snd_seq;
            s->rcv_nxt = 0;
            s->slen = 0; s->rlen = 0;
            s->retries = 0;
            s->last_tick = timer_ticks();
            tcp_send_seg(s, 0x02, s->snd_seq, 0, 0, 0);   /* SYN，seq 占 1 */
            serial_printf("[tcp] connect ");
            ip_print(ip);
            serial_printf(":%u (socket %d, SYN sent)\r\n", (unsigned)port, i);
            return s;
        }
    }
    return 0;
}

/* ---- 发送数据（停等：未确认数据只保留一份，ACK 推进后清空） ---- */
int tcp_send(tcp_sock_t *s, const char *data, u16 len)
{
    if (!s || s->state != TCP_ESTABLISHED) return -1;
    if (len > TCP_BUF_MAX) len = TCP_BUF_MAX;
    if (s->slen > 0) return -2;    /* 上一批还没 ACK，先等 */
    for (u16 i = 0; i < len; i++) s->sbuf[i] = (u8)data[i];
    s->slen = len;
    s->snd_una = s->snd_seq;
    s->last_tick = timer_ticks();
    s->retries = 0;
    tcp_send_seg(s, 0x08 | 0x10, s->snd_seq, s->rcv_nxt, s->sbuf, s->slen);
    return 0;
}

/* ---- 接收（net_rx_ip proto==6 分流进来） ---- */
void tcp_rx(const u8 *seg, u16 len, const u8 *src_ip)
{
    if (len < 20) return;
    u16 sport = ((u16)seg[0] << 8) | seg[1];
    u16 dport = ((u16)seg[2] << 8) | seg[3];
    u32 seq = ((u32)seg[4] << 24) | ((u32)seg[5] << 16) | ((u32)seg[6] << 8) | seg[7];
    u32 ack = ((u32)seg[8] << 24) | ((u32)seg[9] << 16) | ((u32)seg[10] << 8) | seg[11];
    u8  off = (seg[12] >> 4) << 2;
    u8  flg = seg[13];
    u16 dlen = (off < len) ? (u16)(len - off) : 0;
    const u8 *data = seg + off;
    g_tcp_rx++;

    serial_printf("[tcp] rx :%u<-", (unsigned)dport);
    ip_print(src_ip);
    serial_printf(":%u flags=0x%02X seq=%u ack=%u len=%u\r\n",
                  (unsigned)sport, flg, seq, ack, (unsigned)dlen);

    /* 找连接 */
    tcp_sock_t *s = tcp_lookup4(src_ip, sport, dport);

    /* 没找到连接但这是 SYN：若端口在监听则建立（被动握手） */
    if (!s && (flg & 0x02)) {
        s = tcp_lookup_lport(dport);
        if (s && s->state == TCP_LISTEN) {
            s->rport = sport;
            for (int j = 0; j < 4; j++) s->rip[j] = src_ip[j];
            s->rcv_nxt = tcp_seq_add(seq, 1);
            /* SYN+ACK 段的 seq = 自己的 ISN（不推进）；
             * 发完后再把 snd_seq 推进到 ISN+1（SYN 占 1 个序号）。 */
            s->last_tick = timer_ticks();
            u32 synack_seq = s->snd_seq;
            tcp_send_seg(s, 0x12, synack_seq, s->rcv_nxt, 0, 0);   /* SYN+ACK seq=ISN */
            s->snd_seq = tcp_seq_add(synack_seq, 1);               /* 下个数据从 ISN+1 发 */
            s->state = TCP_ESTABLISHED;   /* 简化：SYN+ACK 发出即认为建立 */
            g_tcp_syn++;
            serial_printf("[tcp] accept from ");
            ip_print(src_ip);
            serial_printf(":%u (socket %d, SYN+ACK sent)\r\n", (unsigned)sport, (int)(s - g_tcp));
            return;
        }
        return;
    }
    if (!s) return;

    /* 主动连接方：收到 SYN+ACK → 回 ACK → ESTABLISHED */
    if (s->state == TCP_SYN_SENT && (flg & 0x02) && (flg & 0x10)) {
        s->rcv_nxt = tcp_seq_add(seq, 1);
        s->snd_seq = tcp_seq_add(ack, 0);          /* 对端 ACK 了我们的 SYN */
        s->snd_una = s->snd_seq;
        tcp_send_seg(s, 0x10, s->snd_seq, s->rcv_nxt, 0, 0);  /* ACK */
        s->state = TCP_ESTABLISHED;
        g_tcp_syn++;
        serial_puts("[tcp] connect established (ACK sent)\r\n");
        return;
    }

    /* 已建立：处理 ACK（推进 snd_una，清发送缓冲） */
    if ((flg & 0x10) && tcp_seq_lt(s->snd_una, ack)) {
        s->snd_una = ack;
        s->slen = 0;                     /* 简化：全部确认 */
        s->retries = 0;
    }

    /* 已建立：处理数据（只收正好对序的） */
    if (dlen > 0 && s->state >= TCP_ESTABLISHED) {
        if (seq == s->rcv_nxt) {
            for (u16 i = 0; i < dlen && s->rlen < TCP_BUF_MAX; i++)
                s->rbuf[s->rlen++] = data[i];
            s->rcv_nxt = tcp_seq_add(s->rcv_nxt, dlen);
            tcp_send_seg(s, 0x10, s->snd_seq, s->rcv_nxt, 0, 0);  /* ACK */
            serial_printf("[tcp] data +%u -> rbuf (rlen=%u)\r\n", (unsigned)dlen, (unsigned)s->rlen);
        }
    }

    /* 收到 FIN：回 ACK → CLOSE_WAIT → 自动回 FIN → LAST_ACK */
    if (flg & 0x01) {
        if (s->state == TCP_ESTABLISHED) {
            s->rcv_nxt = tcp_seq_add(s->rcv_nxt, 1);
            s->state = TCP_CLOSE_WAIT;
            tcp_send_seg(s, 0x10, s->snd_seq, s->rcv_nxt, 0, 0);       /* ACK the FIN */
            tcp_send_seg(s, 0x11, s->snd_seq, s->rcv_nxt, 0, 0);       /* FIN+ACK */
            s->snd_seq = tcp_seq_add(s->snd_seq, 1);
            s->state = TCP_LAST_ACK;
            serial_puts("[tcp] FIN rx -> CLOSE_WAIT -> FIN+ACK sent (LAST_ACK)\r\n");
        } else if (s->state == TCP_FIN_WAIT_2) {
            s->rcv_nxt = tcp_seq_add(s->rcv_nxt, 1);
            tcp_send_seg(s, 0x10, s->snd_seq, s->rcv_nxt, 0, 0);
            s->state = TCP_CLOSED;
            s->used = 0;
            serial_puts("[tcp] FIN rx in FIN_WAIT_2 -> closed\r\n");
        }
    }

    /* 对端 ACK 了我们的 FIN（LAST_ACK → CLOSED） */
    if (s->state == TCP_LAST_ACK && (flg & 0x10) &&
        tcp_seq_lt(s->snd_una, ack) && s->slen == 0) {
        s->state = TCP_CLOSED;
        s->used = 0;
        serial_puts("[tcp] final ACK rx -> closed\r\n");
    }
}

/* ---- 周期轮询：SYN_SENT/FIN_WAIT 超时重传，LAST_ACK 等太久清理 ---- */
void tcp_poll(void)
{
    u32 now = timer_ticks();
    for (int i = 0; i < TCP_MAX; i++) {
        tcp_sock_t *s = &g_tcp[i];
        if (!s->used) continue;

        if (s->state == TCP_SYN_SENT) {
            if (now - s->last_tick >= TCP_RTO) {
                if (s->retries >= TCP_RETRY_MAX) {
                    s->used = 0; s->state = TCP_CLOSED;
                    serial_puts("[tcp] connect timeout -> closed\r\n");
                } else {
                    s->retries++;
                    s->last_tick = now;
                    tcp_send_seg(s, 0x02, s->snd_seq, 0, 0, 0);
                    g_tcp_retx++;
                    serial_printf("[tcp] SYN retry %u\r\n", (unsigned)s->retries);
                }
            }
        } else if (s->state == TCP_ESTABLISHED && s->slen > 0) {
            /* 数据未确认超时重传 */
            if (now - s->last_tick >= TCP_RTO) {
                if (s->retries >= TCP_RETRY_MAX) {
                    s->slen = 0;
                    serial_puts("[tcp] data give up (no ack)\r\n");
                } else {
                    s->retries++;
                    s->last_tick = now;
                    tcp_retransmit(s);
                }
            }
        } else if (s->state == TCP_LAST_ACK) {
            /* 最终 ACK 一直不来就释放（教学简化） */
            if (now - s->last_tick >= TCP_RTO * 2) {
                s->used = 0; s->state = TCP_CLOSED;
                serial_puts("[tcp] LAST_ACK timeout -> closed\r\n");
            }
        }
    }
}
