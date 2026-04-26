#ifndef OPAL_PLATFORM_DESCRIPTORS_H
#define OPAL_PLATFORM_DESCRIPTORS_H

#include <stdint.h>

#define KERNEL_CODE_SEGMENT 0x08
#define KERNEL_DATA_SEGMENT 0x10
#define USER_CODE_SEGMENT 0x1b
#define USER_DATA_SEGMENT 0x23

struct context;

void descriptors_init(void);
void descriptors_set_kstack(uintptr_t kstack_top);

#endif
