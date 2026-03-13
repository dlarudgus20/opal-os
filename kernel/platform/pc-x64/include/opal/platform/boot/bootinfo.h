#ifndef OPAL_PLATFORM_BOOT_BOOTINFO_H
#define OPAL_PLATFORM_BOOT_BOOTINFO_H

#define MAX_BOOT_CMDLINE 1024
#define MAX_BOOT_MODULE_NAME 64

#include <opal/mm/types.h>

struct mmap;

struct bootinfo_fb {
    phys_addr_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
};

struct bootinfo_module {
    phys_addr_t begin;
    phys_addr_t end;
    char name[MAX_BOOT_MODULE_NAME];
};

struct bootinfo_module_list {
    const struct bootinfo_module *modules;
    uint32_t len;
};

void bootinfo_init(void);

const char *bootinfo_get_cmdline(void);
const struct bootinfo_module_list *bootinfo_get_modules(void);
const struct mmap *bootinfo_get_mmap(void);
const struct bootinfo_fb *bootinfo_get_fb(void);

#endif
