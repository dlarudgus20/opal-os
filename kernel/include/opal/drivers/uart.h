#ifndef OPAL_DRIVERS_UART_H
#define OPAL_DRIVERS_UART_H

void uart_init(void);
void uart_write_char(char c);
void uart_write(const char *s);
char uart_read_char(void);
void uart_read_line(char *buf, int buf_len, int mask_input);

#endif
