#include <limits.h>

#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/fs/block_device.h>
#include <opal/mm/kmalloc.h>
#include <opal/locks/irqlock.h>

#define MAX_DEVICES INT32_MAX
#define MAX_REFC INT_MAX

static struct singlylist g_device_list = SINGLYLIST_EMPTY;
static size_t g_devices_count = 0;

struct block_device *block_device_create(struct disk *disk, const char *name, fs_size_t offset, fs_size_t sectors) {
    irqlock_t irqlock = irqlock_acquire();

    const fs_size_t end = offset + sectors;
    assert(end > offset);
    assert(end <= disk->sectors);

    if (g_devices_count >= MAX_DEVICES) {
        goto err;
    }

    struct block_device *dev = kzalloc(sizeof(*dev));
    if (!dev) {
        goto err;
    }

    *dev = (struct block_device){
        .disk = disk,
        .name = name,
        .offset = offset,
        .sectors = sectors,
        .refcount = 0,
    };
    singlylist_push_front(&g_device_list, &dev->link);
    g_devices_count++;

    irqlock_release(&irqlock);
    return dev;

err:
    irqlock_release(&irqlock);
    return NULL;
}

bool block_device_destroy(struct block_device *dev) {
    irqlock_t irqlock = irqlock_acquire();

    if (dev->refcount != 1) {
        irqlock_release(&irqlock);
        return false;
    }

    singlylist_foreach_2(before, ptr, &g_device_list) {
        if (ptr == &dev->link) {
            singlylist_remove_after(before);
            kfree(dev, sizeof(*dev));
            g_devices_count--;
            irqlock_release(&irqlock);
            return true;
        }
    }

    panic("invalid block device to destroy");
}

bool block_device_retain(struct block_device *dev) {
    irqlock_t irqlock = irqlock_acquire();
    assert(dev->refcount < MAX_REFC);
    dev->refcount++;
    irqlock_release(&irqlock);
    return true;
}

void block_device_release(struct block_device *dev) {
    irqlock_t irqlock = irqlock_acquire();
    assert(dev->refcount > 0);
    dev->refcount--;
    irqlock_release(&irqlock);
}

size_t bdev_list_count(void) {
    irqlock_t irqlock = irqlock_acquire();
    size_t count = g_devices_count;
    irqlock_release(&irqlock);
    return count;
}

fs_status_t bdev_list_get(size_t index, struct block_device **dev_out) {
    irqlock_t irqlock = irqlock_acquire();

    if (index >= g_devices_count) {
        irqlock_release(&irqlock);
        return FS_ERR_NOENT;
    }

    size_t rindex = g_devices_count - index - 1;
    singlylist_foreach(ptr, &g_device_list) {
        if (rindex > 0) {
            rindex--;
            continue;
        }

        struct block_device *dev = container_of(ptr, struct block_device, link);
        assert(dev->refcount < MAX_REFC);
        dev->refcount++;
        irqlock_release(&irqlock);
        *dev_out = dev;
        return FS_OK;
    }

    panic("unreachable!");
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
