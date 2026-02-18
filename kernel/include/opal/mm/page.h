#ifndef OPAL_MM_PAGE_H
#define OPAL_MM_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <opal/mm/types.h>

enum {
    PAGE_FLAG_FREE,
    PAGE_FLAG_METADATA,
};

struct page {
    uint16_t flags;
    uint16_t refcount;
};

void mm_page_init(const struct mmap *snapshot);

pfn_t mm_get_pfn_end(void);
bool mm_pfn_is_valid(pfn_t pfn);
struct page *mm_page_by_pfn(pfn_t pfn);
pfn_t mm_pfn_by_page(struct page *page);

void mm_pfn_print_all(void);

#endif
