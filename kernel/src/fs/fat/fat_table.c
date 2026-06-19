#include <kc/kassert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/mm/kmalloc.h>

#include "fat.h"

static struct fat_table_ops g_fat12_ops;
static struct fat_table_ops g_fat16_ops;
static struct fat_table_ops g_fat32_ops;

void fat_table_init(struct fat_table *file, struct fat_sb *sb) {
    struct file_ops *ops;
    if (sb->layout.bits == 12) {
        ops = &g_fat12_ops.ops;
    } else if (sb->layout.bits == 16) {
        ops = &g_fat16_ops.ops;
    } else {
        ops = &g_fat32_ops.ops;
    }

    file_init(&file->file, ops, FILE_READ | FILE_WRITE);
    file->sb = sb;
    file->buffer = NULL;
}

static void fat_table_close(struct file *base) {
    struct fat_table *file = container_of(base, struct fat_table, file);
    if (file->buffer) {
        uint32_t fat_bytes = file->sb->layout.fat_sectors * file->sb->layout.bytes_per_sector;
        kfree(file->buffer, fat_bytes);
    }
}

static fs_ssize_t fat_table_seek(struct file *, fs_off_t, enum fs_seek) {
    return OPAL_ENOTSUPP;
}

static kerrno_t fat_table_readall(struct fat_table *file) {
    if (file->buffer) {
        return OPAL_OK;
    }

    uint32_t fat_bytes = file->sb->layout.fat_sectors * file->sb->layout.bytes_per_sector;
    uint32_t fat_sectors = fat_bytes / DISK_SECTOR_SIZE;
    file->buffer = kzalloc(fat_bytes);
    if (!file->buffer) {
        return OPAL_ENOMEM;
    }

    uint32_t lsec = file->sb->layout.bytes_per_sector / DISK_SECTOR_SIZE;
    uint32_t fat_offset = file->sb->layout.reserved_sectors * lsec;
    kerrno_t result = fat_read_sectors(file->sb->bdev, fat_offset, fat_sectors, file->buffer);
    if (!kerrno_ok(result)) {
        kfree(file->buffer, fat_bytes);
        file->buffer = NULL;
    }
    return result;
}

static fs_ssize_t fat_table_read(struct file *base, fs_size_t *pos, void *buffer, fs_size_t size) {
    struct fat_table *file = container_of(base, struct fat_table, file);

    uint32_t fat_bytes = file->sb->layout.fat_sectors * file->sb->layout.bytes_per_sector;
    if (*pos > fat_bytes) {
        return OPAL_ERANGE;
    }
    if (size > (fs_size_t)(fat_bytes - *pos)) {
        size = fat_bytes - *pos;
    }

    if (size == 0) {
        return 0;
    }

    kerrno_t result = fat_table_readall(file);
    if (!kerrno_ok(result)) {
        return result;
    }

    memcpy(buffer, file->buffer + *pos, size);
    *pos += size;
    return (fs_ssize_t)size;
}

