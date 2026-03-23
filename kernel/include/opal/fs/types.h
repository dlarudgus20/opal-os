#ifndef OPAL_FS_TYPES_H
#define OPAL_FS_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef int64_t fs_off_t;
typedef uint64_t fs_size_t;

enum fs_status {
    FS_OK = 0,
    FS_ERR_IO = -1,
};

typedef enum fs_status fs_status_t;

#endif
