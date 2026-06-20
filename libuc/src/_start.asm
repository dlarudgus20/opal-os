bits 64

extern main

section .text
global _start
_start:
    call main
    mov rax, 1
    int 0x80
    jmp $

; TODO: try referencing _panic_format symbol here
