/* caplog code cave: appended to stock V1.2.0 right after its last byte
 * (file end 0x080BE680 -> cave at 0x080BE700, same 2 KB flash page). */
MEMORY
{
    CAVE (rx) : ORIGIN = 0x080BE700, LENGTH = 0x900
}

SECTIONS
{
    .text :
    {
        KEEP(*(.text.stubs))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > CAVE

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) *(.debug*) }

    .data : { *(.data*) } > CAVE
    .bss  : { *(.bss*) *(COMMON) } > CAVE
}

ASSERT(SIZEOF(.data) == 0, "caplog must not have .data")
ASSERT(SIZEOF(.bss) == 0, "caplog must not have .bss")
