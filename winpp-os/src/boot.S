# WinPP OS -- 32-bit 入口 (GAS, i686)
# Multiboot 构建：头直接放在 .text 最前面（boot.S 是第一个输入文件，
# 头字节就是首个 LOAD 段段首，落在文件前 8KB 内）。
# 扁平二进制构建（-DNO_MULTIBOOT=1）：头被剔除，.text 段首即 0x100000 入口。

.section .text
#ifndef NO_MULTIBOOT
.align 4
multiboot_header:
    .long 0x1BADB002            # magic
    .long 0x3                   # flags: 页对齐 + 内存信息
    .long -(0x1BADB002 + 0x3)   # checksum
#endif

.global _start
_start:
    cli
    movl $stack_top, %esp       # 建立内核栈
    andl $-16, %esp             # 16 字节对齐
    pushl %ebx                  # multiboot info 指针
    pushl %eax                  # multiboot magic
    # 开启 SSE (CR4.OSFXSR|OSXMMEXCPT)，否则 SSE 指令 #UD
    movl %cr4, %eax
    orl  $0x600, %eax
    movl %eax, %cr4
    call kmain
.hang:
    cli
    hlt
    jmp .hang

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
