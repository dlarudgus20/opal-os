#ifndef OPAL_MM_TYPES_H
#define OPAL_MM_TYPES_H

#include <stddef.h>

enum {
    // boot mmap should report useable memory areas as this type.
    MMAP_ENTRY_USABLE = 1,

    // other platform-dependent entry types are for unusable memory areas
};

#include <opal/platform/mm/types.h>

#define PRIpfn PRIvirt
#define PFN_INVALID VIRT_ADDR_MAX

typedef virt_addr_t pfn_t;

struct span {
    void *ptr;
    size_t size;
};

#endif
