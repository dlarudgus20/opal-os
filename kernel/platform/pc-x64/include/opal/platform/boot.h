#ifndef OPAL_PLATFORM_BOOT_BOOT_H
#define OPAL_PLATFORM_BOOT_BOOT_H

#define MAX_BOOT_CMDLINE 4096

struct mmap;

void boot_info_init(void);

const struct mmap *boot_get_mmap(void);
const char *boot_get_cmdline(void);

#endif
