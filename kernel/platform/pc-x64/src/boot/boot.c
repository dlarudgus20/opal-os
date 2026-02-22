#include <stdint.h>

#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/platform/boot.h>
#include <opal/mm/map.h>
#include <opal/kmain.h>

#include <opal/platform/boot/multiboot2.h>

#ifndef OPAL_TEST
// boot.asm
extern uint32_t g_mb2_info;
#else
static uint32_t g_mb2_info = 0;
#endif

static struct mmap_entry g_mmap_entries[MAX_MMAP_ENTRIES];
static struct mmap g_boot_mmap = {
    .entries = g_mmap_entries,
    .length = 0
};

static char g_cmdline[MAX_BOOT_CMDLINE];

static void parse_mb2_cmdline(const struct mb_tag_string *strtag) {
    size_t maxsz = strtag->tag.size - offsetof(struct mb_tag_string, string);
    if (maxsz == 0) {
        g_cmdline[0] = '\0';
        return;
    }
    if (maxsz > sizeof(g_cmdline)) {
        maxsz = sizeof(g_cmdline);
    }
    strncpy_s(g_cmdline, sizeof(g_cmdline), strtag->string, maxsz - 1);
}

static void parse_mb2_mmap(const struct mb_tag_mmap *mmap) {
    const char *entry_ptr = (const char *)mmap + sizeof(*mmap);
    const char *entry_end = (const char *)mmap + mmap->tag.size;

    if (mmap->entry_size < sizeof(struct mb_mmap_entry)) {
        // panic("mb2: mmap entry_size too small\n");
        return;
    }

    g_boot_mmap.length = 0;

    while (entry_ptr + mmap->entry_size <= entry_end) {
        const struct mb_mmap_entry *entry = (const struct mb_mmap_entry *)entry_ptr;

        if (g_boot_mmap.length >= MAX_MMAP_ENTRIES) {
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

static void parse_mb2_info(uint32_t mb2_info_pa) {
    const uint8_t *base = (const uint8_t *)(uintptr_t)mb2_info_pa;

    const uint32_t total_size = *(const uint32_t *)base;
    if (total_size < 8) {
        // panic("mb2: invalid total_size\n");
        return;
    }

    for (uint32_t off = 8; off + 8 <= total_size; ) {
        const struct mb_tag *tag = (const struct mb_tag *)(base + off);
        if (tag->size < sizeof(*tag)) {
            // panic("mb2: invalid tag size\n");
            return;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        const uint32_t next_off = align_ceil_u32_p2(off + tag->size, MULTIBOOT_TAG_ALIGN);
        if (next_off <= off || next_off > total_size) {
            // panic("mb2: invalid tag bounds\n");
            return;
        }

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                parse_mb2_cmdline((const struct mb_tag_string *)tag);
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                parse_mb2_mmap((const struct mb_tag_mmap *)tag);
                break;
        }

        off = next_off;
    }
}

void boot_info_init(void) {
    parse_mb2_info(g_mb2_info);
}

const struct mmap *boot_get_mmap(void) {
    return &g_boot_mmap;
}

const char *boot_get_cmdline(void) {
    return g_cmdline;
}
