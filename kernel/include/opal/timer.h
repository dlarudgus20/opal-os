#ifndef OPAL_TIMER_H
#define OPAL_TIMER_H

#include <stdint.h>

#define TIMER_HZ 100

void timer_init(void);
[[nodiscard]] uint64_t timer_get_tick(void);

[[nodiscard]] static inline uint64_t ticks_from_ms(uint32_t ms) {
    return (uint64_t)ms * TIMER_HZ / 1000;
}

#endif
