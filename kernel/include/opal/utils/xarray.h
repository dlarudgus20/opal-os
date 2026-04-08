#ifndef OPAL_UTILS_XARRAY_H
#define OPAL_UTILS_XARRAY_H

#include <stdint.h>

#define XARRAY_SLOT_BITS 6
#define XARRAY_SLOT_SIZE (1 << XARRAY_SLOT_BITS)

struct xa_node {
    uint64_t slots[XARRAY_SLOT_SIZE];
};

struct xarray {
    struct xa_node root;
    uintptr_t stride;
    unsigned depth;
};

void xarray_init(struct xarray *xa, uintptr_t stride);
void xarray_destroy(struct xarray *xa);
void *xarray_get(const struct xarray *xa, uint64_t index);
bool xarray_set(struct xarray *xa, uint64_t index, void *value);
bool xarray_set_range(struct xarray *xa, uint64_t index, uint64_t len, void *start);

#endif
