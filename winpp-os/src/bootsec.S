# WinPP OS -- 引导扇区 (real mode, 512B, GAS)
# 从软盘 LBA 1..N 读取 kernel.bin 到 0x10000 中转缓冲，开 A20，进保护模式，
# 32 位搬运到 0x100000，跳到内核入口。
# 实模式 ES:BX 无法装载超过 64KB 的目标区（BX 16 位截断会把第 129 扇区
# 之后的扇区写回低地址、覆盖内核头）——所以先读进低端缓冲，再在保护
# 模式下用 32 位 rep movsb 搬到最终地址。
# 链接地址 0x7C00；KERNEL_SECTORS 由 build_flat.ps1 通过 -D 注入。

.code16
.section .text
.global _start16
.global _start
_start = _start16
_start16:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp
    sti

    # 复位软驱 A:
    xorw %ax, %ax
    int  $0x13

    # ---- 循环读取: sector index si = 0..KERNEL_SECTORS-1, LBA = si+1 ----
    # 目的 flat = 0x10000 + si*512（中转缓冲）
    # 段寻址: ES = flat>>4, BX = flat - (ES<<4)，逐扇区重算（ES 随 flat 增长）
    xorl %esi, %esi          # si = 已读扇区数
read_one:
    movl %esi, %eax
    shll $9, %eax
    addl $0x10000, %eax      # eax = flat = 0x10000 + si*512
    movl %eax, %edx
    shrl $4, %edx            # edx = ES = flat>>4 (0x1000..0x2200, 16 位内)
    movw %dx, %es
    andl $0xF, %eax          # BX = flat & 0xF (512 对齐 -> 恒 0)
    movw %ax, %bx

    # CHS: LBA = si+1; c = LBA/36, h = (LBA%36)/18, s = LBA%18 + 1
    movl %esi, %eax
    incl %eax                # LBA = si + 1 (内核从扇区 1 开始)
    xorl %edx, %edx
    movl $36, %edi
    divl %edi                # eax = c, edx = rem
    movb %al, %ch            # cylinder
    movl %edx, %eax
    xorl %edx, %edx
    movl $18, %edi
    divl %edi                # eax = h, edx = s-1
    movb %al, %dh            # head
    movb %dl, %cl
    incb %cl                 # sector

    movw $0x0201, %ax        # AH=0x02 读 1 扇区
    movb $0x00, %dl          # 驱动器 A:
    int  $0x13
    jnc  read_ok
    xorw %ax, %ax            # 失败: 复位重试
    int  $0x13
    jmp  read_one
read_ok:
    incl %esi
    cmpl $KERNEL_SECTORS, %esi
    jb   read_one

    # ---- A20 ----
    movw $0x2401, %ax        # BIOS: enable A20
    int  $0x15
    inb  $0x92, %al          # fast A20 兜底
    orb  $0x02, %al
    outb %al, $0x92

    # ---- 进入保护模式 ----
    cli
    lgdt gdt_ptr
    movl %cr0, %eax
    orl  $1, %eax
    movl %eax, %cr0
    ljmp $0x08, $pm_entry32

.code32
pm_entry32:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # 32 位搬运: 0x10000 -> 0x100000, KERNEL_SECTORS*512 字节
    movl $0x10000, %esi
    movl $0x100000, %edi
    movl $KERNEL_SECTORS, %ecx
    shll $9, %ecx
    cld
    rep movsb

    movl $0x7C00, %esp       # 临时栈（内核 _start 会换自己的栈）
    xorl %eax, %eax          # magic = 0
    xorl %ebx, %ebx          # mb_info = 0
    movl $0x100000, %ecx
    jmp  *%ecx               # -> kernel _start (0x100000)

# ---------------- GDT ----------------
.align 4
gdt:
    .long 0, 0               # null
    .long 0x0000FFFF         # code32: base 0, limit 0xFFFFF
    .long 0x00CF9A00         #          D=1 G=1 P=1 S=1 type=0xA
    .long 0x0000FFFF         # data32
    .long 0x00CF9200         #          D=1 G=1 P=1 S=1 type=0x2
gdt_end:
gdt_ptr:
    .word gdt_end - gdt - 1
    .long gdt                # 实模式地址（链接于 0x7C00）
