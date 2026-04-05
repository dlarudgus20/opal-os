#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/fs/vfs.h>
#include <opal/mm/kmalloc.h>

static struct path_entry g_root;

void vfs_init(void) {
    strcpy_sized(g_root.name, VFS_MAX_NAME + 1, "/");
    path_entry_init(&g_root, NULL, NULL);
}

fs_status_t vfs_mount_root(struct superblock *sb) {
    if (!sb || !sb->root || !(sb->root->flags & FS_INODE_DIR)) {
        return FS_ERR_INVAL;
    }
    if (g_root.inode) {
        return FS_ERR_BUSY;
    }
    path_entry_init(&g_root, NULL, sb->root);
    g_root.mounted = sb;
    return FS_OK;
}

fs_status_t vfs_lookup_path(const char *path, struct path_entry **found, const char **unresolved_path) {
    if (path[0] != '/') {
        return FS_ERR_INVAL;
    }
    if (!g_root.inode) {
        *found = NULL;
        if (unresolved_path) {
            *unresolved_path = path;
        }
        return FS_ERR_NOENT;
    }
    return path_entry_lookup(&g_root, path + 1, found, unresolved_path);
}

fs_status_t vfs_create_path(const char *path, enum inode_flags flags, bool truncate, struct file **file_out) {
    if (path[0] != '/') {
        return FS_ERR_INVAL;
    }
    return path_entry_create(&g_root, path + 1, flags, truncate, file_out);
}

fs_status_t vfs_open_path(const char *path, struct file **file_out) {
    if (path[0] != '/') {
        return FS_ERR_INVAL;
    }
    return path_entry_open(&g_root, path + 1, file_out);
}

bool path_entry_init(struct path_entry *restrict pe, struct path_entry *restrict parent, struct inode *inode) {
    if (parent) {
        struct linkedlist_link *link = linkedlist_head(&parent->children);
        while (!linkedlist_is_nil(&parent->children, link)) {
            struct path_entry *child = container_of(link, struct path_entry, link);
            link = link->next;
            if (strncmp(pe->name, child->name, VFS_MAX_NAME + 1) == 0) {
                if (child->inode) {
                    return false;
                }
                linkedlist_remove(link->prev);
                kfree(child, sizeof(*child));
                break;
            }
        }
        linkedlist_push_back(&parent->children, &pe->link);
    }

    pe->parent = parent;
    pe->inode = inode;
    pe->mounted = NULL;
    linkedlist_init(&pe->children);
    return true;
}

static struct path_entry *create_negative(struct path_entry *parent, const char *name, size_t len) {
    struct path_entry *pe = kzalloc(sizeof(*pe));
    if (!pe) {
        return NULL;
    }

    strncpy_sized(pe->name, VFS_MAX_NAME + 1, name, len);
    path_entry_init(pe, parent, NULL);
    return pe;
}

bool path_entry_remove(struct path_entry *pe) {
    if (pe->mounted) {
        return false;
    }
    if (!linkedlist_is_empty(&pe->children)) {
        return false;
    }

    if (pe->parent) {
        linkedlist_remove(&pe->link);
    }
    if (pe->inode) {
        inode_release(pe->inode);
    }

    return true;
}

static struct path_entry *find_child(struct path_entry *parent, const char *name, size_t len) {
    linkedlist_foreach(ptr, &parent->children) {
        struct path_entry *child = container_of(ptr, struct path_entry, link);
        if (memcmp(child->name, name, len) == 0 && child->name[len] == '\0') {
            return child;
        }
    }
    return NULL;
}

fs_status_t path_entry_lookup(struct path_entry *pe, const char *path, struct path_entry **found, const char **unresolved_path) {
    *found = NULL;
    if (unresolved_path) {
        *unresolved_path = NULL;
    }

    const char *subpath = path;
    while (pe->inode) {
        *found = pe;
        if (unresolved_path) {
            *unresolved_path = subpath;
        }

        if (subpath[0] == '\0') {
            return FS_OK;
        }

        size_t sep = strcspn(subpath, "/");
        if (sep > VFS_MAX_NAME) {
            return FS_ERR_NOENT;
        }

        bool retry = false;
        while (1) {
            struct path_entry *child = find_child(pe, subpath, sep);
            if (child) {
                pe = child;
                subpath += sep;
                if (*subpath == '/') {
                    subpath++;
                }
                break;
            }

            if (retry) {
                create_negative(pe, subpath, sep);
                return FS_ERR_NOENT;
            }

            fs_status_t result = pe->inode->ops->lookup(pe->inode, pe);
            if (result != FS_OK) {
                return result;
            }
            retry = true;
        }
    }

    return FS_ERR_NOENT;
}

fs_status_t path_entry_create(struct path_entry *pe, const char *path, enum inode_flags flags, bool truncate, struct file **file_out) {
    if (!pe->inode) {
        return FS_ERR_NOENT;
    }

    struct path_entry *found;
    const char *unresolved_path;
    fs_status_t result = path_entry_lookup(pe, path, &found, &unresolved_path);
    if (result == FS_ERR_NOENT && strchr(unresolved_path, '/') == NULL) {
        struct inode *inode = found->inode;
        struct path_entry *created;
        result = inode->ops->create(inode, found, flags, unresolved_path, &created);
        if (result != FS_OK) {
            return result;
        }
        return created->inode->ops->open(created->inode, file_out);
    } else if (result == FS_OK) {
        if (!truncate) {
            return FS_ERR_EXIST;
        }

        struct inode *inode = found->inode;
        struct file *file;
        result = inode->ops->open(inode, &file);
        if (result != FS_OK) {
            return result;
        }
        result = file->ops->truncate(file, 0);
        if (result != FS_OK) {
            file_release(file);
            return result;
        }

        *file_out = file;
        return FS_OK;
    } else {
        return result;
    }
}

fs_status_t path_entry_open(struct path_entry *pe, const char *path, struct file **file_out) {
    if (!pe->inode) {
        return FS_ERR_NOENT;
    }

    struct path_entry *found;
    fs_status_t result = path_entry_lookup(pe, path, &found, NULL);
    if (result != FS_OK) {
        return result;
    }

    struct inode *inode = found->inode;
    return inode->ops->open(inode, file_out);
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
    inode->refcount++;
}

void inode_release(struct inode *inode) {
    if (--inode->refcount == 0) {
        inode->ops->close(inode);
    }
}

void file_init(struct file *file, const struct file_ops *ops) {
    file->ops = ops;
    file->refcount = 1;
}

void file_retain(struct file *file) {
    file->refcount++;
}

void file_release(struct file *file) {
    if (--file->refcount == 0) {
        file->ops->close(file);
    }
}
