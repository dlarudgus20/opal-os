#include <kc/string.h>
#include <kc/stdlib.h>

#include <opal/fs/cpio.h>
#include <opal/mm/kmalloc.h>

#define CPIO_NEWC_MAGIC "070701"
#define CPIO_CRC_MAGIC  "070702"
#define CPIO_HDR_SIZE   110u

#define CPIO_S_IFMT     0170000u
#define CPIO_S_IFREG    0100000u
#define CPIO_S_IFDIR    0040000u

struct cpio_node {
    struct inode inode;
    struct file file;

    struct cpio_node *parent;
    struct linkedlist children;
    struct linkedlist_link link;
    struct hstr name;

    const unsigned char *data;
    fs_size_t size;
};

struct cpio_sb {
    struct superblock sb;
    struct cpio_node *root;
};

static struct superblock_ops g_cpio_sb_ops;
static struct inode_ops g_cpio_inode_ops;
static struct file_ops g_cpio_file_ops;

static bool parse_hex_u32(const char *p, size_t len, uint32_t *value_out) {
    uint32_t value = 0;
    for (size_t i = 0; i < len; i++) {
        const char ch = p[i];
        uint8_t digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = (uint8_t)(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = (uint8_t)(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            digit = (uint8_t)(ch - 'A' + 10);
        } else {
            return false;
        }
        value = (value << 4) | digit;
    }

    *value_out = value;
    return true;
}

static bool advance_aligned4(size_t *off_io, size_t len) {
    const size_t off = *off_io;
    const size_t aligned = (size_t)align_ceil_fsz_p2((fs_size_t)off, 4);
    if (aligned < off || aligned > len) {
        return false;
    }
    *off_io = aligned;
    return true;
}

static struct cpio_node *find_child(struct cpio_node *parent, const char *name, size_t len) {
    struct hstr hs = hstr_stack(name, (uint32_t)len);
    linkedlist_foreach(ptr, &parent->children) {
        struct cpio_node *child = container_of(ptr, struct cpio_node, link);
        if (hstr_equal(&child->name, &hs)) {
            return child;
        }
    }
    return NULL;
}

static struct cpio_node *alloc_node(
    struct cpio_sb *sb, struct cpio_node *parent, const char *name, size_t len, bool is_dir) {
    struct cpio_node *node = kzalloc(sizeof(*node));
    if (!node) {
        return NULL;
    }

    inode_init(&node->inode, &g_cpio_inode_ops);
    file_init(&node->file, &g_cpio_file_ops);
    node->inode.flags = is_dir ? FS_INODE_DIR : 0;
    node->parent = parent;
    node->data = NULL;
    node->size = 0;
    linkedlist_init(&node->children);

    if (!parent && len == 0) {
        node->name = HSTR_EMPTY;
    } else {
        node->name = hstr_alloc(len);
        if (hstr_is_null(&node->name)) {
            kfree(node, sizeof(*node));
            return NULL;
        }
        memcpy((char *)hstrget(&node->name), name, len);
        hstr_rehash(&node->name);
    }

    if (parent) {
        linkedlist_push_back(&parent->children, &node->link);
    } else {
        sb->root = node;
        sb->sb.root = &node->inode;
    }

    return node;
}

static void free_tree(struct cpio_node *node) {
    while (!linkedlist_is_empty(&node->children)) {
        struct linkedlist_link *link = linkedlist_pop_front(&node->children);
        struct cpio_node *child = container_of(link, struct cpio_node, link);
        free_tree(child);
    }

    hstr_free(&node->name);
    kfree(node, sizeof(*node));
}

static kerrno_t ensure_dir_child(struct cpio_sb *sb, struct cpio_node *parent, const char *name,
    size_t len, struct cpio_node **out) {
    struct cpio_node *child = find_child(parent, name, len);
    if (!child) {
        child = alloc_node(sb, parent, name, len, true);
        if (!child) {
            return OPAL_ENOMEM;
        }
        *out = child;
        return OPAL_OK;
    }

    if (!(child->inode.flags & FS_INODE_DIR)) {
        return OPAL_EINVAL;
    }

    *out = child;
    return OPAL_OK;
}

static bool is_dot(const char *s, size_t len) {
    return len == 1 && s[0] == '.';
}

static bool is_dotdot(const char *s, size_t len) {
    return len == 2 && s[0] == '.' && s[1] == '.';
}

static kerrno_t upsert_leaf(struct cpio_sb *sb, struct cpio_node *parent, const char *name,
    size_t len, bool is_dir, const unsigned char *data, fs_size_t size) {
    struct cpio_node *child = find_child(parent, name, len);
    if (!child) {
        child = alloc_node(sb, parent, name, len, is_dir);
        if (!child) {
            return OPAL_ENOMEM;
        }
    }

    if (is_dir) {
        child->inode.flags |= FS_INODE_DIR;
        child->data = NULL;
        child->size = 0;
        return OPAL_OK;
    }

    if (child->inode.flags & FS_INODE_DIR) {
        if (!linkedlist_is_empty(&child->children)) {
            return OPAL_EINVAL;
        }
        child->inode.flags &= ~FS_INODE_DIR;
    }
    child->data = data;
    child->size = size;
    return OPAL_OK;
}

static kerrno_t add_archive_entry(struct cpio_sb *sb, const char *path, size_t path_len,
    bool is_dir, const unsigned char *data, fs_size_t size) {
    while (path_len > 0 && path[0] == '/') {
        path++;
        path_len--;
    }

    if (path_len == 0 || is_dot(path, path_len)) {
        return is_dir ? OPAL_OK : OPAL_EINVAL;
    }

    struct cpio_node *node = sb->root;
    size_t i = 0;
    while (i < path_len) {
        while (i < path_len && path[i] == '/') {
            i++;
        }
        if (i >= path_len) {
            break;
        }

        const size_t begin = i;
        while (i < path_len && path[i] != '/') {
            i++;
        }
        const size_t seg_len = i - begin;
        if (seg_len == 0 || seg_len > VFS_MAX_NAME || is_dotdot(path + begin, seg_len)) {
            return OPAL_EINVAL;
        }

        size_t next = i;
        while (next < path_len && path[next] == '/') {
            next++;
        }
        const bool is_last = next >= path_len;

        if (is_dot(path + begin, seg_len)) {
            if (is_last) {
                return is_dir ? OPAL_OK : OPAL_EINVAL;
            }
            i = next;
            continue;
        }

        if (!is_last) {
            kerrno_t result = ensure_dir_child(sb, node, path + begin, seg_len, &node);
            if (!kerrno_ok(result)) {
                return result;
            }
            i = next;
            continue;
        }

        return upsert_leaf(sb, node, path + begin, seg_len, is_dir, data, size);
    }

    return OPAL_OK;
}

static void cpio_umount(struct superblock *base) {
    struct cpio_sb *sb = container_of(base, struct cpio_sb, sb);
    if (sb->root) {
        free_tree(sb->root);
    }
    kfree(sb, sizeof(*sb));
}

static void cpio_inode_close(struct inode *) {}

static kerrno_t cpio_inode_open(struct inode *inode, struct file **file_out) {
    if (inode->flags & FS_INODE_DIR) {
        return OPAL_EISDIR;
    }

    struct cpio_node *node = container_of(inode, struct cpio_node, inode);
    *file_out = &node->file;
    file_retain(*file_out);
    return OPAL_OK;
}

static kerrno_t cpio_inode_lookup(struct inode *inode, struct path_entry *pe) {
    if (!(inode->flags & FS_INODE_DIR)) {
        return OPAL_ENOTDIR;
    }

    struct cpio_node *node = container_of(inode, struct cpio_node, inode);
    linkedlist_foreach(ptr, &node->children) {
        struct cpio_node *child = container_of(ptr, struct cpio_node, link);
        struct hstr hs = hstr_clone(&child->name);
        if (hstr_is_null(&hs)) {
            return OPAL_ENOMEM;
        }

        struct path_entry *out = NULL;
        kerrno_t result = path_entry_add(pe, &child->inode, &hs, &out);
        if (!kerrno_ok(result)) {
            hstr_free(&hs);
            if (result != OPAL_EEXIST) {
                return result;
            }
            continue;
        }
        path_entry_release(out);
    }

    return OPAL_OK;
}

static kerrno_t cpio_inode_create(struct inode *, struct path_entry *, enum inode_flags) {
    return OPAL_ENOTSUPP;
}

static void cpio_file_close(struct file *) {}

static kerrno_t cpio_file_seek(
    struct file *file, fs_off_t offset, enum fs_seek origin, fs_size_t *pos) {
    struct cpio_node *node = container_of(file, struct cpio_node, file);
    if (node->inode.flags & FS_INODE_DIR) {
        return OPAL_EISDIR;
    }

    if (origin == FS_SEEK_SET) {
        if (offset < 0 || (fs_size_t)offset > node->size) {
            return OPAL_ERANGE;
        }
        *pos = (fs_size_t)offset;
        return OPAL_OK;
    }
    if (origin == FS_SEEK_END) {
        if (offset > 0 || offset < -(fs_off_t)node->size) {
            return OPAL_ERANGE;
        }
        *pos = (fs_size_t)((fs_off_t)node->size + offset);
        return OPAL_OK;
    }

    return OPAL_EINVAL;
}

static fs_ssize_t cpio_file_read(struct file *file, fs_size_t pos, void *buffer, fs_size_t size) {
    struct cpio_node *node = container_of(file, struct cpio_node, file);
    if (node->inode.flags & FS_INODE_DIR) {
        return OPAL_EISDIR;
    }
    if (pos > node->size) {
        return OPAL_ERANGE;
    }
    if (size > node->size - pos) {
        size = node->size - pos;
    }
    if (size == 0) {
        return 0;
    }

    memcpy(buffer, node->data + pos, (size_t)size);
    return (fs_ssize_t)size;
}

static fs_ssize_t cpio_file_write(struct file *, fs_size_t, const void *, fs_size_t, bool) {
    return OPAL_ENOTSUPP;
}

static kerrno_t cpio_file_truncate(struct file *, fs_size_t) {
    return OPAL_ENOTSUPP;
}

static struct superblock_ops g_cpio_sb_ops = {
    .umount = cpio_umount,
};

static struct inode_ops g_cpio_inode_ops = {
    .close = cpio_inode_close,
    .open = cpio_inode_open,
    .lookup = cpio_inode_lookup,
    .create = cpio_inode_create,
};

static struct file_ops g_cpio_file_ops = {
    .close = cpio_file_close,
    .seek = cpio_file_seek,
    .read = cpio_file_read,
    .write = cpio_file_write,
    .truncate = cpio_file_truncate,
};

kerrno_t cpio_mount(void *cpio, size_t len, struct superblock **sb_out) {
    if (!cpio || !sb_out) {
        return OPAL_EINVAL;
    }

    kerrno_t result = OPAL_ENOMEM;
    struct cpio_sb *sb = kzalloc(sizeof(*sb));
    if (!sb) {
        return result;
    }
    superblock_init(&sb->sb, &g_cpio_sb_ops);

    if (!alloc_node(sb, NULL, "", 0, true)) {
        goto err;
    }

    const unsigned char *buf = cpio;
    size_t off = 0;
    while (off < len) {
        if (len - off < CPIO_HDR_SIZE) {
            result = OPAL_EINVAL;
            goto err;
        }

        const char *hdr = (const char *)(buf + off);
        if (memcmp(hdr, CPIO_NEWC_MAGIC, 6) != 0 && memcmp(hdr, CPIO_CRC_MAGIC, 6) != 0) {
            result = OPAL_EINVAL;
            goto err;
        }

        uint32_t mode = 0;
        uint32_t filesize = 0;
        uint32_t namesize = 0;
        if (!parse_hex_u32(hdr + 14, 8, &mode) || !parse_hex_u32(hdr + 54, 8, &filesize)
            || !parse_hex_u32(hdr + 94, 8, &namesize)) {
            result = OPAL_EINVAL;
            goto err;
        }

        off += CPIO_HDR_SIZE;
        if (namesize == 0 || namesize > len - off) {
            result = OPAL_EINVAL;
            goto err;
        }

        const char *name = (const char *)(buf + off);
        const size_t name_len = namesize - 1;
        if (name[name_len] != '\0' || memchr(name, '\0', name_len)) {
            result = OPAL_EINVAL;
            goto err;
        }
        off += namesize;

        if (!advance_aligned4(&off, len)) {
            result = OPAL_EINVAL;
            goto err;
        }
        if (filesize > len - off) {
            result = OPAL_EINVAL;
            goto err;
        }

        const unsigned char *data = buf + off;
        off += filesize;
        if (!advance_aligned4(&off, len)) {
            result = OPAL_EINVAL;
            goto err;
        }

        if (name_len == 10 && memcmp(name, "TRAILER!!!", 10) == 0) {
            break;
        }

        const uint32_t kind = mode & CPIO_S_IFMT;
        if (kind != CPIO_S_IFREG && kind != CPIO_S_IFDIR) {
            result = OPAL_ENOTSUPP;
            goto err;
        }

        result = add_archive_entry(sb, name, name_len, kind == CPIO_S_IFDIR, data, filesize);
        if (!kerrno_ok(result)) {
            goto err;
        }
    }

    *sb_out = &sb->sb;
    return OPAL_OK;

err:
    sb->sb.ops->umount(&sb->sb);
    return result;
}
