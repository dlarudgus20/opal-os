#include <stdint.h>

#include <opal/platform/drivers/uart.h>
#include <opal/platform/asm.h>
#include <kc/fmt.h>

#define UART_REG_DATA 0
#define UART_REG_IER 1
#define UART_REG_FCR 2
#define UART_REG_LCR 3
#define UART_REG_MCR 4
#define UART_REG_LSR 5
#define UART_REG_DLL 0
#define UART_REG_DLM 1

#define UART_COM1_BASE 0x3F8

void platform_uart_init(void) {
    out8(UART_COM1_BASE + UART_REG_IER, 0x00); /* Disable interrupts */
    out8(UART_COM1_BASE + UART_REG_LCR, 0x80); /* Enable DLAB */

    /* 115200 / 3 = 38400 baud */
    out8(UART_COM1_BASE + UART_REG_DLL, 0x03);
    out8(UART_COM1_BASE + UART_REG_DLM, 0x00);

    out8(UART_COM1_BASE + UART_REG_LCR, 0x03); /* 8 bits, no parity, 1 stop */
    out8(UART_COM1_BASE + UART_REG_FCR, 0xC7); /* Enable FIFO, clear queues */
    out8(UART_COM1_BASE + UART_REG_MCR, 0x0B); /* IRQs disabled, RTS/DSR set */
}

void platform_uart_write_char(char ch) {
    while (!(in8(UART_COM1_BASE + UART_REG_LSR) & 0x20)) {
    }
    out8(UART_COM1_BASE + UART_REG_DATA, (uint8_t)ch);
}

static bool uart_fmt_write(struct fmt *fmt, char ch) {
    (void)fmt;
    if (ch == '\n') {
        platform_uart_write_char('\r');
    }
    platform_uart_write_char(ch);
    return true;
}

void platform_uart_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    platform_uart_vprintf(format, args);
    va_end(args);
}

void platform_uart_vprintf(const char *format, va_list args) {
    struct fmt output = { .write_fn = uart_fmt_write };
    fmt_vsprintf(&output, format, args);
}
