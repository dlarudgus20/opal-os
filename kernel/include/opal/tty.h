#ifndef OPAL_TTY_H
#define OPAL_TTY_H

#include <stddef.h>
#include <stdint.h>

#include <kc/attributes.h>

enum {
    TTY_DARK_BLACK      = 0,
    TTY_DARK_RED        = 1,
    TTY_DARK_GREEN      = 2,
    TTY_DARK_YELLOW     = 3,
    TTY_DARK_BLUE       = 4,
    TTY_DARK_MAGENTA    = 5,
    TTY_DARK_CYAN       = 6,
    TTY_WHITE           = 7,
    TTY_GRAY            = 8,
    TTY_RED             = 9,
    TTY_GREEN           = 10,
    TTY_YELLOW          = 11,
    TTY_BLUE            = 12,
    TTY_MAGENTA         = 13,
    TTY_CYAN            = 14,
    TTY_BRIGHT_WHITE    = 15,
};

typedef uint8_t tty_color_t;

void tty0_init(void);

void tty0_puts(const char *str);
void tty0_puts_len(const char *str, size_t len);
void tty0_printf(const char *fmt, ...) PRINTF_ATTR(1, 2);

void tty0_set_fgcolor(tty_color_t color);
void tty0_set_bgcolor(tty_color_t color);
void tty0_reset_color(void);

#endif
