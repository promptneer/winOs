#ifndef HTTPD_H
#define HTTPD_H

#include "io.h"

/* ============ S15 网络服务（教学级） ============
 * 基于 S13 TCP 协议栈的两种应用层服务：
 *   1) httpd —— 简单 HTTP/1.0 服务器（监听 8080）
 *      - 只支持 GET，解析请求行，返回内建页面（/ 主页、/info 系统信息、其余 404）
 *   2) chat —— 跨机 TCP 聊天服务（监听 7784）
 *      - Windows/其他机器连上后发消息，内核打印到屏幕并原样回显
 * 简化点：TCP 连接表仅 2 条（两个服务各占 1 条），同一时刻各服务只服务一个客户端；
 *         响应单包发送（停等协议限制，≤512 字节）。 */

void httpd_start(u16 port);   /* shell httpd 命令调用：绑定监听 8080 */
void httpd_poll(void);        /* 周期调用（挂 IRQ0，每 10 tick）：解析请求并回响应 */
void chat_start(u16 port);    /* shell chat 命令调用：绑定监听 7784 */
void chat_poll(void);         /* 周期调用：打印并回显聊天消息 */

#endif
