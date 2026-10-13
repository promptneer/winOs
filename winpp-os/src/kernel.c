#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include "rtc.h"
#include "shell.h"
#include "serial.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "tss.h"
#include "sched.h"
#include "fs.h"
#include "ipc.h"
#include "device.h"
#include "user.h"
#include "sem.h"
#include "gui.h"
#include "mouse.h"
#include "ne2k.h"
#include "net.h"

u32  g_mem_kb = 0;
char g_cpu_vendor[13] = "unknown";

void status_bar_redraw(void);   /* irq_handler 先于定义使用，需前置声明 */

/* regs_t 定义见 sched.h（与 isr.S 存根布局一致） */

static const char *exc_names[32] = {
    "#DE Divide Error", "#DB Debug", "#NMI", "#BP Breakpoint",
    "#OF Overflow", "#BR Bound Range Exceeded", "#UD Invalid Opcode",
    "#NM Device Not Available", "#DF Double Fault",
    "#CSO Coprocessor Segment Overrun", "#TS Invalid TSS",
    "#NP Segment Not Present", "#SS Stack-Segment Fault",
    "#GP General Protection Fault", "#PF Page Fault", "#RSVD",
    "#MF x87 FPU Error", "#AC Alignment Check", "#MC Machine Check",
    "#XF SIMD FPU Exception", "#VE Virtualization", "#CP Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"
};

static void cpu_detect(void)
{
    u32 eax = 0, ebx, ecx, edx;
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    ((u32 *)g_cpu_vendor)[0] = ebx;
    ((u32 *)g_cpu_vendor)[1] = edx;
    ((u32 *)g_cpu_vendor)[2] = ecx;
    g_cpu_vendor[12] = 0;
}

/* ---------------- 异常 / IRQ 处理 ---------------- */

void isr_handler(regs_t *r)
{
    /* #PF 专用：打印 CR2/错误码，可恢复（demand paging 演示） */
    if (r->int_no == 14) {
        u32 cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        pf_inc();
        serial_printf("PAGE FAULT @%x err=%x\r\n", cr2, r->err_code);
        {
            u32 pdi = cr2 >> 22, pti = (cr2 >> 12) & 0x3FF;
            u32 cr3 = read_cr3();
            volatile u32 *pdv = (volatile u32 *)cr3;   /* 恒等映射 1MB+，cr3 物理页可直接读 */
            volatile u32 *real_pd = (volatile u32 *)0x10F000; /* boot 时的真 PD 物理页 */
            serial_printf("[pfdbg] cr2=%x eip=%x cs=%x cr3=%x pd0=%x pd1=%x pd1023=%x PTE=%x realPD0=%x realPD1=%x realPD1023=%x\r\n",
                          cr2, r->eip, r->cs, cr3,
                          pdv[0], pdv[1], pdv[1023],
                          ((volatile u32 *)(0xFFC00000u + pdi * 0x1000))[pti],
                          real_pd[0], real_pd[1], real_pd[1023]);
        }
        vga_printf("page fault @%x (err=%x: %s%s%s)\n", cr2, r->err_code,
                   (r->err_code & 1) ? "present" : "not-present",
                   (r->err_code & 2) ? "+write" : "+read",
                   (r->err_code & 4) ? "+user" : "+supervisor");
        /* 按需映射：0x0 未映射访问 -> 映射一页后重试（demand paging 闭环） */
        if (cr2 == 0) {
            u32 page = pmm_alloc_page();
            if (page) {
                map_page(0, page, 0x3);
                vga_printf("demand-mapped page %x for 0x0, retrying\n", page);
                serial_printf("DEMAND MAP 0x0 -> %x\r\n", page);
            }
        }
        return;   /* 返回后 CPU 重试 faulting 指令 */
    }

    if (r->int_no < 32) {
        vga_set_attr(0x1F);
        vga_clear();
        vga_printf("=== KERNEL PANIC ===\n\n");
        vga_printf("exception %s  (int=%u err=%x)\n",
                   exc_names[r->int_no], r->int_no, r->err_code);
        vga_printf("eip=%x  cs=%x  eflags=%x\n", r->eip, r->cs, r->eflags);
        vga_printf("eax=%x ebx=%x ecx=%x edx=%x\n",
                   r->eax, r->ebx, r->ecx, r->edx);
        vga_printf("esi=%x edi=%x ebp=%x esp=%x\n",
                   r->esi, r->edi, r->ebp, r->esp);
        vga_printf("\nSystem halted. Press nothing -- restart the machine.\n");
        for (;;) asm volatile("cli; hlt");
    }
}

