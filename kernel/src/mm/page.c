#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <kc/assert.h>
#include <kc/stdlib.h>

#include <opal/mm/map.h>
#include <opal/mm/page.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>

struct meta_ranges {
    pfn_t pfn_start;
    pfn_t pfn_end;
    virt_size_t page_start;
    virt_size_t page_end;
};

static phys_addr_t g_pages_phys_start;
static pfn_t g_pfn_end;

static struct meta_ranges meta_ranges_for_entry(const struct mmap_entry *entry) {
    const pfn_t pfn_start = entry->addr / PAGE_SIZE;
    const pfn_t pfn_end = pfn_start + entry->len / PAGE_SIZE;

    const virt_size_t byte_start = pfn_start * sizeof(struct page);
    const virt_size_t byte_end = pfn_end * sizeof(struct page);

    const virt_size_t page_start = byte_start / PAGE_SIZE;
    const virt_size_t page_end = (byte_end + PAGE_SIZE - 1) / PAGE_SIZE;

    return (struct meta_ranges){
        .pfn_start = pfn_start,
        .pfn_end = pfn_end,
        .page_start = page_start,
        .page_end = page_end,
    };
}

static phys_addr_t get_first_free_usable(const struct mmap* usable) {
    for (uint32_t i = 0; i < usable->length; i++) {
        const struct mmap_entry *entry = &usable->entries[i];

        if (entry->type != MMAP_ENTRY_USABLE) {
            continue;
        }

        return entry->addr;
    }

    return PHYS_ADDR_MAX;
}

static void prepare_pagetable(virt_addr_t va, virt_size_t pages_needed) {
    for (virt_size_t page = 0; page < pages_needed; page++) {
        mm_pagetable_map(va + page * PAGE_SIZE, 0, 0, true);
    }
}

static virt_addr_t allocate_usables(virt_addr_t va, virt_size_t pages_needed, virt_addr_t allocated_end) {
    if (allocated_end > va) {
        size_t pages_allocated = (allocated_end - va) / PAGE_SIZE;
        if (pages_needed <= pages_allocated) {
            return allocated_end;
        }

        va += pages_allocated * PAGE_SIZE;
        pages_needed -= pages_allocated;
    }

    while (pages_needed > 0) {
        size_t pages_allocated;
        phys_addr_t pa = mm_usable_alloc_metadata(pages_needed, &pages_allocated);

        for (virt_size_t page = 0; page < pages_allocated; page++) {
            virt_size_t offset = page * PAGE_SIZE;
            mm_pagetable_map(va + offset, pa + offset, PTE_FLAG_WRITABLE | PTE_FLAG_PRESENT, false);
            tlb_flush_for(va + offset);
        }

        va += pages_allocated * PAGE_SIZE;
        pages_needed -= pages_allocated;
    }

    return va;
}

static void initialize_pages(pfn_t pfn_start, pfn_t pfn_end, bool is_metadata) {
    struct page *const ptr_start = (struct page *)PAGES_START_VIRT + pfn_start;
    struct page *const ptr_end = (struct page *)PAGES_START_VIRT + pfn_end;

    for (struct page *ptr = ptr_start; ptr < ptr_end; ptr++) {
        *ptr = (struct page){
            .flags = is_metadata ? PAGE_FLAG_METADATA : PAGE_FLAG_FREE,
            .refcount = 0,
        };
    }
}

enum build_stage { STAGE_PREPARE, STAGE_ALLOC, STAGE_INIT };

static void build_metadata(const struct mmap *usable, enum build_stage stage) {
    virt_addr_t allocated_end = PAGES_START_VIRT;

    for (uint32_t i = 0; i < usable->length; i++) {
        const struct mmap_entry *entry = &usable->entries[i];
        const struct meta_ranges ranges = meta_ranges_for_entry(entry);

        const virt_size_t pages_needed = ranges.page_end - ranges.page_start;
        const virt_addr_t va_start = PAGES_START_VIRT + ranges.page_start * PAGE_SIZE;

        if (stage == STAGE_PREPARE) {
            prepare_pagetable(va_start, pages_needed);
        } else if (stage == STAGE_ALLOC) {
            allocated_end = allocate_usables(va_start, pages_needed, allocated_end);
        } else {
            bool is_metadata = entry->type == USABLE_ENTRY_METADATA;
            initialize_pages(ranges.pfn_start, ranges.pfn_end, is_metadata);
        }
    }
}

static void build_metadata_run(const struct mmap *snapshot) {
    // STAGE_PREPARE
    build_metadata(snapshot, STAGE_PREPARE);
    g_pages_phys_start = get_first_free_usable(mm_get_usable_map());

    // STAGE_ALLOC
    build_metadata(snapshot, STAGE_ALLOC);

    // STAGE_INIT
    build_metadata(mm_get_usable_map(), STAGE_INIT);
}

static pfn_t get_pfn_end(void) {
    const struct mmap *usable = mm_get_usable_map();
    if (usable->length == 0) {
        return 0;
    }

    const struct mmap_entry *entry = &usable->entries[usable->length - 1];
    const struct meta_ranges ranges = meta_ranges_for_entry(entry);
    return ranges.pfn_end;
}

