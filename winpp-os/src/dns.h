#ifndef DNS_H
#define DNS_H

#include "io.h"

/* ============ S14 DNS 客户端 ============
 * dns <hostname> —— 向 slirp DNS（10.0.2.3:53）发 A 记录查询（UDP），
 * 解析响应并打印 IP。教学级：单问题、取第一条 Answer 的 A 记录。 */

#define DNS_LOCAL_PORT  53535   /* 内核 DNS 客户端源端口（绑定该 UDP 端口收响应） */

void dns_init(void);                       /* 绑定 DNS 端口 */
/* UDP 收包分发（net.c 调）：dport==53535 时记录响应并置 pending */
void dns_rx(u16 dport, const u8 *src_ip, u16 sport, const u8 *buf, u16 len);
/* 发起查询并等待（shell 命令）：name 为域名，如 www.baidu.com */
int  dns_query(const char *name);

#endif
