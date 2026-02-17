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
