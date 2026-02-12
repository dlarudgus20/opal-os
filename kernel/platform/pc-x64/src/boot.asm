section .multiboot2
align 8
mb2_header_start:
    dd 0xE85250D6                  ; Multiboot2 magic
    dd 0                           ; i386 protected mode
    dd mb2_header_end - mb2_header_start
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))

    dw 0                           ; End tag type
    dw 0                           ; End tag flags
    dd 8                           ; End tag size
mb2_header_end:

section .text
bits 32
global _start
extern kmain

_start:
    cli

    mov esp, stack32_top

    ; PML4[0] -> PDPT
    mov eax, pdpt_table
    or eax, 0x03
    mov [pml4_table], eax
    mov dword [pml4_table + 4], 0

    ; PDPT[0] -> PD
    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax
    mov dword [pdpt_table + 4], 0

    ; Identity map first 1GiB using 2MiB pages
    xor ecx, ecx
.map_pd:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83                  ; Present + Write + Page Size
    mov [pd_table + ecx * 8], eax
    mov dword [pd_table + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map_pd

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode via EFER.LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Load PML4 base
    mov eax, pml4_table
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_start

bits 64
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack64_top
    call kmain

.hang:
    cli
    hlt
    jmp .hang

section .rodata
align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

section .bss
align 4096
pml4_table:
    resq 512
pdpt_table:
    resq 512
pd_table:
    resq 512

align 16
stack32_bottom:
    resb 16384
stack32_top:

align 16
stack64_bottom:
    resb 16384
stack64_top:
