ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Multiboot 头在 boot.S 的 .text 最前面，随 .text 一起成为首段 */
    .text : {
        *(.text*)
    }

    . = ALIGN(4K);
    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(4K);
    .data : {
        *(.data*)
    }

    . = ALIGN(4K);
    .bss : {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4K);
    _kernel_end = .;

    /* 内嵌用户程序（flat binary，启动时由内核拷到用户页） */
    . = ALIGN(4K);
    .userprogs : {
        _user1_start = .; .incbin "user1.bin"; _user1_end = .;
        _user2_start = .; .incbin "user2.bin"; _user2_end = .;
        _user3_start = .; .incbin "user3.bin"; _user3_end = .;
    }
    _userprogs_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
        *(.rela.*)
    }
}
