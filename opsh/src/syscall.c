#include <stdint.h>
#include <stdarg.h>

#include <kc/kassert.h>
#include <kc/fmt.h>

#include "syscall.h"

enum syscall_index : uint64_t {
    SYS_TASK_EXIT = 1,
    SYS_TTY0_PUTC = 2,
    SYS_TTY0_GETC = 3,
    SYS_OPEN = 4,
    SYS_CLOSE = 5,
    SYS_READC = 6,
};

typedef struct sysret {
    int64_t ret1;
    int64_t ret2;
    int64_t ret3;
} sysret_t;

static sysret_t syscall(enum syscall_index index, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    register int64_t rax __asm__("rax") = (int64_t)index;
    register int64_t rcx __asm__("rcx");
    register int64_t r11 __asm__("r11");
    __asm__ volatile (
        "int 0x80\n"
        : "+r"(rax), "=r"(rcx), "=r"(r11)
        : "D"(arg0), "S"(arg1), "d"(arg2)
    );
    return (sysret_t){ rax, rcx, r11 };
}

void task_exit(void) {
    syscall(SYS_TASK_EXIT, 0, 0, 0);
    while (1) {}
}

int putchar(int ch) {
    unsigned char uch = (unsigned char)ch;
    int64_t ret = syscall(SYS_TTY0_PUTC, uch, 0, 0).ret1;
    return ret >= 0 ? uch : -1;
}

int getchar(void) {
    int64_t ret = syscall(SYS_TTY0_GETC, 0, 0, 0).ret1;
    return ret >= 0 ? (int)ret : -1;
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
    struct fmt f = {
        .write_fn = fmt_putchar,
        .size = 0,
        .count = 0,
        .error = false
    };
    va_list args;
    va_start(args, msg);
    int ret = fmt_vsprintf(&f, msg, args);
    va_end(args);
    return ret;
}

void panic_format(const char *msg, const char *file, const char *func, unsigned line, ...) {
    struct fmt f = {
        .write_fn = fmt_putchar,
        .size = 0,
        .count = 0,
        .error = false
    };
    va_list args;
    va_start(args, line);
    printf("[%s:%s:%d] ", file, func, line);
    fmt_vsprintf(&f, msg, args);
    putchar('\n');
    va_end(args);
    task_exit();
}

int open(const char *path) {
    sysret_t ret = syscall(SYS_OPEN, (uint64_t)path, 0, 0);
    return (int)ret.ret1;
}

int close(int fd) {
    sysret_t ret = syscall(SYS_CLOSE, (uint64_t)fd, 0, 0);
    return (int)ret.ret1;
}

int readc(int fd, size_t pos) {
    sysret_t ret = syscall(SYS_READC, (uint64_t)fd, (uint64_t)pos, 0);
    return (int)ret.ret1;
}
