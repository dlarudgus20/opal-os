%define MB2_MAGIC 0xE85250D6

%define DISPLACEMENT (0xffffffff80000000 - 0x00200000)
%define STACK_TOP 0xffffffff8f200000

extern __stack_top_lba
extern __stack_bottom_lba
extern kmain_platform

section .multiboot2

align 8
mb2_header_start:
    dd MB2_MAGIC                    ; Multiboot2 magic
    dd 0                            ; i386 protected mode
    dd mb2_header_end - mb2_header_start
    dd -(MB2_MAGIC + (mb2_header_end - mb2_header_start))
align 8, db 0
    dd 0, 8                         ; end tag
mb2_header_end:

section .bss

align 4096
global tmp_table
tmp_table: resb 0x5000

mb2_info: resd 1

section .startup alloc exec progbits

bits 32
global _start
_start:
    cli
    mov [mb2_info - DISPLACEMENT], ebx
    mov esp, __stack_top_lba

    ; PML4(0)
    mov edi, tmp_table - DISPLACEMENT
    mov ebx, edi
    lea eax, [edi + 0x1003]     ; PML4[0] -> PDPT_low
    mov [ebx], eax              ; 0000 0*
    add eax, 0x1000             ; PML4[0xff8] -> PDPT_high
    mov [ebx + 0xff8], eax      ; ffff ff8*
    ; PDPT_low(0x1000), PDPT_high(0x2000)
    add ebx, 0x1000
    add eax, 0x1000             ; PDPT_low[0] -> PDT_low
    or eax, 3
    mov [ebx], eax
    add ebx, 0x1000             ; PDPT_high[0xff0] -> PDT_high
    add eax, 0x1000             ; ffff ffff 8*
    mov [ebx + 0xff0], eax
    ; PDT_low(0x3000)
    add ebx, 0x1000             ; PDT_low[0x00:0x10] -> [0MB:6MB)
    mov dword [ebx], 0x00000083
    mov dword [ebx + 0x08], 0x00200083
    mov dword [ebx + 0x10], 0x00400083
    ; PDT_high(0x4000)
    add ebx, 0x1000
    ; PDT_high[0x08:0x20) -> [2MB:10MB)
    ; ffff ffff 80*
    mov dword [ebx], 0x00200083
    mov dword [ebx + 0x08], 0x00400083
    mov dword [ebx + 0x10], 0x00600083
    mov dword [ebx + 0x18], 0x00800083
    ; PDT_high[0x3c0] -> stack (2MB)
    ; ffff ffff 8f*
    mov dword [ebx + 0x3c0], __stack_bottom_lba + 0x83

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
    mov eax, tmp_table - DISPLACEMENT
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp 0x08:lm_start

bits 64
lm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    cld

    mov rsp, STACK_TOP
    mov rbp, rsp

    mov edi, [mb2_info]
    mov rax, kmain_platform
    call rax

.hang:
    cli
    hlt
    jmp .hang

align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64