void irq_handler(regs_t *r)
{
    u8 irq = (u8)(r->int_no - 32);

    /* S7 设备驱动层：按 IRQ 号分发到注册设备（timer.irq / kbd.irq） */
    device_t *d = device_by_irq(irq);
    if (d && d->irq) d->irq();

    /* 内核策略层（不属于设备驱动）：调度与状态栏刷新 */
    if (irq == 0) {
        /* S10：图形模式下跳过文本状态栏重绘（VRAM 已被图形帧缓冲占用） */
        if (!gui_active && timer_ticks() % TIMER_HZ == 0) status_bar_redraw();
        /* S9：每 tick 唤醒到期睡眠进程（不依赖 need_resched） */
        if (sched_running()) sched_wakeup_scan();
        /* 多任务：每个时间片强制重新调度 */
        if (sched_running() && (timer_ticks() % SCHED_QUANTUM) == 0)
            sched_need_resched();
    }
    if (irq >= 8) outb(0xA0, 0x20);   /* 从片 EOI */
    outb(0x20, 0x20);                 /* 主片 EOI */

    /* 末尾：必要时切换进程（sched_do_switch 换栈后不返回） */
    if (sched_should_switch())
        sched_tick(r);
}

/* ---------------- 状态栏 / 欢迎屏 ---------------- */

void status_bar_redraw(void)
{
    u8 sx, sy, attr;
    vga_get_pos(&sx, &sy);
    attr = vga_get_attr();

    vga_fill(0, 0, VGA_COLS, 1, ' ', 0x1F);
    vga_gotoxy(1, 0);
    vga_set_attr(0x1F);
    vga_printf("WinPP OS v0.1");
    vga_set_attr(0x3F);
    vga_putc(0xB3);
    vga_set_attr(0x1F);

    u16 yr; u8 mo, da, hh, mi, ss;
    rtc_get_datetime(&yr, &mo, &da, &hh, &mi, &ss);
    vga_printf(" %04u-%02u-%02u %02u:%02u:%02u ", yr, mo, da, hh, mi, ss);

    u32 up = timer_uptime_sec();
    vga_printf("[up %02u:%02u:%02u] [mem %u KB]", up / 3600, (up / 60) % 60, up % 60, g_mem_kb);

    vga_gotoxy(sx, sy);
    vga_set_attr(attr);
}

static void draw_banner(void)
{
    vga_fill(0, 1, VGA_COLS, 2, ' ', 0x1F);
    vga_gotoxy(2, 1);
    vga_set_attr(0x1F);
    vga_printf("WinPP OS v0.1 -- a tiny unix-like kernel for i386");
    vga_gotoxy(2, 2);
    vga_set_attr(0x3F);
    vga_printf("multiboot v1 | VGA text 80x25 | type 'help' for commands");
}

void kernel_redraw_shell(void)
{
    vga_clear();
    draw_banner();
    status_bar_redraw();
    vga_gotoxy(0, 4);
}

/* ---------------- 机器控制 ---------------- */

void kernel_reboot(void)
{
    outb(0x64, 0xFE);               /* 8042 复位 */
    for (;;) asm volatile("cli; hlt");
}

void kernel_poweroff(void)
{
    outw(0x604, 0x2000);            /* QEMU ACPI PM */
    for (;;) asm volatile("cli; hlt");
}

/* ---------------- 入口 ---------------- */

