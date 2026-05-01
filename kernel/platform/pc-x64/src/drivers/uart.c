#include <stdint.h>

#include <kc/kassert.h>

#include <collections/ringbuffer.h>

#include <opal/irq.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/asm.h>
#include <opal/platform/drivers/uart.h>
#include <opal/tty/uart_tty.h>

#define UART_REG_DATA 0
#define UART_REG_IER 1
#define UART_REG_IIR 2
#define UART_REG_FCR 2
#define UART_REG_LCR 3
#define UART_REG_MCR 4
#define UART_REG_LSR 5
#define UART_REG_MSR 6
#define UART_REG_SCR 7

#define UART_REG_DLL 0
#define UART_REG_DLM 1

#define UART_COM1_BASE 0x3f8
#define UART_COM2_BASE 0x2f8

#define UART_IRQ_COM2 3
#define UART_IRQ_COM1 4

#define UART_IER_RX             0x01
#define UART_IER_TX_EMPTY       0x02
#define UART_IER_LINE_STATUS    0x04

#define UART_FCR_ENABLE_FIFO    0x01
#define UART_FCR_CLEAR_RX_FIFO  0x02
#define UART_FCR_CLEAR_TX_FIFO  0x04
#define UART_FCR_TRIG_RX_14     0xc0
#define UART_FCR (UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RX_FIFO | UART_FCR_CLEAR_TX_FIFO | UART_FCR_TRIG_RX_14)

#define UART_IIR_NO_PENDING     0x01
#define UART_IIR_TX_EMPTY       0x02
#define UART_IIR_RX_AVAILABLE   0x04
#define UART_IIR_LINE_STATUS    0x06
#define UART_IIR_RX_TIMEOUT     0x0c
#define UART_IIR_INT_MASK       0x0e

#define UART_LCR_WORD_8         0x03
#define UART_LCR_DLAB           0x80
#define UART_LCR UART_LCR_WORD_8

#define UART_LSR_DATA_READY     0x01
#define UART_LSR_TX_EMPTY       0x20

#define UART_MCR_DTR            0x01
#define UART_MCR_RTS            0x02
#define UART_MCR_OUT2           0x08
#define UART_MCR_LOOP           0x10
#define UART_MCR (UART_MCR_OUT2 | UART_MCR_RTS | UART_MCR_DTR)

#define UART_RX_QUEUE_SIZE      512
#define UART_TX_QUEUE_SIZE      512

#define UART_INTR_SPIN 64

struct uart {
    uint16_t base;
    uint8_t irq_line;
    bool present;
    bool irq_mode;
    bool tx_suppressed;

    uint8_t rx_buf[UART_RX_QUEUE_SIZE];
    uint8_t tx_buf[UART_TX_QUEUE_SIZE];
    struct ringbuffer rx_queue;
    struct ringbuffer tx_queue;
};

static struct uart g_uarts[UART_PORT_COUNT] = {
    [UART_PORT_COM1] = { .base = UART_COM1_BASE, .irq_line = UART_IRQ_COM1 },
    [UART_PORT_COM2] = { .base = UART_COM2_BASE, .irq_line = UART_IRQ_COM2 },
};

static uart_handle_t g_default_uart = UART_INVALID;

static uint8_t reg_read(const struct uart *u, uint8_t reg) {
    return in8(u->base + reg);
}

static void reg_write(const struct uart *u, uint8_t reg, uint8_t value) {
    out8(u->base + reg, value);
}

static void configure_baud_38400(const struct uart *u) {
    reg_write(u, UART_REG_LCR, UART_LCR_DLAB);
    reg_write(u, UART_REG_DLL, 0x03);
    reg_write(u, UART_REG_DLM, 0x00);
    reg_write(u, UART_REG_LCR, UART_LCR);
}

[[nodiscard]] static bool probe_port(const struct uart *u) {
    // test scratch register
    reg_write(u, UART_REG_SCR, 0x5a);
    if (reg_read(u, UART_REG_SCR) != 0x5a) {
        return false;
    }
    reg_write(u, UART_REG_SCR, 0xa5);
    if (reg_read(u, UART_REG_SCR) != 0xa5) {
        return false;
    }

    // test loopback
    reg_write(u, UART_REG_IER, 0x00);
    reg_write(u, UART_REG_LCR, UART_LCR);
    reg_write(u, UART_REG_MCR, UART_MCR_LOOP | UART_MCR);
    reg_write(u, UART_REG_DATA, 0x5a);
    return reg_read(u, UART_REG_DATA) == 0x5a;
}

