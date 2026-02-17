#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <kc/stdlib.h>

#include <opal/test.h>
#include <opal/mm/map.h>
#include <opal/platform/boot/boot.h>
#include <opal/platform/mm/pagetable.h>

// usable_map is a memory map containing usable memory areas for kernel.
// Kernel uses part of them as areas for page metadata. They are marked as type=USABLE_ENTRY_METADATA.
// Others are free for kernel to use. They are marked as type=MMAP_ENTRY_USABLE.
// If not empty, usable_map has an type=0, len=0 entry at front initially.
// Thus paging initialization code can safely insert metadata areas before the first usable entry.
// usable_map is initialized by boot mmap after sorted, aligned in PAGE_SIZE, and its adjacent/overlapped entries merged.
// After page metadata is initialized, usable_map must not be changed.

static struct mmap_entry g_usable_entries[MAX_USABLE_ENTRIES];
static struct mmap g_usable_map = {
    .entries = g_usable_entries,
    .length = 0,
};

static phys_addr_t usable_floor_addr(void) {
    return align_ceil_sz_p2((phys_addr_t)(uintptr_t)__kernel_phys_end, PAGE_SIZE);
}

static phys_addr_t entry_last(const struct mmap_entry *entry) {
    return entry->addr + entry->len - 1;
}

static int mmap_entry_compare(const void *lhs, const void *rhs) {
    const struct mmap_entry *a = lhs;
    const struct mmap_entry *b = rhs;
    phys_addr_t a_last = entry_last(a);
    phys_addr_t b_last = entry_last(b);

    if (a->addr < b->addr) {
        return -1;
    }
    if (a->addr > b->addr) {
        return 1;
    }
    if (a_last < b_last) {
        return -1;
    }
    if (a_last > b_last) {
        return 1;
    }
    return 0;
}

STATIC_OR_TEST void construct_usable_map(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *boot_map) {
    assert(boot_map && boot_map->entries, "boot_map or its entries is null");
    assert(max_entries >= 2, "max_entries is too small");

    // skip first element for USABLE_ENTRY_METADATA entry
    struct mmap_entry *const entries_out = mmap_out->entries + 1;
    uint32_t filtered_len = 0;

    const phys_addr_t floor_addr = usable_floor_addr();

    // write aligned boot_map
    for (uint32_t i = 0; i < boot_map->length; i++) {
        const struct mmap_entry *entry = &boot_map->entries[i];
        phys_addr_t start_aligned = align_ceil_sz_p2(entry->addr, PAGE_SIZE);

        if (start_aligned < entry->addr) {
            continue;
        }
        if (entry->type != MMAP_ENTRY_USABLE) {
            continue;
        }
        if (entry->len == 0) {
            continue;
        }

        if (filtered_len + 1 >= max_entries) {
            panic("usable_map entry buffer overflow");
        }

        if (start_aligned < floor_addr) {
            start_aligned = floor_addr;
        }

        const phys_addr_t end = entry->addr + entry->len;
        phys_addr_t last_aligned;

        if (end < entry->addr) {
            last_aligned = PHYS_ADDR_MAX;
        } else {
            const phys_addr_t end_aligned = align_floor_sz_p2(end, PAGE_SIZE);
            if (end_aligned <= start_aligned) {
                continue;
            }
            last_aligned = end_aligned - 1;
        }

        entries_out[filtered_len++] = (struct mmap_entry){
            .addr = start_aligned,
            .len = last_aligned - start_aligned + 1,
            .type = MMAP_ENTRY_USABLE,
        };
    }

    if (filtered_len == 0) {
        mmap_out->length = 0;
        return;
    }

    // sort entries
    sort(entries_out, filtered_len, sizeof(entries_out[0]), mmap_entry_compare);

    // insert USABLE_ENTRY_METADATA entry at front
    mmap_out->entries[0] = (struct mmap_entry) {
        .addr = entries_out[0].addr,
        .len = 0,
        .type = USABLE_ENTRY_METADATA,
    };

    // merge adjacent/overlapped entries
    uint32_t out_len = 0;

    for (uint32_t i = 0; i < filtered_len; i++) {
        phys_addr_t seg_start = entries_out[i].addr;
        phys_addr_t seg_last = entry_last(&entries_out[i]);

        if (seg_last < seg_start) {
            continue;
        }

        if (out_len > 0) {
            struct mmap_entry *prev = &entries_out[out_len - 1];
            phys_addr_t prev_last = entry_last(prev);

            if (prev_last == PHYS_ADDR_MAX || seg_start <= prev_last + 1) {
                if (seg_last > prev_last) {
                    prev->len += seg_last - prev_last;
                }
                continue;
            }
        }

        if (out_len + 1 >= max_entries) {
            panic("usable_map entry buffer overflow");
        }

        entries_out[out_len++] = (struct mmap_entry){
            .addr = seg_start,
            .len = seg_last - seg_start + 1,
            .type = MMAP_ENTRY_USABLE,
        };
    }

    mmap_out->length = out_len + 1;
}

void mm_map_init(void) {
    construct_usable_map(&g_usable_map, MAX_USABLE_ENTRIES, boot_get_mmap());
}

// this function must not be called after paging is initialized.
phys_addr_t mm_usable_alloc_metadata(size_t max_pages, size_t *allocated_pages) {
    for (uint32_t i = 0; i + 1 < g_usable_map.length; i++) {
        struct mmap_entry *const entry = &g_usable_map.entries[i];
        struct mmap_entry *const next = &g_usable_map.entries[i + 1];

        if (entry->type == MMAP_ENTRY_USABLE) {
            panic("usable_map is corrupted");
        }

        if (next->len < PAGE_SIZE) {
            next->type = USABLE_ENTRY_METADATA;
            continue;
        }

        phys_size_t allocated = next->len / PAGE_SIZE;
        if (allocated > max_pages) {
            allocated = max_pages;
        }

        const phys_addr_t addr = next->addr;

        entry->len += allocated * PAGE_SIZE;
        next->addr += allocated * PAGE_SIZE;
        next->len -= allocated * PAGE_SIZE;
        *allocated_pages = allocated;
        return addr;
    }

    panic("usable_map is already full of metadata page");
}

const struct mmap *mm_get_boot_map(void) {
    return boot_get_mmap();
}

const struct mmap *mm_get_usable_map(void) {
    return &g_usable_map;
}
