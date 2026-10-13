#ifndef TCP_H
#define TCP_H

#include "io.h"

/* ============ S13 TCP 协议栈（教学级） ============
 * 三次握手（被动 listen / 主动 connect）· 序号/ACK 可靠传输 · 简单状态机。
 * 简化点（教学级可接受，真机实测路径不受影响）：
 *   - 无滑动窗口：停等式发送，未确认数据超时重传（3 秒）
 *   - 无乱序重组：只接受 rcv_nxt 正好对上的段
 *   - 连接表固定 2 条 */

#define NET_IP_PROTO_TCP 6

#define TCP_MAX      2
#define TCP_BUF_MAX  512
#define TCP_RTO      300          /* 重传超时：300 tick = 3 秒 */
#define TCP_RETRY_MAX 3           /* 重传上限 */

typedef enum {
    TCP_CLOSED = 0,
    TCP_LISTEN,        /* 监听中（收到 SYN 自动完成握手） */
    TCP_SYN_SENT,      /* 主动 connect，SYN 已发 */
    TCP_ESTABLISHED,   /* 连接建立，可收发 */
    TCP_FIN_WAIT_1,    /* 已发 FIN */
    TCP_FIN_WAIT_2,    /* FIN 已被 ACK，等对端 FIN */
    TCP_CLOSE_WAIT,    /* 收到对端 FIN，已回 ACK */
    TCP_LAST_ACK,      /* 已回 FIN，等最终 ACK */
    TCP_CLOSING
} tcp_state_t;

typedef struct tcp_sock {
    u8   used;
    tcp_state_t state;
    u16  lport;                 /* 本地端口（网络字节序） */
    u8   rip[4];                /* 对端 IP */
    u16  rport;                 /* 对端端口（网络字节序） */
    u32  snd_seq;               /* 下一个发送序号 */
    u32  snd_una;               /* 未确认最低序号（重传基线） */
    u32  rcv_nxt;               /* 下一个期望接收序号 */
    u8   sbuf[TCP_BUF_MAX];     /* 发送缓冲（未确认数据） */
    u16  slen;
    u8   rbuf[TCP_BUF_MAX];     /* 接收缓冲 */
    u16  rlen;
    u32  last_tick;             /* 最近活动时刻（重传计时） */
    u8   retries;               /* 已重传次数 */
} tcp_sock_t;

/* ---- 协议栈 ---- */
void tcp_init(void);                       /* 清连接表 */
void tcp_rx(const u8 *seg, u16 len, const u8 *src_ip);  /* 收到 TCP 段（IP 层分流） */
void tcp_poll(void);                       /* 周期调用：超时重传/清理 */
tcp_sock_t *tcp_listen(u16 port);          /* 绑定并监听；返回 0=失败 */
tcp_sock_t *tcp_connect(const u8 *ip, u16 port);  /* 主动连接；返回 0=失败 */
int  tcp_send(tcp_sock_t *s, const char *data, u16 len); /* 在已建连接上发数据 */
tcp_sock_t *tcp_first(void);
tcp_sock_t *tcp_next(const tcp_sock_t *cur);

/* 状态名 */
const char *tcp_state_name(tcp_state_t st);

extern volatile u32 g_tcp_rx;      /* 收到 TCP 段计数 */
extern volatile u32 g_tcp_tx;      /* 发出 TCP 段计数 */
extern volatile u32 g_tcp_retx;    /* 重传计数 */
extern volatile u32 g_tcp_syn;     /* 握手完成计数 */

#endif
