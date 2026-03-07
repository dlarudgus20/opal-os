#ifndef OPAL_PLATFORM_IRQ_DEVICE_H
#define OPAL_PLATFORM_IRQ_DEVICE_H

#include <stdint.h>

typedef uint8_t irq_t;
typedef void (*irq_handler_t)(void);

void irq_device_init(void);
void irq_register(irq_t irq, irq_handler_t handler);
void irq_enable(irq_t irq);
void irq_disable(irq_t irq);
void irq_enable_intr(void);

void irq_dispatch(irq_t irq);
void irq_send_eoi(irq_t irq);

#endif
