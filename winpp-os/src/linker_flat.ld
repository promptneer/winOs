ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* 扁平二进制：boot.S 的 .text 段首即文件偏移 0（= 入口 0x100000） */
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

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
        *(.rela.*)
    }
}
