#ifndef OPAL_MM_VMAP_H
#define OPAL_MM_VMAP_H

#include <stddef.h>

#include <opal/mm/types.h>

void mm_vmap_init(void);
[[nodiscard]] struct span mm_vmap_alloc(void **va_out, phys_addr_t pa, phys_size_t size);
void mm_vmap_free(struct span span);

#endif
