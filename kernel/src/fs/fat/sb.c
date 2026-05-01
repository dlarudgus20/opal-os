#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/mm/kmalloc.h>

#include "fat.h"

static struct superblock_ops g_fs_ops;

kerrno_t fat_read_sectors(struct block_device *bdev, uint32_t lba, uint32_t sectors, void *buffer) {
    struct disk_request *req = block_device_read(bdev, lba, sectors, buffer);
    if (!req) {
        return OPAL_EBUSY;
    }

    kerrno_t result;
    disk_request_wait(req, TIMEOUT_INFINITY, &result);
    return result;
}

kerrno_t fat_write_sectors(struct block_device *bdev, uint32_t lba, uint32_t sectors, const void *buffer) {
    struct disk_request *req = block_device_write(bdev, lba, sectors, buffer);
    if (!req) {
        return OPAL_EBUSY;
    }

    kerrno_t result;
    disk_request_wait(req, TIMEOUT_INFINITY, &result);
    return result;
}

static bool is_valid_bps(uint16_t bps) {
    return bps == 512 || bps == 1024 || bps == 2048 || bps == 4096;
}

static bool parse_bpb(fs_size_t bdev_sectors, const struct fat_bpb *bpb, struct fat_layout *layout) {
    uint16_t bps = bpb->bytes_per_sector;
    uint8_t spc = bpb->sectors_per_cluster;

    if (!is_valid_bps(bps)) {
        return false;
    }
    if (!ispower2(spc)) {
        return false;
    }
    if (bpb->reserved_sectors == 0 || bpb->num_fats == 0) {
        return false;
    }

    layout->bytes_per_sector = bps;
    layout->sectors_per_cluster = spc;
    layout->reserved_sectors = bpb->reserved_sectors;
    layout->num_fats = bpb->num_fats;

    layout->total_sectors = bpb->total_sectors_16;
    if (layout->total_sectors == 0) {
        layout->total_sectors = bpb->total_sectors_32;
    }

    uint32_t lsec = (uint32_t)bps / DISK_SECTOR_SIZE;
    fs_size_t total_psec = (fs_size_t)layout->total_sectors * lsec;
    if (total_psec == 0 || total_psec > (1ul << 32) || total_psec > bdev_sectors) {
        return false;
    }

    layout->bits = 0;
    layout->fat_sectors = bpb->fat_size_16;
    if (bpb->fat_size_16 == 0) {
        if (bpb->fat_size_32 == 0) {
            return false;
        }

        layout->bits = 32;
        layout->fat_sectors = bpb->fat_size_32;
    }

    uint32_t root_dir_size = (uint32_t)bpb->root_entries * 32;
    uint32_t root_dir_sectors = (root_dir_size + bps - 1) / bps;
    fs_size_t fats_sectors_ = (fs_size_t)bpb->num_fats * layout->fat_sectors;
    fs_size_t data_offset_ = bpb->reserved_sectors + fats_sectors_ + root_dir_sectors;
    if (data_offset_ >= layout->total_sectors) {
        return false;
    }
    uint32_t fats_sectors = (uint32_t)fats_sectors_;
    layout->data_offset = (uint32_t)data_offset_;

    fs_size_t data_sectors = layout->total_sectors - layout->data_offset;
    layout->cluster_count = data_sectors / spc;

    if (layout->bits == 0) {
        layout->bits = (layout->cluster_count <= FAT12_MAX_CLUSTERS) ? 12 : 16;
    }

    if (layout->bits == 32) {
        layout->root_cluster = bpb->root_cluster;
    } else {
        layout->root_offset = layout->reserved_sectors + fats_sectors;
        layout->root_entries = bpb->root_entries;
    }

    return true;
}

static void init_sb_from_layout(struct block_device *bdev, struct fat_sb *sb) {
    superblock_init(&sb->sb, &g_fs_ops);
    sb->sb.root = &sb->root;
    sb->bdev = bdev;

    fat_table_init(&sb->table, sb);

    if (sb->layout.bits == 32) {
        fat_inode_init(&sb->root32, sb, NULL, 0, sb->layout.root_cluster);
    } else {
        fat_root_init(&sb->root1x, sb, sb->layout.root_offset, sb->layout.root_entries);
    }
}

