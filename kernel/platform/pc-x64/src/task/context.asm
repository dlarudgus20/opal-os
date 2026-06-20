bits 64

section .text

; void context_switch_asm(struct context *from, const struct context *to);
global context_switch_asm
context_switch_asm:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp

    mov rsp, [rsi]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; void enter_userland_asm(virt_addr_t entry, virt_addr_t stack_top, uint64_t cs, uint64_t ss);
global enter_userland_asm:
enter_userland_asm:
    push rcx    ; ss
    push rsi    ; rsp = stack_top
    mov rax, 0x202
    push rax    ; rflags = IF
    push rdx    ; cs
    push rdi    ; rip = entry

    ; zero out all registers
    ; before iretq into userland
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    iretq

; void return_to_userland(const struct isr_stackframe *frame);
global return_to_userland
return_to_userland:
    cli
    mov rsp, rdi
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
