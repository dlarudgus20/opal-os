bits 64

section .text

%macro push_all 0
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
%endmacro

%macro pop_all 0
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
%endmacro

%macro int_handler 1
extern isr_impl_%1
global isr_%1
isr_%1:
    push_all
    mov rdi, rsp
    call isr_impl_%1
    pop_all
    iretq
%endmacro

%macro int_handler_err 1
extern isr_impl_%1
global isr_%1
isr_%1:
    push_all
    mov rdi, rsp
    call isr_impl_%1
    pop_all
    add rsp, 8
    iretq
%endmacro

int_handler     divide_by_zero
int_handler     debug
int_handler     nmi
int_handler     breakpoint
int_handler     overflow
int_handler     bound_range_exceeded
int_handler     invalid_opcode
int_handler     device_not_available
int_handler_err double_fault
int_handler_err invalid_tss
int_handler_err segment_not_present
int_handler_err stack_segment_fault
int_handler_err general_protection_fault
int_handler_err page_fault
int_handler     x87_floating_point
int_handler_err alignment_check
int_handler     machine_check
int_handler     simd_floating_point
int_handler     unknown
int_handler     irq0
int_handler     irq1
int_handler     irq2
int_handler     irq3
int_handler     irq4
int_handler     irq5
int_handler     irq6
int_handler     irq7
int_handler     irq8
int_handler     irq9
int_handler     irq10
int_handler     irq11
int_handler     irq12
int_handler     irq13
int_handler     irq14
int_handler     irq15
