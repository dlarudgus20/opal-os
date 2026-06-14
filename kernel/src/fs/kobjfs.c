#include <kc/kassert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/fs/kobjfs.h>
#include <opal/mm/kmalloc.h>
#include <opal/locks/irqlock.h>

#define KODIR_NAME_MAXLEN 4095

struct kodir_file {
    struct file file;
    struct kodir_inode *inode;
};

static const struct superblock_ops g_sb_ops;
static const struct inode_ops g_kodir_ops;
static const struct file_ops g_kodir_fops;

static bool kodir_init(struct kodir_inode *dir, const char *name);

void kobjfs_init(struct kobjfs *sb) {
    superblock_init(&sb->sb, &g_sb_ops);
    kodir_init(&sb->root, "");
    sb->sb.root = &sb->root.inode;
}

void kobjfs_inode_init(struct ko_inode *inode, const struct inode_ops *ops, const char *name) {
    inode_init(&inode->inode, ops);
    inode->inode.flags = INODE_DEV;
    inode->is_kodir = false;
    inode->name = name;
}

void kobjfs_dir_add_inode(struct kodir_inode *dir, struct ko_inode *child) {
    irqlock_t irqlock = irqlock_acquire();
    linkedlist_push_back(&dir->children, &child->link);
    irqlock_release(&irqlock);
}

static bool kodir_init(struct kodir_inode *dir, const char *name) {
    size_t len = strnlen_s(name, KODIR_NAME_MAXLEN + 1);
    if (len > KODIR_NAME_MAXLEN) {
        return false;
    }

    char *namebuf;
    if (len < KODIR_NAMEBUF_LEN) {
        namebuf = dir->namebuf;
    } else {
        namebuf = kzalloc(len + 1);
        if (!namebuf) {
            return false;
        }
        dir->namebuf[KODIR_NAMEBUF_LEN - 1] = '\xff';
    }
    memcpy(namebuf, name, len + 1);
    dir->ko_inode.name = namebuf;

    inode_init(&dir->inode, &g_kodir_ops);
    dir->inode.flags = INODE_DIR;
    dir->ko_inode.is_kodir = true;
    linkedlist_init(&dir->children);
    return true;
}

kerrno_t kobjfs_get_subdir(struct kodir_inode *dir, const char *name, struct kodir_inode **out) {
    irqlock_t irqlock = irqlock_acquire();
    kerrno_t err = OPAL_OK;

    linkedlist_foreach(ptr, &dir->children) {
        struct ko_inode *inode = container_of(ptr, typeof(*inode), link);
        if (strcmp(inode->name, name) == 0) {
            if (inode->is_kodir) {
                *out = container_of(inode, typeof(**out), inode);
                kodir_retain(*out);
            } else {
                err = OPAL_ENOTDIR;
            }
            goto ret;
        }
    }

    struct kodir_inode *sub = kzalloc(sizeof(*sub));
    if (!sub) {
        err = OPAL_ENOMEM;
        goto ret;
    }

    if (!kodir_init(sub, name)) {
        kfree(sub, sizeof(*sub));
        err = OPAL_ENOMEM;
        goto ret;
    }

    linkedlist_push_back(&dir->children, &sub->ko_inode.link);
    kodir_retain(sub);
    *out = sub;

ret:
    irqlock_release(&irqlock);
    return err;
}

static void sb_umount(struct superblock *) {
    panic();
}

static const struct superblock_ops g_sb_ops = {
    .umount = sb_umount,
};

static void kodir_inode_close(struct inode *inode) {
    struct ko_inode *ko = container_of(inode, typeof(*ko), inode);
    struct kodir_inode *kodir = container_of(ko, typeof(*kodir), ko_inode);
    if (kodir->namebuf[KODIR_NAMEBUF_LEN - 1] != 0) {
        kfree((void *)ko->name, strlen(ko->name) + 1);
    }
    kfree(kodir, sizeof(*kodir));
}

static kerrno_t kodir_inode_open(struct inode *inode, enum open_mode mode, struct file **file_out) {
    struct kodir_inode *kodir = container_of(inode, typeof(*kodir), inode);
    struct kodir_file *file = kzalloc(sizeof(*file));
    if (!file) {
        return OPAL_ENOMEM;
    }
    file_init(&file->file, &g_kodir_fops, fmode_from_omode(mode));
    inode_retain(inode);
    file->inode = kodir;
    *file_out = &file->file;
    return OPAL_OK;
}

static kerrno_t kodir_inode_lookup(struct inode *inode, struct path_entry *pe) {
    struct kodir_inode *kodir = container_of(inode, typeof(*kodir), inode);

    irqlock_t irqlock = irqlock_acquire();
    kerrno_t err = OPAL_OK;

    struct path_entry *out;
    linkedlist_foreach(ptr, &kodir->children) {
        struct ko_inode *child = container_of(ptr, typeof(*child), link);

        struct hstr name = hstrdup(child->name);
        if (hstr_is_null(&name)) {
            err = OPAL_ENOMEM;
            break;
        }

        err = path_entry_add(pe, &child->inode, &name, &out);
        if (!kerrno_ok(err)) {
            hstr_free(&name);
            if (err != OPAL_EEXIST) {
                break;
            }
        } else {
            path_entry_release(out);
        }
    }

    irqlock_release(&irqlock);
    return err;
}

static kerrno_t kodir_inode_create(struct inode *, struct path_entry *, enum inode_flags) {
    return OPAL_ENOTSUPP;
}

static const struct inode_ops g_kodir_ops = {
    .close = kodir_inode_close,
    .open = kodir_inode_open,
    .lookup = kodir_inode_lookup,
    .create = kodir_inode_create,
};

static void kodir_file_close(struct file *base) {
    struct kodir_file *file = container_of(base, typeof(*file), file);
    inode_release(&file->inode->inode);
    kfree(file, sizeof(*file));
}

static fs_ssize_t kodir_file_seek(struct file *, fs_off_t, enum fs_seek) {
    return OPAL_EISDIR;
}

static fs_ssize_t kodir_file_read(struct file *, fs_size_t *, void *, fs_size_t) {
    return OPAL_EISDIR;
}

static fs_ssize_t kodir_file_write(struct file *, fs_size_t *, const void *, fs_size_t) {
    return OPAL_EISDIR;
}

static kerrno_t kodir_file_truncate(struct file *, fs_size_t) {
    return OPAL_EISDIR;
}

static const struct file_ops g_kodir_fops = {
    .close = kodir_file_close,
    .seek = kodir_file_seek,
    .read = kodir_file_read,
    .write = kodir_file_write,
    .truncate = kodir_file_truncate,
};
