#include <stdint.h>

#include <kc/stdlib.h>

#include <opal/platform/boot/boot.h>
#include <opal/mm/map.h>
#include <opal/kmain.h>

static struct mmap_entry g_mmap_entries[MAX_BOOT_MMAP_ENTRIES];
static struct mmap g_boot_mmap = {
    .entries = g_mmap_entries,
    .length = 0
};

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_mmap_tag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
};

static void parse_mb2_mmap(const struct mb2_mmap_tag *mmap) {
    const uint8_t *entry_ptr = (const uint8_t *)mmap + sizeof(*mmap);
    const uint8_t *entry_end = (const uint8_t *)mmap + mmap->size;

    if (mmap->entry_size < sizeof(struct mb2_mmap_entry)) {
        // panic("mb2: mmap entry_size too small\n");
        return;
    }

    g_boot_mmap.length = 0;

    while (entry_ptr + mmap->entry_size <= entry_end) {
        const struct mb2_mmap_entry *entry = (const struct mb2_mmap_entry *)entry_ptr;

        if (g_boot_mmap.length >= MAX_BOOT_MMAP_ENTRIES) {
            // log("mb2: too many mmap entries, some entries are ignored\n");
            break;
        }

        g_mmap_entries[g_boot_mmap.length++] = (struct mmap_entry){
            .addr = entry->addr,
            .len = entry->len,
            .type = entry->type
        };

        entry_ptr += mmap->entry_size;
    }
}

static void parse_mb2_info(uint32_t mb2_info_lba) {
    const uint8_t *base = (const uint8_t *)(uintptr_t)mb2_info_lba;

    const uint32_t total_size = *(const uint32_t *)base;
    if (total_size < 8) {
        // panic("mb2: invalid total_size\n");
        return;
    }

    for (uint32_t off = 8; off + 8 <= total_size; ) {
        const struct mb2_tag *tag = (const struct mb2_tag *)(base + off);
        if (tag->size < 8) {
            // panic("mb2: invalid tag size\n");
            return;
        }

        if (tag->type == 0) {
            break;
        }

        const uint32_t next_off = align_ceil_u32_p2(off + tag->size, 8);
        if (next_off <= off || next_off > total_size) {
            // panic("mb2: invalid tag bounds\n");
            return;
        }

        if (tag->type == 6) {
            parse_mb2_mmap((const struct mb2_mmap_tag *)tag);
        }

        off = next_off;
    }

    if (g_boot_mmap.length == 0) {
        // panic("mb2: no mmap found\n");
    }
}

void kmain_platform(uint32_t mb2_info_lba) {
    parse_mb2_info(mb2_info_lba);
    kmain();
}

const struct mmap *boot_get_mmap(void) {
    return &g_boot_mmap;
}
