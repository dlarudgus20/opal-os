#ifndef KC_FMT_H
#define KC_FMT_H

#include <stddef.h>
#include <stdarg.h>

#include "attributes.h"

struct fmt;

typedef bool (*fmt_write)(struct fmt *fmt, char ch);

struct fmt {
    // size > 0 -> buffer mode
    // size == 0 && write_fn != NULL -> write function mode
    // size == 0 && write_fn == NULL -> counting mode
    union {
        char *buffer;
        fmt_write write_fn;
    };
    unsigned size;
    unsigned count;
    bool error;
};

PRINTF_ATTR(2, 3)
int fmt_sprintf(struct fmt *restrict fmt, const char *restrict format, ...);

PRINTF_ATTR(2, 0)
int fmt_vsprintf(struct fmt *restrict fmt, const char *restrict format, va_list arg);

#endif
