#ifndef KC_STDLIB_H
#define KC_STDLIB_H

#include <stddef.h>
#include <stdint.h>

static inline uint32_t align_ceil_u32_p2(uint32_t x, uint32_t align) {
    const uint32_t mask = align - 1;
    return (x + mask) & ~mask;
}

static inline size_t align_ceil_sz_p2(size_t x, size_t align) {
    const size_t mask = align - 1;
    return (x + mask) & ~mask;
}

static inline size_t align_floor_sz_p2(size_t x, size_t align) {
    const size_t mask = align - 1;
    return x & ~mask;
}

void sort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#endif
