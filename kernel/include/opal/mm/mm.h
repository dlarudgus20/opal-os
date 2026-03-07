#ifndef OPAL_MM_MM_H
#define OPAL_MM_MM_H

#include <stdint.h>

#include <opal/mm/pfn.h>
#include <opal/platform/mm/defines.h>

void mm_init(void);

[[nodiscard]] pfn_t mm_alloc_page(uint8_t order);
void mm_free_page(pfn_t pfn, uint8_t order);
struct buddy *mm_get_buddy(void);

void mm_log_map(void);

#endif
