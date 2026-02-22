#include <stdint.h>

#include <kc/string.h>

#include <opal/klog.h>
#include <opal/mm/mm.h>
#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/mm/slab.h>
#include <opal/platform/boot.h>
#include <opal/platform/mm/pagetable.h>

static void log_mm(void) {
    mm_log_map();

    kinfo("buddy max order=%u / free pages=%zu", mm_buddy_get_max_order(), mm_buddy_get_free_pages());
}

void mm_init(void) {
    mm_map_init();

    mm_tmp_alloc_create();

    mm_pagetable_init();
    mm_pfn_init();

    mm_tmp_alloc_finalize();
    mm_buddy_init();

    log_mm();
}

static void log_map(const struct mmap *mmap, const char *(*entry_type_str)(mmap_entry_type_t)) {
    for (uint32_t i = 0; i < mmap->length; i++) {
        const struct mmap_entry *entry = &mmap->entries[i];

        phys_addr_t end = entry->addr + entry->len;
        if (entry->addr <= end) {
            kinfo("  [%#018"PRIphys", %#018"PRIphys") %s",
                entry->addr, end, entry_type_str(entry->type));
        } else {
            kinfo("  [%#018"PRIphys", %#018"PRIphys"] %s",
                entry->addr, PHYS_ADDR_MAX, entry_type_str(entry->type));
        }
    }
}

void mm_log_map(void) {
    kinfo("boot memory map:");
    log_map(boot_get_mmap(), mmap_entry_type_str);
    kinfo("canonical memory map:");
    log_map(mm_get_memory_map(), mmap_entry_type_str);
    kinfo("memory section map:");
    log_map(mm_get_section_map(), mm_sec_entry_type_str);
}
