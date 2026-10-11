# WinPP OS -- 引导扇区 (real mode, 512B, GAS)
# 从软盘 LBA 1..N 读取 kernel.bin 到 0x100000，开 A20，进保护模式，跳到内核入口。
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
    xorl %esi, %esi          # si = 已读扇区数
read_one:
    # 目的地址 flat = 0x100000 + si*512
    # 实模式 ES:BX 够不到 1MB(段寄存器仅16位)，用经典方案:
    #   ES = 0xFFFF, BX = flat - 0xFFFF0  (0x10..0xFFFF 内)
    movl $0x100000, %eax
    movl %esi, %edx
    shll $9, %edx
    addl %edx, %eax          # eax = flat
    subl $0xFFFF0, %eax      # eax = BX 值 (flat - 0xFFFF0)
    movw %ax, %bx
    movw $0xFFFF, %ax
    movw %ax, %es

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
