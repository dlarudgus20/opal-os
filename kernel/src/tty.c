#include <stdarg.h>

#include <kc/fmt.h>
#include <kc/assert.h>

#include <opal/tty.h>
#include <opal/drivers/uart.h>
#include <opal/platform/asm.h>

void tty0_init(void) {
}

void tty0_puts(const char *str) {
    while (*str != '\0') {
        uart_write_char(*str++);
    }
}

void tty0_puts_len(const char *str, size_t len) {
    while (len-- > 0) {
        uart_write_char(*str++);
    }
}

static bool write_uart(char ch) {
    uart_write_char(ch);
    return true;
}

void tty0_printf(const char *fmt, ...) {
    struct fmt f = {
        .write_fn = write_uart,
        .size = 0,
        .count = 0,
        .error = false
    };

    va_list args;
    va_start(args, fmt);
    fmt_vsprintf(&f, fmt, args);
    va_end(args);
}

void tty0_set_fgcolor(tty_color_t color) {
    assert(color < 0x10, "invalid tty color");
    if (color & ~7) {
        color = (color & 7) + 90;
    } else {
        color += 30;
    }
    tty0_printf("\x1b[%dm", color);
}

void tty0_set_bgcolor(tty_color_t color) {
    assert(color < 0x10, "invalid tty color");
    if (color & ~7) {
        color = (color & 7) + 100;
    } else {
        color += 40;
    }
    tty0_printf("\x1b[%dm", color);
}

void tty0_reset_color(void) {
    tty0_puts("\x1b[0m");
}

#ifndef OPAL_TEST
noreturn void panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...) {
    disable_interrupts();

    tty0_set_bgcolor(TTY_RED);
    tty0_set_fgcolor(TTY_BRIGHT_WHITE);
    tty0_printf("[%s:%u %s()] ", file, line, func);

    struct fmt f = {
        .write_fn = write_uart,
        .size = 0,
        .count = 0,
        .error = false
    };

    va_list args;
    va_start(args, line);
    fmt_vsprintf(&f, fmt, args);
    va_end(args);

    tty0_reset_color();
    tty0_puts("\n");

    while (1) {
        wait_for_interrupt();
    }
}
#endif
