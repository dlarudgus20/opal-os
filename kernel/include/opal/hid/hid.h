#ifndef OPAL_HID_HID_H
#define OPAL_HID_HID_H

#include <stdint.h>

#include <opalsys/hid.h>

enum hid_button {
    HID_BUTTON_LEFT = 1u << 0,
    HID_BUTTON_RIGHT = 1u << 1,
    HID_BUTTON_MIDDLE = 1u << 2,

    HID_BUTTON_MASK = HID_BUTTON_LEFT | HID_BUTTON_RIGHT | HID_BUTTON_MIDDLE,
};

void hid_init(void);
void hid_report_key(hid_keycode_t keycode, bool pressed);
void hid_report_pointer(int16_t dx, int16_t dy, uint8_t buttons);

#endif
