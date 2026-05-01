#ifndef OPAL_LOCKS_IRQLOCK_H
#define OPAL_LOCKS_IRQLOCK_H

struct irqlock {
    bool flag;
};

typedef struct irqlock irqlock_t;

#ifndef OPAL_TEST

[[nodiscard]] irqlock_t irqlock_acquire(void);
void irqlock_release(irqlock_t *lock);

#else

#include <kc/kassert.h>

[[nodiscard]] static inline irqlock_t irqlock_acquire(void) {
    return (struct irqlock){ .flag = true };
}

static inline void irqlock_release(irqlock_t *lock) {
    kassert(lock->flag);
    lock->flag = false;
}

#endif

#endif
