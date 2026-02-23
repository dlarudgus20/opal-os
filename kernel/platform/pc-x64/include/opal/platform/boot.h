#ifndef OPAL_PLATFORM_BOOT_BOOT_H
#define OPAL_PLATFORM_BOOT_BOOT_H

#define MAX_BOOT_CMDLINE 4096

#include <opal/mm/types.h>

struct mmap;

struct boot_fbinfo {
    phys_addr_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
};

void boot_info_init(void);

const struct mmap *boot_get_mmap(void);
const char *boot_get_cmdline(void);
const struct boot_fbinfo *boot_get_fbinfo(void);

#endif
