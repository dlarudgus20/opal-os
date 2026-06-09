#ifndef KC_STDIO_H
#define KC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#include "attributes.h"

PRINTF_ATTR(3, 4)
int ksnprintf(char *restrict buffer, size_t bufsz, const char *restrict format, ...);

PRINTF_ATTR(3, 0)
int kvsnprintf(char *restrict buffer, size_t bufsz, const char *restrict format, va_list ap);

#endif
