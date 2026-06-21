#include <limits.h>
#include <stdalign.h>

#include <kc/kassert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/fs/vfs.h>
#include <opal/fs/globals.h>
#include <opal/mm/kmalloc.h>

#define MAX_REFC INT_MAX

static struct path_entry g_root;

static void path_entry_init_empty(struct path_entry *pe);
static fs_ssize_t file_dir_read(struct file *file, fs_size_t *pos, void *buffer, fs_size_t size);

void vfs_init(void) {
    path_entry_init_empty(&g_root);
    vfs_globals_init();
}

struct path_entry *vfs_get_root(void) {
    return &g_root;
}

kerrno_t vfs_mount_path(
    struct path_entry *pe, const char *path, struct superblock *sb, struct path_entry **mounted) {
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
    if (!kerrno_ok(result)) {
        path_entry_release(mount_pe);
        return result;
    }
    *mounted = mount_pe;
    return OPAL_OK;
}

kerrno_t vfs_lookup_path(struct path_entry *pe, const char *path, struct path_entry **found,
    const char **unresolved_path) {
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
        } else if (!(pe->inode->flags & INODE_DIR)) {
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

        if (!kerrno_ok(result)) {
            return result;
        }
    }

    return OPAL_ENOENT;
}

kerrno_t vfs_create_path(struct path_entry *pe, const char *path, enum inode_flags flags,
    enum open_mode mode, struct file **file_out) {
    struct path_entry *found;
    const char *unresolved_path;
    kerrno_t result = vfs_lookup_path(pe, path, &found, &unresolved_path);
    if (!found) {
        return result;
    }
    if (*unresolved_path != '\0') {
        goto ret;
    }

    result = path_entry_create(found, flags, mode, file_out);

ret:
    path_entry_release(found);
    return result;
}

kerrno_t vfs_open_path(
    struct path_entry *pe, const char *path, enum open_mode mode, struct file **file_out) {
    return vfs_create_path(pe, path, INODE_NORMAL, mode, file_out);
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
    kassert(pe->refcount < MAX_REFC);
    pe->refcount++;
}

void path_entry_release(struct path_entry *pe) {
    kassert(pe->refcount > 0);
    pe->refcount--;
}

kerrno_t path_entry_mount_super(struct path_entry *pe, struct superblock *sb) {
    if (!sb || !sb->root || !(sb->root->flags & INODE_DIR)) {
        return OPAL_EINVAL;
    }
    if (pe->inode) {
        return OPAL_EBUSY;
    }

    pe->mounted = sb;
    pe->inode = sb->root;
    return OPAL_OK;
}

