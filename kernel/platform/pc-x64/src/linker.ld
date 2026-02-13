OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

MEMORY {
    LOAD : ORIGIN = 0x00200000, LENGTH = 0x00600000
    HIGH : ORIGIN = 0xffffffff80000000, LENGTH = 0x00600000
}

SECTIONS {
    . = 0x200000;
    __higher_half_displacement = 0xffffffff80000000 - .;

    .startup ALIGN(4K) : {
        KEEP(*(.multiboot2))
        KEEP(*(.startup))
    } >LOAD

    . += __higher_half_displacement;

    .text ALIGN(16) : AT(ADDR(.text) - __higher_half_displacement) {
        *(.text* .stub .gnu.linkonce.t.*)
    } >HIGH

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - __higher_half_displacement) {
        *(.rodata* .gnu.linkonce.r.*)
    } >HIGH

    .data ALIGN(4K) : AT(ADDR(.data) - __higher_half_displacement) {
        *(.data* .gnu.linkonce.d.*)
    } >HIGH

    .bss ALIGN(4K) : AT(ADDR(.bss) - __higher_half_displacement) {
        *(COMMON)
        *(.bss* .gnu.linkonce.b.*)

        . = ALIGN(2M);
        __stack_bottom_lba = . - __higher_half_displacement;
        . += 0x200000;
        __stack_top_lba = . - __higher_half_displacement;
    } >HIGH
}
