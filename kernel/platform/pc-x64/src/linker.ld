OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

KERNEL_LMA = 0x00200000;
KERNEL_VMA = 0xffffffff80000000;

MEMORY {
    LOAD : ORIGIN = KERNEL_LMA, LENGTH = 0x00600000
    HIGH : ORIGIN = KERNEL_VMA, LENGTH = 0x00600000
}

SECTIONS {
    __kernel_start_lba = KERNEL_LMA;
    __higher_half_displacement = KERNEL_VMA - KERNEL_LMA;

    __startup_lba = ALIGN(KERNEL_LMA, 4K);

    .startup __startup_lba : AT(__startup_lba) {
        KEEP(*(.multiboot2))
        KEEP(*(.startup))
    } >LOAD

    __startup_end_lba = ALIGN(LOADADDR(.startup) + SIZEOF(.startup), 16);
    __text_lba = __startup_end_lba;

    .text (__text_lba + __higher_half_displacement) : AT(__text_lba) {
        *(.text* .stub .gnu.linkonce.t.*)
    } >HIGH

    __text_end_lba = ALIGN(LOADADDR(.text) + SIZEOF(.text), 4K);
    __rodata_lba = __text_end_lba;

    .rodata (__rodata_lba + __higher_half_displacement) : AT(__rodata_lba) {
        *(.rodata* .gnu.linkonce.r.*)

        . = ALIGN(8);
        unit_test_start__ = .;
        KEEP(*(.unittest))
        unit_test_end__ = .;
    } >HIGH

    __rodata_end_lba = ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4K);
    __data_lba = __rodata_end_lba;

    .data (__data_lba + __higher_half_displacement) : AT(__data_lba) {
        *(.data* .gnu.linkonce.d.*)
    } >HIGH

    __data_end_lba = ALIGN(LOADADDR(.data) + SIZEOF(.data), 4K);
    __bss_lba = __data_end_lba;

    .bss (__bss_lba + __higher_half_displacement) : AT(__bss_lba) {
        *(COMMON)
        *(.bss* .gnu.linkonce.b.*)
        . = ALIGN(4K);
        __before_stack_lba = . - __higher_half_displacement;

        . = ALIGN(2M);
        __stack_bottom_lba = . - __higher_half_displacement;
        . += 0x200000;
        __stack_top_lba = . - __higher_half_displacement;
    } >HIGH

    __bss_end_lba = ALIGN(LOADADDR(.bss) + SIZEOF(.bss), 4K);
    __kernel_end_lba = __bss_end_lba;
}
