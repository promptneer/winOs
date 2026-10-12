ENTRY(_start)

SECTIONS
{
    . = 0x610000;
    .text : { *(.text*) }
    . = ALIGN(4);
    .data : { *(.data*) }
    . = ALIGN(4);
    .bss : { *(COMMON) *(.bss*) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.rela.*) }
}
