#include <limits.h>

#include <kc/kassert.h>
#include <kc/string.h>
#include <kc/stdlib.h>

#include <opal/tty.h>
#include <opal/fs/block_device.h>
#include <opal/fs/disk.h>
#include <opal/utils/dynarray.h>
#include <opal/locks/irqlock.h>

static struct disk *parse_disk_arg(const char *cmd, const char *arg, unsigned long *disk_ul_out) {
    unsigned long disk_ul = 0;
    if (!kerrno_ok(kstrtoul_exact(arg, 10, ULONG_MAX, &disk_ul))) {
        tty0_printf("%s: invalid disk\n", cmd);
        return NULL;
    }

    struct disk *disk = disk_list_get((size_t)disk_ul);
    if (!disk) {
        size_t count = disk_list_count();
        tty0_printf(
            "%s: invalid disk %lu (expected 0..%zu)\n", cmd, disk_ul, count ? count - 1 : 0);
        return NULL;
    }

    if (disk_ul_out) {
        *disk_ul_out = disk_ul;
    }

    return disk;
}

int shell_cmd_diskreset(int argc, char **argv) {
    unsigned long disk_ul = 0;
    char answer[16];

    if (argc != 2) {
        tty0_puts("usage: diskreset [disk]\n");
        return 1;
    }

    struct disk *disk = parse_disk_arg("diskreset", argv[1], &disk_ul);
    if (!disk) {
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
    if (!kerrno_ok(comp.result)) {
        tty0_printf("diskreset: error %s (%d)\n", kerrno_str(comp.result), comp.result);
        return 1;
    }

    tty0_puts("diskreset: done\n");
    return 0;
}

int shell_cmd_diskrescan(int argc, char **argv) {
    unsigned long disk_ul = 0;

    if (argc != 2) {
        tty0_puts("usage: diskrescan [disk]\n");
        return 1;
    }

    struct disk *disk = parse_disk_arg("diskrescan", argv[1], &disk_ul);
    if (!disk) {
        return 1;
    }

    struct fs_completion comp;
    fs_completion_init(&comp);
    disk_rescan_partition(disk, &comp);
    fs_completion_wait(&comp, TIMEOUT_INFINITY);
    if (!kerrno_ok(comp.result)) {
        tty0_printf("diskrescan: error %s (%d)\n", kerrno_str(comp.result), comp.result);
        return 1;
    }

    tty0_printf("diskrescan: done (%s)\n", disk->name ? disk->name : "-");
    return 0;
}

static int lspart_print_disk(size_t disk_idx, struct disk *disk) {
    int ret = 0;

    struct dynarray snapshot;
    dynarray_init(&snapshot);

    irqlock_t irqlock = irqlock_acquire();

    size_t total_size = disk->partitions.size;
    if (total_size > 0 && !dynarray_resize(&snapshot, total_size)) {
        irqlock_release(&irqlock);
        tty0_puts("lspart: out of memory\n");
        return 1;
    }

    memcpy(snapshot.data, disk->partitions.data, total_size);

    struct partition_entry *begin = snapshot.data;
    struct partition_entry *entry = begin;
    if (snapshot.size > 0) {
        struct partition_entry *end = begin + snapshot.size / sizeof(*end);
        for (; entry < end; entry++) {
            if (!block_device_retain(entry->bdev)) {
                tty0_puts("lspart: failed to retain block device\n");
                irqlock_release(&irqlock);
                ret = -1;
                goto exit;
            }
        }
    }

    irqlock_release(&irqlock);

    tty0_printf("disk %zu (%s)\n", disk_idx, disk->name ? disk->name : "-");
    tty0_printf("  %-6s %-10s %-10s %s\n", "part", "name", "lba", "sectors");

    if (snapshot.size == 0) {
        tty0_puts("  (no partitions)\n");
        goto exit;
    }

    dynarray_foreach(struct partition_entry *, entry, &snapshot) {
        struct block_device *dev = entry->bdev;
        const char *name = dev->name ? dev->name : "-";
        tty0_printf("  %-6zu %-10s %-10zu %zu\n", entry->index, name, dev->offset, dev->sectors);
    }

    ret = 0;

exit:
    if (snapshot.size > 0) {
        for (struct partition_entry *p = begin; p < entry; p++) {
            block_device_release(p->bdev);
        }
    }

    dynarray_destroy(&snapshot);
    return ret;
}

int shell_cmd_lspart(int argc, char **argv) {
    unsigned long disk_ul = 0;

    if (argc != 1 && argc != 2) {
        tty0_puts("usage: lspart [disk]\n");
        return 1;
    }

    if (argc == 2) {
        struct disk *disk = parse_disk_arg("lspart", argv[1], &disk_ul);
        if (!disk) {
            return 1;
        }

        return lspart_print_disk((size_t)disk_ul, disk);
    }

    size_t count = disk_list_count();
    if (count == 0) {
        tty0_puts("(no disks)\n");
        return 0;
    }

    int ret = 0;
    for (size_t i = 0; i < count; i++) {
        struct disk *disk = disk_list_get(i);
        if (!disk) {
            continue;
        }

        if (lspart_print_disk(i, disk) != 0) {
            ret = 1;
        }
    }

    return ret;
}

int shell_cmd_mkpart(int argc, char **argv) {
    unsigned long disk_ul = 0;
    unsigned long part_ul = 0;
    unsigned long lba_ul = 0;
    unsigned long sectors_ul = 0;
    unsigned long type_ul = 0;

    if (argc != 6 || !parse_disk_arg("mkpart", argv[1], &disk_ul)
        || !kerrno_ok(kstrtoul_exact(argv[2], 10, ULONG_MAX, &part_ul))
        || !kerrno_ok(kstrtoul_exact(argv[3], 10, ULONG_MAX, &lba_ul))
        || !kerrno_ok(kstrtoul_exact(argv[4], 10, ULONG_MAX, &sectors_ul))
        || !kerrno_ok(kstrtoul_exact(argv[5], 0, ULONG_MAX, &type_ul))) {
        tty0_puts("usage: mkpart [disk] [part] [lba] [sectors] [type]\n");
        return 1;
    }

    if (type_ul > UINT8_MAX) {
        tty0_printf("mkpart: invalid type %lu (expected 0..255)\n", type_ul);
        return 1;
    }

    struct disk *disk = disk_list_get((size_t)disk_ul);
    kassert(disk);

    struct fs_completion comp;
    fs_completion_init(&comp);
    disk_create_partition(disk, (size_t)part_ul, lba_ul, sectors_ul, (uint8_t)type_ul, &comp);
    fs_completion_wait(&comp, TIMEOUT_INFINITY);
    if (!kerrno_ok(comp.result)) {
        tty0_printf("mkpart: error %s (%d)\n", kerrno_str(comp.result), comp.result);
        return 1;
    }

    tty0_printf("mkpart: done (%s:%lu)\n", disk->name ? disk->name : "-", part_ul);
    return 0;
}

int shell_cmd_rmpart(int argc, char **argv) {
    unsigned long disk_ul = 0;
    unsigned long part_ul = 0;

    if (argc != 3 || !parse_disk_arg("rmpart", argv[1], &disk_ul)
        || !kerrno_ok(kstrtoul_exact(argv[2], 10, ULONG_MAX, &part_ul))) {
        tty0_puts("usage: rmpart [disk] [part]\n");
        return 1;
    }

    struct disk *disk = disk_list_get((size_t)disk_ul);
    kassert(disk);

    struct fs_completion comp;
    fs_completion_init(&comp);
    disk_remove_partition(disk, (size_t)part_ul, &comp);
    fs_completion_wait(&comp, TIMEOUT_INFINITY);
    if (!kerrno_ok(comp.result)) {
        tty0_printf("rmpart: error %s (%d)\n", kerrno_str(comp.result), comp.result);
        return 1;
    }

    tty0_printf("rmpart: done (%s:%lu)\n", disk->name ? disk->name : "-", part_ul);
    return 0;
}
