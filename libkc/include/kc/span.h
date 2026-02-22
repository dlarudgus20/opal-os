#ifndef KC_SPAN_H
#define KC_SPAN_H

#include <stddef.h>

struct span {
    void *ptr;
    size_t size;
};

#endif
