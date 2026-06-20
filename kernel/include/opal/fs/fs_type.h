#ifndef OPAL_FS_FS_TYPE_H
#define OPAL_FS_FS_TYPE_H

#include <stdint.h>

#include <kc/kerrno.h>

#include <collections/linkedlist.h>

struct block_device;
struct superblock;

struct fs_type {
    struct linkedlist_link link;
    const char *name;

    kerrno_t (*mount)(struct block_device *bdev, struct superblock **sb_out);
};

#endif
