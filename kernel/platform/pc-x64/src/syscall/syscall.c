#include <opal/syscall/syscall.h>
#include <opal/platform/interrupt.h>
#include <opal/platform/asm.h>

void isr_impl_int80(struct isr_stackframe *frame) {
    interrupts_enable();
    struct sysret ret =
        syscall_dispatch(frame->rax, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8);
    frame->rax = (uint64_t)ret.ret0;
    frame->rcx = (uint64_t)ret.ret1;
    frame->r11 = (uint64_t)ret.ret2;
}
