#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <kc/stdlib.h>

#include <opal/test.h>
#include <opal/mm/map.h>
#include <opal/platform/boot.h>
#include <opal/platform/mm/pagetable.h>

static struct mmap_entry g_mmap_entries[MAX_MMAP_ENTRIES];
static struct mmap g_mmap = {
    .entries = g_mmap_entries,
    .length = 0,
};

static struct mmap_entry g_mm_sec_sentries[MAX_MMAP_ENTRIES];
static struct mmap g_mm_sec = {
    .entries = g_mm_sec_sentries,
    .length = 0,
};

static bool align_if_usable(struct mmap_entry* entry) {
    if (entry->type != MMAP_ENTRY_USABLE) {
        return true;
    }

    phys_addr_t start_aligned = align_ceil_sz_p2(entry->addr, PAGE_SIZE);
    if (start_aligned < entry->addr) {
        return false;
    }

    const phys_addr_t end = entry->addr + entry->len;
    phys_addr_t last_aligned;

    if (end < entry->addr) {
        last_aligned = PHYS_ADDR_MAX;
    } else {
        const phys_addr_t end_aligned = align_floor_sz_p2(end, PAGE_SIZE);
        if (end_aligned <= start_aligned) {
            return false;
        }
        last_aligned = end_aligned - 1;
    }

    entry->addr = start_aligned;
    entry->len = last_aligned - start_aligned + 1;
    return true;
}

static void align_usables_mmap(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *mmap_in) {
    mmap_out->length = 0;

    for (uint32_t i = 0; i < mmap_in->length; i++) {
        struct mmap_entry entry = mmap_in->entries[i];

        if (entry.len == 0) {
            continue;
        }

        if (!align_if_usable(&entry)) {
            continue;
        }

        if (mmap_out->length >= max_entries) {
            panic("too many mmap entries");
        }

        mmap_out->entries[mmap_out->length++] = entry;
    }
}

static int entry_compare(const void *lhs, const void *rhs) {
    const struct mmap_entry *a = lhs;
    const struct mmap_entry *b = rhs;

    // prefer lower address, shorter length

    if (a->addr < b->addr) {
        return -1;
    } else if (a->addr > b->addr) {
        return 1;
    } else if (a->len < b->len) {
        return -1;
    } else if (a->len > b->len) {
        return 1;
    } else {
        return 0;
    }
}

static bool consume_remaining(size_t *remaining) {
    if (*remaining > 0) {
        (*remaining)--;
        return true;
    } else {
        return false;
    }
}

static bool is_preferred_entry(const struct mmap_entry *a, const struct mmap_entry *b) {
    int ap = mmap_entry_overlap_priority(a->type);
    int bp = mmap_entry_overlap_priority(b->type);
    return ap <= bp;
}

static bool insert_with_overlap(struct mmap_entry *prev, size_t *remaining, const struct mmap_entry *in) {
    phys_addr_t prev_last = prev->addr + prev->len - 1;
    phys_addr_t in_last = in->addr + in->len - 1;

    if (prev->type == MMAP_ENTRY_USABLE && in->type == MMAP_ENTRY_USABLE) {
        // usable overlap/adjacent
        if (in->addr == 0 || prev_last >= in->addr - 1) {
            if (prev_last < in_last) {
                prev->len += in_last - prev_last;
            }
            return true;
        }
    }

    if (in->addr > 0 && prev_last <= in->addr - 1) {
        // without overlap
        if (!consume_remaining(remaining)) {
            return false;
        }
        prev[1] = *in;
        return true;
    }

    if (is_preferred_entry(prev, in)) {
        // prev is preferred
        if (prev_last < in_last) {
            struct mmap_entry entry = {
                .addr = prev_last + 1,
                .len = in_last - prev_last,
                .type = in->type,
            };
            if (align_if_usable(&entry)) {
                if (!consume_remaining(remaining)) {
                    return false;
                }
                prev[1] = entry;
            }
        }
        return true;
    }

    // in is preferred
    const mmap_entry_type_t prev_type = prev->type;
    bool prev_is_consumed = true;

    if (prev->addr != in->addr) {
        // leading piece of prev exists
        prev->len = in->addr - prev->addr;
        if (align_if_usable(prev)) {
            prev_is_consumed = false;
            prev++;
        }
    }

    if (prev_is_consumed || !consume_remaining(remaining)) {
        return false;
    }
    *prev++ = *in;

    if (in_last < prev_last) {
        // trailing piece of prev exists
        struct mmap_entry entry = {
            .addr = in_last + 1,
            .len = prev_last - in_last,
            .type = prev_type,
        };
        if (align_if_usable(&entry)) {
            if (!consume_remaining(remaining)) {
                return false;
            }
            *prev = entry;
        }
    }

    return true;
}

