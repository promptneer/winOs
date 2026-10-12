#ifndef IPC_H
#define IPC_H

#include "io.h"

/* S5 进程间通信（IPC）：
 *   管道 pipe   —— 匿名环形缓冲，pipe() 返回读端 fd，写端 = fd+1
 *   FIFO       —— 命名管道（名字表 + 环形缓冲），fifo_create(name) 返回读端 fd
 *   消息队列 mq —— 全局环形消息队列（32 条 x 32B），msgsend / msgrecv
 * 环形缓冲对象由 fs 槽的 obj 指针引用，read/write 系统调用按 fd 类型分发。 */

#define IPC_PIPES   4
#define IPC_FIFOS   8
#define IPC_BUF_SZ  4096

typedef struct {
    u8   buf[IPC_BUF_SZ];
    u32  head;            /* 读位置 */
    u32  count;           /* 缓冲中字节数，尾 = (head+count) % 4096 */
} ipc_buf_t;

#define IPC_MQ_MSGS   32
#define IPC_MQ_MAX    32

void    ipc_init(void);
int     ipc_pipe_create(void);                 /* 返回读端 fd；写端 = fd+1 */
int     ipc_fifo_create(const char *name);     /* 返回读端 fd；写端 = fd+1 */
int     ipc_read(ipc_buf_t *b, char *buf, int len);
int     ipc_write(ipc_buf_t *b, const char *buf, int len);
int     mq_send(const char *src, int len);     /* 满则丢最旧 */
int     mq_recv(char *dst);                    /* 返回本消息字节数；空返回 -1 */

#endif
