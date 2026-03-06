#ifndef OPAL_MM_PFN_H
#define OPAL_MM_PFN_H

#include <opal/mm/types.h>

struct page;

void mm_pfn_init(void);

pfn_t mm_get_pfn_end(void);
bool mm_pfn_is_valid(pfn_t pfn);

phys_addr_t mm_pfn_to_phys(pfn_t pfn);
pfn_t mm_phys_to_pfn(phys_addr_t pa);

struct page *mm_pfn_to_page(pfn_t pfn);
pfn_t mm_page_to_pfn(struct page *page);

void *mm_pfn_to_ptr(pfn_t pfn);
pfn_t mm_ptr_to_pfn(void *ptr);

void mm_pfn_print_all(void);

#endif