[[nodiscard]] static bool hw_early_init(struct uart *u) {
    u->present = probe_port(u);
    if (!u->present) {
        return false;
    }

    reg_write(u, UART_REG_IER, 0x00);
    configure_baud_38400(u);
    reg_write(u, UART_REG_FCR, UART_FCR);
    reg_write(u, UART_REG_MCR, UART_MCR);

    (void)reg_read(u, UART_REG_LSR);
    (void)reg_read(u, UART_REG_MSR);
    (void)reg_read(u, UART_REG_DATA);
    return true;
}

static void hw_init(struct uart *u) {
    if (!u->present && !hw_early_init(u)) {
        return;
    }

    reg_write(u, UART_REG_IER, 0x00);
    configure_baud_38400(u);
    reg_write(u, UART_REG_FCR, UART_FCR);
    reg_write(u, UART_REG_MCR, UART_MCR);
    reg_write(u, UART_REG_IER, UART_IER_RX | UART_IER_LINE_STATUS);
}

static void enable_tx_irq(struct uart *u, bool enabled) {
    if (!u->present) {
        return;
    }

    uint8_t ier = reg_read(u, UART_REG_IER);
    if (enabled) {
        ier |= UART_IER_TX_EMPTY;
    } else {
        ier &= (uint8_t)~UART_IER_TX_EMPTY;
    }
    reg_write(u, UART_REG_IER, ier);
}

[[nodiscard]] static bool can_tx(const struct uart *u) {
    return u->present && (reg_read(u, UART_REG_LSR) & UART_LSR_TX_EMPTY) != 0;
}

[[nodiscard]] static bool has_data(const struct uart *u) {
    return u->present && (reg_read(u, UART_REG_LSR) & UART_LSR_DATA_READY) != 0;
}

[[nodiscard]] static bool hw_try_read(struct uart *u, uint8_t *out) {
    if (!has_data(u)) {
        return false;
    }
    *out = reg_read(u, UART_REG_DATA);
    return true;
}

[[nodiscard]] static bool hw_try_write(struct uart *u, uint8_t c) {
    if (!can_tx(u)) {
        return false;
    }
    reg_write(u, UART_REG_DATA, c);
    return true;
}

static void tx_pump_locked(struct uart *u) {
    if (u->tx_suppressed) {
        enable_tx_irq(u, false);
        return;
    }

    while (!ringbuffer_is_empty(&u->tx_queue) && can_tx(u)) {
        uint8_t c = ringbuffer_pop(&u->tx_queue, uint8_t);
        reg_write(u, UART_REG_DATA, c);
    }

    enable_tx_irq(u, !ringbuffer_is_empty(&u->tx_queue));
}

[[nodiscard]] static bool tx_push_locked(struct uart *u, uint8_t c) {
    if (!ringbuffer_is_full(&u->tx_queue)) {
        ringbuffer_push(&u->tx_queue, uint8_t, c);
        return true;
    }
    return false;
}

static void uart_isr(struct uart *uart) {
    bool rx_ready = false;

    for (int i = 0; i < UART_INTR_SPIN; i++) {
        if (!uart->present) {
            break;
        }

        uint8_t iir = reg_read(uart, UART_REG_IIR);
        if ((iir & UART_IIR_NO_PENDING) != 0) {
            break;
        }

        switch (iir & UART_IIR_INT_MASK) {
            case UART_IIR_RX_AVAILABLE:
            case UART_IIR_RX_TIMEOUT: {
                uint8_t data = 0;
                while (hw_try_read(uart, &data)) {
                    if (!ringbuffer_is_full(&uart->rx_queue)) {
                        ringbuffer_push(&uart->rx_queue, uint8_t, data);
                        rx_ready = true;
                    }
                }
                break;
            }
            case UART_IIR_TX_EMPTY:
                tx_pump_locked(uart);
                break;
            case UART_IIR_LINE_STATUS:
                (void)reg_read(uart, UART_REG_LSR);
                break;
            default:
                break;
        }
    }

    if (rx_ready) {
        uint8_t port = (uint8_t)(uart - g_uarts);
        irqmsg_push((struct irqmsg){ .type = IRQMSG_UART_RX, .data = port });
    }

    irq_send_eoi(uart->irq_line);
}

static void uart_isr_com1(void) {
    uart_isr(&g_uarts[UART_PORT_COM1]);
}

static void uart_isr_com2(void) {
    uart_isr(&g_uarts[UART_PORT_COM2]);
}

