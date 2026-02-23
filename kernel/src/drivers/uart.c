#include <stdint.h>

#include <opal/tty.h>
#include <opal/drivers/uart.h>
#include <opal/platform/drivers/uart.h>

static void write_char(struct tty *, char ch) {
    uart_write_char(ch);
}

static void set_color(struct tty *, int fg, int bg) {
    uart_set_color(fg, bg);
}

static struct tty_ops g_tty_ops = {
    .write = write_char,
    .set_color = set_color,
};

static struct tty g_tty;

void uart_init(void) {
    platform_uart_init();
    tty_init(&g_tty, &g_tty_ops);
    tty0_register(&g_tty);
}

void uart_write_char(char ch) {
    if (ch == '\n') {
        platform_uart_write_char('\r');
    }
    platform_uart_write_char(ch);
}

static void write_fg(tty_color_t color) {
    if (color & 8) {
        uart_write_char('9');
    } else {
        uart_write_char('3');
    }
    uart_write_char((color & 7) + '0');
}

static void write_bg(tty_color_t color) {
    if (color & 8) {
        uart_write_char('1');
        uart_write_char('0');
    } else {
        uart_write_char('4');
    }
    uart_write_char((color & 7) + '0');
}

void uart_set_color(int fg, int bg) {
    uart_write_char('\x1b');
    uart_write_char('[');
    if (fg == -1 && bg == -1) {
        uart_write_char('0');
    } else if (fg != -1 && bg != -1) {
        write_fg((tty_color_t)fg);
        uart_write_char(';');
        write_bg((tty_color_t)bg);
    } else if (fg != -1) {
        write_fg((tty_color_t)fg);
    } else {
        write_bg((tty_color_t)bg);
    }
    uart_write_char('m');
}

char uart_read_char(void) {
    return platform_uart_read_char();
}

void uart_read_line(char *buf, int buf_len) {
    int idx = 0;

    if (buf_len <= 0) {
        return;
    }

    while (1) {
        char c = uart_read_char();

        if (c == '\r' || c == '\n') {
            uart_write_char('\n');
            break;
        }

        if ((c == 0x08 || c == 0x7F) && idx > 0) {
            idx--;
            uart_write_char('\b');
            uart_write_char(' ');
            uart_write_char('\b');
            continue;
        }

        if (c >= 32 && c <= 126 && idx < (buf_len - 1)) {
            buf[idx++] = c;
            uart_write_char(c);
        }
    }

    buf[idx] = '\0';
}
