#ifndef OPAL_MM_TYPES_H
#define OPAL_MM_TYPES_H

enum {
    USABLE_ENTRY_METADATA = 0,

    // boot mmap should report useable memory areas as this type.
    MMAP_ENTRY_USABLE = 1,

    // other platform-dependent entry types are for unusable memory areas
};

#include <opal/platform/mm/types.h>

#define PRIpfn PRIvirt

typedef virt_addr_t pfn_t;

const char *usable_entry_type_str(mmap_entry_type_t type);

#endif
