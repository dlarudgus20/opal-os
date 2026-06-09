#include <stdarg.h>

#include <kc/stdio.h>
#include <kc/stdlib.h>
#include <kc/string.h>
#include <kc/kassert.h>

#include <opal/tty.h>
#include <opal/platform/asm.h>

#ifndef OPAL_TEST

enum { PANICK_NONE, PANICK_FORMAT, PANICK_FORMAT_BASIC, PANICK_PUTS, PANICK_PUTS_FORMAT };

static void format_panic(char *buffer, size_t bufsz, const char *fmt, const char *file,
    const char *func, unsigned line, va_list args) {
    int prefix = ksnprintf(buffer, bufsz, "[%s:%u %s()] ", file, line, func);
    if (0 <= prefix && (size_t)prefix < bufsz) {
        kvsnprintf(buffer + prefix, bufsz - prefix, fmt, args);
    }
}

static void format_panic_basic(char *buffer, size_t bufsz, const char *fmt, const char *file,
    const char *func, unsigned line, const char *append) {
    ksnprintf(buffer, bufsz, "[%s:%u %s()] %s%s", file, line, func, fmt, append);
}

static void print_panic(struct tty *tty, const char *msg) {
    if (tty->ops->set_panic_mode) {
        tty->ops->set_panic_mode(tty);
    }
    tty->ops->set_color(tty, TTY_BRIGHT_WHITE, TTY_RED);
    tty_puts(tty, msg);
    tty->ops->set_color(tty, -1, -1);
    tty_puts(tty, "\n");
}

[[noreturn]] void _panic_format(
    const char *fmt, const char *file, const char *func, unsigned line, ...) {
    static char buffer[4096];
    static size_t prev_len;

    static int stage = PANICK_NONE;

    interrupts_disable();

    va_list args;
    va_start(args, line);

    // detect reentrant panic

    if (stage == PANICK_NONE) {
        stage = PANICK_FORMAT;
        format_panic(buffer, sizeof(buffer), fmt, file, func, line, args);

    } else if (stage == PANICK_FORMAT) {
        // format_panic cannot be trusted
        stage = PANICK_FORMAT_BASIC;
        format_panic_basic(buffer, sizeof(buffer), fmt, file, func, line,
            "\n+ [panick_format] formatting panicked");

    } else if (stage == PANICK_FORMAT_BASIC) {
        // arguments cannot be trusted
        ksnprintf(buffer, sizeof(buffer),
            "[panick_format] ksnprintf panicked fmt=%p file=%p func=%p line=%u", fmt, file, func,
            line);

    } else if (stage == PANICK_PUTS) {
        // multiple panics; try to append message
        stage = PANICK_PUTS_FORMAT;
        buffer[sizeof(buffer) - 1] = '\0';
        size_t len = strlen(buffer);
        prev_len = len;

        // append if more than 10 bytes + '\0' are left.
        if (len + 11 < sizeof(buffer)) {
            buffer[len] = '\n';
            buffer[len + 1] = '+';
            buffer[len + 2] = ' ';
            format_panic(buffer + len + 3, sizeof(buffer) - len - 3, fmt, file, func, line, args);
        }

    } else {
        // PANICK_PUTS_FORMAT: rollback
        buffer[prev_len] = '\0';
        strscat(buffer + prev_len, sizeof(buffer) - prev_len,
            "\n+ [panick_format] formatting panicked");
    }

    // print message to tty0

    stage = PANICK_PUTS;
    struct tty_0 *tty_0 = (struct tty_0 *)tty0_get();
    struct linkedlist_link *ptr = linkedlist_head(&tty_0->subtty_list);

    while (!linkedlist_is_nil(&tty_0->subtty_list, ptr)) {
        struct linkedlist_link *next = ptr->next;
        linkedlist_remove(ptr);

        // if reentrant panic occur, this tty stays removed from tty0
        struct tty *tty = container_of(ptr, struct tty, link);
        print_panic(tty, buffer);

        linkedlist_insert_before(next, ptr);
        ptr = next;
    }

    va_end(args);

    while (1) {
        wait_for_interrupt();
    }
}

#endif
