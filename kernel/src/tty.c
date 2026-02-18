#include <stdarg.h>

#include <kc/fmt.h>
#include <kc/assert.h>

#include <opal/tty.h>
#include <opal/drivers/uart.h>
#include <opal/platform/asm.h>

void tty0_init(void) {
}

void tty0_puts(const char *str) {
    uart_write(str);
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

#ifndef OPAL_TEST
noreturn void panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...) {
    disable_interrupts();

    tty0_printf("[%s:%s:%u] ", file, func, line);

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

    tty0_puts("\n");

    while (1) {
        wait_for_interrupt();
    }
}
#endif
