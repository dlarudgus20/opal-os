#ifndef OPAL_MM_BUDDY_H
#define OPAL_MM_BUDDY_H

#include <stddef.h>
#include <stdint.h>

#include <collections/linkedlist.h>

#include <opal/mm/types.h>
#include <opal/platform/mm/defines.h>

struct mmap;

#define BUDDY_MAX_ORDERS PFN_VALID_BIT_WIDTH

struct buddy {
    struct linkedlist free_list[BUDDY_MAX_ORDERS];
    uint8_t max_order;
    size_t free_pages;
    size_t total_pages;
};

void buddy_create(struct buddy *buddy, const struct mmap *mmap);

[[nodiscard]] pfn_t buddy_alloc(struct buddy *buddy, uint8_t order);
void buddy_free(struct buddy *buddy, pfn_t pfn, uint8_t order);

[[nodiscard]] size_t buddy_get_free_pages(struct buddy *buddy);
[[nodiscard]] size_t buddy_get_total_pages(struct buddy *buddy);
[[nodiscard]] uint8_t buddy_get_max_order(struct buddy *buddy);

#endif
