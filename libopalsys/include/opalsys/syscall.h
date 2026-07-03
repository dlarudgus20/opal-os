#ifndef OPALSYS_SYSCALL_H
#define OPALSYS_SYSCALL_H

#include <stdint.h>

enum syscall_index : uintptr_t {
    SYS_TASK_EXIT = 1,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_DUP,
    SYS_STAT,
    SYS_READ,
    SYS_WRITE,
    SYS_IOCTL,
    SYS_MOUNT,
    SYS_PIPE,
    SYS_FORK,
    SYS_EXEC,
    SYS_WAITPID,
};

struct sysret {
    intptr_t ret0;
    intptr_t ret1;
    intptr_t ret2;
};

#endif
