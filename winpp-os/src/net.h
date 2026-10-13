#ifndef NET_H
#define NET_H

#include "io.h"

/* ============ S11-2/3/4 网络协议栈 ============
 * 以太网帧 → ARP / IPv4 → ICMP / UDP。
 * 环回设计：目标 IP == 本机 IP 或 127.0.0.1 时，IP 层直接短路回环，
 * 不经过网卡——内核内部可完整实测（无需外部网络）。
 * 网卡（ne2k）提供真实收发通道（主机桥接后可用）。 */

#define NET_ETH_TYPE_ARP  0x0806
#define NET_ETH_TYPE_IP   0x0800

#define NET_IP_PROTO_ICMP 1
#define NET_IP_PROTO_UDP  17

#define NET_ICMP_ECHO     8
#define NET_ICMP_REPLY    0

/* IPv4 地址：网络字节序存（big-endian），用 u8[4] 或 u32 表示 */
typedef u8 ip4_t[4];

#define UDP_EP_MAX   4
#define UDP_BUF_MAX  256

typedef struct udp_ep {
    u8   used;
    u16  port;              /* 本地端口（网络字节序） */
    u8   buf[UDP_BUF_MAX];  /* 最近收到的数据 */
    u16  len;
    ip4_t from;             /* 最近来源 IP */
    u16   from_port;
    u8   auto_ack;          /* =1 时收到即回一个 ACK 包（宿主↔内核闭环用） */
    u8   is_cmd;            /* =1 时收到的内容当 shell 命令行执行（远程命令通道） */
} udp_ep_t;

/* ---- 协议栈入口 ---- */
void net_init(void);                       /* 协议栈初始化（设默认 IP/MAC、清表） */
void net_rx_eth(const u8 *frame, u16 len); /* 网卡收到以太网帧 */
u8  *net_rx_buf(void);                     /* 网卡收包缓冲 */

/* 发送 UDP 数据报：dst_ip 为本机则环回 */
int net_udp_send(ip4_t dst, u16 src_port, u16 dst_port, const u8 *data, u16 len);
/* 发送 IPv4 包（S13 TCP 用；proto=6 时 payload 是 TCP 段） */
int net_ip_send(ip4_t dst, u8 proto, const u8 *payload, u16 plen);
/* 发送 ICMP echo request */
int net_ping(ip4_t dst);
/* ICMP echo 回复（供 ping 自己） */
extern volatile u32 g_ping_rx;             /* 收到回显的计数 */
extern volatile u32 g_udp_rx;              /* UDP 收包计数 */

/* UDP 端点管理 */
udp_ep_t *udp_bind(u16 port);              /* 绑定本地端口（网络字节序） */
udp_ep_t *udp_lookup(u16 port);
udp_ep_t *udp_first(void);
udp_ep_t *udp_next(const udp_ep_t *cur);   /* 遍历下一个已用端点 */
void udp_stats(u32 *sent, u32 *recv);

/* ARP 表 */
#define ARP_MAX 8
typedef struct arp_ent { u8 used; ip4_t ip; u8 mac[6]; } arp_ent_t;
arp_ent_t *arp_lookup(ip4_t ip);
int arp_add(ip4_t ip, const u8 *mac);
void arp_clear(void);
arp_ent_t *net_arp_table(int *n);          /* 返回 ARP 表数组与数量 */
extern volatile u32 g_arp_rx;

/* 本机配置 */
extern u8 g_local_ip[4];       /* 本机 IPv4 */
extern u8 g_local_gw[4];       /* 网关 = 宿主侧（slirp 下为 10.0.2.2） */
extern u8 g_local_mac[6];
extern u16 g_eth_hdr_buf[3];   /* 预留 */

/* 运行时改地址（shell 的 ip / gw 命令用） */
void net_set_ip(const u8 *ip);
void net_set_gw(const u8 *gw);

/* ARP：主动请求 + 待发帧补发（无 ARP 条目时先暂存，回复到了自动补发） */
void net_arp_request(ip4_t target);
void net_arp_announce(void);            /* 广播自己的 IP/MAC，让 slirp 记住本机 */
int  net_pending(void);                 /* 是否有等 ARP 的待发帧 */
extern volatile u32 g_arp_req;          /* 发出 ARP 请求计数 */
extern volatile u32 g_arp_rep;          /* 收到 ARP 回复计数 */
extern volatile u32 g_pend_tx;          /* 待发帧补发成功计数 */

/* 宿主通讯默认端口（内核 → 宿主 的 UDP 目的端口；宿主侧工具监听它） */
#define NET_HOST_PORT_DEFAULT 40001
extern u16 g_host_port;

/* ---- 远程命令通道（chatd <port> cmd）----
 * 宿主把一整行命令用 UDP 发到这个端口，内核在 shell 上下文里执行，
 * 执行完把 25x80 文本屏分片回传，宿主就能"远程操作 + 远程看屏"。 */
#define NET_CMD_MAX 120
int  net_cmd_take(char *out, int max);   /* shell 取走待执行命令；1=取到 */
int  net_cmd_active(void);               /* 上一条命令是否来自网络 */
void net_send_dump(void);                /* 把当前文本屏回传给命令来源 */

/* 工具 */
void ip_print(const u8 *ip);   /* serial 打印 IP */

#endif
