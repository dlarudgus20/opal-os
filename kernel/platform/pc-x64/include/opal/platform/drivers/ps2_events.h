#ifndef OPAL_DRIVERS_PS2_EVENTS_H
#define OPAL_DRIVERS_PS2_EVENTS_H

#include <opal/irq.h>

struct ps2_kbd_event {
    irqmsg_type_t type;
    uint8_t scancode;
    bool released;
    bool extended;
};

struct ps2_mouse_event {
    irqmsg_type_t type;
    uint8_t buttons;
    int16_t dx;
    int16_t dy;
    bool overflow_x;
    bool overflow_y;
};

#endif
