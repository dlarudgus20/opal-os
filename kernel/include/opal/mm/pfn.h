#ifndef OPAL_MM_PFN_H
#define OPAL_MM_PFN_H

#include <opal/mm/types.h>

struct tmpalloc;
struct page;

void pfn_init(struct tmpalloc *ta);

[[nodiscard]] pfn_t pfn_get_end(void);
[[nodiscard]] bool pfn_is_valid(pfn_t pfn);

[[nodiscard]] phys_addr_t pfn_to_phys(pfn_t pfn);
[[nodiscard]] pfn_t phys_to_pfn(phys_addr_t pa);

[[nodiscard]] struct page *pfn_to_page(pfn_t pfn);
[[nodiscard]] pfn_t page_to_pfn(struct page *page);

[[nodiscard]] void *pfn_to_direct_ptr(pfn_t pfn);
[[nodiscard]] pfn_t direct_ptr_to_pfn(void *ptr);

[[nodiscard]] void *phys_to_direct_ptr(phys_addr_t pa);
[[nodiscard]] phys_addr_t direct_ptr_to_phys(void *ptr);

void pfn_print_all(void);

#endif
