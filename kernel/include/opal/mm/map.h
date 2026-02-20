#ifndef OPAL_MM_MAP_H
#define OPAL_MM_MAP_H

#include <stddef.h>

#include <opal/mm/types.h>

#define MAX_MMAP_ENTRIES 128

// +1 required for metadata entry
#define MAX_MM_SEC_ENTRIES (MAX_MMAP_ENTRIES + 1)

enum {
    MM_SEC_ENTRY_METADATA = 0,
    MM_SEC_ENTRY_USABLE = 1,
};

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
phys_addr_t mm_sec_alloc_metadata(size_t max_pages, size_t *allocated_pages);

const struct mmap *mm_get_memory_map(void);
const struct mmap *mm_get_section_map(void);

#ifdef OPAL_TEST
void refine_mmap(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *boot_map);
#endif

const char *mm_sec_entry_type_str(mmap_entry_type_t type);

#endif