static fs_ssize_t fat_table_write(
    struct file *base, fs_size_t *pos, const void *buffer, fs_size_t size) {
    struct fat_table *file = container_of(base, struct fat_table, file);
    struct fat_sb *sb = file->sb;

    if (size > FS_SSIZE_MAX) {
        return OPAL_EINVAL;
    }

    uint32_t fat_bytes = file->sb->layout.fat_sectors * file->sb->layout.bytes_per_sector;
    uint32_t fat_sectors = fat_bytes / DISK_SECTOR_SIZE;
    if (*pos > fat_bytes) {
        return OPAL_ERANGE;
    }
    if (size > fat_bytes - *pos) {
        size = fat_bytes - *pos;
    }

    if (size == 0) {
        return 0;
    }

    kerrno_t result = fat_table_readall(file);
    if (!kerrno_ok(result)) {
        return result;
    }

    memcpy(file->buffer + *pos, buffer, size);

    uint32_t lsec = file->sb->layout.bytes_per_sector / DISK_SECTOR_SIZE;
    uint32_t fat_offset = file->sb->layout.reserved_sectors * lsec;

    fs_size_t sector_pos = *pos / DISK_SECTOR_SIZE;
    fs_size_t front_pos = sector_pos * DISK_SECTOR_SIZE;
    fs_size_t front_gap = *pos - front_pos;
    fs_size_t sectors = (front_gap + size + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE;
    for (uint8_t fati = 0; fati < sb->layout.num_fats; fati++) {
        uint32_t lba = fat_offset + fat_sectors * fati;
        result = fat_write_sectors(sb->bdev, lba + sector_pos, sectors, file->buffer + front_pos);
        if (!kerrno_ok(result)) {
            return result;
        }
    }

    *pos += size;
    return (fs_ssize_t)size;
}

kerrno_t fat_table_append(struct fat_table *file, uint32_t cluster, uint32_t *new_cluster) {
    uint32_t entry;
    kerrno_t result = file->ops->table_at(file, cluster, &entry);
    if (!kerrno_ok(result)) {
        return result;
    }

    entry &= 0x0fffffff;
    if (entry < 0x0ffffff8) {
        return OPAL_EINVAL;
    }

    result = file->ops->table_alloc(file, new_cluster);
    if (!kerrno_ok(result)) {
        return result;
    }

    result = file->ops->table_set(file, cluster, *new_cluster);
    if (!kerrno_ok(result)) {
        file->ops->table_set(file, *new_cluster, 0);
    }
    return result;
}

static kerrno_t fat12_table_at(struct fat_table *file, uint32_t cluster, uint32_t *value) {
    unsigned char entry[2];
    const fs_size_t pos = cluster * 3 / 2;
    fs_ssize_t n = fat_table_read(&file->file, &(fs_size_t){ pos }, entry, 2);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 2) {
        return OPAL_ERANGE;
    }

    if (cluster % 2 == 0) {
        *value = entry[0] | ((entry[1] & 0xf) << 8);
    } else {
        *value = ((entry[0] & 0xf0) >> 4) | (entry[1] << 4);
    }

    if ((*value & 0xff0) == 0xff0) {
        *value |= 0xfffffff0;
    }
    return OPAL_OK;
}

static kerrno_t fat12_table_set(struct fat_table *file, uint32_t cluster, uint32_t value) {
    unsigned char entry[2];
    const fs_size_t pos = cluster * 3 / 2;
    fs_ssize_t n = fat_table_read(&file->file, &(fs_size_t){ pos }, entry, 2);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 2) {
        return OPAL_ERANGE;
    }

    if (cluster % 2 == 0) {
        entry[0] = (uint8_t)value;
        entry[1] = (entry[1] & 0xf0) | (uint8_t)((value >> 8) & 0xf);
    } else {
        entry[0] = (entry[0] & 0xf) | (uint8_t)((value & 0xf) << 4);
        entry[1] = (uint8_t)(value >> 4);
    }

    n = fat_table_write(&file->file, &(fs_size_t){ pos }, entry, 2);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 2) {
        return OPAL_ERANGE;
    }
    return OPAL_OK;
}

static kerrno_t fat12_table_alloc(struct fat_table *file, uint32_t *cluster_out) {
    kerrno_t result = fat_table_readall(file);
    if (!kerrno_ok(result)) {
        return result;
    }

    uint32_t cluster_count = file->sb->layout.cluster_count;
    uint32_t cluster2 = 1;
    uint32_t offset = 2;
    for (; cluster2 * 2 < cluster_count + 2; cluster2++) {
        uint16_t e1 = file->buffer[cluster2 * 3] | ((file->buffer[cluster2 * 3 + 1] & 0xf) << 8);
        uint16_t e2 =
            ((file->buffer[cluster2 * 3 + 1] & 0xf0) >> 4) | (file->buffer[cluster2 * 3 + 2] << 4);
        if (e1 == 0) {
            offset = 0;
            break;
        }
        if (e2 == 0) {
            if (cluster2 * 2 + 1 < cluster_count + 2) {
                offset = 1;
            }
            break;
        }
    }

    if (offset == 2) {
        return OPAL_ENOSPC;
    }

    *cluster_out = cluster2 * 2 + offset;
    return fat12_table_set(file, *cluster_out, 0xfff);
}

