#include <opal/mm/types.h>

const char *mmap_entry_type_str(mmap_entry_type_t type) {
    switch (type) {
        case MMAP_ENTRY_USABLE:
            return "Available";
        case MMAP_ENTRY_RESERVED:
            return "Reserved";
        case MMAP_ENTRY_ACPI:
            return "AcpiReclaimable";
        case MMAP_ENTRY_NVS:
            return "AcpiNvs";
        case MMAP_ENTRY_BAD:
            return "BadRAM";
        default:
            return "(unknown)";
    }
}

int mmap_entry_overlap_priority(mmap_entry_type_t type) {
    switch (type) {
        case MMAP_ENTRY_USABLE:
            return 100;
        case MMAP_ENTRY_RESERVED:
            return 2;
        case MMAP_ENTRY_ACPI:
            return 52;
        case MMAP_ENTRY_NVS:
            return 51;
        case MMAP_ENTRY_BAD:
            return 1;
        default:
            return 50;
    }
}
