#ifndef OPAL_TIMER_H
#define OPAL_TIMER_H

#define TIMER_HZ 100

#include <stdint.h>

void timer_init(void);
[[nodiscard]] uint64_t timer_get_tick(void);

#endif
