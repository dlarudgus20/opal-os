#ifndef OPAL_FS_GLOBALS_H
#define OPAL_FS_GLOBALS_H

#include <collections/linkedlist.h>

#include <opal/fs/kobjfs.h>

struct fs_type;

struct vfs_globals {
    struct linkedlist fs_type_list;
    struct kobjfs devfs;
};

void vfs_globals_init(void);
[[nodiscard]] struct vfs_globals *vfs_globals(void);

bool vfs_fstype_register(struct fs_type *fs);
struct fs_type *vfs_fstype_get(const char *name);

#endif
