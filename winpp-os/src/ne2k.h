#ifndef NE2K_H
#define NE2K_H

#include "io.h"

/* ============ S11-1 NE2000 (DP8390) ISA 网卡驱动 ============
 * Bochs 虚拟网卡：ioaddr=0x240, irq=10, MAC 52:54:00:12:34:56
 * 16KB 板载 SRAM（0x4000-0x7FFF）：
 *   发送缓冲 0x4000-0x47FF（页 0x40-0x47）
 *   接收缓冲 0x4800-0x7FFF（页 0x48-0x7F，PSTART=0x48 PSTOP=0x80）
 * 16 位远程 DMA 收/发帧。中断在 IRQ10，由 S7 设备框架分发。 */

#define NE2K_IO        0x240
#define NE2K_IRQ       10
#define NE2K_MAC0      0x52
#define NE2K_MAC1      0x54
#define NE2K_MAC2      0x00
#define NE2K_MAC3      0x12
#define NE2K_MAC4      0x34
#define NE2K_MAC5      0x56

/* 板载 SRAM 页地址 */
#define NE2K_TX_PAGE   0x40        /* 发送缓冲起始页 */
#define NE2K_RX_START  0x48        /* 接收缓冲起始页 */
#define NE2K_RX_STOP   0x80        /* 接收缓冲结束页（独占） */

/* 寄存器偏移（页 0/1 由 CR 的 PS0/PS1 选择） */
#define NE2K_CR        0x00
#define NE2K_CLDA0     0x01
#define NE2K_CLDA1     0x02
#define NE2K_BNRY      0x03
#define NE2K_TSR       0x04
#define NE2K_NCR       0x05
#define NE2K_ISR       0x06
#define NE2K_IMR       0x07
#define NE2K_MAR0      0x08
#define NE2K_TBCR0     0x0A
#define NE2K_TBCR1     0x0B
#define NE2K_RSAR0     0x0C
#define NE2K_RSAR1     0x0D
#define NE2K_RBCR0     0x0E
#define NE2K_RBCR1     0x0F
#define NE2K_RCR       0x10
#define NE2K_TCR       0x11
#define NE2K_DCR       0x12
#define NE2K_IMCR      0x13
#define NE2K_CNTR0     0x14
#define NE2K_CNTR1     0x15
#define NE2K_CNTR2     0x16
#define NE2K_CNTR3     0x17
#define NE2K_CNTR4     0x18
#define NE2K_CNTR5     0x19
#define NE2K_PSTART    0x1E
#define NE2K_PSTOP     0x1F
/* 页 1 */
#define NE2K_PAR0      0x01
#define NE2K_CURR      0x08
#define NE2K_MAR        0x0E   /* MAR0..5 in page1 */

/* CR 命令位 */
#define NE2K_CR_STA    0x01    /* 启动 */
#define NE2K_CR_STOP   0x02    /* 停止 */
#define NE2K_CR_TXP    0x04    /* 发送触发 */
#define NE2K_CR_RD0    0x08    /* 远程 DMA 写 */
#define NE2K_CR_RD1    0x10    /* 远程 DMA 读 */
#define NE2K_CR_PS1    0x20    /* 页选择 bit1 */
#define NE2K_CR_PS0    0x40    /* 页选择 bit0 */

/* ISR 位 */
#define NE2K_ISR_RX    0x01    /* 收到帧 */
#define NE2K_ISR_TX    0x02    /* 发送完成 */
#define NE2K_ISR_RXERR 0x04
#define NE2K_ISR_TXERR 0x08
#define NE2K_ISR_OVW   0x10
#define NE2K_ISR_CNT   0x20
#define NE2K_ISR_RDC   0x40
#define NE2K_ISR_RESET 0x80

#define NE2K_DCR_BYTE  0x49    /* 8 位总线 + FIFO 阈值 8 + 长位 */

extern void ne2k_init(void);
extern int  ne2k_tx(const u8 *frame, int len);   /* 发送一帧以太网数据 */
extern void ne2k_irq(void);                      /* IRQ10 中断处理 */
extern void ne2k_get_mac(u8 *out6);
extern u8   ne2k_regs_diag(u8 *isr, u8 *curr, u8 *bnry); /* 诊断：读关键寄存器 */

#endif
