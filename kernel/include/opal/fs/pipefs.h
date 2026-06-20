#ifndef OPAL_FS_PIPEFS_H
#define OPAL_FS_PIPEFS_H

#include <opal/fs/vfs.h>
#include <opal/task/event.h>

#define PIPE_BUFLEN 4000

struct pipefs {
    struct superblock sb;
    struct inode inode;

    uint16_t rpos;
    uint16_t wpos;
    uint16_t count;
    uint16_t readers;
    uint16_t writers;

    struct event readable;
    struct event writable;

    unsigned char buffer[PIPE_BUFLEN];
};

struct pipefs *pipefs_create(void);
struct file *pipefs_open_reader(struct pipefs *pipe);
struct file *pipefs_open_writer(struct pipefs *pipe);

#endif
