#ifndef OPAL_FS_TYPES_H
#define OPAL_FS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include <kc/kerrno.h>

#include <opal/task/completion.h>

#define FS_OFF_MAX      INT64_MAX
#define FS_OFF_MIN      INT64_MIN
#define FS_SSIZE_MAX    INT64_MAX
#define FS_SSIZE_MIN    INT64_MIN
#define FS_SIZE_MAX     UINT64_MAX

typedef int64_t fs_off_t;
typedef int64_t fs_ssize_t;
typedef uint64_t fs_size_t;

enum fs_seek : uint8_t {
    FS_SEEK_SET,
    FS_SEEK_END,
};

static inline kerrno_t fs_ssize_errno(fs_ssize_t ss) {
    if (ss >= 0) {
        return OPAL_OK;
    } else if (ss < OPAL_EUNKNOWN) {
        return OPAL_EUNKNOWN;
    } else {
        return (kerrno_t)ss;
    }
}

static inline fs_size_t align_ceil_fsz_p2(fs_size_t x, fs_size_t align) {
    const fs_size_t mask = align - 1;
    return (x + mask) & ~mask;
}

struct fs_completion {
    struct completion comp;
    kerrno_t result;
};

static inline void fs_completion_init(struct fs_completion *c) {
    completion_init(&c->comp);
    c->result = OPAL_EUNKNOWN;
}

static inline bool fs_completion_wait(struct fs_completion *c, uint64_t timeout) {
    return completion_wait(&c->comp, timeout);
}

static inline void fs_completion_signal(struct fs_completion *c, kerrno_t result) {
    c->result = result;
    completion_signal(&c->comp);
}

#endif
