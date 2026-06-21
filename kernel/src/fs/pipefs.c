#include <kc/kassert.h>
#include <kc/stdlib.h>

#include <opal/fs/pipefs.h>
#include <opal/mm/kmalloc.h>
#include <opal/locks/irqlock.h>
#include <opal/task/wait_list.h>

struct pipe_file {
    struct file file;
};

static const struct superblock_ops g_sb_ops;
static const struct inode_ops g_pipe_ops;
static const struct file_ops g_pipe_fops;

struct pipefs *pipefs_create(void) {
    struct pipefs *pipe = kzalloc(sizeof(*pipe));
    if (!pipe) {
        return NULL;
    }

    superblock_init(&pipe->sb, &g_sb_ops);
    inode_init(&pipe->inode, &g_pipe_ops, INODE_PIPE);
    pipe->sb.root = &pipe->inode;
    event_init(&pipe->readable, true);
    event_init(&pipe->writable, true);

    return pipe;
}

static void sb_umount(struct superblock *sb) {
    struct pipefs *pipe = container_of(sb, typeof(*pipe), sb);
    inode_release(&pipe->inode);
}

static const struct superblock_ops g_sb_ops = {
    .umount = sb_umount,
};

static void pipe_inode_close(struct inode *inode) {
    struct pipefs *pipe = container_of(inode, typeof(*pipe), inode);
    kfree(pipe, sizeof(*pipe));
}

static kerrno_t pipe_inode_open(struct inode *inode, enum open_mode mode, struct file **file_out) {
    enum open_mode fmode = mode & OPEN_MASK_FMODE;
    if (fmode != OPEN_READ && fmode != OPEN_WRITE) {
        return OPAL_ENOTSUPP;
    }

    struct pipefs *pipe = container_of(inode, typeof(*pipe), inode);
    struct pipe_file *file = kzalloc(sizeof(*file));
    if (!file) {
        return OPAL_ENOMEM;
    }

    file_init(&file->file, &g_pipe_fops, fmode_from_omode(mode), inode);

    irqlock_t irqlock = irqlock_acquire();
    if (fmode == OPEN_READ) {
        pipe->readers++;
    } else {
        pipe->writers++;
    }
    irqlock_release(&irqlock);

    *file_out = &file->file;
    return OPAL_OK;
}

struct file *pipefs_open_reader(struct pipefs *pipe) {
    struct file *file;
    kerrno_t err = pipe_inode_open(&pipe->inode, OPEN_READ, &file);
    return kerrno_ok(err) ? file : NULL;
}

struct file *pipefs_open_writer(struct pipefs *pipe) {
    struct file *file;
    kerrno_t err = pipe_inode_open(&pipe->inode, OPEN_WRITE, &file);
    return kerrno_ok(err) ? file : NULL;
}

static kerrno_t pipe_inode_iterate_dir(struct inode *, fs_size_t, inode_iterate_dir_cb, void *) {
    return OPAL_ENOTDIR;
}

static kerrno_t pipe_inode_get_child(struct inode *, fs_size_t, struct inode **) {
    return OPAL_ENOTDIR;
}

static kerrno_t pipe_inode_create_child(
    struct inode *, const struct hstr *, enum inode_flags, struct inode **) {
    return OPAL_ENOTDIR;
}

static const struct inode_ops g_pipe_ops = {
    .close = pipe_inode_close,
    .open = pipe_inode_open,
    .iterate_dir = pipe_inode_iterate_dir,
    .get_child = pipe_inode_get_child,
    .create_child = pipe_inode_create_child,
};

static void pipe_file_close(struct file *base) {
    struct pipe_file *file = container_of(base, typeof(*file), file);
    struct pipefs *pipe = container_of(base->inode, typeof(*pipe), inode);

    irqlock_t irqlock = irqlock_acquire();
    if (base->mode & FILE_READ) {
        kassert(pipe->readers > 0);
        pipe->readers--;
        event_signal(&pipe->writable);
    }
    if (base->mode & FILE_WRITE) {
        kassert(pipe->writers > 0);
        pipe->writers--;
        event_signal(&pipe->readable);
    }
    irqlock_release(&irqlock);

    kfree(file, sizeof(*file));
}

static fs_ssize_t pipe_file_seek(struct file *, fs_off_t, enum fs_seek) {
    return OPAL_ENOTSUPP;
}

static uint16_t pipe_read_some(struct pipefs *pipe, void *buffer, fs_size_t size) {
    uint16_t to_read = size < pipe->count ? (uint16_t)size : pipe->count;
    unsigned char *out = buffer;
    for (uint16_t i = 0; i < to_read; i++) {
        out[i] = pipe->buffer[pipe->rpos++];
        if (pipe->rpos >= PIPE_BUFLEN) {
            pipe->rpos = 0;
        }
    }
    pipe->count -= to_read;
    return to_read;
}

static fs_ssize_t pipe_file_read(struct file *base, fs_size_t *, void *buffer, fs_size_t size) {
    if (size == 0) {
        return 0;
    }

    struct pipefs *pipe = container_of(base->inode, typeof(*pipe), inode);

    irqlock_t irqlock = irqlock_acquire();
    while (pipe->count == 0) {
        if (pipe->writers == 0) {
            event_signal(&pipe->readable);
            irqlock_release(&irqlock);
            return 0;
        }
        event_wait(&pipe->readable, TIMEOUT_INFINITY);
    }

    uint16_t n = pipe_read_some(pipe, buffer, size);
    event_signal(&pipe->writable);
    if (pipe->count > 0) {
        event_signal(&pipe->readable);
    }

    irqlock_release(&irqlock);
    return n;
}

static uint16_t pipe_write_some(struct pipefs *pipe, const void *buffer, fs_size_t size) {
    uint16_t space = PIPE_BUFLEN - pipe->count;
    uint16_t to_write = size < space ? (uint16_t)size : space;
    const unsigned char *in = buffer;
    for (uint16_t i = 0; i < to_write; i++) {
        pipe->buffer[pipe->wpos++] = in[i];
        if (pipe->wpos >= PIPE_BUFLEN) {
            pipe->wpos = 0;
        }
    }
    pipe->count += to_write;
    return to_write;
}

static fs_ssize_t pipe_file_write(
    struct file *base, fs_size_t *, const void *buffer, fs_size_t size) {
    if (size == 0) {
        return 0;
    }

    struct pipefs *pipe = container_of(base->inode, typeof(*pipe), inode);
    const unsigned char *in = buffer;
    fs_size_t written = 0;

    irqlock_t irqlock = irqlock_acquire();
    while (written < size) {
        while (1) {
            if (pipe->readers == 0) {
                event_signal(&pipe->writable);
                irqlock_release(&irqlock);
                return written > 0 ? (fs_ssize_t)written : OPAL_EIO;
            }

            if (pipe->count < PIPE_BUFLEN) {
                break;
            }
            event_wait(&pipe->writable, TIMEOUT_INFINITY);
        }

        uint16_t n = pipe_write_some(pipe, in + written, size - written);
        written += n;
        event_signal(&pipe->readable);
        if (pipe->count < PIPE_BUFLEN) {
            event_signal(&pipe->writable);
        }
    }

    irqlock_release(&irqlock);
    return (fs_ssize_t)written;
}

static kerrno_t pipe_file_truncate(struct file *, fs_size_t) {
    return OPAL_ENOTSUPP;
}

static const struct file_ops g_pipe_fops = {
    .close = pipe_file_close,
    .seek = pipe_file_seek,
    .read = pipe_file_read,
    .write = pipe_file_write,
    .truncate = pipe_file_truncate,
};