static void remove_overlaps_mmap(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *mmap_in) {
    mmap_out->length = 0;
    if (max_entries == 0) {
        return;
    }

    for (uint32_t i = 0; i < mmap_in->length; i++) {
        const struct mmap_entry *const in = &mmap_in->entries[i];

        if (i == 0) {
            mmap_out->entries[mmap_out->length++] = *in;
            continue;
        }

        struct mmap_entry *const prev = &mmap_out->entries[mmap_out->length - 1];
        size_t remaining = max_entries - mmap_out->length;

        if (!insert_with_overlap(prev, &remaining, in)) {
            panic("too many mmap entries");
        }

        mmap_out->length = max_entries - remaining;
    }
}

STATIC_OR_TEST void refine_mmap(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *boot_map) {
    assert(boot_map && boot_map->entries, "boot_map or its entries is null");

    struct mmap_entry aligned_entries[MAX_MMAP_ENTRIES];
    struct mmap aligned_mmap = { .entries = aligned_entries };
    align_usables_mmap(&aligned_mmap, MAX_MMAP_ENTRIES, boot_map);

    sort(aligned_entries, aligned_mmap.length, sizeof(struct mmap_entry), entry_compare);

    remove_overlaps_mmap(mmap_out, max_entries, &aligned_mmap);
}

static void init_mm_section(void) {
    if (g_mmap.length == 0) {
        g_mm_sec.length = 0;
        return;
    }

    const phys_addr_t kernel_end = (phys_addr_t)__kernel_end_lba;

    uint32_t sec_len = 1;

    for (uint32_t i = 0; i < g_mmap.length; i++) {
        const struct mmap_entry *entry = &g_mmap.entries[i];

        if (g_mmap.entries[i].type != MMAP_ENTRY_USABLE) {
            continue;
        }

        phys_addr_t entry_start = entry->addr;
        const phys_addr_t entry_last = entry->addr + entry->len - 1;

        if (entry_last < kernel_end) {
            continue;
        }
        if (entry_start < kernel_end) {
            entry_start = kernel_end;
        }

        if (sec_len >= MAX_MM_SEC_ENTRIES) {
            panic("too many mmap entries");
        }

        g_mm_sec.entries[sec_len++] = (struct mmap_entry){
            .addr = entry_start,
            .len = entry_last - entry_start + 1,
            .type = MM_SEC_ENTRY_USABLE,
        };
    }

    g_mm_sec.entries[0] = (struct mmap_entry){
        .addr = g_mm_sec.entries[1].addr,
        .len = 0,
        .type = MM_SEC_ENTRY_METADATA,
    };

    g_mm_sec.length = sec_len;
}

void mm_map_init(void) {
    refine_mmap(&g_mmap, MAX_MMAP_ENTRIES, boot_get_mmap());
    init_mm_section();
}

// this function must not be called after paging is initialized.
phys_addr_t mm_sec_alloc_metadata(size_t max_pages, size_t *allocated_pages) {
    for (uint32_t i = 0; i + 1 < g_mm_sec.length; i++) {
        struct mmap_entry *const entry = &g_mm_sec.entries[i];
        struct mmap_entry *const next = &g_mm_sec.entries[i + 1];

        if (entry->type == MM_SEC_ENTRY_USABLE) {
            panic("mm_section is corrupted");
        }

        if (next->len < PAGE_SIZE) {
            next->type = MM_SEC_ENTRY_METADATA;
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

    panic("mm_section is already full of metadata page");
}

const struct mmap *mm_get_memory_map(void) {
    return &g_mmap;
}

const struct mmap *mm_get_section_map(void) {
    return &g_mm_sec;
}

const char *mm_sec_entry_type_str(mmap_entry_type_t type) {
    switch (type) {
        case MM_SEC_ENTRY_METADATA:
            return "Metadata";
        case MM_SEC_ENTRY_USABLE:
            return "Usable";
        default:
            return "(unknown)";
    }
}
