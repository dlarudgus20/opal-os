#ifndef OPAL_PLATFORM_BOOT_BOOTINFO_H
#define OPAL_PLATFORM_BOOT_BOOTINFO_H

#define MAX_BOOT_CMDLINE 4096

#include <opal/mm/types.h>

struct mmap;

struct bootinfo_fb {
    phys_addr_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
};

void bootinfo_init(void);

const struct mmap *bootinfo_get_mmap(void);
const char *bootinfo_get_cmdline(void);
const struct bootinfo_fb *bootinfo_get_fb(void);

#endif
