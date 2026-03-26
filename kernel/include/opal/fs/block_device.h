#ifndef OPAL_FS_BLOCK_DEVICE_H
#define OPAL_FS_BLOCK_DEVICE_H

#include <collections/singlylist.h>

#include <opal/dynarray.h>
#include <opal/fs/disk.h>

struct block_device {
    struct singlylist_link link;
    struct disk* disk;
    const char *name;
    fs_size_t offset;
    fs_size_t sectors;
    int refcount;
};

[[nodiscard]] struct block_device *block_device_create(struct disk *disk, const char *name, fs_size_t offset, fs_size_t sectors);
bool block_device_destroy(struct block_device *dev);

bool block_device_retain(struct block_device *dev);
void block_device_release(struct block_device *dev);
bool block_device_retain_exclusive(struct block_device *dev);
void block_device_release_exclusive(struct block_device *dev);
[[nodiscard]] size_t bdev_list_count(void);
[[nodiscard]] fs_status_t bdev_list_get(size_t index, struct block_device **dev_out);

struct disk_request *block_device_read(struct block_device *dev, fs_size_t lba, fs_size_t sectors, void *buffer);
struct disk_request *block_device_write(struct block_device *dev, fs_size_t lba, fs_size_t sectors, const void *buffer);

#endif
