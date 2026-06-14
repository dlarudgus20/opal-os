#ifndef OPAL_SYSCALL_SYSCALL_H
#define OPAL_SYSCALL_SYSCALL_H

#include <stdint.h>

#include <opal/platform/interrupt.h>

enum syscall_index : uint64_t {
    SYS_TASK_EXIT = 1,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_DUP,
    SYS_READC,
    SYS_WRITEC,
    SYS_IOCTL,
    SYS_MOUNT,
    SYS_PIPE,
    SYS_FORK,
    SYS_EXEC,
};

struct sysret {
    intptr_t ret0;
    intptr_t ret1;
    intptr_t ret2;
};

struct sysret syscall_dispatch(struct isr_stackframe *frame, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

#endif
