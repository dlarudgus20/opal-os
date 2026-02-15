#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <kc/stdlib.h>

#include <opal/test.h>
#include <opal/mm/map.h>
#include <opal/platform/boot/boot.h>

#define MM_BOOT_MAP_MAX_ENTRIES 128

static struct mmap_entry g_sanitized_entries[MM_BOOT_MAP_MAX_ENTRIES];
static struct mmap g_sanitized_map = {
    .entries = g_sanitized_entries,
    .length = 0,
};

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

STATIC_OR_TEST void boot_map_sanitize(struct mmap *mmap_out, uint32_t max_entries, const struct mmap *boot_map) {
    uint32_t filtered_len = 0;
    uint32_t out_len = 0;

    mmap_out->length = 0;
    if (boot_map == NULL || boot_map->entries == NULL || max_entries == 0) {
        return;
    }

    for (uint32_t i = 0; i < boot_map->length; i++) {
        const struct mmap_entry *entry = &boot_map->entries[i];
        const phys_addr_t start_aligned = align_ceil_sz_p2(entry->addr, PAGE_SIZE);

        if (start_aligned < entry->addr) {
            continue;
        }
        if (entry->type != MMAP_ENTRY_USABLE) {
            continue;
        }
        if (entry->len == 0) {
            continue;
        }
        if (filtered_len >= max_entries) {
            break;
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

        mmap_out->entries[filtered_len++] = (struct mmap_entry){
            .addr = start_aligned,
            .len = last_aligned - start_aligned + 1,
            .type = MMAP_ENTRY_USABLE,
        };
    }

    sort(mmap_out->entries, filtered_len, sizeof(mmap_out->entries[0]), mmap_entry_compare);

    for (uint32_t i = 0; i < filtered_len; i++) {
        phys_addr_t seg_start = mmap_out->entries[i].addr;
        phys_addr_t seg_last = entry_last(&mmap_out->entries[i]);

        if (seg_last < seg_start) {
            continue;
        }

        if (out_len > 0) {
            struct mmap_entry *prev = &mmap_out->entries[out_len - 1];
            phys_addr_t prev_last = entry_last(prev);

            if (prev_last == PHYS_ADDR_MAX || seg_start <= prev_last + 1) {
                if (seg_last > prev_last) {
                    prev->len += seg_last - prev_last;
                }
                continue;
            }
        }

        if (out_len >= max_entries) {
            break;
        }

        mmap_out->entries[out_len++] = (struct mmap_entry){
            .addr = seg_start,
            .len = seg_last - seg_start + 1,
            .type = MMAP_ENTRY_USABLE,
        };
    }

    mmap_out->length = out_len;
}

const struct mmap *mm_get_boot_map(void) {
    return boot_get_mmap();
}

const struct mmap *mm_get_sanitized_map(void) {
    return &g_sanitized_map;
}

void mm_map_init(void) {
    boot_map_sanitize(&g_sanitized_map, MM_BOOT_MAP_MAX_ENTRIES, boot_get_mmap());
}
