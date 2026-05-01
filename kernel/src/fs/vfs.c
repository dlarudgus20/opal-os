#include <limits.h>

#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/fs/vfs.h>
#include <opal/mm/kmalloc.h>

#define MAX_REFC INT_MAX

static struct path_entry g_root;

static void path_entry_init_empty(struct path_entry *pe);

void vfs_init(void) {
    path_entry_init_empty(&g_root);
}

struct path_entry *vfs_get_root(void) {
    return &g_root;
}

kerrno_t vfs_mount_path(struct path_entry *pe, const char *path, struct superblock *sb, struct path_entry **mounted) {
    struct path_entry *mount_pe = NULL;
    const char *unresolved_path = NULL;
    kerrno_t result = vfs_lookup_path(pe, path, &mount_pe, &unresolved_path);
    if (!mount_pe || unresolved_path[0] != '\0') {
        if (mount_pe) {
            path_entry_release(mount_pe);
        }
        return result;
    }

    result = path_entry_mount_super(mount_pe, sb);
    if (result != OPAL_OK) {
        path_entry_release(mount_pe);
        return result;
    }
    *mounted = mount_pe;
    return OPAL_OK;
}

kerrno_t vfs_lookup_path(struct path_entry *pe, const char *path, struct path_entry **found, const char **unresolved_path) {
    *found = NULL;
    if (unresolved_path) {
        *unresolved_path = path;
    }

    const char *subpath = path;
    if (path[0] == '/') {
        pe = &g_root;
        subpath += strspn(subpath, "/");
    } else if (path[0] == '\0') {
        return OPAL_EINVAL;
    } else if (!pe) {
        return OPAL_EINVAL;
    }

    path_entry_retain(pe);
    *found = pe;
    if (unresolved_path) {
        *unresolved_path = subpath;
    }

    while (pe->inode) {
        if (subpath[0] == '\0') {
            return OPAL_OK;
        } else if (!(pe->inode->flags & FS_INODE_DIR)) {
            return OPAL_ENOTDIR;
        }

        size_t sep = strcspn(subpath, "/");
        if (sep > VFS_MAX_NAME) {
            return OPAL_ENOENT;
        }

        struct path_entry *lookup;
        kerrno_t result = path_entry_lookup(pe, subpath, sep, &lookup);
        if (!lookup) {
            return result;
        }
        path_entry_release(pe);
        pe = lookup;
        subpath += sep;
        subpath += strspn(subpath, "/");

        *found = pe;
        if (unresolved_path) {
            *unresolved_path = subpath;
        }

        if (result != OPAL_OK) {
            return result;
        }
    }

    return OPAL_ENOENT;
}

kerrno_t vfs_create_path(struct path_entry *pe, const char *path, enum inode_flags flags, bool truncate, struct file **file_out) {
    struct path_entry *found;
    const char *unresolved_path;
    kerrno_t result = vfs_lookup_path(pe, path, &found, &unresolved_path);
    if (!found || *unresolved_path != '\0') {
        if (found) {
            path_entry_release(found);
        }
        return result;
    }

    result = path_entry_create(found, flags, truncate, file_out);
    path_entry_release(found);
    return result;
}

kerrno_t vfs_open_path(struct path_entry *pe, const char *path, struct file **file_out) {
    struct path_entry *found;
    kerrno_t result = vfs_lookup_path(pe, path, &found, NULL);
    if (result != OPAL_OK) {
        if (found) {
            path_entry_release(found);
        }
        return result;
    }

    struct inode *inode = found->inode;
    result = inode->ops->open(inode, file_out);
    path_entry_release(found);
    return result;
}

static void path_entry_init(struct path_entry *pe) {
    pe->inode = NULL;
    pe->mounted = NULL;
    pe->refcount = 1;
    linkedlist_init(&pe->children);
}

static void path_entry_init_empty(struct path_entry *pe) {
    path_entry_init(pe);
    pe->parent = NULL;
    pe->name = HSTR_EMPTY;
}

void path_entry_retain(struct path_entry *pe) {
    assert(pe->refcount < MAX_REFC);
    pe->refcount++;
}

void path_entry_release(struct path_entry *pe) {
    assert(pe->refcount > 0);
    pe->refcount--;
}

kerrno_t path_entry_mount_super(struct path_entry *pe, struct superblock *sb) {
    if (!sb || !sb->root || !(sb->root->flags & FS_INODE_DIR)) {
        return OPAL_EINVAL;
    }
    if (pe->inode) {
        return OPAL_EBUSY;
    }

    pe->mounted = sb;
    pe->inode = sb->root;
    return OPAL_OK;
}

