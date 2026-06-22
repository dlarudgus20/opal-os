#ifndef OPAL_SYSCALL_SYSCALL_H
#define OPAL_SYSCALL_SYSCALL_H

#include <stdint.h>

#include <opal/platform/interrupt.h>

#include <opalsys/syscall.h>

struct sysret syscall_dispatch(struct isr_stackframe *frame, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

#endif
