#include "ne2k.h"
#include "io.h"
#include "serial.h"
#include "device.h"
#include "net.h"

/* ============ NE2000 (DP8390) 驱动实现 ============
 * 寄存器布局按 Bochs ne2k.cc（RTL8029 兼容）：
 *   页0: PSTART=0x01 PSTOP=0x02 BNRY=0x03 TPSR=0x04 TBCR=0x05/0x06
 *        ISR=0x07 RSAR=0x08/0x09 RBCR=0x0A/0x0B RCR=0x0C TCR=0x0D
 *        DCR=0x0E IMR=0x0F；页1: PAR=0x01-0x06 CURR=0x07 MAR=0x08-0x0F
 *   ASIC: DATA=0x10 RESET=0x1F
 * 接收 DMA：CR 写 rdma=3(0x18)+START(0x02)=0x1A 后，Bochs 自动从
 * BNRY 页开始连续读出「4 字节头 + 帧数据」，长度取自帧头 len 字段。 */

static u8 g_mac[6] = { NE2K_MAC0, NE2K_MAC1, NE2K_MAC2, NE2K_MAC3, NE2K_MAC4, NE2K_MAC5 };
static u8 g_ready = 0;
static u8 g_tx_done = 1;   /* 上一次发送是否完成（简化串行发送） */

static inline void wr(u8 reg, u8 v) { outb(NE2K_IO + reg, v); }
static inline u8  rd(u8 reg)        { return inb(NE2K_IO + reg); }

void ne2k_get_mac(u8 *out6)
{
    for (int i = 0; i < 6; i++) out6[i] = g_mac[i];
}

u8 ne2k_regs_diag(u8 *isr, u8 *curr, u8 *bnry)
{
    *isr = rd(NE2K_ISR);
    wr(NE2K_CR, 0x41);              /* 页1 + STOP */
    *curr = rd(NE2K_CURR);
    wr(NE2K_CR, 0x01);              /* 页0 + STOP */
    *bnry = rd(NE2K_BNRY);
    wr(NE2K_CR, 0x02);              /* 页0 + START，恢复正常运行 */
    return g_ready;
}

void ne2k_init(void)
{
    /* 1. 复位：RESET 口（0x240+0x1F）写两次，等待 ISR RESET 位 */
    outb(NE2K_IO + NE2K_RESET, 0xFF);
    outb(NE2K_IO + NE2K_RESET, 0xFF);
    for (volatile u32 i = 0; i < 50000 && !(rd(NE2K_ISR) & NE2K_ISR_RESET); i++) ;
    wr(NE2K_ISR, 0xFF);             /* 清全部中断标志 */

    /* 2. DCR：8 位总线 + LAS + loop（Bochs 的 DCR 在 0x0E） */
    wr(NE2K_DCR, NE2K_DCR_BYTE);

    /* 3. 页1：写 MAC 地址 + CURR + MAR */
    wr(NE2K_CR, 0x41);              /* 页1 + STOP */
    for (int i = 0; i < 6; i++) wr(NE2K_PAR0 + i, g_mac[i]);
    for (int i = 0; i < 6; i++) wr(NE2K_MAR + i, 0x00);
    wr(NE2K_CURR, NE2K_RX_START);

    /* 4. 页0：缓冲边界 + 发送页 + 接收/发送配置 */
    wr(NE2K_CR, 0x01);              /* 页0 + STOP */
    wr(NE2K_PSTART, NE2K_RX_START);
    wr(NE2K_PSTOP,  NE2K_RX_STOP);
    wr(NE2K_BNRY,   NE2K_RX_START);
    wr(NE2K_TPSR,   NE2K_TX_PAGE);
    wr(NE2K_RCR, 0x04);             /* 接受广播 + 本机帧 */
    wr(NE2K_TCR, 0x00);
    wr(NE2K_ISR, 0xFF);             /* 清 */
    wr(NE2K_IMR, 0x0F);             /* RX/TX/RXERR/TXERR 中断 */

    /* 5. 启动（页0 + STA） */
    wr(NE2K_CR, 0x02);
    wr(NE2K_ISR, 0xFF);

    /* 6. 使能 IRQ10（PIC2 级联，mask 位 10-8=2） */
    outb(0xA1, (u8)(inb(0xA1) & ~(1U << (NE2K_IRQ - 8))));
    outb(0x20, 0x20);   /* 如果之前 PIC2 未级联就绪，补一发 EOI */

    g_ready = 1;
    serial_printf("[ne2k] init ok: IO=0x%X IRQ=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  NE2K_IO, NE2K_IRQ,
                  g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);
}

