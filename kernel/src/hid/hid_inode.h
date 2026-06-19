#ifndef HID_HID_INODE_H
#define HID_HID_INODE_H

#include <opal/hid/keycode.h>

struct hid_char {
    bool raw;
    char ch;
    hid_keycode_t keycode;
};

void hid_inode_init(void);
void hid_inode_onkey(const struct hid_char *input);

#endif
