#include <limits.h>

#include <stdint.h>
#include <stdarg.h>

#include <kc/kassert.h>
#include <kc/fmt.h>

#include <libuc.h>

enum syscall_index : uint64_t {
    SYS_TASK_EXIT = 1,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_DUP,
    SYS_READ,
    SYS_WRITE,
    SYS_IOCTL,
    SYS_MOUNT,
};

typedef struct sysret {
    int64_t ret0;
    int64_t ret1;
    int64_t ret2;
} sysret_t;

static sysret_t syscall(enum syscall_index index, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    register int64_t rax __asm__("rax") = (int64_t)index;
    register int64_t rcx __asm__("rcx");
    register int64_t r11 __asm__("r11");
    /* clang-format off */ __asm__ volatile (
        "int 0x80\n"
        : "+r"(rax), "=r"(rcx), "=r"(r11)
        : "D"(arg0), "S"(arg1), "d"(arg2)
        : "memory", "cc"
    ); // clang-format on
    return (sysret_t){ rax, rcx, r11 };
}

void task_exit(void) {
    syscall(SYS_TASK_EXIT, 0, 0, 0);
    unreachable();
}

int putchar(int ch) {
    unsigned char uch = (unsigned char)ch;
    return write(1, &uch, 1) == 1 ? uch : -1;
}

int getchar(void) {
    unsigned char ch;
    return read(0, &ch, 1) == 1 ? ch : -1;
}

int puts(const char *str) {
    while (*str != '\0') {
        if (putchar(*str++) < 0) {
            return -1;
        }
    }
    putchar('\n');
    return 0;
}

size_t getline(char *buf, size_t len) {
    size_t pos = 0;
    while (pos + 1 < len) {
        int ch = getchar();
        if (ch < 0 || ch == '\n') {
            break;
        }
        buf[pos++] = (char)ch;
    }
    buf[pos] = '\0';
    return pos;
}

static bool fmt_putchar(struct fmt *, char ch) {
    return putchar(ch) >= 0;
}

int printf(const char *msg, ...) {
    struct fmt f = { .write_fn = fmt_putchar, .size = 0, .count = 0, .error = false };
    va_list args;
    va_start(args, msg);
    int ret = fmt_vsprintf(&f, msg, args);
    va_end(args);
    return ret;
}

void _panic_format(const char *msg, const char *file, const char *func, unsigned line, ...) {
    struct fmt f = { .write_fn = fmt_putchar, .size = 0, .count = 0, .error = false };
    va_list args;
    va_start(args, line);
    printf("[%s:%s:%d] ", file, func, line);
    fmt_vsprintf(&f, msg, args);
    putchar('\n');
    va_end(args);
    task_exit();
}

int open(int fd, const char *path, enum open_mode mode) {
    sysret_t ret = syscall(SYS_OPEN, (uint64_t)fd, (uint64_t)path, mode);
    return (int)ret.ret0;
}

int close(int fd) {
    sysret_t ret = syscall(SYS_CLOSE, (uint64_t)fd, 0, 0);
    return (int)ret.ret0;
}

int dup(int oldfd, int newfd) {
    sysret_t ret = syscall(SYS_DUP, (uint64_t)oldfd, (uint64_t)newfd, 0);
    return (int)ret.ret0;
}

ssize_t read(int fd, void *buffer, size_t size) {
    sysret_t ret = syscall(SYS_READ, (uint64_t)fd, (uint64_t)buffer, size);
    return (ssize_t)ret.ret0;
}

ssize_t write(int fd, const void *buffer, size_t size) {
    sysret_t ret = syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buffer, size);
    return (ssize_t)ret.ret0;
}

long ioctl(int fd, unsigned long op, unsigned long arg) {
    return syscall(SYS_IOCTL, (uint64_t)fd, op, arg).ret0;
}

int mount(const char *fstype, int arg, const char *path) {
    sysret_t ret = syscall(SYS_MOUNT, (uint64_t)fstype, (uint64_t)arg, (uint64_t)path);
    return (int)ret.ret0;
}
