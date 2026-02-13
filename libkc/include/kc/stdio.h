#ifndef KC_STDIO_H
#define KC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#include "attributes.h"

int snprintf_s(char *restrict buffer, size_t bufsz, const char *restrict format, ...) PRINTF_ATTR(3, 4);
int vsnprintf_s(char *restrict buffer, size_t bufsz, const char *restrict format, va_list ap) PRINTF_ATTR(3, 0);

#endif