void uart_early_init(void) {
    for (int i = 0; i < UART_PORT_COUNT; i++) {
        struct uart *u = &g_uarts[i];
        u->irq_mode = false;
        u->tx_suppressed = false;
        (void)hw_early_init(u);
    }

    if (g_uarts[UART_PORT_COM1].present) {
        g_default_uart = &g_uarts[UART_PORT_COM1];
    } else if (g_uarts[UART_PORT_COM2].present) {
        g_default_uart = &g_uarts[UART_PORT_COM2];
    } else {
        g_default_uart = UART_INVALID;
    }

    if (g_default_uart) {
        uart_tty_init();
    }
}

void uart_init(void) {
    for (int i = 0; i < UART_PORT_COUNT; i++) {
        struct uart *u = &g_uarts[i];
        if (!u->present) {
            continue;
        }

        ringbuffer_init(&u->rx_queue, u->rx_buf, UART_RX_QUEUE_SIZE);
        ringbuffer_init(&u->tx_queue, u->tx_buf, UART_TX_QUEUE_SIZE);

        hw_init(u);
        if (i == UART_PORT_COM1) {
            irq_register(u->irq_line, uart_isr_com1);
        } else {
            irq_register(u->irq_line, uart_isr_com2);
        }
        irq_enable(u->irq_line);
        u->irq_mode = true;
        u->tx_suppressed = false;
    }

    if (g_default_uart) {
        uart_tty_enable_irqmsg();
    }
}

void uart_enter_panic_mode(void) {
    irqlock_t lock = irqlock_acquire();

    for (int i = 0; i < UART_PORT_COUNT; i++) {
        struct uart *u = &g_uarts[i];
        if (!u->present) {
            continue;
        }

        reg_write(u, UART_REG_IER, 0x00);
        u->irq_mode = false;
        u->tx_suppressed = false;
    }

    irqlock_release(&lock);
}

uart_handle_t uart_get_default(void) {
    return g_default_uart;
}

uart_handle_t uart_get(enum uart_port port) {
    if ((unsigned)port >= UART_PORT_COUNT) {
        return UART_INVALID;
    }

    uart_handle_t uart = &g_uarts[port];
    return uart->present ? uart : UART_INVALID;
}

bool uart_is_available(uart_handle_t uart) {
    return uart && uart->present;
}

size_t uart_rx_pending(uart_handle_t uart) {
    if (!uart_is_available(uart)) {
        return 0;
    }

    if (!uart->irq_mode) {
        return has_data(uart) ? 1 : 0;
    }

    irqlock_t lock = irqlock_acquire();
    size_t count = uart->rx_queue.count;
    irqlock_release(&lock);
    return count;
}

void uart_suppress_tx(uart_handle_t uart, bool suppress) {
    if (!uart_is_available(uart) || !uart->irq_mode) {
        return;
    }

    irqlock_t lock = irqlock_acquire();
    uart->tx_suppressed = suppress;
    if (suppress) {
        enable_tx_irq(uart, false);
    } else if (!ringbuffer_is_empty(&uart->tx_queue)) {
        enable_tx_irq(uart, true);
    }
    irqlock_release(&lock);
}

[[nodiscard]] static size_t early_try_write(uart_handle_t uart, const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        while (!hw_try_write(uart, (uint8_t)buf[i])) {}
    }
    return len;
}

size_t uart_try_write(uart_handle_t uart, const char *buf, size_t len) {
    kassert(buf);

    if (!uart_is_available(uart)) {
        return 0;
    }
    if (!uart->irq_mode) {
        return early_try_write(uart, buf, len);
    }

    irqlock_t lock = irqlock_acquire();

    size_t i = 0;
    while (i < len) {
        if (!tx_push_locked(uart, (uint8_t)buf[i])) {
            break;
        }
        i++;
    }

    if (!uart->tx_suppressed && !ringbuffer_is_empty(&uart->tx_queue)) {
        enable_tx_irq(uart, true);
    }

    irqlock_release(&lock);
    return i;
}

[[nodiscard]] static size_t early_try_read(uart_handle_t uart, char *buf, size_t len) {
    size_t i = 0;
    for (; i < len; i++) {
        uint8_t c = 0;
        if (!hw_try_read(uart, &c)) {
            break;
        }
        buf[i] = (char)c;
    }
    return i;
}

size_t uart_try_read(uart_handle_t uart, char *buf, size_t len) {
    kassert(buf);

    if (!uart_is_available(uart)) {
        return 0;
    }
    if (!uart->irq_mode) {
        return early_try_read(uart, buf, len);
    }

    irqlock_t lock = irqlock_acquire();

    size_t i = 0;
    while (i < len && !ringbuffer_is_empty(&uart->rx_queue)) {
        uint8_t c = ringbuffer_pop(&uart->rx_queue, uint8_t);
        buf[i++] = (char)c;
    }

    irqlock_release(&lock);
    return i;
}
