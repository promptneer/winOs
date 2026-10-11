ENTRY(_start16)

SECTIONS
{
    . = 0x7C00;

    .text : {
        *(.text*)
    }

    . = 0x7DFE;
    .magic : {
        SHORT(0xAA55)
    }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
    }
}
