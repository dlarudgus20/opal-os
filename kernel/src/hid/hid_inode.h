#ifndef HID_HID_INODE_H
#define HID_HID_INODE_H

#include <opalsys/hid.h>

void hid_inode_init(void);
void hid_inode_onkey(const struct hid_char *input);

#endif