kerrno_t fat_mount(struct block_device *bdev, struct superblock **sb_out) {
    if (!sb_out) {
        return OPAL_EINVAL;
    }

    kerrno_t result = OPAL_ENOMEM;

    struct fat_sb *sb = kzalloc(sizeof(*sb));
    if (!sb) {
        goto err;
    }

    union {
        unsigned char buffer[DISK_SECTOR_SIZE];
        struct fat_bpb bpb;
    } vbr;

    result = fat_read_sectors(bdev, 0, 1, vbr.buffer);
    if (result != OPAL_OK) {
        goto err_alloc;
    }

    if (!parse_bpb(bdev->sectors, &vbr.bpb, &sb->layout)) {
        result = OPAL_ENOENT;
        goto err_alloc;
    }

    init_sb_from_layout(bdev, sb);
    *sb_out = &sb->sb;
    return OPAL_OK;

err_alloc:
    kfree(sb, sizeof(*sb));
err:
    return result;
}

static void fat_umount(struct superblock *base) {
    struct fat_sb *fs = container_of(base, struct fat_sb, sb);
    inode_release(&fs->root);
    file_release(&fs->table.file);
    block_device_release(fs->bdev);
    kfree(fs, sizeof(*fs));
}

static struct superblock_ops g_fs_ops = {
    .umount = fat_umount,
};

static int8_t recommended_sectors_per_cluster(fs_size_t total_sectors) {
    if (total_sectors < 0x00100000u) {
        return 1;
    } else if (total_sectors < 0x00200000u) {
        return 2;
    } else if (total_sectors < 0x00400000u) {
        return 4;
    } else if (total_sectors < 0x00800000u) {
        return 8;
    } else if (total_sectors < 0x01000000u) {
        return 16;
    } else if (total_sectors < 0x02000000u) {
        return 32;
    } else if (total_sectors <= UINT32_MAX) {
        return 64;
    } else {
        return -1;
    }
}

static bool is_valid_clusters(uint8_t bits, uint32_t cluster_count) {
    if (bits == 12) {
        return cluster_count <= FAT12_MAX_CLUSTERS;
    } else if (bits == 16) {
        return cluster_count <= FAT16_MAX_CLUSTERS;
    } else {
        return cluster_count <= FAT32_MAX_CLUSTERS;
    }
}

static bool calc_fat_layout(
    uint8_t bits, uint32_t sectors, uint16_t bps, uint8_t spc,
    struct fat_layout *layout
) {
    uint16_t reserved_sectors = FAT32_RESERVED_SECTORS;
    uint16_t root_entries = 0;

    if (bits == 16) {
        reserved_sectors = 1;
        root_entries = FAT16_ROOT_ENTRIES;
    } else if (bits == 12) {
        reserved_sectors = 1;
        root_entries = FAT12_ROOT_ENTRIES;
    } else {
        assert(bits == 32);
    }

    uint32_t root_dir_size = (uint32_t)root_entries * 32;
    uint32_t root_dir_sectors = (root_dir_size + bps - 1) / bps;

    uint32_t fat_sectors_candidate = 1;
    for (int i = 0; i < 32; i++) {
        uint32_t data_offset = reserved_sectors;
        uint32_t fats_total = FAT_NUM_FATS * fat_sectors_candidate;
        uint32_t required;
        if (bits != 32) {
            data_offset += fats_total + root_dir_sectors;
            required = data_offset;
        } else {
            required = data_offset + fats_total;
        }

        // required + 1 available cluster
        if (required + spc >= sectors) {
            return false;
        }

        uint32_t data_sectors = sectors - data_offset;
        uint32_t cluster_count = data_sectors / spc;
        uint32_t fat_entries = cluster_count + 2;

        uint32_t fat_bytes = fat_entries * bits / 8;
        if (bits == 12) {
            fat_bytes = (fat_entries * 3 + 1) / 2;
        }

        uint32_t fat_sectors = (fat_bytes + bps - 1) / bps;

        if (fat_sectors_candidate == fat_sectors) {
            if (!is_valid_clusters(bits, cluster_count)) {
                return false;
            }
            *layout = (struct fat_layout){
                .bits = bits,
                .bytes_per_sector = bps,
                .sectors_per_cluster = spc,
                .reserved_sectors = reserved_sectors,
                .num_fats = FAT_NUM_FATS,
                .fat_sectors = fat_sectors,
                .total_sectors = sectors,
                .data_offset = data_offset,
                .cluster_count = cluster_count,
            };
            if (bits == 32) {
                layout->root_cluster = FAT32_ROOT_CLUSTER;
            } else {
                layout->root_offset = reserved_sectors + fats_total;
                layout->root_entries = root_entries;
            }
            return true;
        }

        fat_sectors_candidate = fat_sectors;
    }

    return false;
}

