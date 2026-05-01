#include <opal/fs/block_device.h>
#include <opal/tty.h>

int shell_cmd_lsblk(int argc, char **argv) {
    (void)argv;

    if (argc != 1) {
        tty0_puts("usage: lsblk\n");
        return 1;
    }

    size_t count = bdev_list_count();
    tty0_printf("%-5s %-8s %s\n", "index", "name", "sectors");

    for (size_t i = 0; i < count; i++) {
        struct block_device *dev;
        if (bdev_list_get(i, &dev) != OPAL_OK) {
            continue;
        }
        const char *name = dev->name ? dev->name : "-";

        tty0_printf("%-5zu %-8s %zu\n", i, name, dev->sectors);
        block_device_release(dev);
    }

    return 0;
}
