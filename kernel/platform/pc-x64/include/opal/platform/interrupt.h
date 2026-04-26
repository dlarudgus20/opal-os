#ifndef OPAL_PLATFORM_INTERRUPT_H
#define OPAL_PLATFORM_INTERRUPT_H

#pragma once

#include <stdint.h>

#include <opal/attributes.h>

#define ISR_STACKFRAME_PUSH \
    uint64_t gs; \
    uint64_t fs; \
    uint64_t es; \
    uint64_t ds; \
    uint64_t r15; \
    uint64_t r14; \
    uint64_t r13; \
    uint64_t r12; \
    uint64_t r11; \
    uint64_t r10; \
    uint64_t r9; \
    uint64_t r8; \
    uint64_t rsi; \
    uint64_t rdi; \
    uint64_t rdx; \
    uint64_t rcx; \
    uint64_t rbx; \
    uint64_t rax; \
    uint64_t rbp; \

#define ISR_STACKFRAME_IRETQ \
    uint64_t rip; \
    uint64_t cs; \
    uint64_t rflags; \
    uint64_t rsp; \
    uint64_t ss;

struct isr_stackframe {
    ISR_STACKFRAME_PUSH
    ISR_STACKFRAME_IRETQ
};

struct isr_stackframe_ec {
    ISR_STACKFRAME_PUSH
    uint64_t error_code;
    ISR_STACKFRAME_IRETQ
};

#define PRI_ISR_STACKFRAME \
    "gs=%#06lx fs=%#06lx es=%#06lx ds=%#06lx " \
    "r15=%#018lx r14=%#018lx r13=%#018lx r12=%#018lx r11=%#018lx r10=%#018lx r9=%#018lx r8=%#018lx " \
    "rsi=%#018lx rdi=%#018lx rdx=%#018lx rcx=%#018lx rbx=%#018lx rax=%#018lx " \
    "rbp=%#018lx rip=%#018lx cs=%#06lx rflags=%#010lx rsp=%#018lx ss=%#06lx"

#define ARG_ISR_STACKFRAME(frame) \
    frame->gs, frame->fs, frame->es, frame->ds, \
    frame->r15, frame->r14, frame->r13, frame->r12, frame->r11, frame->r10, frame->r9, frame->r8, \
    frame->rsi, frame->rdi, frame->rdx, frame->rcx, frame->rbx, frame->rax, \
    frame->rbp, frame->rip, frame->cs, frame->rflags, frame->rsp, frame->ss

struct PACKED tss {
    uint32_t reserved1;
    uint64_t rsp[3];
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t io_map_base;
};

void descriptors_init(void);

void isr_divide_by_zero();
void isr_debug();
void isr_nmi();
void isr_breakpoint();
void isr_overflow();
void isr_bound_range_exceeded();
void isr_invalid_opcode();
void isr_device_not_available();
void isr_double_fault();
void isr_invalid_tss();
void isr_segment_not_present();
void isr_stack_segment_fault();
void isr_general_protection_fault();
void isr_page_fault();
void isr_x87_floating_point();
void isr_alignment_check();
void isr_machine_check();
void isr_simd_floating_point();
void isr_unknown();
void isr_int80();

void isr_impl_divide_by_zero(struct isr_stackframe* frame);
void isr_impl_debug(struct isr_stackframe* frame);
void isr_impl_nmi(struct isr_stackframe* frame);
void isr_impl_breakpoint(struct isr_stackframe* frame);
void isr_impl_overflow(struct isr_stackframe* frame);
void isr_impl_bound_range_exceeded(struct isr_stackframe* frame);
void isr_impl_invalid_opcode(struct isr_stackframe* frame);
void isr_impl_device_not_available(struct isr_stackframe* frame);
void isr_impl_double_fault(struct isr_stackframe_ec* frame);
void isr_impl_invalid_tss(struct isr_stackframe_ec* frame);
void isr_impl_segment_not_present(struct isr_stackframe_ec* frame);
void isr_impl_stack_segment_fault(struct isr_stackframe_ec* frame);
void isr_impl_general_protection_fault(struct isr_stackframe_ec* frame);
void isr_impl_page_fault(struct isr_stackframe_ec* frame);
void isr_impl_x87_floating_point(struct isr_stackframe* frame);
void isr_impl_alignment_check(struct isr_stackframe_ec* frame);
void isr_impl_machine_check(struct isr_stackframe* frame);
void isr_impl_simd_floating_point(struct isr_stackframe* frame);
void isr_impl_unknown(struct isr_stackframe* frame);
void isr_impl_int80(struct isr_stackframe* frame);

#endif
