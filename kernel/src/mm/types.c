#include <opal/mm/types.h>

const char *usable_entry_type_str(mmap_entry_type_t type) {
    switch (type) {
        case USABLE_ENTRY_METADATA:
            return "Metadata";
        case MMAP_ENTRY_USABLE:
            return "Available";
        default:
            return "(unknown)";
    }
}
