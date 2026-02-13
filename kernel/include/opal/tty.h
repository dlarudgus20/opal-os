#ifndef OPAL_TTY_H
#define OPAL_TTY_H

#include <kc/attributes.h>

void tty0_init(void);
void tty0_puts(const char *str);
void tty0_printf(const char *fmt, ...) PRINTF_ATTR(1, 2);

#endif
