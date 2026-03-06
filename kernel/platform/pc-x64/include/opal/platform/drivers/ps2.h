#ifndef OPAL_PLATFORM_DRIVERS_PS2_H
#define OPAL_PLATFORM_DRIVERS_PS2_H

#include <stdbool.h>

#include <opal/platform/drivers/ps2_events.h>

void ps2_init(void);
bool ps2_is_enabled(void);

#endif
