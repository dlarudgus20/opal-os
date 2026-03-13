#include <opal/platform/boot/boot.h>
#include <opal/platform/boot/bootinfo.h>
#include <opal/platform/descriptors.h>

void boot_init(void) {
    bootinfo_init();
    descriptors_init();
}
