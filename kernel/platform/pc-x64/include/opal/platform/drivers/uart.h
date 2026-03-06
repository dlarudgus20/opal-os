#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_UART_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_UART_H

#include <stddef.h>

#define INVALID_UART NULL

enum uart_port {
    UART_PORT_COM1 = 0,
    UART_PORT_COM2 = 1,
    UART_PORT_COUNT,
};

struct uart;
typedef struct uart* uart_handle_t;

void uart_early_init(void);
void uart_init(void);

uart_handle_t uart_get_default(void);
uart_handle_t uart_get(enum uart_port port);
bool uart_is_available(uart_handle_t uart);

size_t uart_rx_pending(uart_handle_t uart);
size_t uart_try_write(uart_handle_t uart, const char *buf, size_t len);
size_t uart_try_read(uart_handle_t uart, char *buf, size_t len);

#endif
