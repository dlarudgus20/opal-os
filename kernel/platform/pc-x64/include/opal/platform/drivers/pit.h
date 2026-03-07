#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_PIT_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_PIT_H

#include <opal/irq.h>

void pit_init(uint32_t hz, irq_handler_t isr);

#endif
