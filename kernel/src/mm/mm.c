#include <stdint.h>

#include <kc/string.h>

#include <opal/klog.h>
#include <opal/mm/mm.h>
#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/mm/vmap.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/boot.h>
#include <opal/platform/mm/pagetable.h>

static struct buddy g_buddy;

static void log_mm(void) {
    mm_log_map();
    mm_log_buddy();
}

void mm_init(void) {
    mm_map_init();

    mm_tmp_alloc_create();

    mm_pagetable_init();
    mm_pfn_init();

    mm_tmp_alloc_finalize();
    buddy_create(&g_buddy, mm_get_section_map());

    mm_vmap_init();

    log_mm();
}

pfn_t mm_alloc_page(uint8_t order) {
    irqlock_t irqlock = irqlock_acquire();
    pfn_t pfn = buddy_alloc(&g_buddy, order);
    irqlock_release(&irqlock);
    return pfn;
}

void mm_free_page(pfn_t pfn, uint8_t order) {
    irqlock_t irqlock = irqlock_acquire();
    buddy_free(&g_buddy, pfn, order);
    irqlock_release(&irqlock);
}

struct buddy *mm_get_buddy(void) {
    return &g_buddy;
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

void mm_log_buddy(void) {
    struct buddy *buddy = mm_get_buddy();
    size_t free = buddy_get_free_pages(buddy);
    size_t total = buddy_get_total_pages(buddy);
    uint8_t max_order = buddy_get_max_order(buddy);
    kinfo("buddy: used: %zu / free: %zu / total=%zu / max_order=%u",
        total - free, free, total, max_order);
}
