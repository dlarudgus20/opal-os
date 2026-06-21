#include <kc/kassert.h>
#include <kc/stdlib.h>

#include <collections/linkedlist.h>

#include <opal/mm/kmalloc.h>
#include <opal/fs/kobjfs.h>
#include <opal/fs/globals.h>
#include <opal/task/event.h>
#include <opal/locks/irqlock.h>

#include "hid_inode.h"

#define INPUT_BUFSZ 256

struct hid_file {
    struct file file;
    struct linkedlist_link link;

    struct event event;

    uint16_t rpos;
    uint16_t wpos;
    uint16_t count;
    struct hid_char buffer[INPUT_BUFSZ];
};

static struct ko_inode g_hid_inode;
static struct linkedlist g_listeners;

static const struct inode_ops g_inode_ops;
static const struct file_ops g_file_ops;

void hid_inode_init(void) {
    kobjfs_inode_init(&g_hid_inode, &g_inode_ops, "hid");
    linkedlist_init(&g_listeners);

    kobjfs_add_inode(&vfs_globals()->devfs, &g_hid_inode);
}

static bool hid_file_try_write(struct hid_file *file, const struct hid_char *input) {
    if (file->count >= INPUT_BUFSZ) {
        return false;
    }

    file->buffer[file->wpos++] = *input;
    if (file->wpos >= INPUT_BUFSZ) {
        file->wpos -= INPUT_BUFSZ;
    }
    file->count++;
    return true;
}

static fs_ssize_t hid_file_try_read(struct hid_file *file, void *buffer, fs_size_t size) {
    if (size < sizeof(struct hid_char)) {
        return OPAL_EINVAL;
    }
    if (file->count == 0) {
        return 0;
    }

    fs_size_t max_entries = size / sizeof(struct hid_char);
    uint16_t to_read = max_entries < file->count ? (uint16_t)max_entries : file->count;
    struct hid_char *output = buffer;
    for (uint16_t i = 0; i < to_read; i++) {
        output[i] = file->buffer[file->rpos++];
        if (file->rpos >= INPUT_BUFSZ) {
            file->rpos -= INPUT_BUFSZ;
        }
    }

    file->count -= to_read;
    return (fs_ssize_t)(to_read * sizeof(struct hid_char));
}

void hid_inode_onkey(const struct hid_char *input) {
    irqlock_t irqlock = irqlock_acquire();

    linkedlist_foreach(ptr, &g_listeners) {
        struct hid_file *file = container_of(ptr, struct hid_file, link);
        if (hid_file_try_write(file, input)) {
            event_signal(&file->event);
        }
    }

    irqlock_release(&irqlock);
}

static void hid_inode_close(struct inode *) {
    panic();
}

static kerrno_t hid_inode_open(struct inode *inode, enum open_mode mode, struct file **file_out) {
    enum open_mode fmode = mode & OPEN_MASK_FMODE;
    if (fmode != OPEN_NONE && fmode != OPEN_READ) {
        return OPAL_ENOTSUPP;
    }

    struct hid_file *file = kzalloc(sizeof(*file));
    if (!file) {
        return OPAL_ENOMEM;
    }

    file_init(&file->file, &g_file_ops, fmode_from_omode(mode), inode);
    event_init(&file->event, true);
    file->rpos = 0;
    file->wpos = 0;
    file->count = 0;

    irqlock_t irqlock = irqlock_acquire();
    linkedlist_push_back(&g_listeners, &file->link);
    irqlock_release(&irqlock);

    *file_out = &file->file;
    return OPAL_OK;
}

static kerrno_t hid_inode_iterate_dir(
    struct inode *, fs_size_t, inode_iterate_dir_cb, void *) {
    return OPAL_ENOTDIR;
}

static kerrno_t hid_inode_get_child(struct inode *, fs_size_t, struct inode **) {
    return OPAL_ENOTDIR;
}

static kerrno_t hid_inode_create_child(
    struct inode *, const struct hstr *, enum inode_flags, struct inode **) {
    return OPAL_ENOTDIR;
}

static const struct inode_ops g_inode_ops = {
    .close = hid_inode_close,
    .open = hid_inode_open,
    .iterate_dir = hid_inode_iterate_dir,
    .get_child = hid_inode_get_child,
    .create_child = hid_inode_create_child,
};

static void hid_file_close(struct file *file_) {
    struct hid_file *file = container_of(file_, typeof(*file), file);

    irqlock_t irqlock = irqlock_acquire();
    linkedlist_remove(&file->link);
    irqlock_release(&irqlock);

    kfree(file, sizeof(*file));
}

static fs_ssize_t hid_file_seek(struct file *, fs_off_t, enum fs_seek) {
    return OPAL_ENOTSUPP;
}

static fs_ssize_t hid_file_read(struct file *base, fs_size_t *pos, void *buffer, fs_size_t size) {
    struct hid_file *file = container_of(base, typeof(*file), file);
    (void)pos;

    if (!(base->mode & FILE_READ)) {
        return OPAL_ENOTSUPP;
    }
    if (size == 0) {
        return 0;
    }

    irqlock_t irqlock = irqlock_acquire();
    fs_ssize_t len;

    while ((len = hid_file_try_read(file, buffer, size)) == 0) {
        event_wait(&file->event, TIMEOUT_INFINITY);
    }

    irqlock_release(&irqlock);
    return len;
}

static fs_ssize_t hid_file_write(struct file *, fs_size_t *, const void *, fs_size_t) {
    return OPAL_ENOTSUPP;
}

static kerrno_t hid_file_truncate(struct file *, fs_size_t) {
    return OPAL_ENOTSUPP;
}

static const struct file_ops g_file_ops = {
    .close = hid_file_close,
    .seek = hid_file_seek,
    .read = hid_file_read,
    .write = hid_file_write,
    .truncate = hid_file_truncate,
};
