#ifndef OPAL_DRIVERS_UART_H
#define OPAL_DRIVERS_UART_H

void uart_init(void);
void uart_write_char(char ch);
void uart_set_color(int fg, int bg);
char uart_read_char(void);
void uart_read_line(char *buf, int buf_len);

#endif
