#include <opal/locks/irqlock.h>
#include <opal/platform/asm.h>

#ifndef OPAL_TEST

irqlock_t irqlock_acquire(void) {
    bool flag = interrupt_is_enabled();
    interrupts_disable();
    return (struct irqlock){ .flag = flag };
}

void irqlock_release(irqlock_t *lock) {
    if (lock->flag) {
        interrupts_enable();
    }
}

#endif