static kerrno_t setup_layout(fs_size_t sectors, struct fat_layout *layout) {
    int8_t spc_i = recommended_sectors_per_cluster(sectors);
    if (spc_i < 0) {
        return OPAL_ETOOBIG;
    }
    uint8_t spc = (uint8_t)spc_i;

    uint8_t bits_list[] = { 12, 16, 32 };
    uint8_t bits_idx = 0;
    for (; bits_idx < 3; bits_idx++) {
        if (calc_fat_layout(bits_list[bits_idx], (uint32_t)sectors, DISK_SECTOR_SIZE, spc, layout)) {
            break;
        }
    }

    if (bits_idx >= 3) {
        return OPAL_ETOOBIG;
    }

    return OPAL_OK;
}

static void setup_bpb(const struct fat_layout *layout, struct fat_bpb *bpb) {
    memcpy(bpb->jmp_boot, "\xeb\x58\x90", 3);
    memcpy(bpb->oem_name, "OPALFAT ", 8);
    bpb->bytes_per_sector = layout->bytes_per_sector;
    bpb->sectors_per_cluster = layout->sectors_per_cluster;
    bpb->reserved_sectors = layout->reserved_sectors;
    bpb->num_fats = layout->num_fats;
    bpb->total_sectors_16 = layout->total_sectors >= UINT16_MAX ? 0 : (uint16_t)layout->total_sectors;
    bpb->media = 0xf8;
    bpb->fat_size_16 = layout->bits == 32 ? 0 : (uint16_t)layout->fat_sectors;
    bpb->sectors_per_track = 0x3f;
    bpb->num_heads = 0xff;
    bpb->hidden_sectors = 0;
    bpb->total_sectors_32 = layout->total_sectors;
    if (layout->bits != 32) {
        bpb->root_entries = layout->root_entries;
        bpb->drive_number = 0x80;
        bpb->reserved1 = 0;
        bpb->boot_signature = 0x29;
        bpb->volume_id = 0;
        memcpy(bpb->volume_label, "OPALFAT   ", 11);
        memcpy(bpb->fs_type, layout->bits == 16 ? "FAT16   " : "FAT12   ", 8);
    } else {
        bpb->root_entries = 0;
        bpb->fat_size_32 = layout->fat_sectors;
        bpb->fat32_flags = 0;
        bpb->fat32_version = 0;
        bpb->root_cluster = FAT32_ROOT_CLUSTER;
        bpb->fsinfo_sector = FAT32_FSINFO_SECTOR;
        bpb->backup_boot_sector = FAT32_BACKUP_BOOT_SECTOR;
    }
}

static kerrno_t write_vbr(struct block_device *bdev, const struct fat_layout *layout, uint8_t *media) {
    union {
        unsigned char buffer[DISK_SECTOR_SIZE];
        struct fat_bpb bpb;
    } vbr;

    memset(&vbr, 0, sizeof(vbr));
    setup_bpb(layout, &vbr.bpb);
    *media = vbr.bpb.media;
    vbr.buffer[510] = 0x55;
    vbr.buffer[511] = 0xaa;

    kerrno_t result = fat_write_sectors(bdev, 0, 1, vbr.buffer);
    if (result != OPAL_OK) {
        return result;
    }

    if (layout->bits == 32) {
        result = fat_write_sectors(bdev, FAT32_BACKUP_BOOT_SECTOR, 1, vbr.buffer);
        if (result != OPAL_OK) {
            return result;
        }
    }

    return OPAL_OK;
}

