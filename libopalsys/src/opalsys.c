#include <limits.h>

#include <stdint.h>
#include <stdarg.h>

#include <opalsys/opalsys.h>
#include <opalsys/syscall.h>

typedef struct sysret sysret_t;

static sysret_t syscall(
    enum syscall_index index, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    register int64_t rax __asm__("rax") = (int64_t)index;
    register int64_t rcx __asm__("rcx");
    register int64_t r11 __asm__("r11");
    register uint64_t r10 __asm__("r10") = arg4;
    /* clang-format off */ __asm__ volatile (
        "int 0x80\n"
        : "+r"(rax), "=r"(rcx), "=r"(r11)
        : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "memory"
    ); // clang-format on
    return (sysret_t){ rax, rcx, r11 };
}

static int assume_int(int64_t ret) {
    if (ret < INT_MIN || ret > INT_MAX) {
        unreachable();
    }
    return (int)ret;
}

void task_exit(void) {
    syscall(SYS_TASK_EXIT, 0, 0, 0, 0);
    unreachable();
}

int open(int fd, const char *path, enum open_mode mode, enum inode_flags flags) {
    sysret_t ret = syscall(SYS_OPEN, (uint64_t)fd, (uint64_t)path, mode, (uint64_t)flags);
    return assume_int(ret.ret0);
}

int close(int fd) {
    sysret_t ret = syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0);
    return assume_int(ret.ret0);
}

int dup(int oldfd, int newfd) {
    sysret_t ret = syscall(SYS_DUP, (uint64_t)oldfd, (uint64_t)newfd, 0, 0);
    return assume_int(ret.ret0);
}

int stat(int fd) {
    sysret_t ret = syscall(SYS_STAT, (uint64_t)fd, 0, 0, 0);
    return assume_int(ret.ret0);
}

ssize_t read(int fd, void *buffer, size_t size) {
    sysret_t ret = syscall(SYS_READ, (uint64_t)fd, (uint64_t)buffer, size, 0);
    return ret.ret0;
}

ssize_t write(int fd, const void *buffer, size_t size) {
    sysret_t ret = syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buffer, size, 0);
    return ret.ret0;
}

long ioctl(int fd, unsigned long op, unsigned long arg) {
    return syscall(SYS_IOCTL, (uint64_t)fd, op, arg, 0).ret0;
}

int mount(const char *fstype, int arg, const char *path) {
    sysret_t ret = syscall(SYS_MOUNT, (uint64_t)fstype, (uint64_t)arg, (uint64_t)path, 0);
    return assume_int(ret.ret0);
}

int pipe(int fds[2]) {
    sysret_t ret = syscall(SYS_PIPE, 0, 0, 0, 0);
    if (ret.ret0 < 0) {
        return assume_int(ret.ret0);
    }
    fds[0] = assume_int(ret.ret0);
    fds[1] = assume_int(ret.ret1);
    return 0;
}

pid_t fork(void) {
    sysret_t ret = syscall(SYS_FORK, 0, 0, 0, 0);
    return assume_int(ret.ret0);
}

int exec(int fd) {
    sysret_t ret = syscall(SYS_EXEC, (uint64_t)fd, 0, 0, 0);
    return assume_int(ret.ret0);
}
