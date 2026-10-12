#include "ipc.h"
#include "fs.h"
#include "serial.h"
#include "vga.h"

/* S5 进程间通信实现。
 * 管道/FIFO 共用环形缓冲（ipc_buf_t）；fs 槽的 obj 指向缓冲对象。
 * fd 槽分配：找相邻两个空槽 i、i+1，读端绑 type=1/3、写端绑 type=2/4。
 * 消息队列：全局一条环形队列，每条消息 4B 长度 + 最多 32B 数据。 */

static ipc_buf_t pipes[IPC_PIPES];

typedef struct {
    ipc_buf_t b;
    char name[16];
    int  used;
} ipc_fifo_t;
static ipc_fifo_t fifos[IPC_FIFOS];

typedef struct {
    u32 len;
    u8  data[IPC_MQ_MAX];
} mq_msg_t;
static mq_msg_t mq[IPC_MQ_MSGS];
static u32 mq_head, mq_count;

/* fs.c 导出（fstab 槽操作） */
extern fs_ent_t fstab[];

static int slot_free(u32 i) { return !fstab[i].used; }

static int alloc_pair(int type_r, int type_w, void *obj)
{
    for (u32 i = 0; i + 1 < FS_MAX_FILES; i++) {
        if (slot_free(i) && slot_free(i + 1)) {
            fstab[i].used = 1;
            fstab[i].type = type_r;
            fstab[i].obj = obj;
            fstab[i].offset = 0;
            fstab[i].size = 0;
            fstab[i].name[0] = 0;
            fstab[i + 1].used = 1;
            fstab[i + 1].type = type_w;
            fstab[i + 1].obj = obj;
            fstab[i + 1].offset = 0;
            fstab[i + 1].size = 0;
            fstab[i + 1].name[0] = 0;
            return (int)i;
        }
    }
    return -1;
}

static void s_copy(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
}

static int s_eq(const char *a, const char *b)
{
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

void ipc_init(void)
{
    for (u32 i = 0; i < IPC_FIFOS; i++) {
        fifos[i].used = 0;
        fifos[i].name[0] = 0;
        fifos[i].b.head = 0;
        fifos[i].b.count = 0;
    }
    mq_head = 0;
    mq_count = 0;
    serial_puts("[ipc] init: pipes + fifos + mq ready\r\n");
}

int ipc_pipe_create(void)
{
    for (u32 k = 0; k < IPC_PIPES; k++) {
        if (pipes[k].count == 0 && pipes[k].head == 0) {
            /* 管道对象复用检测：count/head 同时为 0 视为空闲 */
            int fd = alloc_pair(1, 2, &pipes[k]);
            if (fd >= 0) {
                pipes[k].count = 0;
                pipes[k].head = 0;
                serial_printf("[ipc] pipe created: read-fd=%d write-fd=%d\r\n", fd, fd + 1);
                return fd;
            }
        }
    }
    return -1;
}

int ipc_fifo_create(const char *name)
{
    if (!name) return -1;
    /* 名字已存在：直接复用（返回读端 fd 或新建一对） */
    for (u32 i = 0; i < IPC_FIFOS; i++) {
        if (fifos[i].used && s_eq(fifos[i].name, name)) {
            int fd = alloc_pair(3, 4, &fifos[i].b);
            if (fd >= 0) {
                serial_printf("[ipc] fifo '%s' reopened: read-fd=%d write-fd=%d\r\n", name, fd, fd + 1);
                return fd;
            }
            return -1;
        }
    }
    for (u32 i = 0; i < IPC_FIFOS; i++) {
        if (fifos[i].used) continue;
        s_copy(fifos[i].name, name);
        fifos[i].used = 1;
        fifos[i].b.head = 0;
        fifos[i].b.count = 0;
        int fd = alloc_pair(3, 4, &fifos[i].b);
        if (fd >= 0) {
            serial_printf("[ipc] fifo '%s' created: read-fd=%d write-fd=%d\r\n", name, fd, fd + 1);
            return fd;
        }
        fifos[i].used = 0;
        return -1;
    }
    return -1;
}

int ipc_read(ipc_buf_t *b, char *buf, int len)
{
    if (!b || !buf || len < 0) return -1;
    int n = 0;
    while (b->count && n < len) {
        buf[n++] = b->buf[b->head];
        b->head = (b->head + 1) & (IPC_BUF_SZ - 1);
        b->count--;
    }
    return n;
}

int ipc_write(ipc_buf_t *b, const char *buf, int len)
{
    if (!b || !buf || len < 0) return -1;
    for (int i = 0; i < len; i++) {
        if (b->count == IPC_BUF_SZ) {      /* 满：丢最旧 */
            b->head = (b->head + 1) & (IPC_BUF_SZ - 1);
            b->count--;
        }
        b->buf[(b->head + b->count) & (IPC_BUF_SZ - 1)] = (u8)buf[i];
        b->count++;
    }
    return len;
}

int mq_send(const char *src, int len)
{
    if (!src) return -1;
    if (len <= 0) len = 0;
    if (len > IPC_MQ_MAX) len = IPC_MQ_MAX;
    if (mq_count == IPC_MQ_MSGS) {             /* 满：丢最旧 */
        mq_head = (mq_head + 1) % IPC_MQ_MSGS;
        mq_count--;
    }
    u32 idx = (mq_head + mq_count) % IPC_MQ_MSGS;
    mq[idx].len = (u32)len;
    for (int i = 0; i < len; i++) mq[idx].data[i] = (u8)src[i];
    mq_count++;
    serial_printf("[mq] send len=%d total=%u\r\n", len, mq_count);
    return len;
}

int mq_recv(char *dst)
{
    if (!dst) return -1;
    if (mq_count == 0) return -1;
    u32 len = mq[mq_head].len;
    for (u32 i = 0; i < len; i++) dst[i] = (char)mq[mq_head].data[i];
    mq_head = (mq_head + 1) % IPC_MQ_MSGS;
    mq_count--;
    serial_printf("[mq] recv len=%u total=%u\r\n", len, mq_count);
    return (int)len;
}