static kerrno_t write_fsinfo(struct block_device *bdev) {
    union {
        unsigned char buffer[DISK_SECTOR_SIZE];
        struct fat32_fsinfo fsinfo;
    } sec;

    memset(&sec, 0, sizeof(sec));
    memcpy(sec.fsinfo.signature1, "RRaA", 4);
    memcpy(sec.fsinfo.signature2, "rrAa", 4);
    sec.fsinfo.free_clusters = 0xffffffff;
    sec.fsinfo.next_free_cluster = 0xffffffff;
    sec.fsinfo.signature3[2] = 0x55;
    sec.fsinfo.signature3[3] = 0xaa;

    kerrno_t result = fat_write_sectors(bdev, FAT32_FSINFO_SECTOR, 1, sec.buffer);
    if (result != OPAL_OK) {
        return result;
    }

    return fat_write_sectors(bdev, FAT32_BACKUP_FSINFO_SECTOR, 1, sec.buffer);
}

static kerrno_t write_fats(struct block_device *bdev, const struct fat_layout *layout, uint8_t media) {
    unsigned char first[DISK_SECTOR_SIZE] = { };
    unsigned char empty[DISK_SECTOR_SIZE] = { };

    first[0] = media;
    if (layout->bits == 12) {
        memcpy(first + 1, "\xff\xff", 2);
    } else if (layout->bits == 16) {
        memcpy(first + 1, "\xff\xff\xff", 3);
    } else {
        memcpy(first + 1, "\xff\xff\x0f\xff\xff\xff\x0f\xff\xff\xff\x0f", 11);
    }

    uint32_t fat_offset = layout->reserved_sectors;
    for (uint8_t fati = 0; fati < layout->num_fats; fati++) {
        kerrno_t result = fat_write_sectors(bdev, fat_offset++, 1, first);
        if (result != OPAL_OK) {
            return result;
        }

        for (uint32_t i = 1; i < layout->fat_sectors; i++) {
            result = fat_write_sectors(bdev, fat_offset++, 1, empty);
            if (result != OPAL_OK) {
                return result;
            }
        }
    }

    return OPAL_OK;
}

static kerrno_t write_root(struct block_device *bdev, const struct fat_layout *layout) {
    unsigned char empty[DISK_SECTOR_SIZE] = { };

    uint32_t offset;
    uint32_t sectors;
    if (layout->bits == 32) {
        offset = layout->data_offset;
        sectors = layout->sectors_per_cluster;
    } else {
        uint32_t root_dir_size = (uint32_t)layout->root_entries * 32;
        offset = layout->root_offset;
        sectors = (root_dir_size + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE;
    }

    for (uint32_t sec = 0; sec < sectors; sec++) {
        kerrno_t result = fat_write_sectors(bdev, offset + sec, 1, empty);
        if (result != OPAL_OK) {
            return result;
        }
    }

    return OPAL_OK;
}

kerrno_t fat_format(struct block_device *bdev, struct superblock **sb_out) {
    if (!sb_out) {
        return OPAL_EINVAL;
    }

    kerrno_t result = OPAL_ENOMEM;

    struct fat_sb *sb = kzalloc(sizeof(*sb));
    if (!sb) {
        goto err;
    }

    if (bdev->sectors > UINT32_MAX) {
        result = OPAL_ETOOBIG;
        goto err_alloc;
    }

    result = setup_layout(bdev->sectors, &sb->layout);
    if (result != OPAL_OK) {
        goto err_alloc;
    }

    uint8_t media;
    result = write_vbr(bdev, &sb->layout, &media);
    if (result != OPAL_OK) {
        goto err_alloc;
    }

    if (sb->layout.bits == 32) {
        result = write_fsinfo(bdev);
        if (result != OPAL_OK) {
            goto err_alloc;
        }
    }

    result = write_fats(bdev, &sb->layout, media);
    if (result != OPAL_OK) {
        goto err_alloc;
    }

    result = write_root(bdev, &sb->layout);
    if (result != OPAL_OK) {
        goto err_alloc;
    }

    init_sb_from_layout(bdev, sb);
    *sb_out = &sb->sb;
    return OPAL_OK;

err_alloc:
    kfree(sb, sizeof(*sb));
err:
    return result;
}
