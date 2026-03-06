#ifndef OPAL_PLATFORM_DRIVERS_PS2_H
#define OPAL_PLATFORM_DRIVERS_PS2_H

#include <stdbool.h>
#include <stdint.h>

void ps2_init(void);
bool ps2_is_enabled(void);

void ps2_keyboard_feed_raw(uint8_t data);
void ps2_mouse_feed_raw(uint8_t data);

#endif