static kerrno_t fat16_table_at(struct fat_table *file, uint32_t cluster, uint32_t *value) {
    uint16_t entry;
    const fs_size_t pos = cluster * 2;
    fs_ssize_t n = fat_table_read(&file->file, &(fs_size_t){ pos }, &entry, 2);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 2) {
        return OPAL_ERANGE;
    }

    *value = entry;
    if ((*value & 0xfff0) == 0xfff0) {
        *value |= 0xfffffff0;
    }
    return OPAL_OK;
}

static kerrno_t fat16_table_set(struct fat_table *file, uint32_t cluster, uint32_t value) {
    uint16_t entry = (uint16_t)value;
    const fs_size_t pos = cluster * 2;
    fs_ssize_t n = fat_table_write(&file->file, &(fs_size_t){ pos }, &entry, 2);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 2) {
        return OPAL_ERANGE;
    }
    return OPAL_OK;
}

static kerrno_t fat16_table_alloc(struct fat_table *file, uint32_t *cluster_out) {
    kerrno_t result = fat_table_readall(file);
    if (!kerrno_ok(result)) {
        return result;
    }

    uint16_t *fat = (uint16_t *)file->buffer;
    uint32_t cluster = 2;
    for (; cluster < file->sb->layout.cluster_count + 2; cluster++) {
        if (fat[cluster] == 0) {
            goto found;
        }
    }
    return OPAL_ENOSPC;

found:
    *cluster_out = cluster;
    return fat16_table_set(file, cluster, 0xffff);
}

static kerrno_t fat32_table_at(struct fat_table *file, uint32_t cluster, uint32_t *value) {
    uint32_t entry = 0;
    const fs_size_t pos = cluster * 4;
    fs_ssize_t n = fat_table_read(&file->file, &(fs_size_t){ pos }, &entry, 4);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 4) {
        return OPAL_ERANGE;
    }

    *value = entry;
    return OPAL_OK;
}

static kerrno_t fat32_table_set(struct fat_table *file, uint32_t cluster, uint32_t value) {
    const fs_size_t pos = cluster * 4;
    fs_ssize_t n = fat_table_write(&file->file, &(fs_size_t){ pos }, &value, 4);
    if (n < 0) {
        return fs_ssize_errno(n);
    } else if (n != 4) {
        return OPAL_ERANGE;
    }
    return OPAL_OK;
}

static kerrno_t fat32_table_alloc(struct fat_table *file, uint32_t *cluster_out) {
    kerrno_t result = fat_table_readall(file);
    if (!kerrno_ok(result)) {
        return result;
    }

    uint32_t *fat = (uint32_t *)file->buffer;
    uint32_t cluster = 2;
    for (; cluster < file->sb->layout.cluster_count + 2; cluster++) {
        if (fat[cluster] == 0) {
            goto found;
        }
    }
    return OPAL_ENOSPC;

found:
    *cluster_out = cluster;
    return fat32_table_set(file, cluster, 0x0fffffff);
}

#define TABLE_FILE_OPS { \
    .close = fat_table_close, \
    .seek = fat_table_seek, \
    .read = fat_table_read, \
    .write = fat_table_write, \
}

static struct fat_table_ops g_fat12_ops = {
    .ops = TABLE_FILE_OPS,
    .table_at = fat12_table_at,
    .table_set = fat12_table_set,
    .table_alloc = fat12_table_alloc,
};

static struct fat_table_ops g_fat16_ops = {
    .ops = TABLE_FILE_OPS,
    .table_at = fat16_table_at,
    .table_set = fat16_table_set,
    .table_alloc = fat16_table_alloc,
};

static struct fat_table_ops g_fat32_ops = {
    .ops = TABLE_FILE_OPS,
    .table_at = fat32_table_at,
    .table_set = fat32_table_set,
    .table_alloc = fat32_table_alloc,
};
