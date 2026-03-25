#ifndef KC_STDLIB_H
#define KC_STDLIB_H

#include <stddef.h>
#include <stdint.h>

#include <kc/errno.h>

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

static inline bool ispower2(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

void sort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));

kerrno_t kstrtoul(const char *str, int base, char **endptr, unsigned long *result);
kerrno_t kstrtoul_exact(const char *str, int base, unsigned long max, unsigned long *result);

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#endif
