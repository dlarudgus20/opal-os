#include <stdint.h>

#include <kc/assert.h>
#include <kc/string.h>

#include <opal/klog.h>
#include <opal/mm/mm.h>
#include <opal/mm/map.h>
#include <opal/mm/tmpalloc.h>
#include <opal/mm/pfn.h>
#include <opal/mm/buddy.h>
#include <opal/mm/kmalloc.h>
#include <opal/mm/vmap.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/boot/bootinfo.h>
#include <opal/platform/mm/pagetable.h>

static struct buddy g_buddy;

static void early_init(void) {
    mm_map_init();

    struct mmap_entry buffer[MAX_MMAP_ENTRIES];
    struct tmpalloc ta;
    tmpalloc_create(&ta, buffer, MAX_MMAP_ENTRIES, mm_get_section_map());

    mm_pagetable_init(&ta);
    pfn_init(&ta);

    mm_pagetable_unuse_tmpalloc();
    mm_map_finalize_tmpalloc(&ta);
}

static void log_mm(void) {
    mm_log_map();
    mm_log_buddy();
}

void mm_init(void) {
    early_init();

    buddy_create(&g_buddy, mm_get_section_map());
    kmalloc_init();
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

void *mm_alloc_page_ptr(uint8_t order) {
    irqlock_t irqlock = irqlock_acquire();
    pfn_t pfn = buddy_alloc(&g_buddy, order);
    void *direct_ptr = NULL;
    if (pfn != PFN_INVALID) {
        direct_ptr = pfn_to_direct_ptr(pfn);
    }
    irqlock_release(&irqlock);
    return direct_ptr;
}

void mm_free_page_ptr(void *direct_ptr, uint8_t order) {
    irqlock_t irqlock = irqlock_acquire();
    pfn_t pfn = direct_ptr_to_pfn(direct_ptr);
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
    log_map(bootinfo_get_mmap(), mmap_entry_type_str);
    kinfo("canonical memory map:");
    log_map(mm_get_memory_map(), mmap_entry_type_str);
    kinfo("memory section map:");
    log_map(mm_get_section_map(), mm_sec_entry_type_str);
}

void mm_log_buddy(void) {
    irqlock_t irqlock = irqlock_acquire();

    struct buddy *buddy = mm_get_buddy();
    size_t free = buddy_get_free_pages(buddy);
    size_t reserved = buddy_get_reserved_pages(buddy);
    size_t total = buddy_get_total_pages(buddy);
    uint8_t max_order = buddy_get_max_order(buddy);

    irqlock_release(&irqlock);

    assert(total >= free + reserved);
    size_t used = total - free - reserved;
    kinfo("buddy: used: %zu / free: %zu / reserved: %zu / total=%zu / max_order=%u",
        used, free, reserved, total, max_order);
}