void kmain(u32 magic, u32 mb_info)
{
    if (magic == 0x2BADB002 && mb_info) {
        u32 flags = *(volatile u32 *)mb_info;
        if (flags & 1)
            g_mem_kb = *(volatile u32 *)(mb_info + 4) +
                       *(volatile u32 *)(mb_info + 8);
    } else {
        /* 扁平引导（无 multiboot）：CMOS 扩展内存 + 640KB 基本内存 */
        outb(0x70, 0x30);
        u32 ext = inb(0x71);
        outb(0x70, 0x31);
        ext |= (u32)inb(0x71) << 8;
        g_mem_kb = 640 + ext;
    }

    serial_init();
    serial_puts("\r\n=== WinPP kernel boot ===\r\n");
    vga_clear();
    vga_printf("kmain magic=%x mb=%x\n", magic, mb_info);
    serial_printf("kmain magic=%x mb=%x\n", magic, mb_info);

    serial_puts("[1] gdt ...\r\n");
    vga_printf("[1] gdt ...\n");
    tss_init();          /* 必须先于 gdt_init（TSS 描述符需要 tss 地址） */
    gdt_init();
    serial_puts("[2] idt ...\r\n");
    vga_printf("[2] idt ...\n");
    idt_init();
    serial_puts("[3] pic ...\r\n");
    vga_printf("[3] pic ...\n");
    pic_remap();
    serial_puts("[3b] device table ...\r\n");
    vga_printf("[3b] device table ...\n");
    device_init_all();
    serial_puts("[4] timer ...\r\n");
    vga_printf("[4] timer ...\n");
    timer_init();
    serial_puts("[5] kbd ...\r\n");
    vga_printf("[5] kbd ...\n");
    keyboard_init();
    serial_puts("[5b] mouse ...\r\n");
    vga_printf("[5b] mouse ...\n");
    /* S10 Bochs 实测：Bochs 模拟器下启用 PS/2 鼠标后键盘注入失效（模拟器兼容差异，真实硬件正常）。
     * 鼠标驱动保留（0xD4 修复版），GUI 交互实测走键盘。 */
    /* mouse_init(); */
    serial_puts("[5b] mouse SKIP for bochs kbd test\r\n");
    serial_puts("[6] cpu ...\r\n");
    vga_printf("[6] cpu ...\n");
    cpu_detect();
    serial_puts("[7] pmm ...\r\n");
    vga_printf("[7] pmm ...\n");
    pmm_init(g_mem_kb);
    serial_puts("[8] paging ...\r\n");
    vga_printf("[8] paging ...\n");
    paging_init(g_mem_kb);
    serial_puts("[9] kheap ...\r\n");
    vga_printf("[9] kheap ...\n");
    kheap_init();
    serial_puts("[10] sched ...\r\n");
    vga_printf("[10] sched ...\n");
    sched_init();
    serial_puts("[10] sched ...\r\n");
    vga_printf("[10] sched ...\n");
    sched_init();
    serial_puts("[10b] user ...\r\n");
    vga_printf("[10b] user ...\n");
    user_init();
    serial_puts("[11] fs ...\r\n");
    vga_printf("[11] fs ...\n");
    fs_init();
    serial_puts("[11b] ipc ...\r\n");
    vga_printf("[11b] ipc ...\n");
    ipc_init();
    serial_puts("[11c] sem ...\r\n");
    vga_printf("[11c] sem ...\n");
    sem_init();
    serial_puts("[11d] ne2k ...\r\n");
    vga_printf("[11d] ne2k ...\n");
    ne2k_init();
    serial_puts("[11e] net ...\r\n");
    vga_printf("[11e] net ...\n");
    net_init();
    serial_puts("[12] all init ok, drawing UI ...\r\n");
    vga_printf("[12] all init ok, drawing UI ...\n");

    vga_set_area(4, 24);
    kernel_redraw_shell();

    vga_gotoxy(0, 4);
    vga_set_attr(0x07);
    vga_printf("WinPP kernel alive. RAM: %u KB. CPU: %s\n\n",
               g_mem_kb, g_cpu_vendor);

    asm volatile("sti");
    shell_run();
}
