#ifndef OPAL_PLATFORM_MM_TYPES_H
#define OPAL_PLATFORM_MM_TYPES_H

#include <limits.h>
#include <stdint.h>

#include <kc/inttypes.h>

#define PHYS_ADDR_MAX UINTPTR_MAX
#define PHYS_SIZE_MAX UINTPTR_MAX
#define VIRT_ADDR_MAX UINTPTR_MAX
#define VIRT_SIZE_MAX UINTPTR_MAX

#define PRIvirt PRIxPTR
#define PRIphys PRIxPTR

typedef uintptr_t phys_addr_t;
typedef uintptr_t phys_size_t;
typedef uintptr_t virt_addr_t;
typedef uintptr_t virt_size_t;

enum {
    MMAP_ENTRY_RESERVED = 2,
    MMAP_ENTRY_ACPI = 3,
    MMAP_ENTRY_NVS = 4,
    MMAP_ENTRY_BAD = 5,
};

typedef uint32_t mmap_entry_type_t;

const char *mmap_entry_type_str(mmap_entry_type_t type);

#endif
