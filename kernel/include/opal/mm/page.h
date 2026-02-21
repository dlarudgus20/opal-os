#ifndef OPAL_MM_PAGE_H
#define OPAL_MM_PAGE_H

#include <opal/mm/types.h>

enum {
    PAGE_FLAG_METADATA = 1u << 0,

    PAGE_FLAG_BUDDY_FREE = 1u << 1,
    PAGE_FLAG_BUDDY_HEAD = 1u << 2,
};

struct page_buddy {
    uint8_t order;
    pfn_t next;
};

struct page {
    uint16_t flags;
    uint16_t refcount;
    struct page_buddy buddy;
};

#endif
