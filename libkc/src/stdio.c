#include <limits.h>

#include <kc/stdio.h>
#include <kc/fmt.h>

int ksnprintf(char *restrict buffer, size_t bufsz, const char *restrict format, ...) {
    va_list arg;
    va_start(arg, format);
    int result = kvsnprintf(buffer, bufsz, format, arg);
    va_end(arg);
    return result;
}

int kvsnprintf(char *restrict buffer, size_t bufsz, const char *restrict format, va_list arg) {
    if (buffer && bufsz > INT_MAX) {
        return -1;
    }
    if (!buffer && bufsz > 0) {
        return -1;
    }

    if (!format) {
        if (buffer && bufsz > 0) {
            buffer[0] = '\0';
        }
        return -1;
    }

    struct fmt fmt = {
        .buffer = bufsz > 0 ? buffer : NULL,
        .size = (unsigned)bufsz,
        .count = 0,
        .error = false,
    };
    int result = fmt_vsprintf(&fmt, format, arg);
    if (result < 0 && buffer && bufsz > 0) {
        buffer[0] = '\0';
    }
    return result;
}
