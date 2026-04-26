OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 0x200000;

    .text ALIGN(4K) : {
        *(.text* .stub .gnu.linkonce.t.*)
    }

    .rodata ALIGN(4K) :{
        *(.rodata* .gnu.linkonce.r.*)
    }

    .data ALIGN(4K) : {
        *(.data* .gnu.linkonce.d.*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss* .gnu.linkonce.b.*)
    }
}
