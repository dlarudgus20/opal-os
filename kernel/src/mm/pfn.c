#include <stddef.h>
#include <stdint.h>

#include <kc/assert.h>
#include <kc/stdlib.h>

#include <opal/mm/map.h>
#include <opal/mm/tmpalloc.h>
#include <opal/mm/pfn.h>
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

static virt_addr_t allocate_pages(
    struct tmpalloc *ta,
    virt_addr_t va,
    virt_size_t pages_needed,
    virt_addr_t allocated_end
) {
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
        phys_addr_t pa = tmpalloc_alloc_pages(ta, pages_needed, &pages_allocated);

        pagetable_map(mm_kptbl_get(), va, pa, pages_allocated * PAGE_SIZE, PTE_FLAG_WRITABLE | PTE_FLAG_PRESENT);

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
            .flags = is_metadata ? PAGE_FLAG_METADATA : 0,
            .refcount = 0,
            .buddy_order = 0,
            .buddy_link = { .prev = NULL, .next = NULL },
        };
    }
}

enum build_stage { STAGE_ALLOC, STAGE_INIT };

static void build_metadata(struct tmpalloc *ta, const struct mmap *section_map, enum build_stage stage) {
    virt_addr_t allocated_end = PAGES_START_VIRT;

    for (uint32_t i = 0; i < section_map->length; i++) {
        const struct mmap_entry *entry = &section_map->entries[i];
        const struct meta_ranges ranges = meta_ranges_for_entry(entry);

        const virt_size_t pages_needed = ranges.page_end - ranges.page_start;
        const virt_addr_t va_start = PAGES_START_VIRT + ranges.page_start * PAGE_SIZE;

        if (stage == STAGE_ALLOC) {
            allocated_end = allocate_pages(ta, va_start, pages_needed, allocated_end);
        } else {
            bool is_metadata = entry->type == MM_SEC_ENTRY_METADATA;
            initialize_pages(ranges.pfn_start, ranges.pfn_end, is_metadata);
        }
    }
}

static void build_metadata_run(struct tmpalloc *ta) {
    // STAGE_ALLOC
    build_metadata(ta, mm_get_section_map(), STAGE_ALLOC);

    // STAGE_INIT
    build_metadata(ta, &ta->mm, STAGE_INIT);
}

static pfn_t get_pfn_end(void) {
    const struct mmap *section_map = mm_get_section_map();
    if (section_map->length == 0) {
        return 0;
    }

    const struct mmap_entry *entry = &section_map->entries[section_map->length - 1];
    const struct meta_ranges ranges = meta_ranges_for_entry(entry);
    return ranges.pfn_end;
}

void pfn_init(struct tmpalloc *ta) {
    build_metadata_run(ta);
    g_pfn_end = get_pfn_end();
}

pfn_t pfn_get_end(void) {
    return g_pfn_end;
}

bool pfn_is_valid(pfn_t pfn) {
    const struct mmap *section_map = mm_get_section_map();

    for (uint32_t i = 0; i < section_map->length; i++) {
        const struct mmap_entry *entry = &section_map->entries[i];
        const struct meta_ranges ranges = meta_ranges_for_entry(entry);

        if (pfn < ranges.pfn_start) {
            return false;
        } else if (pfn < ranges.pfn_end) {
            return true;
        }
    }

    return false;
}

phys_addr_t pfn_to_phys(pfn_t pfn) {
    return pfn * PAGE_SIZE;
}

pfn_t phys_to_pfn(phys_addr_t pa) {
    return pa / PAGE_SIZE;
}

struct page *pfn_to_page(pfn_t pfn) {
    assert(pfn < g_pfn_end);
    return (struct page *)PAGES_START_VIRT + pfn;
}

pfn_t page_to_pfn(struct page *page) {
    virt_addr_t va = (virt_addr_t)page;
    assert(va >= PAGES_START_VIRT);
    pfn_t pfn = (va - PAGES_START_VIRT) / sizeof(struct page);
    assert(pfn < g_pfn_end);
    return pfn;
}

void *pfn_to_direct_ptr(pfn_t pfn) {
    assert(pfn < g_pfn_end);
    return (void *)(DIRECT_MAP_START_VIRT + pfn * PAGE_SIZE);
}

pfn_t direct_ptr_to_pfn(void *ptr) {
    virt_addr_t va = (virt_addr_t)ptr;
    assert(DIRECT_MAP_START_VIRT <= va && va < DIRECT_MAP_END_VIRT);
    return (va - DIRECT_MAP_START_VIRT) / PAGE_SIZE;
}

void *phys_to_direct_ptr(phys_addr_t pa) {
    assert(pa <= DIRECT_MAP_END_VIRT - DIRECT_MAP_START_VIRT);
    return (void *)(DIRECT_MAP_START_VIRT + pa);
}

phys_addr_t direct_ptr_to_phys(void *ptr) {
    virt_addr_t va = (virt_addr_t)ptr;
    assert(DIRECT_MAP_START_VIRT <= va && va <= DIRECT_MAP_END_VIRT);
    return va - DIRECT_MAP_START_VIRT;
}

#include <opal/tty.h>

static void print_pfns(pfn_t pfn_start, pfn_t pfn_end, uint16_t flags) {
    tty0_printf("PFN [%#015"PRIpfn", %#015"PRIpfn")%s\n",
        pfn_start, pfn_end, flags & PAGE_FLAG_METADATA ? " (metadata)" : "");
}

void pfn_print_all(void) {
    const struct mmap *section_map = mm_get_section_map();
    if (section_map->length == 0) {
        return;
    }

    struct page *prev = NULL;
    pfn_t run_start = 0;
    pfn_t prev_pfn = 0;

    for (uint32_t i = 0; i < section_map->length; i++) {
        const struct mmap_entry *entry = &section_map->entries[i];
        const struct meta_ranges ranges = meta_ranges_for_entry(entry);

        for (pfn_t pfn = ranges.pfn_start; pfn < ranges.pfn_end; pfn++) {
            struct page *page = pfn_to_page(pfn);
            if (!prev) {
                prev = page;
                run_start = pfn;
                prev_pfn = pfn;
                continue;
            }

            const bool gap = (pfn != prev_pfn + 1);
            const bool flag_changed = ((prev->flags ^ page->flags) & PAGE_FLAG_METADATA) != 0;

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
