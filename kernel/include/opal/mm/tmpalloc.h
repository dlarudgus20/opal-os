#ifndef OPAL_MM_TMPALLOC_H
#define OPAL_MM_TMPALLOC_H

#include <opal/mm/map.h>

struct tmpalloc {
    struct mmap mm;
};

void tmpalloc_create(struct tmpalloc *ta, struct mmap_entry *buffer, size_t len, const struct mmap *src);
[[nodiscard]] phys_addr_t tmpalloc_alloc_pages(struct tmpalloc *ta, size_t max_pages, size_t *allocated_pages);

#endif
