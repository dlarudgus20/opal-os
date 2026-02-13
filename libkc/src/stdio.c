#include <limits.h>

#include <kc/stdio.h>
#include <kc/fmt.h>

int snprintf_s(char *restrict buffer, size_t bufsz, const char *restrict format, ...) {
    va_list arg;
    va_start(arg, format);
    int result = vsnprintf_s(buffer, bufsz, format, arg);
    va_end(arg);
    return result;
}

int vsnprintf_s(char *restrict buffer, size_t bufsz, const char *restrict format, va_list arg) {
    if (!buffer) {
        return -1;
    }
    if (bufsz == 0 || bufsz > INT_MAX) {
        return -1;
    }
    if (!format) {
        buffer[0] = '\0';
        return -1;
    }

    struct fmt fmt = {
        .buffer = buffer,
        .size = (unsigned)bufsz,
        .count = 0,
        .error = false
    };
    int result = fmt_vsprintf(&fmt, format, arg);
    if (result < 0) {
        buffer[0] = '\0';
    }
    return result;
}
