#ifndef OPAL_FS_VFS_H
#define OPAL_FS_VFS_H

#include <collections/linkedlist.h>

#include <opal/fs/types.h>

#define VFS_MAX_NAME 255

enum inode_flags {
    FS_INODE_DIR = 0x10,
};

struct superblock;
struct inode;
struct file;

struct path_entry {
    struct path_entry *parent;
    struct inode *inode;
    struct superblock *mounted;

    struct linkedlist children;
    struct linkedlist_link link;
    char name[VFS_MAX_NAME + 1];
};

struct superblock_ops {
    void (*umount)(struct superblock *fs);
};

struct superblock {
    const struct superblock_ops *ops;
    struct inode *root;
};

struct inode_ops {
    void (*close)(struct inode *inode);
    fs_status_t (*open)(struct inode *inode, struct file **file_out);
    fs_status_t (*lookup)(struct inode *inode, struct path_entry *pe);
    fs_status_t (*create)(
        struct inode *inode, struct path_entry *pe,
        enum inode_flags flags, const char *filename, struct path_entry **child_out
    );
};

struct inode {
    const struct inode_ops *ops;
    enum inode_flags flags;
    unsigned refcount;
};

struct file_ops {
    void (*close)(struct file *file);
    fs_status_t (*seek)(struct file *file, fs_off_t offset, enum fs_seek origin, fs_size_t *pos);
    fs_ssize_t (*read)(struct file *file, fs_size_t pos, void *buffer, fs_size_t size);
    fs_ssize_t (*write)(struct file *file, fs_size_t pos, const void *buffer, fs_size_t size, bool append);
    fs_status_t (*truncate)(struct file *file, fs_size_t size);
};

struct file {
    const struct file_ops *ops;
    unsigned refcount;
};

void vfs_init(void);
fs_status_t vfs_mount_root(struct superblock *sb);
fs_status_t vfs_lookup_path(const char *path, struct path_entry **found, const char **unresolved_path);
fs_status_t vfs_create_path(const char *path, enum inode_flags flags, bool truncate, struct file **file_out);
fs_status_t vfs_open_path(const char *path, struct file **file_out);

bool path_entry_init(struct path_entry *restrict pe, struct path_entry *restrict parent, struct inode *inode);
bool path_entry_remove(struct path_entry *pe);
fs_status_t path_entry_lookup(struct path_entry *pe, const char *path, struct path_entry **found, const char **unresolved_path);
fs_status_t path_entry_create(struct path_entry *pe, const char *path, enum inode_flags flags, bool truncate, struct file **file_out);
fs_status_t path_entry_open(struct path_entry *pe, const char *path, struct file **file_out);

void superblock_init(struct superblock *sb, const struct superblock_ops *ops);

void inode_init(struct inode *inode, const struct inode_ops *ops);
void inode_retain(struct inode *inode);
void inode_release(struct inode *inode);

void file_init(struct file *file, const struct file_ops *ops);
void file_retain(struct file *file);
void file_release(struct file *file);

#endif
