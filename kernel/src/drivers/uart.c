#include <opal/drivers/uart.h>
#include <opal/platform/drivers/uart.h>

void uart_init(void) {
    platform_uart_init();
}

void uart_write_char(char c) {
    if (c == '\n') {
        platform_uart_write_char('\r');
    }
    platform_uart_write_char(c);
}

void uart_write(const char *s) {
    while (*s != '\0') {
        uart_write_char(*s++);
    }
}

char uart_read_char(void) {
    return platform_uart_read_char();
}

void uart_read_line(char *buf, int buf_len, int mask_input) {
    int idx = 0;

    if (buf_len <= 0) {
        return;
    }

    while (1) {
        char c = uart_read_char();

        if (c == '\r' || c == '\n') {
            uart_write("\n");
            break;
        }

        if ((c == 0x08 || c == 0x7F) && idx > 0) {
            idx--;
            uart_write("\b \b");
            continue;
        }

        if (c >= 32 && c <= 126 && idx < (buf_len - 1)) {
            buf[idx++] = c;
            if (mask_input) {
                uart_write_char('*');
            } else {
                uart_write_char(c);
            }
        }
    }

    buf[idx] = '\0';
}
