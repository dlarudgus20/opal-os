#include <stdarg.h>

#include <kc/fmt.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/tty.h>

static size_t write_0(struct tty *, const char *buf, size_t len);
static void set_color_0(struct tty *, int fg, int bg);
static void tty0_flush(void);

static struct tty_ops g_tty0_ops = {
    .write = write_0,
    .set_color = set_color_0,
};

static struct tty_0 g_tty_0;

void tty_init(struct tty *tty, const struct tty_ops *ops) {
    tty->ops = ops;
    tty->buffered = false;
}

void tty_buffered_init(struct tty_buffered *tty, const struct tty_ops *ops) {
    tty_init(&tty->tty, ops);
    tty->tty.buffered = true;
    tty->buflen = 0;
}

void tty_flush(struct tty *tty) {
    if (tty == &g_tty_0.tty) {
        tty0_flush();
        return;
    }

    if (!tty->buffered) {
        return;
    }

    struct tty_buffered *buffered = (struct tty_buffered *)tty;

    if (buffered->buflen == 0) {
        return;
    }

    size_t written = buffered->ops->write(tty, buffered->buffer, buffered->buflen);
    if (written >= buffered->buflen) {
        buffered->buflen = 0;
        return;
    }

    buffered->buflen -= (uint16_t)written;
    memmove(buffered->buffer, buffered->buffer + written, buffered->buflen);
}

static void tty_write(struct tty *tty, char ch) {
    if (!tty->buffered) {
        tty->ops->write(tty, &ch, 1);
        return;
    }

    struct tty_buffered *buffered = (struct tty_buffered *)tty;

    if (buffered->buflen >= TTY_BUFFER_SIZE) {
        tty_flush(tty);
    }

    if (buffered->buflen >= TTY_BUFFER_SIZE) {
        // drop
        return;
    }

    buffered->buffer[buffered->buflen++] = ch;
}

void tty_puts(struct tty *tty, const char *str) {
    bool flush = false;

    while (*str != '\0') {
        flush |= (*str == '\n');
        tty_write(tty, *str++);
    }

    if (flush) {
        tty_flush(tty);
    }
}

void tty_puts_len(struct tty *tty, const char *str, size_t len) {
    bool flush = false;

    while (len-- > 0) {
        flush |= (*str == '\n');
        tty_write(tty, *str++);
    }

    if (flush) {
        tty_flush(tty);
    }
}

struct fmt_tty {
    struct fmt fmt;
    struct tty *tty;
    bool flush;
};

static bool write_fmt(struct fmt *fmt, char ch) {
    struct fmt_tty *f = (struct fmt_tty *)fmt;
    struct tty *tty = f->tty;
    f->flush |= (ch == '\n');
    tty_write(tty, ch);
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
        .flush = false,
    };

    fmt_vsprintf(&f.fmt, fmt, args);

    if (f.flush) {
        tty_flush(tty);
    }
}

void tty_printf(struct tty *tty, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    tty_vprintf(tty, fmt, args);
    va_end(args);
}

void tty0_init(void) {
    linkedlist_init(&g_tty_0.subtty_list);
    tty_init(&g_tty_0.tty, &g_tty0_ops);
}

struct tty *tty0_get(void) {
    return &g_tty_0.tty;
}

static size_t write_0(struct tty *, const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        linkedlist_foreach(ptr, &g_tty_0.subtty_list) {
            struct tty *x = container_of(ptr, struct tty, link);
            tty_write(x, buf[i]);
        }
    }
    return len;
}

static void set_color_0(struct tty *, int fg, int bg) {
    linkedlist_foreach(ptr, &g_tty_0.subtty_list) {
        struct tty *x = container_of(ptr, struct tty, link);

        tty_flush(x);
        x->ops->set_color(x, fg, bg);
    }
}

static void tty0_flush(void) {
    linkedlist_foreach(ptr, &g_tty_0.subtty_list) {
        struct tty *x = container_of(ptr, struct tty, link);
        tty_flush(x);
    }
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
