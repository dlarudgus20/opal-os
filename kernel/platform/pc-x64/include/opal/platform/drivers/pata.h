#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_PATA_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_PATA_H

#include <stdint.h>

void pata_init(void);
void pata_on_timer(uint64_t now_tick);

#endif
