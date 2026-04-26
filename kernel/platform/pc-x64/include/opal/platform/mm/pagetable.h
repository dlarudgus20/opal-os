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

#define PTBL_USER       (PTE_FLAG_PRESENT | PTE_FLAG_USER)
#define PTBL_WRITABLE   PTE_FLAG_WRITABLE

#define PAGETABLE_LENGTH 512

struct pagetable {
    alignas(PAGE_SIZE) page_entry_t entries[PAGETABLE_LENGTH];
};

struct tmpalloc;

void mm_kptbl_init(struct tmpalloc *ta);
void mm_kptbl_unuse_tmpalloc(void);
[[nodiscard]] struct pagetable *mm_kptbl_get(void);
void mm_kptbl_print(void);

[[nodiscard]] struct pagetable *pagetable_clone(struct pagetable *ptbl);
[[nodiscard]] struct pagetable *pagetable_create(void);
void pagetable_destroy(struct pagetable *ptbl);
void pagetable_apply(struct pagetable *ptbl);

virt_addr_t pagetable_map(struct pagetable *ptbl, virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags);
virt_addr_t pagetable_unmap(struct pagetable *ptbl, virt_addr_t va, virt_size_t len, bool flush_tlb);
bool pagetable_lookup(struct pagetable *ptbl, virt_addr_t va, phys_addr_t *pa_out);

#endif
