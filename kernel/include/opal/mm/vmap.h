#ifndef OPAL_MM_VMAP_H
#define OPAL_MM_VMAP_H

#include <stddef.h>

#include <opal/mm/types.h>

void mm_vmap_init(void);
[[nodiscard]] void *mm_vmap_alloc(phys_addr_t pa, phys_size_t size, struct span *va_span_out);
void mm_vmap_free(struct span span);

#endif
