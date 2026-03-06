#ifndef KC_FMT_H
#define KC_FMT_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "attributes.h"

struct fmt;

typedef bool (*fmt_write)(struct fmt *fmt, char ch);

struct fmt {
    // size > 0 -> buffer mode
    // size == 0 && write_fn != NULL -> write function mode
    // size == 0 && write_fn == NULL -> counting mode
    union {
        char* buffer;
        fmt_write write_fn;
    };
    unsigned size;
    unsigned count;
    bool error;
};

int fmt_sprintf(struct fmt *restrict fmt, const char *restrict format, ...) PRINTF_ATTR(2, 3);
int fmt_vsprintf(struct fmt *restrict fmt, const char *restrict format, va_list arg) PRINTF_ATTR(2, 0);

#endif
