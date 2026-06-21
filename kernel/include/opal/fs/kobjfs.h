#ifndef OPAL_FS_KOBJFS_H
#define OPAL_FS_KOBJFS_H

#include <collections/linkedlist.h>

#include <opal/fs/vfs.h>

#define KODIR_NAMEBUF_LEN 16

struct ko_inode {
    struct inode inode;
    struct linkedlist_link link;
    fs_size_t ino;
    const char *name;
    bool is_kodir;
};

struct kobjfs;

struct kodir_inode {
    union {
        struct inode inode;
        struct ko_inode ko_inode;
    };
    struct kobjfs *fs;
    struct linkedlist children;
    char namebuf[KODIR_NAMEBUF_LEN];
};

struct kobjfs {
    struct superblock sb;
    struct kodir_inode root;
    fs_size_t next_ino;
};

void kobjfs_init(struct kobjfs *sb);
void kobjfs_inode_init(struct ko_inode *inode, const struct inode_ops *ops, const char *name);
void kobjfs_dir_add_inode(struct kodir_inode *dir, struct ko_inode *child);
kerrno_t kobjfs_get_subdir(struct kodir_inode *dir, const char *name, struct kodir_inode **out);

static inline void kodir_retain(struct kodir_inode *dir) {
    inode_retain(&dir->inode);
}

static inline void kodir_release(struct kodir_inode *dir) {
    inode_release(&dir->inode);
}

static inline void kobjfs_add_inode(struct kobjfs *sb, struct ko_inode *child) {
    kobjfs_dir_add_inode(&sb->root, child);
}

#endif
