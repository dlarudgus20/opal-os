#ifndef OPAL_SYSCALL_SYSCALL_H
#define OPAL_SYSCALL_SYSCALL_H

#include <stdint.h>

#include <opal/platform/interrupt.h>

enum syscall_index : uint64_t {
    SYS_TASK_EXIT = 1,
    SYS_TTY0_PUTC = 2,
    SYS_TTY0_GETC = 3,
    SYS_OPEN = 4,
    SYS_CLOSE = 5,
    SYS_READC = 6,
};

struct sysret {
    intptr_t ret0;
    intptr_t ret1;
    intptr_t ret2;
};

struct sysret syscall_dispatch(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

#endif
