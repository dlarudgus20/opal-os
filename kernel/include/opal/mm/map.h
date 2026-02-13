#ifndef OPAL_MM_MAP_H
#define OPAL_MM_MAP_H

#include <opal/platform/mm/types.h>

struct mmap_entry {
    phys_addr_t addr;
    phys_addr_t len;
    mmap_entry_type_t type;
};

struct mmap {
    struct mmap_entry *entries;
    uint32_t length;
};

const struct mmap *mm_get_boot_map(void);

#endif
