#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/fs/fs_type.h>
#include <opal/fs/globals.h>
#include <opal/locks/irqlock.h>

static struct vfs_globals g_globals;

static struct fs_type g_devfs_type;

void vfs_globals_init(void) {
    linkedlist_init(&g_globals.fs_type_list);
    kobjfs_init(&g_globals.devfs);

    vfs_fstype_register(&g_devfs_type);
}

struct vfs_globals *vfs_globals(void) {
    return &g_globals;
}

bool vfs_fstype_register(struct fs_type *fs) {
    irqlock_t irqlock = irqlock_acquire();
    bool ok = false;

    linkedlist_foreach(ptr, &g_globals.fs_type_list) {
        struct fs_type *item = container_of(ptr, typeof(*item), link);
        if (strcmp(item->name, fs->name) == 0) {
            goto ret;
        }
    }

    linkedlist_push_back(&g_globals.fs_type_list, &fs->link);
    ok = true;

ret:
    irqlock_release(&irqlock);
    return ok;
}

struct fs_type *vfs_fstype_get(const char *name) {
    irqlock_t irqlock = irqlock_acquire();
    struct fs_type *fs;

    linkedlist_foreach(ptr, &g_globals.fs_type_list) {
        fs = container_of(ptr, typeof(*fs), link);
        if (strcmp(fs->name, name) == 0) {
            goto found;
        }
    }
    fs = NULL;

found:
    irqlock_release(&irqlock);
    return fs;
}

static kerrno_t devfs_mount(struct block_device *bdev, struct superblock **sb_out) {
    if (bdev) {
        return OPAL_EINVAL;
    }

    *sb_out = &g_globals.devfs.sb;
    return OPAL_OK;
}

static struct fs_type g_devfs_type = {
    .name = "devfs",
    .mount = devfs_mount,
};
