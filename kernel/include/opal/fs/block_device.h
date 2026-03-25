#ifndef OPAL_FS_BLOCK_DEVICE_H
#define OPAL_FS_BLOCK_DEVICE_H

#include <opal/fs/disk.h>

struct block_device {
    struct disk* disk;
    const char *name;
    fs_size_t offset;
    fs_size_t sectors;
};

struct block_device *block_device_register(struct disk *disk, const char *name, fs_size_t offset, fs_size_t sectors);
void block_device_unregister(struct block_device *dev);
void block_device_unregister_partitions(struct disk *disk);

[[nodiscard]] size_t bdev_list_count(void);
[[nodiscard]] struct block_device *bdev_list_get(size_t index);

struct disk_request *block_device_read(struct block_device *dev, fs_size_t lba, fs_size_t sectors, void *buffer);
struct disk_request *block_device_write(struct block_device *dev, fs_size_t lba, fs_size_t sectors, const void *buffer);

#endif
