#include <stdint.h>

#include <opal/platform/drivers/uart.h>
#include <opal/platform/asm.h>

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
    outb(UART_COM1_BASE + UART_REG_IER, 0x00); /* Disable interrupts */
    outb(UART_COM1_BASE + UART_REG_LCR, 0x80); /* Enable DLAB */

    /* 115200 / 3 = 38400 baud */
    outb(UART_COM1_BASE + UART_REG_DLL, 0x03);
    outb(UART_COM1_BASE + UART_REG_DLM, 0x00);

    outb(UART_COM1_BASE + UART_REG_LCR, 0x03); /* 8 bits, no parity, 1 stop */
    outb(UART_COM1_BASE + UART_REG_FCR, 0xC7); /* Enable FIFO, clear queues */
    outb(UART_COM1_BASE + UART_REG_MCR, 0x0B); /* IRQs disabled, RTS/DSR set */
}

int platform_uart_can_tx(void) {
    return (inb(UART_COM1_BASE + UART_REG_LSR) & 0x20) != 0;
}

int platform_uart_has_data(void) {
    return (inb(UART_COM1_BASE + UART_REG_LSR) & 0x01) != 0;
}

void platform_uart_write_char(char c) {
    while (!platform_uart_can_tx()) {
    }
    outb(UART_COM1_BASE + UART_REG_DATA, (uint8_t)c);
}

char platform_uart_read_char(void) {
    while (!platform_uart_has_data()) {
    }
    return (char)inb(UART_COM1_BASE + UART_REG_DATA);
}
