bits 64

section .text

; void context_switch(struct context *from, const struct context *to)
global context_switch
context_switch:
    push rbp
    mov rbp, rsp

    push rax
    pushfq
    cli

    xor rax, rax
    mov ax, ss
    mov [rdi + 23 * 8], rax  ; ss

    mov rax, rbp
    add rax, 16              ; push rbp + return address
    mov [rdi + 22 * 8], rax  ; rsp

    pop rax
    mov [rdi + 21 * 8], rax  ; rflags

    xor rax, rax
    mov ax, cs
    mov [rdi + 20 * 8], rax  ; cs

    mov rax, [rbp + 8]       ; return address
    mov [rdi + 19 * 8], rax  ; rip

    pop rax
    pop rbp

    ; save other registers
    mov rsp, rdi
    add rsp, 19 * 8
    push rbp
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax
    push fs
    push gs

    ; restore context
    mov rsp, rsi
    pop gs
    pop fs
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rbp
    iretq