void mm_page_init(const struct mmap *snapshot) {
    build_metadata_run(snapshot);
    g_pfn_end = get_pfn_end();
}

pfn_t mm_get_pfn_end(void) {
    return g_pfn_end;
}

bool mm_pfn_is_valid(pfn_t pfn) {
    const struct mmap *usable = mm_get_usable_map();

    for (uint32_t i = 0; i < usable->length; i++) {
        const struct mmap_entry *entry = &usable->entries[i];
        const struct meta_ranges ranges = meta_ranges_for_entry(entry);

        if (pfn < ranges.pfn_start) {
            return false;
        } else if (pfn < ranges.pfn_end) {
            return true;
        }
    }

    return false;
}

struct page *mm_page_by_pfn(pfn_t pfn) {
    assert(pfn < g_pfn_end);
    return (struct page *)PAGES_START_VIRT + pfn;
}

pfn_t mm_pfn_by_page(struct page *page) {
    virt_addr_t va = (virt_addr_t)page;
    assert(va >= PAGES_START_VIRT);
    pfn_t pfn = (va - PAGES_START_VIRT) / sizeof(struct page);
    assert(pfn < g_pfn_end);
    return pfn;
}

static phys_addr_t entry_end_exclusive(const struct mmap_entry *entry) {
    if (entry->addr > PHYS_ADDR_MAX - entry->len) {
        return PHYS_ADDR_MAX;
    }
    return entry->addr + entry->len;
}

static virt_addr_t pages_va_start(const struct mmap *usable) {
    if (usable->length == 0) {
        return PAGES_START_VIRT;
    }

    const struct meta_ranges first = meta_ranges_for_entry(&usable->entries[0]);
    return PAGES_START_VIRT + first.page_start * PAGE_SIZE;
}

bool phys_to_virt_metadata(phys_addr_t pa, virt_addr_t *va_out) {
    const struct mmap* usable = mm_get_usable_map();
    virt_addr_t pagetable_va = PAGETABLE_START_VIRT;
    virt_addr_t pages_va = pages_va_start(usable);

    for (size_t i = 0; i < usable->length; i++) {
        const struct mmap_entry* entry = &usable->entries[i];
        const phys_addr_t seg_start = entry->addr;
        const phys_addr_t seg_end = entry_end_exclusive(entry);

        if (entry->type == MMAP_ENTRY_USABLE || seg_start >= seg_end) {
            continue;
        }

        if (seg_end <= g_pages_phys_start) {
            if (seg_start <= pa && pa < seg_end) {
                *va_out = pagetable_va + (pa - seg_start);
                return true;
            }
            pagetable_va += seg_end - seg_start;
            continue;
        }

        if (g_pages_phys_start <= seg_start) {
            if (seg_start <= pa && pa < seg_end) {
                *va_out = pages_va + (pa - seg_start);
                return true;
            }
            pages_va += seg_end - seg_start;
            continue;
        }

        const phys_addr_t lo_len = g_pages_phys_start - seg_start;
        const phys_addr_t hi_len = seg_end - g_pages_phys_start;

        if (seg_start <= pa && pa < g_pages_phys_start) {
            *va_out = pagetable_va + (pa - seg_start);
            return true;
        }
        if (g_pages_phys_start <= pa && pa < seg_end) {
            *va_out = pages_va + (pa - g_pages_phys_start);
            return true;
        }

        pagetable_va += lo_len;
        pages_va += hi_len;
    }

    return false;
}

#include <opal/tty.h>

static const char *page_flags_str(uint16_t flags) {
    switch (flags) {
        case PAGE_FLAG_FREE:
            return "Free";
        case PAGE_FLAG_METADATA:
            return "Metadata";
        default:
            return "(unknown)";
    }
}

static void print_pfns(pfn_t pfn_start, pfn_t pfn_end, uint16_t flags) {
    tty0_printf("PFN [%#015"PRIpfn", %#015"PRIpfn") %s\n", pfn_start, pfn_end, page_flags_str(flags));
}

void mm_pfn_print_all(void) {
    const struct mmap *usable = mm_get_usable_map();
    if (usable->length == 0) {
        return;
    }

    struct page *prev = NULL;
    pfn_t run_start = 0;
    pfn_t prev_pfn = 0;

    for (uint32_t i = 0; i < usable->length; i++) {
        const struct mmap_entry *entry = &usable->entries[i];
        const struct meta_ranges ranges = meta_ranges_for_entry(entry);

        for (pfn_t pfn = ranges.pfn_start; pfn < ranges.pfn_end; pfn++) {
            struct page *page = mm_page_by_pfn(pfn);
            if (!prev) {
                prev = page;
                run_start = pfn;
                prev_pfn = pfn;
                continue;
            }

            const bool gap = (pfn != prev_pfn + 1);
            const bool flag_changed = (prev->flags != page->flags);

            if (gap || flag_changed) {
                print_pfns(run_start, pfn, prev->flags);
                prev = page;
                run_start = pfn;
            }

            prev_pfn = pfn;
        }
    }

    if (prev) {
        print_pfns(run_start, prev_pfn + 1, prev->flags);
    }
}
