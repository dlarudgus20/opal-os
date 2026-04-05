#ifndef OPAL_FS_FAT_H
#define OPAL_FS_FAT_H

#include <opal/fs/block_device.h>

fs_status_t fat_mount(struct block_device *bdev, struct superblock **sb_out);
fs_status_t fat_format(struct block_device *bdev, struct superblock **sb_out);

#endif
