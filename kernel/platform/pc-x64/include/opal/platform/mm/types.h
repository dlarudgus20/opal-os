#ifndef OPAL_PLATFORM_MM_TYPES_H
#define OPAL_PLATFORM_MM_TYPES_H

#include <limits.h>
#include <stdint.h>

#define PAGE_SIZE 0x1000 // 4KB

#define PHYS_ADDR_MAX UINTPTR_MAX
#define PHYS_SIZE_MAX UINTPTR_MAX
#define VIRT_ADDR_MAX UINTPTR_MAX
#define VIRT_SIZE_MAX UINTPTR_MAX

typedef uintptr_t phys_addr_t;
typedef uintptr_t phys_size_t;
typedef uintptr_t virt_addr_t;
typedef uintptr_t virt_size_t;

enum {
    MMAP_ENTRY_USABLE = 1,
    MMAP_ENTRY_RESERVED = 2,
    MMAP_ENTRY_ACPI = 3,
    MMAP_ENTRY_NVS = 4,
    MMAP_ENTRY_BAD = 5,
};

typedef uint32_t mmap_entry_type_t;

#endif
