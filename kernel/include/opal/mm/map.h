#ifndef OPAL_MM_MAP_H
#define OPAL_MM_MAP_H

#include <stddef.h>

#include <opal/mm/types.h>

#define MAX_BOOT_MMAP_ENTRIES 128

// +1 required for metadata entry
#define MAX_USABLE_ENTRIES (MAX_BOOT_MMAP_ENTRIES + 1)

struct mmap_entry {
    phys_addr_t addr;
    phys_size_t len;
    mmap_entry_type_t type;
};

struct mmap {
    struct mmap_entry *entries;
    uint32_t length;
};

void mm_map_init(void);
phys_addr_t mm_usable_alloc_metadata(size_t max_pages, size_t *allocated_pages);

const struct mmap *mm_get_boot_map(void);
const struct mmap *mm_get_usable_map(void);

#ifdef OPAL_TEST
void construct_usable_map(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *boot_map);
#endif

#endif
