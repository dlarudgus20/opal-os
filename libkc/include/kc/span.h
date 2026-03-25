#ifndef KC_SPAN_H
#define KC_SPAN_H

#include <stddef.h>

struct span {
    void *ptr;
    size_t size;
};

#define SPAN(ptr_, size_) ((struct span){ .ptr = (ptr_), .size = (size_) })
#define SPAN_NULL SPAN(NULL, 0)

#endif