kerrno_t path_entry_add(
    struct path_entry *parent, struct inode *inode, struct hstr *name, struct path_entry **out) {
    if (!parent->inode || !(parent->inode->flags & INODE_DIR)) {
        return OPAL_ENOTDIR;
    }

    struct linkedlist_link *link = linkedlist_head(&parent->children);
    struct path_entry *found = NULL;
    while (!linkedlist_is_nil(&parent->children, link)) {
        struct path_entry *child = container_of(link, struct path_entry, link);
        link = link->next;
        if (hstr_equal(name, &child->name)) {
            if (child->inode) {
                return OPAL_EEXIST;
            }
            found = child;
            break;
        }
    }

    if (!found) {
        found = kzalloc(sizeof(*found));
        if (!found) {
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

static kerrno_t create_negative(
    struct path_entry *parent, const struct hstr *name, struct path_entry **out) {
    struct hstr str = hstr_clone(name);
    if (hstr_is_null(&str)) {
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

struct path_lookup_ctx {
    const struct hstr *name;
    struct inode_dirent entry;
    bool found;
};

static bool path_lookup_cb(
    struct inode *, fs_size_t, const struct inode_dirent *entry, void *ctx_) {
    struct path_lookup_ctx *ctx = ctx_;
    if (entry->name_len != hstrlen(ctx->name)) {
        return true;
    }
    if (memcmp(entry->name, hstrget(ctx->name), entry->name_len) != 0) {
        return true;
    }

    ctx->entry = *entry;
    ctx->found = true;
    return false;
}

kerrno_t path_entry_lookup(
    struct path_entry *pe, const char *name, size_t len, struct path_entry **found) {
    // On error, returns with `found == NULL`
    // except when a negative entry is created or found.
    // In that case, returns `OPAL_ENOENT` with `found != NULL`.
    *found = NULL;

    if (len == 0) {
        return OPAL_EINVAL;
    }
    if (len > VFS_MAX_NAME) {
        return OPAL_ENOENT;
    }
    if (!pe->inode || !(pe->inode->flags & INODE_DIR)) {
        return OPAL_ENOTDIR;
    }

    struct hstr hs = hstr_stack(name, len);
    struct path_entry *child = find_child(pe, &hs);
    if (child) {
        *found = child;
        return child->inode ? OPAL_OK : OPAL_ENOENT;
    }

    if (!pe->inode->ops->iterate_dir || !pe->inode->ops->get_child) {
        return OPAL_ENOTSUPP;
    }

    struct path_lookup_ctx ctx = {
        .name = &hs,
        .found = false,
    };
    kerrno_t result = pe->inode->ops->iterate_dir(pe->inode, 0, path_lookup_cb, &ctx);
    if (!kerrno_ok(result)) {
        return result;
    }
    if (!ctx.found) {
        result = create_negative(pe, &hs, found);
        return kerrno_ok(result) ? OPAL_ENOENT : result;
    }

    struct hstr child_name = hstr_clone(&hs);
    if (hstr_is_null(&child_name)) {
        return OPAL_ENOMEM;
    }

    struct inode *inode = NULL;
    result = pe->inode->ops->get_child(pe->inode, ctx.entry.id, &inode);
    if (!kerrno_ok(result)) {
        hstr_free(&child_name);
        return result;
    }

    result = path_entry_add(pe, inode, &child_name, found);
    if (!kerrno_ok(result)) {
        hstr_free(&child_name);
        return result;
    }

    return OPAL_OK;
}

kerrno_t path_entry_create(
    struct path_entry *pe, enum inode_flags flags, enum open_mode mode, struct file **file_out) {
    struct inode *inode = pe->inode;
    bool existed = inode != NULL;
    if (!inode) {
        if (!(mode & OPEN_CREATE) || !pe->parent) {
            return OPAL_ENOENT;
        }

        struct inode *pinode = pe->parent->inode;
        if (!pinode->ops->create_child) {
            return OPAL_ENOTSUPP;
        }
        kerrno_t result = pinode->ops->create_child(pinode, &pe->name, flags, &inode);
        if (!kerrno_ok(result)) {
            return result;
        }
        pe->inode = inode;
    } else if (mode & OPEN_NONEXIST) {
        return OPAL_EEXIST;
    }

    struct file *file;
    kerrno_t result = inode->ops->open(inode, mode, &file);
    if (!kerrno_ok(result)) {
        return result;
    }

    if (existed && (mode & OPEN_TRUNC)) {
        result = file_truncate(file, 0);
        if (!kerrno_ok(result)) {
            file_release(file);
            return result;
        }
    }

    *file_out = file;
    return OPAL_OK;
}

kerrno_t path_entry_open(struct path_entry *pe, enum open_mode mode, struct file **file_out) {
    return path_entry_create(pe, INODE_NORMAL, mode, file_out);
}

void superblock_init(struct superblock *sb, const struct superblock_ops *ops) {
    sb->ops = ops;
    sb->root = NULL;
}

void inode_init(struct inode *inode, const struct inode_ops *ops, enum inode_flags flags) {
    kassert((flags & ~INODE_MASK_ALL) == 0);
    inode->ops = ops;
    inode->flags = flags;
    inode->refcount = 1;
}

void inode_retain(struct inode *inode) {
    kassert(inode->refcount < MAX_REFC);
    inode->refcount++;
}

void inode_release(struct inode *inode) {
    kassert(inode->refcount > 0);
    if (--inode->refcount == 0) {
        inode->ops->close(inode);
    }
}

void file_init(
    struct file *file, const struct file_ops *ops, enum file_mode mode, struct inode *inode) {
    file->ops = ops;
    file->inode = inode;
    if (inode) {
        inode_retain(inode);
        if (inode->flags & INODE_DIR) {
            mode |= FILE_POSLOCK;
        }
    }
    file->refcount = 1;
    file->mode = mode;
    file->pos = 0;
    if (mode & FILE_POSLOCK) {
        mutex_init(&file->pos_mutex);
    }
}

void file_retain(struct file *file) {
    kassert(file->refcount < MAX_REFC);
    file->refcount++;
}

static void file_close(struct file *file) {
    struct inode *inode = file->inode;
    file->ops->close(file);
    if (inode) {
        inode_release(inode);
    }
}

void file_release(struct file *file) {
    kassert(file->refcount > 0);
    if (--file->refcount == 0) {
        file_close(file);
    }
}

static void file_poslock_acquire(struct file *file) {
    if (file->mode & FILE_POSLOCK) {
        mutex_lock(&file->pos_mutex);
    }
}

static void file_poslock_release(struct file *file) {
    if (file->mode & FILE_POSLOCK) {
        mutex_unlock(&file->pos_mutex);
    }
}

fs_uint_or_err file_stat(struct file *file) {
    if (!file->inode) {
        return OPAL_ENOTSUPP;
    }
    return file->inode->flags;
}

fs_ssize_t file_seek(struct file *file, fs_off_t offset, enum fs_seek origin) {
    file_poslock_acquire(file);

    fs_ssize_t pos = file->ops->seek(file, offset, origin);
    if (kerrno_ok(pos)) {
        file->pos = pos;
    }

    file_poslock_release(file);
    return pos;
}

fs_ssize_t file_read(struct file *file, void *buffer, fs_size_t size) {
    if (!(file->mode & FILE_READ)) {
        return OPAL_ENOTSUPP;
    }
    file_poslock_acquire(file);

    fs_size_t pos = file->pos;
    fs_ssize_t n;
    if (file->inode && (file->inode->flags & INODE_DIR)) {
        n = file_dir_read(file, &pos, buffer, size);
    } else {
        n = file->ops->read(file, &pos, buffer, size);
    }

    if (kerrno_ok(n)) {
        file->pos = pos;
    }

    file_poslock_release(file);
    return n;
}

struct file_dir_read_ctx {
    unsigned char *buffer;
    fs_size_t size;
    fs_size_t written;
    fs_size_t count;
    kerrno_t error;
};

static fs_size_t dirent_record_size(uint16_t name_len) {
    return align_ceil_fsz_p2(sizeof(struct dirent) + name_len, alignof(struct dirent));
}

static bool file_dir_read_cb(
    struct inode *, fs_size_t, const struct inode_dirent *entry, void *ctx_) {
    struct file_dir_read_ctx *ctx = ctx_;
    fs_size_t record_size = dirent_record_size(entry->name_len);
    if (record_size > ctx->size - ctx->written) {
        if (ctx->written == 0) {
            ctx->error = OPAL_EBUFSIZE;
        }
        return false;
    }

    struct dirent *out = (struct dirent *)(ctx->buffer + ctx->written);
    out->next_offset = 0;
    out->name_len = entry->name_len;
    out->flags = entry->flags;
    memcpy(out->name, entry->name, entry->name_len);
    memset((unsigned char *)out + sizeof(*out) + entry->name_len, 0,
        record_size - sizeof(*out) - entry->name_len);

    ctx->written += record_size;
    ctx->count++;
    return true;
}

static void dirent_fix_offsets(void *buffer, fs_size_t size) {
    fs_size_t pos = 0;
    while (pos < size) {
        struct dirent *entry = (struct dirent *)((unsigned char *)buffer + pos);
        fs_size_t record_size = dirent_record_size(entry->name_len);
        if (pos + record_size >= size) {
            entry->next_offset = 0;
            return;
        }
        entry->next_offset = (uint32_t)record_size;
        pos += record_size;
    }
}

static fs_ssize_t file_dir_read(struct file *file, fs_size_t *pos, void *buffer, fs_size_t size) {
    if (size == 0) {
        return 0;
    }
    if (!file->inode->ops->iterate_dir) {
        return OPAL_ENOTSUPP;
    }

    struct file_dir_read_ctx ctx = {
        .buffer = buffer,
        .size = size,
        .written = 0,
        .count = 0,
        .error = OPAL_OK,
    };
    kerrno_t result = file->inode->ops->iterate_dir(file->inode, *pos, file_dir_read_cb, &ctx);
    if (!kerrno_ok(result)) {
        return result;
    }
    if (!kerrno_ok(ctx.error)) {
        return ctx.error;
    }

    dirent_fix_offsets(buffer, ctx.written);
    *pos += ctx.count;
    return (fs_ssize_t)ctx.written;
}

fs_ssize_t file_write(struct file *file, const void *buffer, fs_size_t size) {
    if (!(file->mode & FILE_WRITE)) {
        return OPAL_ENOTSUPP;
    }
    file_poslock_acquire(file);

    fs_size_t pos = file->pos;
    fs_ssize_t n = file->ops->write(file, &pos, buffer, size);
    if (kerrno_ok(n)) {
        file->pos = pos;
    }

    file_poslock_release(file);
    return n;
}

kerrno_t file_truncate(struct file *file, fs_size_t size) {
    if (!(file->mode & FILE_WRITE)) {
        return OPAL_ENOTSUPP;
    }
    file_poslock_acquire(file);

    kerrno_t err = file->ops->truncate(file, size);
    if (kerrno_ok(err)) {
        file->pos = 0;
    }

    file_poslock_release(file);
    return err;
}

kerrno_t file_ioctl(struct file *file, uintptr_t op, uintptr_t arg) {
    if (!file->ops->ioctl) {
        return OPAL_ENOTSUPP;
    }
    return file->ops->ioctl(file, op, arg);
}
