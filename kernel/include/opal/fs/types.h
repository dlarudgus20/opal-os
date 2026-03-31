#ifndef OPAL_FS_TYPES_H
#define OPAL_FS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include <opal/task/completion.h>

typedef int64_t fs_off_t;
typedef int64_t fs_ssize_t;
typedef uint64_t fs_size_t;

enum fs_status {
    FS_OK = 0,
    FS_ERR_IO = -1,
    FS_ERR_NOMEM = -2,
    FS_ERR_BUSY = -3,
    FS_ERR_NOENT = -4,
    FS_ERR_EXIST = -5,
    FS_ERR_RANGE = -6,
    FS_ERR_INVAL = -7,
    FS_ERR_UNKNOWN = -1000,
};

typedef enum fs_status fs_status_t;

struct fs_completion {
    struct completion comp;
    fs_status_t result;
};

static inline void fs_completion_init(struct fs_completion *c) {
    completion_init(&c->comp);
    c->result = FS_ERR_UNKNOWN;
}

static inline bool fs_completion_wait(struct fs_completion *c, uint64_t timeout) {
    return completion_wait(&c->comp, timeout);
}

static inline void fs_completion_signal(struct fs_completion *c, fs_status_t result) {
    c->result = result;
    completion_signal(&c->comp);
}

#endif
