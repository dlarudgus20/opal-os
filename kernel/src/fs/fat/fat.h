#ifndef SRC_FS_FAT_FAT_H
#define SRC_FS_FAT_FAT_H

#include <opal/attributes.h>
#include <opal/fs/vfs.h>
#include <opal/fs/block_device.h>

#define FAT12_MAX_CLUSTERS 4084
#define FAT16_MAX_CLUSTERS 65524
#define FAT32_MAX_CLUSTERS 0x0ffffff4

#define FAT12_ROOT_ENTRIES 224
#define FAT16_ROOT_ENTRIES 512

#define FAT_NUM_FATS                2
#define FAT32_RESERVED_SECTORS      32
#define FAT32_ROOT_CLUSTER          2
#define FAT32_FSINFO_SECTOR         1
#define FAT32_BACKUP_BOOT_SECTOR    6
#define FAT32_BACKUP_FSINFO_SECTOR  7

#define FAT_ATTR_READONLY   0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VLABEL     0x08
#define FAT_ATTR_DIRECTORY  0x10

struct PACKED fat_bpb {
    uint8_t jmp_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;

    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    union {
        struct {
            // FAT12/16
            uint8_t drive_number;
            uint8_t reserved1;
            uint8_t boot_signature;
            uint32_t volume_id;
            char volume_label[11];
            char fs_type[8];
        };
        struct {
            // FAT32
            uint32_t fat_size_32;
            uint16_t fat32_flags;
            uint16_t fat32_version;
            uint32_t root_cluster;
            uint16_t fsinfo_sector;
            uint16_t backup_boot_sector;
        };
    };
};

struct PACKED fat32_fsinfo {
    unsigned char signature1[4];
    unsigned char reserved1[480];
    unsigned char signature2[4];
    uint32_t free_clusters;
    uint32_t next_free_cluster;
    unsigned char reserved2[12];
    unsigned char signature3[4];
};

struct PACKED fat_dentry {
    char name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
};

struct fat_layout {
    uint8_t bits;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t fat_sectors;
    uint32_t total_sectors;
    uint32_t data_offset;
    uint32_t cluster_count;
    union {
        struct {
            uint32_t root_offset;
            uint16_t root_entries;
        };
        uint32_t root_cluster;
    };
};

struct fat_sb;
struct fat_table;
struct fat_inode_base;

struct fat_inode_ops {
    struct inode_ops ops;
    fs_status_t (*write_dentry)(struct fat_inode_base *inode, const struct fat_dentry *dentry, uint32_t index);
    fs_ssize_t (*alloc_dentry)(struct fat_inode_base *inode);
};

struct fat_inode_base {
    union {
        const struct fat_inode_ops *ops;
        struct inode inode;
    };
    struct file file;
    struct fat_sb *sb;
    unsigned char *buffer;
    size_t buflen;
};

struct fat_root_inode {
    union {
        const struct fat_inode_ops *ops;
        struct fat_inode_base base;
        struct {
            struct inode inode;
            struct file file;
            struct fat_sb *sb;
            struct fat_dentry *buffer;
            size_t buflen;
        };
    };

    uint32_t offset;
    uint16_t entries;
};

struct fat_inode {
    union {
        const struct fat_inode_ops *ops;
        struct fat_inode_base base;
        struct {
            struct inode inode;
            struct file file;
            struct fat_sb *sb;
            unsigned char *buffer;
            size_t buflen;
        };
    };
    uint32_t dentry_idx;
    struct fat_inode_base *parent;
    uint32_t first_cluster;
    uint32_t filesize;
};

struct fat_table_ops {
    struct file_ops ops;
    fs_status_t (*table_at)(struct fat_table *file, uint32_t cluster, uint32_t *value);
    fs_status_t (*table_set)(struct fat_table *file, uint32_t cluster, uint32_t value);
    fs_status_t (*table_alloc)(struct fat_table *file, uint32_t *cluster_out);
};

struct fat_table {
    union {
        const struct fat_table_ops *ops;
        struct file file;
    };
    struct fat_sb *sb;
    unsigned char *buffer;
};

struct fat_sb {
    struct superblock sb;
    struct block_device *bdev;
    struct fat_layout layout;
    struct fat_table table;
    union {
        struct inode root;
        struct fat_inode root32;
        struct fat_root_inode root1x;
    };
};

// sb.c
fs_status_t fat_read_sectors(struct block_device *bdev, uint32_t lba, uint32_t sectors, void *buffer);
fs_status_t fat_write_sectors(struct block_device *bdev, uint32_t lba, uint32_t sectors, const void *buffer);

// inode.c
void fat_root_init(struct fat_root_inode *inode, struct fat_sb *sb, uint32_t offset, uint16_t entries);
void fat_inode_init(struct fat_inode *inode, struct fat_sb *sb, struct fat_inode_base *parent, uint32_t dentry_idx, uint32_t first_cluster);

// fat_table.c
void fat_table_init(struct fat_table *file, struct fat_sb *sb);
fs_status_t fat_table_append(struct fat_table *file, uint32_t cluster, uint32_t *new_cluster);

#endif
