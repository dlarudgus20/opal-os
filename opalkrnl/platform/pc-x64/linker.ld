OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

KERNEL_LMA = 0x00200000;
KERNEL_VMA = 0xffffffff80000000;

MEMORY {
    LOAD : ORIGIN = KERNEL_LMA, LENGTH = 0x00600000
    HIGH : ORIGIN = KERNEL_VMA, LENGTH = 0x00600000
}

SECTIONS {
    __kernel_start_lma = KERNEL_LMA;
    __higher_half_displacement = KERNEL_VMA - KERNEL_LMA;

    __startup_lma = ALIGN(KERNEL_LMA, 4K);

    .startup __startup_lma : AT(__startup_lma) {
        KEEP(*(.multiboot2))
        KEEP(*(.startup))
    } >LOAD

    __startup_end_lma = ALIGN(LOADADDR(.startup) + SIZEOF(.startup), 16);
    __text_lma = __startup_end_lma;

    .text (__text_lma + __higher_half_displacement) : AT(__text_lma) {
        *(.text* .stub .gnu.linkonce.t.*)
    } >HIGH

    __text_end_lma = ALIGN(LOADADDR(.text) + SIZEOF(.text), 4K);
    __rodata_lma = __text_end_lma;

    .rodata (__rodata_lma + __higher_half_displacement) : AT(__rodata_lma) {
        *(.rodata* .gnu.linkonce.r.*)

        . = ALIGN(8);
        __ktest_start = .;
        KEEP(*(.ktest))
        __ktest_end = .;
    } >HIGH

    __rodata_end_lma = ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4K);
    __data_lma = __rodata_end_lma;

    .data (__data_lma + __higher_half_displacement) : AT(__data_lma) {
        *(.data* .gnu.linkonce.d.*)
    } >HIGH

    __data_end_lma = ALIGN(LOADADDR(.data) + SIZEOF(.data), 4K);
    __bss_lma = __data_end_lma;

    .bss (__bss_lma + __higher_half_displacement) : AT(__bss_lma) {
        *(COMMON)
        *(.bss* .gnu.linkonce.b.*)
        . = ALIGN(4K);
        __before_stack_lma = . - __higher_half_displacement;

        . = ALIGN(2M);
        __stack_bottom_lma = . - __higher_half_displacement;
        . += 0x200000;
        __stack_top_lma = . - __higher_half_displacement;
    } >HIGH

    __bss_end_lma = ALIGN(LOADADDR(.bss) + SIZEOF(.bss), 4K);
    __kernel_end_lma = __bss_end_lma;
}
