#ifndef OPAL_LOCKS_IRQLOCK_H
#define OPAL_LOCKS_IRQLOCK_H

#include <stdbool.h>

struct irqlock {
    bool flag;
};

typedef struct irqlock irqlock_t;

irqlock_t irqlock_acquire(void);
void irqlock_release(irqlock_t *lock);

#endif
