#ifndef OPAL_FS_VFS_H
#define OPAL_FS_VFS_H

#include <collections/linkedlist.h>

#include <opal/fs/types.h>
#include <opal/fs/hstr.h>
#include <opal/task/mutex.h>

#define VFS_MAX_NAME UINT16_MAX

enum inode_flags {
    INODE_NORMAL = 0,
    INODE_DIR = 0x1,
    INODE_DEV = 0x2,
    INODE_PIPE = 0x4,
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

enum file_mode : uint16_t {
    FILE_NONE = 0,
    FILE_READ = OPEN_READ,
    FILE_WRITE = OPEN_WRITE,
    FILE_APPEND = OPEN_APPEND,

    FILE_POSLOCK = 0x10,
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
    struct hstr name;

    unsigned refcount;
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
    kerrno_t (*open)(struct inode *inode, enum open_mode mode, struct file **file_out);
    kerrno_t (*lookup)(struct inode *inode, struct path_entry *pe);
    kerrno_t (*create)(struct inode *inode, struct path_entry *pe, enum inode_flags flags);
};

struct inode {
    const struct inode_ops *ops;
    unsigned refcount;
    enum inode_flags flags;
};

struct file_ops {
    void (*close)(struct file *file);
    fs_ssize_t (*seek)(struct file *file, fs_off_t offset, enum fs_seek origin);
    fs_ssize_t (*read)(struct file *file, fs_size_t *pos, void *buffer, fs_size_t size);
    fs_ssize_t (*write)(struct file *file, fs_size_t *pos, const void *buffer, fs_size_t size);
    kerrno_t (*truncate)(struct file *file, fs_size_t size);
    kerrno_t (*ioctl)(struct file *file, uintptr_t op, uintptr_t arg);
};

struct file {
    const struct file_ops *ops;
    unsigned refcount;
    enum file_mode mode;

    struct mutex pos_mutex;
    fs_size_t pos;
};

void vfs_init(void);
struct path_entry *vfs_get_root(void);
kerrno_t vfs_mount_path(
    struct path_entry *pe, const char *path, struct superblock *sb, struct path_entry **mounted);
kerrno_t vfs_lookup_path(struct path_entry *pe, const char *path, struct path_entry **found,
    const char **unresolved_path);
kerrno_t vfs_create_path(struct path_entry *pe, const char *path, enum inode_flags flags,
    enum open_mode mode, struct file **file_out);
kerrno_t vfs_open_path(
    struct path_entry *pe, const char *path, enum open_mode mode, struct file **file_out);

void path_entry_retain(struct path_entry *pe);
void path_entry_release(struct path_entry *pe);
kerrno_t path_entry_mount_super(struct path_entry *pe, struct superblock *sb);
kerrno_t path_entry_add(
    struct path_entry *parent, struct inode *inode, struct hstr *name, struct path_entry **out);
kerrno_t path_entry_lookup(
    struct path_entry *pe, const char *name, size_t len, struct path_entry **found);
kerrno_t path_entry_create(
    struct path_entry *pe, enum inode_flags flags, enum open_mode mode, struct file **file_out);
kerrno_t path_entry_open(struct path_entry *pe, enum open_mode mode, struct file **file_out);

void superblock_init(struct superblock *sb, const struct superblock_ops *ops);

void inode_init(struct inode *inode, const struct inode_ops *ops);
void inode_retain(struct inode *inode);
void inode_release(struct inode *inode);

void file_init(struct file *file, const struct file_ops *ops, enum file_mode mode);
void file_retain(struct file *file);
void file_release(struct file *file);

fs_ssize_t file_seek(struct file *file, fs_off_t offset, enum fs_seek origin);
fs_ssize_t file_read(struct file *file, void *buffer, fs_size_t size);
fs_ssize_t file_write(struct file *file, const void *buffer, fs_size_t size);
kerrno_t file_truncate(struct file *file, fs_size_t size);
kerrno_t file_ioctl(struct file *file, uintptr_t op, uintptr_t arg);

static inline enum file_mode fmode_from_omode(enum open_mode mode) {
    return (enum file_mode)(mode & OPEN_MASK_FMODE);
}

#endif
