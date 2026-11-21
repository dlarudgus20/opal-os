#include <opal/platform/asm.h>
#include <opal/platform/drivers/uart.h>
#include <opal/test.h>
#include <kc/kassert.h>

[[noreturn]] void _panic_format(
    const char *fmt, const char *file, const char *func, unsigned line, ...) {
    static bool panicking;
    interrupts_disable();

    test_mark_panicked();

    if (!panicking) {
        panicking = true;
        platform_uart_printf(" [%s:%u %s()] ", file, line, func);

        va_list args;
        va_start(args, line);
        platform_uart_vprintf(fmt, args);
        va_end(args);
        platform_uart_write_char('\r');
        platform_uart_write_char('\n');
    }

    test_abort();

    for (;;) {
        wait_for_interrupt();
    }
}
