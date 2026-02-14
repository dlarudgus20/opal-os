#ifndef KC_STDLIB_H
#define KC_STDLIB_H

#include <stddef.h>

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#endif
