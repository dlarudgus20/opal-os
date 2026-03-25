#include <kc/assert.h>
#include <kc/string.h>

#include <opal/fs/block_device.h>
#include <opal/mm/kmalloc.h>
#include <opal/locks/irqlock.h>

#define MAX_DEVICES 64

static struct block_device g_devices[MAX_DEVICES];
static size_t g_devices_count = 0;

struct block_device *block_device_register(struct disk *disk, const char *name, fs_size_t offset, fs_size_t sectors) {
    irqlock_t irqlock = irqlock_acquire();

    const fs_size_t end = offset + sectors;
    assert(end > offset);
    assert(end <= disk->sectors);

    if (g_devices_count >= MAX_DEVICES) {
        irqlock_release(&irqlock);
        return false;
    }

    struct block_device *dev = &g_devices[g_devices_count++];
    *dev = (struct block_device){
        .disk = disk,
        .name = name,
        .offset = offset,
        .sectors = sectors,
    };

    irqlock_release(&irqlock);
    return dev;
}

void block_device_unregister(struct block_device *dev) {
    irqlock_t irqlock = irqlock_acquire();

    uintptr_t dev_i = (uintptr_t)dev;
    uintptr_t ar_i = (uintptr_t)g_devices;
    assert(ar_i <= dev_i && dev_i < ar_i + sizeof(g_devices));

    if (dev->offset > 0) {
        kfree((void *)dev->name, strlen(dev->name) + 1);
    }

    size_t idx = dev - g_devices;
    size_t count = g_devices_count - idx - 1;
    memmove(&g_devices[idx], &g_devices[idx + 1], count * sizeof(*g_devices));
    g_devices_count--;

    irqlock_release(&irqlock);
}

void block_device_unregister_partitions(struct disk *disk) {
    irqlock_t irqlock = irqlock_acquire();

    for (size_t i = 0; i < g_devices_count; ) {
        if (g_devices[i].disk == disk && g_devices[i].offset > 0) {
            block_device_unregister(&g_devices[i]);
            continue;
        }

        i++;
    }

    irqlock_release(&irqlock);
}

size_t bdev_list_count(void) {
    irqlock_t irqlock = irqlock_acquire();
    size_t count = g_devices_count;
    irqlock_release(&irqlock);
    return count;
}

struct block_device *bdev_list_get(size_t index) {
    irqlock_t irqlock = irqlock_acquire();

    if (index >= g_devices_count) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct block_device *dev = &g_devices[index];
    irqlock_release(&irqlock);
    return dev;
}

[[nodiscard]] static bool range_valid(struct block_device *dev, fs_size_t lba, fs_size_t sectors) {
    const fs_size_t end = lba + sectors;
    return end > lba && end <= dev->sectors;
}

struct disk_request *block_device_read(struct block_device *dev, fs_size_t lba, fs_size_t sectors, void *buffer) {
    if (!range_valid(dev, lba, sectors)) {
        return NULL;
    }

    return disk_read(dev->disk, lba + dev->offset, sectors, buffer);
}

struct disk_request *block_device_write(struct block_device *dev, fs_size_t lba, fs_size_t sectors, const void *buffer) {
    if (!range_valid(dev, lba, sectors)) {
        return NULL;
    }

    return disk_write(dev->disk, lba + dev->offset, sectors, buffer);
}