kerrno_t path_entry_add(struct path_entry *parent, struct inode *inode, struct hstr *name, struct path_entry **out) {
    if (!parent->inode || !(parent->inode->flags & FS_INODE_DIR)) {
        return OPAL_ENOTDIR;
    }

    struct linkedlist_link *link = linkedlist_head(&parent->children);
    struct path_entry *found = NULL;
    while (!linkedlist_is_nil(&parent->children, link)) {
        struct path_entry *child = container_of(link, struct path_entry, link);
        link = link->next;
        if (hstr_equal(name, &child->name)) {
            if (child->inode) {
                *out = NULL;
                return OPAL_EEXIST;
            }
            found = child;
            break;
        }
    }

    if (!found) {
        found = kzalloc(sizeof(*found));
        if (!found) {
            *out = NULL;
            return OPAL_ENOMEM;
        }
        path_entry_init(found);
        found->parent = parent;
        found->name = *name;
        linkedlist_push_back(&parent->children, &found->link);
    } else {
        hstr_free(name);
        path_entry_retain(found);
    }

    found->inode = inode;
    found->mounted = NULL;

    *out = found;
    return OPAL_OK;
}

static kerrno_t create_negative(struct path_entry *parent, const struct hstr *name, struct path_entry **out) {
    struct hstr str = hstr_clone(name);
    if (hstr_is_null(&str)) {
        *out = NULL;
        return OPAL_ENOMEM;
    }

    return path_entry_add(parent, NULL, &str, out);
}

static struct path_entry *find_child(struct path_entry *parent, const struct hstr *name) {
    linkedlist_foreach(ptr, &parent->children) {
        struct path_entry *child = container_of(ptr, struct path_entry, link);
        if (hstr_equal(name, &child->name)) {
            path_entry_retain(child);
            return child;
        }
    }
    return NULL;
}

kerrno_t path_entry_lookup(struct path_entry *pe, const char *name, size_t len, struct path_entry **found) {
    if (len == 0) {
        return OPAL_EINVAL;
    }
    if (len > VFS_MAX_NAME) {
        return OPAL_ENOENT;
    }

    struct hstr hs = hstr_stack(name, len);
    struct path_entry *child = find_child(pe, &hs);
    if (child) {
        *found = child;
        return child->inode ? OPAL_OK : OPAL_ENOENT;
    }

    kerrno_t result = pe->inode->ops->lookup(pe->inode, pe);
    if (result != OPAL_OK) {
        *found = NULL;
        return result;
    }

    child = find_child(pe, &hs);
    if (child) {
        *found = child;
        return child->inode ? OPAL_OK : OPAL_ENOENT;
    }

    return create_negative(pe, &hs, found);
}

kerrno_t path_entry_create(struct path_entry *pe, enum inode_flags flags, bool truncate, struct file **file_out) {
    kerrno_t result;
    if (!pe->inode) {
        if (!pe->parent) {
            return OPAL_ENOENT;
        }

        struct inode *pinode = pe->parent->inode;
        result = pinode->ops->create(pinode, pe, flags);
        if (result != OPAL_OK) {
            return result;
        }

        struct inode *inode = pe->inode;
        result = inode->ops->open(inode, file_out);
        return result;
    } else {
        if (!truncate) {
            return OPAL_EEXIST;
        }

        struct inode *inode = pe->inode;
        struct file *file;
        result = inode->ops->open(inode, &file);
        if (result != OPAL_OK) {
            return result;
        }
        result = file->ops->truncate(file, 0);
        if (result != OPAL_OK) {
            file_release(file);
            return result;
        }

        *file_out = file;
        return OPAL_OK;
    }
}

void superblock_init(struct superblock *sb, const struct superblock_ops *ops) {
    sb->ops = ops;
    sb->root = NULL;
}

void inode_init(struct inode *inode, const struct inode_ops *ops) {
    inode->ops = ops;
    inode->flags = 0;
    inode->refcount = 1;
}

void inode_retain(struct inode *inode) {
    assert(inode->refcount < MAX_REFC);
    inode->refcount++;
}

void inode_release(struct inode *inode) {
    assert(inode->refcount > 0);
    if (--inode->refcount == 0) {
        inode->ops->close(inode);
    }
}

void file_init(struct file *file, const struct file_ops *ops) {
    file->ops = ops;
    file->refcount = 1;
}

void file_retain(struct file *file) {
    assert(file->refcount < MAX_REFC);
    file->refcount++;
}

void file_release(struct file *file) {
    assert(file->refcount > 0);
    if (--file->refcount == 0) {
        file->ops->close(file);
    }
}
