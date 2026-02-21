#ifndef OPAL_MM_PAGE_H
#define OPAL_MM_PAGE_H

#include <opal/mm/types.h>

enum {
    PAGE_FLAG_METADATA = 1u << 0,

    PAGE_FLAG_BUDDY_FREE = 1u << 1,
    PAGE_FLAG_BUDDY_HEAD = 1u << 2,
};

struct page {
    uint16_t flags;
    uint16_t refcount;
    uint8_t buddy_order;
    pfn_t buddy_next;
};

#endif
