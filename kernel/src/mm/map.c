#include <opal/platform/boot/boot.h>

const struct mmap *mm_get_boot_map(void) {
    return boot_get_mmap();
}
