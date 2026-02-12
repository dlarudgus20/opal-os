#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_UART_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_UART_H

#include <stdint.h>

void platform_uart_init(void);
int platform_uart_can_tx(void);
int platform_uart_has_data(void);
void platform_uart_write_char(char c);
char platform_uart_read_char(void);

#endif
