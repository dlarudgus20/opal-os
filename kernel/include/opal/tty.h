#ifndef OPAL_TTY_H
#define OPAL_TTY_H

#include <stddef.h>
#include <stdint.h>

#include <kc/attributes.h>

#include <collections/linkedlist.h>

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

struct tty;

struct tty_ops {
    void (*write)(struct tty *tty, char ch);
    // if only one of color arguments is -1, it means "do not change"
    // if both arguments are -1, it means "reset"
    void (*set_color)(struct tty *tty, int fg, int bg);
};

struct tty {
    const struct tty_ops *ops;
    struct linkedlist_link link;
};

struct tty_0 {
    struct tty tty;
    struct linkedlist subtty_list;
};

extern struct tty_0 g_tty_0;

void tty_init(struct tty* tty, const struct tty_ops *ops);
void tty_puts(struct tty* tty, const char *str);
void tty_puts_len(struct tty* tty, const char *str, size_t len);
void tty_printf(struct tty* tty, const char *fmt, ...) PRINTF_ATTR(2, 3);

void tty0_init(void);
void tty0_register(struct tty* tty);
void tty0_unregister(struct tty* tty);

void tty0_set_fgcolor(tty_color_t color);
void tty0_set_bgcolor(tty_color_t color);
void tty0_reset_color(void);

#define tty0_puts(str) tty_puts(&g_tty_0.tty, str)
#define tty0_puts_len(str, len) tty_puts_len(&g_tty_0.tty, str, len)
#define tty0_printf(...) tty_printf(&g_tty_0.tty, ##__VA_ARGS__)

#endif
