#include <kc/assert.h>
#include <kc/string.h>

#include <opal/locks/irqlock.h>
#include <opal/platform/irq_device.h>
#include <opal/platform/drivers/pic.h>

#define IRQ_COUNT PIC_IRQ_COUNT

static irq_handler_t g_irq_handlers[IRQ_COUNT];

void irq_device_init(void) {
    memset(g_irq_handlers, 0, sizeof(g_irq_handlers));
    pic_init();
}

void irq_register(irq_t irq, irq_handler_t handler) {
    assert(irq < IRQ_COUNT);
    irqlock_t irqlock = irqlock_acquire();
    g_irq_handlers[irq] = handler;
    irqlock_release(&irqlock);
}

void irq_enable(irq_t irq) {
    assert(irq < IRQ_COUNT);
    irqlock_t irqlock = irqlock_acquire();

    if (irq >= 8) {
        pic_enable_irq(PIC_IRQ_SLAVE);
    }
    pic_enable_irq(irq);

    irqlock_release(&irqlock);
}

void irq_disable(irq_t irq) {
    assert(irq < IRQ_COUNT);
    irqlock_t irqlock = irqlock_acquire();
    pic_disable_irq(irq);
    irqlock_release(&irqlock);
}

void irq_enable_intr(void) {
    pic_enable_intr();
}

void irq_dispatch(irq_t irq) {
    if (irq >= IRQ_COUNT) {
        return;
    }

    irq_handler_t handler = g_irq_handlers[irq];
    if (handler) {
        handler();
    }

    pic_send_eoi(irq);
}
