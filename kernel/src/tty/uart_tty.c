#include <stddef.h>
#include <stdbool.h>

#include <opal/tty.h>
#include <opal/tty/uart_tty.h>
#include <opal/platform/drivers/uart.h>

static struct tty_buffered g_tty;
static bool g_registered;
static uart_handle_t g_uart;

static size_t write_char_tty(struct tty *, const char *buf, size_t len) {
    if (!uart_is_available(g_uart)) {
        return 0;
    }

    return uart_try_write(g_uart, buf, len);
}

static size_t write_fg(char *dst, int color) {
    if (color & 8) {
        dst[0] = '9';
    } else {
        dst[0] = '3';
    }
    dst[1] = (char)(((unsigned)color & 7u) + '0');
    return 2;
}

static size_t write_bg(char *dst, int color) {
    if (color & 8) {
        dst[0] = '1';
        dst[1] = '0';
        dst[2] = (char)(((unsigned)color & 7u) + '0');
        return 3;
    } else {
        dst[0] = '4';
        dst[1] = (char)(((unsigned)color & 7u) + '0');
        return 2;
    }
}

static void set_color_tty(struct tty *, int fg, int bg) {
    if (!uart_is_available(g_uart)) {
        return;
    }

    char seq[16];
    size_t len = 0;
    seq[len++] = '\x1b';
    seq[len++] = '[';

    if (fg == -1 && bg == -1) {
        seq[len++] = '0';
    } else if (fg != -1 && bg != -1) {
        len += write_fg(seq + len, fg);
        seq[len++] = ';';
        len += write_bg(seq + len, bg);
    } else if (fg != -1) {
        len += write_fg(seq + len, fg);
    } else {
        len += write_bg(seq + len, bg);
    }
    seq[len++] = 'm';

    tty_puts_len(&g_tty.tty, seq, len);
    tty_flush(&g_tty.tty);
}

static struct tty_ops g_tty_ops = {
    .write = write_char_tty,
    .set_color = set_color_tty,
};

void uart_tty_init(void) {
    g_uart = uart_get_default();
    tty_buffered_init(&g_tty, &g_tty_ops);

    if (g_uart && !g_registered) {
        tty0_register(&g_tty.tty);
        g_registered = true;
    }
}
