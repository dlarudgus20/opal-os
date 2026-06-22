#include <limits.h>

#include <stdint.h>
#include <stdarg.h>

#include <kc/kassert.h>
#include <kc/fmt.h>

#include <opalsys/opalsys.h>

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
