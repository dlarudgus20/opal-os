#ifndef OPAL_PLATFORM_MM_PAGE_H
#define OPAL_PLATFORM_MM_PAGE_H

#include <opal/mm/types.h>
#include <opal/platform/mm/defines.h>

typedef uint64_t page_entry_t;

#define PTE_FLAG_PRESENT        ((page_entry_t)1 << 0)
#define PTE_FLAG_WRITABLE       ((page_entry_t)1 << 1)
#define PTE_FLAG_USER           ((page_entry_t)1 << 2)
#define PTE_FLAG_WRITE_THROUGH  ((page_entry_t)1 << 3)
#define PTE_FLAG_NO_CACHE       ((page_entry_t)1 << 4)
#define PTE_FLAG_ACCESSED       ((page_entry_t)1 << 5)
#define PTE_FLAG_DIRTY          ((page_entry_t)1 << 6)
#define PTE_FLAG_HUGE           ((page_entry_t)1 << 7)
#define PTE_FLAG_GLOBAL         ((page_entry_t)1 << 8)
#define PTE_FLAG_NO_EXECUTE     ((page_entry_t)1 << 63)
#define PTE_MASK_ADDR           ((page_entry_t)0x000ffffffffff000)

void mm_pagetable_init(void);
virt_addr_t mm_pagetable_map(virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags);
void mm_pagetable_unmap(virt_addr_t va, phys_size_t len);

void mm_pagetable_print(void);

#endif
