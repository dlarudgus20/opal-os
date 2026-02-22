#ifndef OPAL_MM_MM_H
#define OPAL_MM_MM_H

#include <stdint.h>

void mm_init(void);

void *mm_alloc_page(uint8_t order);
void mm_free_page(void *ptr, uint8_t order);
struct buddy *mm_get_buddy(void);

void mm_log_map(void);

#endif
