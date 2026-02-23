#include <stdarg.h>

#include <kc/fmt.h>
#include <kc/stdlib.h>

#include <opal/tty.h>

struct tty_0 g_tty_0;

void tty_init(struct tty *tty, const struct tty_ops *ops) {
    tty->ops = ops;
}

void tty_puts(struct tty *tty, const char *str) {
    while (*str != '\0') {
        tty->ops->write(tty, *str++);
    }
}

void tty_puts_len(struct tty *tty, const char *str, size_t len) {
    while (len-- > 0) {
        tty->ops->write(tty, *str++);
    }
}

struct fmt_tty {
    struct fmt fmt;
    struct tty *tty;
};

static bool write_fmt(struct fmt *fmt, char ch) {
    struct tty *tty = ((struct fmt_tty *)fmt)->tty;
    tty->ops->write(tty, ch);
    return true;
}

static void tty_vprintf(struct tty *tty, const char *fmt, va_list args) {
    struct fmt_tty f = {
        .fmt = {
            .write_fn = write_fmt,
            .size = 0,
            .count = 0,
            .error = false
        },
        .tty = tty,
    };

    fmt_vsprintf(&f.fmt, fmt, args);
}

void tty_printf(struct tty *tty, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    tty_vprintf(tty, fmt, args);
    va_end(args);
}

static void write_0(struct tty *, char ch) {
    linkedlist_foreach(ptr, &g_tty_0.subtty_list) {
        struct tty *x = container_of(ptr, struct tty, link);
        x->ops->write(x, ch);
    }
}

static void set_color_0(struct tty *, int fg, int bg) {
    linkedlist_foreach(ptr, &g_tty_0.subtty_list) {
        struct tty *x = container_of(ptr, struct tty, link);
        x->ops->set_color(x, fg, bg);
    }
}

static struct tty_ops g_tty0_ops = {
    .write = write_0,
    .set_color = set_color_0,
};

void tty0_init(void) {
    linkedlist_init(&g_tty_0.subtty_list);
    tty_init(&g_tty_0.tty, &g_tty0_ops);
}

void tty0_register(struct tty* tty) {
    linkedlist_push_back(&g_tty_0.subtty_list, &tty->link);
}

void tty0_unregister(struct tty* tty) {
    linkedlist_remove(&tty->link);
}

void tty0_set_fgcolor(tty_color_t color) {
    set_color_0(&g_tty_0.tty, color, -1);
}

void tty0_set_bgcolor(tty_color_t color) {
    set_color_0(&g_tty_0.tty, -1, color);
}

void tty0_reset_color(void) {
    set_color_0(&g_tty_0.tty, -1, -1);
}
