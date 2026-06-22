#ifndef OPALSYS_VFS_H
#define OPALSYS_VFS_H

#include <stdint.h>

enum inode_flags : uint16_t {
    INODE_NORMAL = 0,
    INODE_DIR = 0x1,
    INODE_DEV = 0x2,
    INODE_PIPE = 0x4,

    INODE_MASK_ALL = INODE_DIR | INODE_DEV | INODE_PIPE,
};

enum open_mode : uint16_t {
    OPEN_NONE = 0,
    OPEN_READ = 0x01,
    OPEN_WRITE = 0x02,
    OPEN_APPEND = 0x04,

    OPEN_CREATE = 0x10,
    OPEN_NONEXIST = 0x20,
    OPEN_TRUNC = 0x40,

    OPEN_MASK_FMODE = 0x0f,
    OPEN_MASK_ALL = OPEN_READ | OPEN_WRITE | OPEN_APPEND | OPEN_CREATE | OPEN_NONEXIST | OPEN_TRUNC,
};

struct dirent {
    uint32_t next_offset;
    uint16_t name_len;
    uint16_t flags;
    char name[];
};

#endif
