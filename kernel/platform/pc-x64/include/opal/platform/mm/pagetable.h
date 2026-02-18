#ifndef OPAL_PLATFORM_MM_PAGE_H
#define OPAL_PLATFORM_MM_PAGE_H

#include <stdbool.h>

#include <opal/mm/types.h>

#define PAGE_SIZE 0x1000

#define KERNEL_START_PHYS       0x00200000u
#define KERNEL_START_VIRT       0xffffffff80000000u
#define KSTACK_START_VIRT       0xffffffff8f000000u

#define DIRECT_MAP_START_VIRT   0xffff900000000000u
#define PAGES_START_VIRT        0xffffe00000000000u

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

void mm_pagetable_init(const struct mmap *snapshot);
virt_addr_t mm_pagetable_map(virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags);

void mm_pagetable_print(void);

#ifdef OPAL_TEST
#define __kernel_start_lba  ((char*)1)
#define __rodata_end_lba    ((char*)1)
#define __before_stack_lba  ((char*)1)
#define __stack_bottom_lba  ((char*)1)
#define __kernel_end_lba    ((char*)1)
#else
// linker script
extern char __kernel_start_lba[];
extern char __rodata_end_lba[];
extern char __before_stack_lba[];
extern char __stack_bottom_lba[];
extern char __kernel_end_lba[];
#endif

#endif
