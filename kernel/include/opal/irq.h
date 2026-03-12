#ifndef OPAL_IRQ_H
#define OPAL_IRQ_H

#include <stdint.h>

#include <opal/platform/irq_device.h>

enum irqmsg_type {
    IRQMSG_DROP,
    IRQMSG_PS2_KBD,
    IRQMSG_PS2_MOUSE,
    IRQMSG_UART_RX,
    IRQMSG_SCHED_TIMEOUT,
    IRQMSG_COUNT,
};

typedef uint8_t irqmsg_type_t;

struct irqmsg {
    irqmsg_type_t type;
    uint8_t data;
};

typedef void (*irqmsg_handler_t)(struct irqmsg msg);

void irq_init(void);
void irqmsg_register(irqmsg_type_t msg, irqmsg_handler_t handler);
bool irqmsg_push(struct irqmsg msg);
[[noreturn]] void irqmsg_drain_loop(void);

#endif
