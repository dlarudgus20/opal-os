#include <stdint.h>

#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/klog.h>
#include <opal/kmain.h>
#include <opal/mm/map.h>
#include <opal/platform/boot/bootinfo.h>

#include "multiboot2.h"

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

static struct bootinfo_fb g_fbinfo;

static void parse_mb2_cmdline(const struct mb_tag_string *strtag) {
    if (g_cmdline[0] != '\0') {
        kerror("multiboot: duplicated boot cmdline is ignored");
        return;
    }

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
    if (g_boot_mmap.length > 0) {
        kerror("multiboot: duplicated boot mmap is ignored");
        return;
    }

    const char *entry_ptr = (const char *)mmap + sizeof(*mmap);
    const char *entry_end = (const char *)mmap + mmap->tag.size;

    if (mmap->entry_size < sizeof(struct mb_mmap_entry)) {
        kerror("multiboot: boot mmap entry_size too small");
        return;
    }

    g_boot_mmap.length = 0;

    while (entry_ptr + mmap->entry_size <= entry_end) {
        const struct mb_mmap_entry *entry = (const struct mb_mmap_entry *)entry_ptr;

        if (g_boot_mmap.length >= MAX_MMAP_ENTRIES) {
            kerror("multiboot: too many boot mmap entries, some entries are ignored");
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

static bool is_supported_rgb(const struct mb_tag_framebuffer_direct *fb) {
    if (fb->fb.framebuffer_bpp != 32) {
        return false;
    } else if (fb->framebuffer_red_field_position != 16) {
        return false;
    } else if (fb->framebuffer_red_mask_size != 8) {
        return false;
    } else if (fb->framebuffer_green_field_position != 8) {
        return false;
    } else if (fb->framebuffer_green_mask_size != 8) {
        return false;
    } else if (fb->framebuffer_blue_field_position != 0) {
        return false;
    } else if (fb->framebuffer_blue_mask_size != 8) {
        return false;
    }
    return true;
}

static void parse_mb2_fb(const struct mb_tag_framebuffer *fb) {
    if (g_fbinfo.addr != 0) {
        kerror("multiboot: duplicated boot fbinfo is ignored");
        return;
    }

    const struct mb_tag_framebuffer_direct *direct = NULL;

    if (fb->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        direct = (const struct mb_tag_framebuffer_direct *)fb;
        if (!is_supported_rgb(direct)) {
            kwarn("multiboot: unsupported framebuffer");
            return;
        }
    } else {
        kwarn("multiboot: unsupported framebuffer");
        return;
    }

    g_fbinfo.addr = fb->framebuffer_addr;
    g_fbinfo.pitch = fb->framebuffer_pitch;
    g_fbinfo.width = fb->framebuffer_width;
    g_fbinfo.height = fb->framebuffer_height;
    g_fbinfo.bpp = fb->framebuffer_bpp;
}

static void parse_mb2_info(uint32_t mb2_info_pa) {
    const uint8_t *base = (const uint8_t *)(uintptr_t)mb2_info_pa;

    const uint32_t total_size = *(const uint32_t *)base;
    if (total_size < 8) {
        kerror("multiboot: invalid total_size");
        return;
    }

    for (uint32_t off = 8; off + 8 <= total_size; ) {
        const struct mb_tag *tag = (const struct mb_tag *)(base + off);
        if (tag->size < sizeof(*tag)) {
            kerror("multiboot: invalid tag size");
            return;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        const uint32_t next_off = align_ceil_u32_p2(off + tag->size, MULTIBOOT_TAG_ALIGN);
        if (next_off <= off || next_off > total_size) {
            kerror("multiboot: invalid tag bounds");
            return;
        }

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                parse_mb2_cmdline((const struct mb_tag_string *)tag);
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                parse_mb2_mmap((const struct mb_tag_mmap *)tag);
                break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                parse_mb2_fb((const struct mb_tag_framebuffer *)tag);
                break;
        }

        off = next_off;
    }
}

void bootinfo_init(void) {
    parse_mb2_info(g_mb2_info);
    kinfo("boot args=%s", g_cmdline);
}

const struct mmap *bootinfo_get_mmap(void) {
    return &g_boot_mmap;
}

const char *bootinfo_get_cmdline(void) {
    return g_cmdline;
}

const struct bootinfo_fb *bootinfo_get_fb(void) {
    if (g_fbinfo.addr == 0) {
        return NULL;
    }
    return &g_fbinfo;
}
