#include <stddef.h>
#include <stdint.h>

#include <kc/stdlib.h>

#include <opal/test.h>
#include <opal/klog.h>
#include <opal/kargs.h>
#include <opal/mm/map.h>
#include <opal/mm/tmpalloc.h>
#include <opal/platform/mm/defines.h>
#include <opal/platform/boot/bootinfo.h>

static struct mmap_entry g_mmap_entries[MAX_MMAP_ENTRIES];
static struct mmap g_mmap = {
    .entries = g_mmap_entries,
    .length = 0,
};

static struct mmap_entry g_mm_sec_entries[MAX_MMAP_ENTRIES];
static struct mmap g_mm_sec = {
    .entries = g_mm_sec_entries,
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

static void append_section(struct mmap *sec, phys_addr_t start, phys_addr_t last, mmap_entry_type_t type) {
    if (sec->length >= MAX_MMAP_ENTRIES) {
        panic("too many mmap entries");
    }

    sec->entries[sec->length++] = (struct mmap_entry){
        .addr = start,
        .len = last - start + 1,
        .type = type,
    };
}

STATIC_OR_TEST void init_mm_section(
    struct mmap *sec,
    const struct mmap *mmap,
    phys_addr_t section_start,
    phys_addr_t reserved_start,
    phys_addr_t reserved_end
) {
    sec->length = 0;

    for (uint32_t i = 0; i < mmap->length; i++) {
        const struct mmap_entry *entry = &mmap->entries[i];

        if (entry->type != MMAP_ENTRY_USABLE) {
            continue;
        }

        phys_addr_t entry_start = entry->addr;
        const phys_addr_t entry_last = entry->addr + entry->len - 1;

        if (entry_last < section_start) {
            continue;
        }
        if (entry_start < section_start) {
            entry_start = section_start;
        }

        if (reserved_end <= entry_start || entry_last < reserved_start) {
            append_section(sec, entry_start, entry_last, MM_SEC_ENTRY_USABLE);
            continue;
        }

        if (entry_start < reserved_start) {
            append_section(sec, entry_start, reserved_start - 1, MM_SEC_ENTRY_USABLE);
        }

        const phys_addr_t reserved_last = reserved_end - 1;
        const phys_addr_t overlap_start = MAX(entry_start, reserved_start);
        const phys_addr_t overlap_last = MIN(entry_last, reserved_last);
        append_section(sec, overlap_start, overlap_last, MM_SEC_ENTRY_RESERVED);

        if (reserved_last < entry_last) {
            append_section(sec, reserved_last + 1, entry_last, MM_SEC_ENTRY_USABLE);
        }
    }
}

static void get_initramfs_reserved_range(phys_addr_t *start_out, phys_addr_t *end_out) {
    *start_out = 0;
    *end_out = 0;

    const struct kargs *kargs = kargs_get();
    if (!kargs->initramfs) {
        return;
    }

    const phys_addr_t module_begin = kargs->initramfs->begin;
    const phys_addr_t module_end = kargs->initramfs->end;

    if (module_begin >= module_end) {
        kwarn("mm: invalid initramfs range [%#018"PRIphys", %#018"PRIphys") is ignored",
            module_begin, module_end);
        return;
    }

    const phys_addr_t start = align_floor_sz_p2(module_begin, PAGE_SIZE);
    const phys_addr_t end = align_ceil_sz_p2(module_end, PAGE_SIZE);

    if (end < module_end || end <= start) {
        kwarn("mm: failed to align initramfs range [%#018"PRIphys", %#018"PRIphys")",
            module_begin, module_end);
        return;
    }

    *start_out = start;
    *end_out = end;
}

void mm_map_init(void) {
    phys_addr_t reserved_start;
    phys_addr_t reserved_end;
    get_initramfs_reserved_range(&reserved_start, &reserved_end);
    if (reserved_start < reserved_end) {
        kinfo("mm: reserved initramfs pages [%#018"PRIphys", %#018"PRIphys")",
            reserved_start, reserved_end);
    }

    refine_mmap(&g_mmap, MAX_MMAP_ENTRIES, bootinfo_get_mmap());
    init_mm_section(&g_mm_sec, &g_mmap, (phys_addr_t)__kernel_end_lba, reserved_start, reserved_end);
}

void mm_map_finalize_tmpalloc(struct tmpalloc *ta) {
    assert(ta->mm.entries, "tmp_alloc is already finalized");

    memcpy(g_mm_sec.entries, ta->mm.entries, ta->mm.length * sizeof(struct mmap_entry));
    g_mm_sec.length = ta->mm.length;
    ta->mm.entries = NULL;
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
        case MM_SEC_ENTRY_RESERVED:
            return "Reserved";
        case MM_SEC_ENTRY_USABLE:
            return "Usable";
        default:
            return "(unknown)";
    }
}
