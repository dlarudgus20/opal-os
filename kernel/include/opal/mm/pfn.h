#ifndef OPAL_MM_PFN_H
#define OPAL_MM_PFN_H

#include <stdbool.h>

#include <opal/mm/page.h>

void mm_pfn_init(void);

pfn_t mm_get_pfn_end(void);
bool mm_pfn_is_valid(pfn_t pfn);
struct page *mm_page_by_pfn(pfn_t pfn);
pfn_t mm_pfn_by_page(struct page *page);

void mm_pfn_print_all(void);

#endif