/* 发送一帧（远程 DMA 写入 0x4000 发送缓冲，再触发 TXP） */
int ne2k_tx(const u8 *frame, int len)
{
    if (!g_ready) return -1;
    if (len < 1 || len > 1500) return -2;
    /* 等待上一次发送完成（轮询 TSR bit0 或简单延时；本内核串行发送） */
    for (volatile u32 i = 0; i < 50000 && !g_tx_done; i++) ;
    g_tx_done = 0;

    wr(NE2K_TBCR0, len & 0xFF);     /* 发送字节计数 */
    wr(NE2K_TBCR1, (len >> 8) & 0xFF);
    wr(NE2K_RSAR0, 0x00);           /* 发送缓冲地址 = 0x4000 */
    wr(NE2K_RSAR1, NE2K_TX_PAGE);
    wr(NE2K_RBCR0, len & 0xFF);     /* 远程 DMA 字节计数 */
    wr(NE2K_RBCR1, (len >> 8) & 0xFF);

    wr(NE2K_CR, 0x0A);              /* 页0 + START + RD0：远程 DMA 写 */
    for (int i = 0; i < len; i++) outb(NE2K_IO + NE2K_DATA, frame[i]);

    wr(NE2K_CR, 0x06);              /* 页0 + START + TXP：触发发送 */
    return 0;
}

/* IRQ10 中断：接收帧（读 CURR/BNRY → 远程 DMA 读 → 交上层 → 更新 BNRY）
 * Bochs 接收：CR=0x1A（rdma=3）后自动从 BNRY 页连续读出
 * 「4 字节头(RSR/NEXT/LENlo/LENhi) + 帧数据」，长度=len(含头)。 */
void ne2k_irq(void)
{
    u8 isr = rd(NE2K_ISR);
    wr(NE2K_ISR, isr);              /* 写 1 清位 */

    if (isr & NE2K_ISR_TX) g_tx_done = 1;

    if (isr & NE2K_ISR_RX) {
        wr(NE2K_CR, 0x41);          /* 页1 */
        u8 curr = rd(NE2K_CURR);
        wr(NE2K_CR, 0x01);          /* 页0 */
        u8 bnry = rd(NE2K_BNRY);

        while (bnry != curr) {
            /* 启动接收 DMA（rdma=3）：Bochs 自动从 BNRY 页开始读 */
            wr(NE2K_CR, 0x1A);      /* 页0 + START + RD3 */

            /* 读 4 字节接收头：RSR / NEXT / SIZE */
            u8 hdr[4];
            for (int i = 0; i < 4; i++) hdr[i] = inb(NE2K_IO + NE2K_DATA);

            u8  rsr  = hdr[0];
            u16 flen = hdr[2] | ((u16)hdr[3] << 8);   /* 含 4 字节头 */
            if (!(rsr & 0x01) || flen <= 4 || flen > 1604) { bnry = hdr[1]; continue; }

            /* 读帧数据（flen-4 字节，DMA 地址已自动递增到 BNRY 页+4） */
            u16 dlen = flen - 4;
            u8 *buf = net_rx_buf();
            for (u16 i = 0; i < dlen; i++) buf[i] = inb(NE2K_IO + NE2K_DATA);

            /* 交给协议栈（以太网帧） */
            net_rx_eth(buf, dlen);

            bnry = hdr[1];
            wr(NE2K_BNRY, bnry);
        }
        wr(NE2K_CR, 0x02);          /* 页0 + START */
    }
}

/* 设备框架注册（major=5, irq=10） */
static int ne2k_dev_read(char *buf, int len) { (void)buf; (void)len; return -1; }
static int ne2k_dev_write(const char *buf, int len) { return ne2k_tx((const u8 *)buf, len); }

device_t *ne2k_device(void)
{
    static device_t d;
    d.name   = "ne2k";
    d.type   = DEV_CHAR;
    d.major  = DEV_MAJOR_NE2K;
    d.irqno  = NE2K_IRQ;
    d.init   = 0;
    d.read   = ne2k_dev_read;
    d.write  = ne2k_dev_write;
    d.ioctl  = 0;
    d.irq    = ne2k_irq;
    return &d;
}
