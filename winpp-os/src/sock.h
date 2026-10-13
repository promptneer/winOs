#ifndef SOCK_H
#define SOCK_H

#include "io.h"
#include "net.h"   /* ip4_t */

/* ============ S14 统一 socket 抽象层 ============
 * 把 S11 的 UDP 与 S13 的 TCP 收敛成一套"面向应用"的接口：
 *   sock open udp <port>   —— 开一个 UDP socket（绑定本地端口）
 *   sock open tcp <port>   —— 开一个 TCP 监听 socket
 *   sock connect <ip> <p>  —— UDP 设定对端 / TCP 主动三次握手
 *   sock send <text>       —— 发数据
 *   sock recv              —— 显示接收缓冲
 *   sock close             —— 关闭
 *   sock list              —— 列 socket 表
 * 对 UDP：薄封装 udp_ep（收包按端口分派进本 socket 缓冲）。
 * 对 TCP：关联 tcp_sock 槽位（状态机/重传仍在 tcp.c）。 */

#define SOCK_MAX      4
#define SOCK_BUF_MAX  512

typedef enum { SOCK_NONE = 0, SOCK_UDP, SOCK_TCP } sock_type_t;

typedef struct sock {
    u8    used;
    u8    type;            /* SOCK_UDP / SOCK_TCP */
    u8    state;           /* UDP: 1=bound, 2=connected; TCP: 借用 tcp 状态机 */
    u16   lport;           /* 本地端口（网络字节序） */
    ip4_t rip;             /* 对端 IP */
    u16   rport;           /* 对端端口（网络字节序） */
    u8    rbuf[SOCK_BUF_MAX];
    u16   rlen;            /* 已收数据长度（UDP 直接进这里） */
    int   tcp_idx;         /* TCP 类型：关联 g_tcp[] 下标；-1 无 */
} sock_t;

void   sock_init(void);
/* UDP 收包分发（net.c 调）：dport 若被某 UDP socket 占用 → 进其缓冲 */
void   sock_udp_rx(u16 dport, const u8 *src_ip, u16 sport, const u8 *buf, u16 len);

/* shell 命令接口 */
int    sock_open_udp(u16 port);
int    sock_open_tcp(u16 port);
int    sock_connect(const u8 *ip, u16 port);
int    sock_send(const char *text);
int    sock_recv(void);              /* 打印接收缓冲；1=有新数据 */
int    sock_close(void);
int    sock_list(void);

sock_t *sock_first(void);
sock_t *sock_next(const sock_t *cur);

/* 当前活跃 socket（shell 命令操作对象）；无则返回 0 */
sock_t *sock_active(void);

#endif
