ENTRY(kernel_main)

/* hey its me its linkity */

SECTIONS {
    . = 0x10000;
    .text : ALIGN(4) {
        KEEP(*(.text.entry))
        *(.text*)
    }
    .rodata : ALIGN(4) {
        *(.rodata*)
    }
    .data : ALIGN(4) {
        *(.data*)
    }
    .bss : ALIGN(4) {
        *(COMMON)
        *(.bss*)
    }
}
