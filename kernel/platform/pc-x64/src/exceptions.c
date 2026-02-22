#include <kc/assert.h>

#include <opal/platform/interrupt.h>

void isr_impl_divide_by_zero(struct isr_stackframe* frame) {
    panicf("#DE "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_debug(struct isr_stackframe* frame) {
    panicf("#DB "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_nmi(struct isr_stackframe* frame) {
    panicf("#NMI "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_breakpoint(struct isr_stackframe* frame) {
    panicf("#BP "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_overflow(struct isr_stackframe* frame) {
    panicf("#OF "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_bound_range_exceeded(struct isr_stackframe* frame) {
    panicf("#BR "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_invalid_opcode(struct isr_stackframe* frame) {
    panicf("#UD "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_device_not_available(struct isr_stackframe* frame) {
    panicf("#NM "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_double_fault(struct isr_stackframe_ec* frame) {
    panicf("#DF:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_invalid_tss(struct isr_stackframe_ec* frame) {
    panicf("#TS:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_segment_not_present(struct isr_stackframe_ec* frame) {
    panicf("#NP:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_stack_segment_fault(struct isr_stackframe_ec* frame) {
    panicf("#SS:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_general_protection_fault(struct isr_stackframe_ec* frame) {
    panicf("#GP:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_page_fault(struct isr_stackframe_ec* frame) {
    panicf("#PF:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_x87_floating_point(struct isr_stackframe* frame) {
    panicf("#MF "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_alignment_check(struct isr_stackframe_ec* frame) {
    panicf("#AC:%#018lx "PRI_ISR_STACKFRAME, frame->error_code, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_machine_check(struct isr_stackframe* frame) {
    panicf("#MC "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_simd_floating_point(struct isr_stackframe* frame) {
    panicf("#XM "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

void isr_impl_unknown(struct isr_stackframe* frame) {
    panicf("#UNKNOWN "PRI_ISR_STACKFRAME, ARG_ISR_STACKFRAME(frame));
}

#ifdef OPAL_TEST
void isr_divide_by_zero() {}
void isr_debug() {}
void isr_nmi() {}
void isr_breakpoint() {}
void isr_overflow() {}
void isr_bound_range_exceeded() {}
void isr_invalid_opcode() {}
void isr_device_not_available() {}
void isr_double_fault() {}
void isr_invalid_tss() {}
void isr_segment_not_present() {}
void isr_stack_segment_fault() {}
void isr_general_protection_fault() {}
void isr_page_fault() {}
void isr_x87_floating_point() {}
void isr_alignment_check() {}
void isr_machine_check() {}
void isr_simd_floating_point() {}
void isr_unknown() {}
#endif
