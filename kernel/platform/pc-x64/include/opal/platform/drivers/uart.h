#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_UART_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_UART_H

#include <stdarg.h>

#include <kc/attributes.h>

void platform_uart_init(void);
void platform_uart_write_char(char ch);

PRINTF_ATTR(1, 2)
void platform_uart_printf(const char *format, ...);

PRINTF_ATTR(1, 0)
void platform_uart_vprintf(const char *format, va_list args);

#endif
