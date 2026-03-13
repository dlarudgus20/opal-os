#ifndef OPAL_MM_MAP_H
#define OPAL_MM_MAP_H

#include <stddef.h>

#include <opal/mm/types.h>

#define MAX_MMAP_ENTRIES 128

enum {
    MM_SEC_ENTRY_METADATA,
    MM_SEC_ENTRY_RESERVED,
    MM_SEC_ENTRY_USABLE,
};

struct tmpalloc;

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
void mm_map_finalize_tmpalloc(struct tmpalloc *ta);

[[nodiscard]] const struct mmap *mm_get_memory_map(void);
[[nodiscard]] const struct mmap *mm_get_section_map(void);

const char *mm_sec_entry_type_str(mmap_entry_type_t type);

#ifdef OPAL_TEST
void refine_mmap(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *boot_map);
void init_mm_section(
    struct mmap *sec,
    const struct mmap *mmap,
    phys_addr_t section_start,
    phys_addr_t reserved_start,
    phys_addr_t reserved_end
);
#endif

#endif
