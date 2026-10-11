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

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
        *(.rela.*)
    }
}
