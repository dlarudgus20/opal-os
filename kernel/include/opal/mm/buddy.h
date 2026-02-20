#ifndef OPAL_MM_BUDDY_H
#define OPAL_MM_BUDDY_H

#include <stddef.h>
#include <stdint.h>

#include <opal/mm/types.h>

void mm_buddy_init(void);
pfn_t mm_buddy_alloc(uint8_t order);
void mm_buddy_free(pfn_t pfn, uint8_t order);
size_t mm_buddy_get_free_pages(void);
uint8_t mm_buddy_get_max_order(void);

#endif
