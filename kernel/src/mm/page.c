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

static void prepare_pagetable(virt_addr_t va, virt_size_t pages_needed) {
    mm_pagetable_map(va, 0, pages_needed * PAGE_SIZE, 0);
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

        mm_pagetable_map(va, pa, pages_allocated * PAGE_SIZE, PTE_FLAG_WRITABLE | PTE_FLAG_PRESENT);

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
                print_pfns(run_start, prev_pfn + 1, prev->flags);
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
