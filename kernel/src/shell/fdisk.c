#include <limits.h>

#include <kc/string.h>
#include <kc/stdlib.h>

#include <opal/tty.h>
#include <opal/fs/disk.h>

int shell_cmd_diskreset(int argc, char **argv) {
    unsigned long disk_ul = 0;
    char answer[16];

    if (argc != 2 || kstrtoul_exact(argv[1], 10, ULONG_MAX, &disk_ul) != KE_OK) {
        tty0_puts("usage: diskreset [disk]\n");
        return 1;
    }

    struct disk *disk = disk_list_get((size_t)disk_ul);
    if (!disk) {
        size_t count = disk_list_count();
        tty0_printf("diskreset: invalid disk %lu (expected 0..%zu)\n",
            disk_ul, count ? count - 1 : 0);
        return 1;
    }

    tty0_printf("diskreset: this will reset partition table on disk %lu (%s). continue? [y/yes]: ",
        disk_ul, disk->name ? disk->name : "-");
    tty0_getline(answer, sizeof(answer));

    if (!(strcmp(answer, "y") == 0 || strcmp(answer, "yes") == 0)) {
        tty0_puts("diskreset: cancelled\n");
        return 1;
    }

    struct fs_completion comp;
    fs_completion_init(&comp);
    disk_reset_partition(disk, &comp);
    fs_completion_wait(&comp, TIMEOUT_INFINITY);
    if (comp.result == FS_OK) {
        tty0_puts("diskreset: done\n");
        return 0;
    } else {
        tty0_printf("diskreset: error %d\n", comp.result);
        return 1;
    }
}
