#include <kc/kassert.h>
#include <kc/stdlib.h>
#include <kc/string.h>
#include <kc/ctype.h>

#include <opal/mm/kmalloc.h>
#include <opal/utils/dynarray.h>

#include "fat.h"

static struct fat_inode_ops g_root_ops;
static struct fat_inode_ops g_inode_ops;
static struct file_ops g_root_fops;
static struct file_ops g_inode_fops;

static uint32_t dentry_get_first_cluster(struct fat_dentry *dentry, uint8_t bits) {
    uint32_t first_cluster = dentry->first_cluster_low;
    if (bits == 32) {
        first_cluster |= dentry->first_cluster_high << 16;
    }
    return first_cluster;
}

static void dentry_set_first_cluster(struct fat_dentry *dentry, uint8_t bits, uint32_t cluster) {
    if (bits == 12) {
        dentry->first_cluster_low = (uint16_t)(cluster & 0xfff);
    } else {
        dentry->first_cluster_low = (uint16_t)cluster;
        if (bits == 32) {
            dentry->first_cluster_high = (uint16_t)(cluster >> 16);
        }
    }
}

static void extract_filename(const char name[11], char out[13]) {
    int ext_begin = 8;
    int name_end = ext_begin;
    while (name_end > 0 && name[name_end - 1] == ' ') {
        name_end--;
    }

    int ext_end = 11;
    while (ext_end > ext_begin && name[ext_end - 1] == ' ') {
        ext_end--;
    }

    int nidx = 0;
    for (; nidx < VFS_MAX_NAME && nidx < name_end; nidx++) {
        out[nidx] = name[nidx];
    }

    if (ext_begin == ext_end || nidx >= VFS_MAX_NAME) {
        out[nidx] = '\0';
        return;
    }
    out[nidx++] = '.';

    int eidx = 0;
    for (; nidx + eidx < VFS_MAX_NAME && ext_begin + eidx < ext_end; eidx++) {
        out[nidx + eidx] = name[ext_begin + eidx];
    }
    out[nidx + eidx] = 0;
}

static struct hstr extract_filename_hs(const char name[11]) {
    char out[13];
    extract_filename(name, out);
    return hstrdup(out);
}

static bool pack_filename(const char *filename, size_t len, char out[11]) {
    const char *special = "!#$%&'()-@^_`{}~";
    memset(out, ' ', 11);

    if (len == 0) {
        return false;
    }

    size_t i = 0;
    for (; i < len && i < 8; i++) {
        char ch = filename[i];
        if (ch == '.') {
            break;
        }
        if (!(isupper(ch) || isdigit(ch) || strchr(special, ch))) {
            return false;
        }
        out[i] = toupper(ch);
    }
    if (i == len) {
        return true;
    }

    size_t j = 0;
    for (; i + 1 + j < len && j < 3; j++) {
        char ch = filename[i + 1 + j];
        if (ch == '\0') {
            break;
        }
        if (!(isupper(ch) || isdigit(ch) || strchr(special, ch))) {
            return false;
        }
        out[8 + j] = toupper(ch);
    }

    return i + 1 + j == len;
}

static kerrno_t dentry_lookup(
    struct fat_sb *sb, struct fat_dentry *dentries, uint32_t count, struct path_entry *pe) {
    for (uint32_t idx = 0; idx < count; idx++) {
        struct fat_dentry *dentry = &dentries[idx];

        unsigned char flag = (unsigned char)dentry->name[0];
        if (flag == 0) {
            break;
        } else if (flag == 0xe5 || flag == '.' || (dentry->attr & FAT_ATTR_VLABEL)) {
            continue;
        }

        struct fat_inode *child = kzalloc(sizeof(*child));
        if (!child) {
            return OPAL_ENOMEM;
        }
        struct fat_inode_base *parent = (struct fat_inode_base *)pe->inode;
        fat_inode_init(child, sb, parent, idx, dentry_get_first_cluster(dentry, sb->layout.bits));
        child->parent = parent;

        struct hstr hs = extract_filename_hs(dentry->name);
        if (hstr_is_null(&hs)) {
            inode_release(&child->inode);
            return OPAL_ENOMEM;
        }

        struct path_entry *out;
        kerrno_t result = path_entry_add(pe, &child->inode, &hs, &out);
        if (!kerrno_ok(result)) {
            hstr_free(&hs);
            inode_release(&child->inode);
            if (result != OPAL_EEXIST) {
                return result;
            }
        } else {
            path_entry_release(out);
        }
    }
    return OPAL_OK;
}

static void fat_root_file_init(
    struct fat_file *file, struct fat_root_inode *inode, enum file_mode mode) {
    file_init(&file->file, &g_root_fops, mode);
    inode_retain(&inode->inode);
    file->inode = &inode->base;
}

static void fat_file_init(struct fat_file *file, struct fat_inode *inode, enum file_mode mode) {
    file_init(&file->file, &g_inode_fops, mode);
    inode_retain(&inode->inode);
    file->inode = &inode->base;
}

void fat_root_init(
    struct fat_root_inode *inode, struct fat_sb *sb, uint32_t offset, uint16_t entries) {
    inode_init(&inode->inode, &g_root_ops.ops);
    inode->inode.flags = INODE_DIR;
    inode->sb = sb;
    inode->offset = offset;
    inode->entries = entries;
    inode->buffer = NULL;
    inode->buflen = 0;
}

static void root_inode_close(struct inode *base) {
    struct fat_root_inode *inode = container_of(base, struct fat_root_inode, inode);
    if (inode->buffer) {
        kfree(inode->buffer, inode->buflen);
    }
}

static kerrno_t root_inode_open(struct inode *base, enum open_mode mode, struct file **file_out) {
    struct fat_root_inode *inode = container_of(base, struct fat_root_inode, inode);
    struct fat_file *file = kzalloc(sizeof(*file));
    if (!file) {
        return OPAL_ENOMEM;
    }
    fat_root_file_init(file, inode, fmode_from_omode(mode) | FILE_POSLOCK);
    *file_out = &file->file;
    return OPAL_OK;
}

static kerrno_t root_inode_readall(struct fat_root_inode *inode) {
    uint32_t lsec = inode->sb->layout.bytes_per_sector / DISK_SECTOR_SIZE;
    kerrno_t result = OPAL_ENOMEM;

    if (inode->buffer) {
        return OPAL_OK;
    }

    size_t size = sizeof(*inode->buffer) * inode->entries;
    size_t sectors = (size + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE;
    inode->buflen = sectors * DISK_SECTOR_SIZE;
    inode->buffer = kzalloc(inode->buflen);
    if (!inode->buffer) {
        goto err;
    }

    struct disk_request *req =
        block_device_read(inode->sb->bdev, inode->offset * lsec, sectors, inode->buffer);
    if (!req) {
        result = OPAL_EBUSY;
        goto err_alloc;
    }

    disk_request_wait(req, TIMEOUT_INFINITY, &result);
    if (!kerrno_ok(result)) {
        goto err_alloc;
    }

    return OPAL_OK;

err_alloc:
    kfree(inode->buffer, inode->buflen);
    inode->buffer = NULL;
err:
    return result;
}

static kerrno_t root_inode_lookup(struct inode *base, struct path_entry *pe) {
    struct fat_root_inode *inode = container_of(base, struct fat_root_inode, inode);

    kerrno_t result = root_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    return dentry_lookup(inode->sb, inode->buffer, inode->entries, pe);
}

static kerrno_t fat_inode_create(
    struct inode *base, struct path_entry *pe, enum inode_flags flags) {
    struct fat_inode_base *inode = container_of(base, struct fat_inode_base, inode);

    if (!(inode->inode.flags & INODE_DIR)) {
        return OPAL_ENOTDIR;
    }

    char name[11];
    if (!pack_filename(hstrget(&pe->name), hstrlen(&pe->name), name)) {
        return OPAL_EINVAL;
    }

    kerrno_t result = OPAL_ENOMEM;
    struct fat_inode *child = kzalloc(sizeof(*child));
    if (!child) {
        goto err;
    }

    fs_ssize_t didx = inode->ops->alloc_dentry(inode);
    if (didx < 0) {
        result = didx;
        goto err_alloc;
    }

    uint8_t bits = inode->sb->layout.bits;
    struct fat_table *table = &inode->sb->table;

    uint32_t first_cluster = 0;
    if (flags & INODE_DIR) {
        result = table->ops->table_alloc(table, &first_cluster);
        if (!kerrno_ok(result)) {
            goto err_alloc;
        }
    }

    struct fat_dentry dentry = {
        .attr = (flags & INODE_DIR) ? FAT_ATTR_DIRECTORY : 0,
        .crt_time_tenth = 0,
        .crt_time = 0,
        .crt_date = 0,
        .last_access_date = 0,
        .first_cluster_high = 0,
        .write_time = 0,
        .write_date = 0,
        .first_cluster_low = 0,
        .file_size = 0,
    };
    dentry_set_first_cluster(&dentry, bits, first_cluster);
    memcpy(dentry.name, name, sizeof(name));

    result = inode->ops->write_dentry(inode, &dentry, didx);
    if (!kerrno_ok(result)) {
        goto err_alloc;
    }

    fat_inode_init(child, inode->sb, inode, didx, first_cluster);

    if (flags & INODE_DIR) {
        uint32_t parent_cluster = 0;
        if (&inode->sb->root != &inode->inode || bits == 32) {
            parent_cluster = ((struct fat_inode *)inode)->first_cluster;
        }

        // directory is already created; ignore errors
        memcpy(dentry.name, ".          ", 11);
        child->ops->write_dentry(&child->base, &dentry, 0);
        memcpy(dentry.name, "..         ", 11);
        dentry_set_first_cluster(&dentry, bits, parent_cluster);
        child->ops->write_dentry(&child->base, &dentry, 1);
        memset(&dentry, 0, sizeof(dentry));
        child->ops->write_dentry(&child->base, &dentry, 2);
    }

    pe->inode = &child->inode;
    return OPAL_OK;

err_alloc:
    kfree(child, sizeof(*child));
err:
    return result;
}

static kerrno_t root_inode_write_dentry(
    struct fat_inode_base *base, const struct fat_dentry *dentry, uint32_t index) {
    struct fat_root_inode *inode = container_of(base, struct fat_root_inode, base);

    kerrno_t result = root_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    if (index > inode->entries) {
        return OPAL_ERANGE;
    } else if (index == inode->entries) {
        return OPAL_ENOSPC;
    }

    struct fat_dentry old = inode->buffer[index];
    inode->buffer[index] = *dentry;

    uint32_t lsec = inode->sb->layout.bytes_per_sector / DISK_SECTOR_SIZE;

    uint32_t de_off = sizeof(*dentry) * index;
    uint32_t de_sec = de_off / DISK_SECTOR_SIZE;
    unsigned char *b = (unsigned char *)inode->buffer;
    result = fat_write_sectors(
        inode->sb->bdev, inode->offset * lsec + de_sec, 1, b + de_sec * DISK_SECTOR_SIZE);
    if (!kerrno_ok(result)) {
        inode->buffer[index] = old;
    }
    return result;
}

static fs_ssize_t root_inode_alloc_dentry(struct fat_inode_base *base) {
    struct fat_root_inode *inode = container_of(base, struct fat_root_inode, base);

    kerrno_t result = root_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    for (uint32_t i = 0; i < inode->entries; i++) {
        unsigned char flag = (unsigned char)inode->buffer[i].name[0];
        if (flag == 0 || flag == 0xe5) {
            return i;
        }
    }

    return OPAL_ENOSPC;
}

static void root_file_close(struct file *base) {
    struct fat_file *file = container_of(base, typeof(*file), file);
    inode_release(&file->inode->inode);
    kfree(file, sizeof(*file));
}

static fs_ssize_t root_file_seek(struct file *, fs_off_t, enum fs_seek) {
    return OPAL_ENOTSUPP;
}

static fs_ssize_t root_file_read(struct file *, fs_size_t *, void *, fs_size_t) {
    return OPAL_EISDIR;
}

static fs_ssize_t root_file_write(struct file *, fs_size_t *, const void *, fs_size_t) {
    return OPAL_EISDIR;
}

static kerrno_t root_file_truncate(struct file *, fs_size_t) {
    return OPAL_EISDIR;
}

static struct fat_inode_ops g_root_ops = {
    .ops = {
        .close = root_inode_close,
        .open = root_inode_open,
        .lookup = root_inode_lookup,
        .create = fat_inode_create,
    },
    .write_dentry = root_inode_write_dentry,
    .alloc_dentry = root_inode_alloc_dentry,
};

static struct file_ops g_root_fops = {
    .close = root_file_close,
    .seek = root_file_seek,
    .read = root_file_read,
    .write = root_file_write,
    .truncate = root_file_truncate,
};

static const struct fat_dentry *get_dentry(struct fat_inode *inode) {
    if (!inode->parent) {
        return NULL;
    }
    struct fat_dentry *b = (struct fat_dentry *)inode->parent->buffer;
    return b + inode->dentry_idx;
}

void fat_inode_init(struct fat_inode *inode, struct fat_sb *sb, struct fat_inode_base *parent,
    uint32_t dentry_idx, uint32_t first_cluster) {
    inode_init(&inode->inode, &g_inode_ops.ops);

    inode->sb = sb;
    inode->parent = parent;
    inode->dentry_idx = dentry_idx;
    inode->first_cluster = first_cluster;
    inode->buffer = NULL;
    inode->buflen = 0;

    const struct fat_dentry *dentry = get_dentry(inode);
    if (!dentry || (dentry->attr & FAT_ATTR_DIRECTORY)) {
        inode->inode.flags = INODE_DIR;
    }
}

static void fat_inode_close(struct inode *base) {
    struct fat_inode *inode = container_of(base, struct fat_inode, inode);
    if (inode->buffer) {
        kfree(inode->buffer, inode->buflen);
    }
    if (&inode->sb->root32 != inode) {
        kfree(inode, sizeof(*inode));
    }
}

[[nodiscard]] static bool is_cluster_eof(struct fat_layout *layout, uint32_t cluster) {
    return !(2 <= cluster && cluster < 0x0ffffff0 && cluster - 2 < layout->cluster_count);
}

static kerrno_t fat_inode_readall(struct fat_inode *inode) {
    if (inode->buffer) {
        return OPAL_OK;
    }

    const struct fat_dentry *dentry = get_dentry(inode);

    uint32_t filesize = UINT32_MAX;
    bool fsz_exists = false;
    if (dentry) {
        uint8_t attr = dentry->attr;
        if (!(attr & (FAT_ATTR_VLABEL | FAT_ATTR_DIRECTORY))) {
            filesize = dentry->file_size;
            fsz_exists = true;
        }
    }

    if (filesize == 0) {
        inode->filesize = 0;
        return OPAL_OK;
    }

    struct dynarray buffer;
    dynarray_init(&buffer);

    struct fat_layout *layout = &inode->sb->layout;
    uint16_t lsec = layout->bytes_per_sector / DISK_SECTOR_SIZE;
    uint32_t pspc = layout->sectors_per_cluster * lsec;
    uint32_t data_offset = layout->data_offset * lsec;

    kerrno_t result = OPAL_OK;

    struct fat_table *table = &inode->sb->table;
    uint32_t cluster = inode->first_cluster & 0x0fffffff;
    while (!is_cluster_eof(layout, cluster) && buffer.size < filesize) {
        void *cluster_buffer = dynarray_push_back(&buffer, pspc * DISK_SECTOR_SIZE);
        if (!cluster_buffer) {
            result = OPAL_ENOMEM;
            goto err;
        }

        result = fat_read_sectors(
            inode->sb->bdev, data_offset + (cluster - 2) * pspc, pspc, cluster_buffer);
        if (!kerrno_ok(result)) {
            goto err;
        }

        uint32_t entry;
        result = table->ops->table_at(table, cluster, &entry);
        if (!kerrno_ok(result)) {
            goto err;
        }
        cluster = entry & 0x0fffffff;
    }

    if (fsz_exists) {
        inode->filesize = dentry->file_size;
        if (inode->filesize > buffer.size) {
            inode->filesize = buffer.size;
        }
    } else {
        inode->filesize = buffer.size;
    }
    inode->buffer = buffer.data;
    inode->buflen = buffer.capacity;
    return OPAL_OK;

err:
    dynarray_destroy(&buffer);
    return result;
}

static kerrno_t fat_inode_open(struct inode *base, enum open_mode mode, struct file **file_out) {
    struct fat_inode *inode = container_of(base, struct fat_inode, inode);
    struct fat_file *file = kzalloc(sizeof(*file));
    if (!file) {
        return OPAL_ENOMEM;
    }
    fat_file_init(file, inode, fmode_from_omode(mode) | FILE_POSLOCK);
    *file_out = &file->file;
    return OPAL_OK;
}

static kerrno_t fat_inode_lookup(struct inode *base, struct path_entry *pe) {
    struct fat_inode *inode = container_of(base, struct fat_inode, inode);
    const struct fat_dentry *dentry = get_dentry(inode);
    if (dentry && !(dentry->attr & FAT_ATTR_DIRECTORY)) {
        return OPAL_ENOTDIR;
    }

    kerrno_t result = fat_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    struct fat_dentry *dentries = (struct fat_dentry *)inode->buffer;
    return dentry_lookup(inode->sb, dentries, inode->filesize / sizeof(*dentries), pe);
}

static void fat_file_close(struct file *base) {
    struct fat_file *file = container_of(base, typeof(*file), file);
    inode_release(&file->inode->inode);
    kfree(file, sizeof(*file));
}

static fs_ssize_t fat_file_seek(struct file *base, fs_off_t offset, enum fs_seek origin) {
    struct fat_file *file = container_of(base, typeof(*file), file);
    struct fat_inode *inode = container_of(file->inode, typeof(*inode), base);
    const struct fat_dentry *dentry = get_dentry(inode);
    if (!dentry || (dentry->attr & FAT_ATTR_DIRECTORY)) {
        return OPAL_EISDIR;
    }

    kerrno_t result = fat_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    if (origin == FS_SEEK_SET) {
        if (offset < 0 || offset > (fs_off_t)inode->filesize) {
            return OPAL_ERANGE;
        }
        return offset;
    } else if (origin == FS_SEEK_END) {
        if (offset > 0 || offset < -(fs_off_t)inode->filesize) {
            return OPAL_ERANGE;
        }
        return inode->filesize + offset;
    } else {
        return OPAL_EINVAL;
    }
}

static fs_ssize_t fat_file_read(struct file *base, fs_size_t *pos, void *buffer, fs_size_t size) {
    struct fat_file *file = container_of(base, typeof(*file), file);
    struct fat_inode *inode = container_of(file->inode, typeof(*inode), base);
    const struct fat_dentry *dentry = get_dentry(inode);
    if (!dentry || dentry->attr & FAT_ATTR_DIRECTORY) {
        return OPAL_EISDIR;
    }

    kerrno_t result = fat_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    uint32_t fsz = inode->filesize;
    if (*pos > fsz) {
        return OPAL_ERANGE;
    }
    if (size > fsz - *pos) {
        size = fsz - *pos;
    }

    if (size == 0) {
        return 0;
    }

    memcpy(buffer, inode->buffer + *pos, size);
    *pos += size;
    return (fs_ssize_t)size;
}

static fs_ssize_t fat_inode_write(
    struct inode *base, fs_size_t pos, const void *buffer, fs_size_t size, bool append) {
    struct fat_inode *inode = container_of(base, struct fat_inode, inode);

    kerrno_t result = fat_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    uint32_t fsz = inode->filesize;
    if (!append) {
        if (pos > fsz || size > fsz - pos) {
            return OPAL_ERANGE;
        }
    } else {
        if (size > UINT32_MAX - fsz) {
            return OPAL_ENOSPC;
        }
        pos = fsz;
        fsz += (uint32_t)size;
    }

    if (size == 0) {
        return 0;
    }

    struct fat_layout *layout = &inode->sb->layout;
    uint16_t lsec = layout->bytes_per_sector / DISK_SECTOR_SIZE;
    uint32_t pspc = layout->sectors_per_cluster * lsec;
    uint32_t data_offset = layout->data_offset * lsec;

    size_t bpc = layout->bytes_per_sector * layout->sectors_per_cluster;
    size_t newlen = (fsz + bpc - 1) / bpc * bpc;
    unsigned char *newbuf = kzalloc(newlen);
    if (!newbuf) {
        return OPAL_ENOMEM;
    }
    memcpy(newbuf, inode->buffer, inode->filesize);
    memcpy(newbuf + pos, buffer, size);

    struct fat_table *table = &inode->sb->table;
    uint32_t first_cluster = inode->first_cluster & 0x0fffffff;
    if (is_cluster_eof(layout, first_cluster)) {
        result = table->ops->table_alloc(table, &first_cluster);
        if (!kerrno_ok(result)) {
            goto err;
        }
    }

    uint32_t byte_index = 0;
    uint32_t cluster = first_cluster;
    while (1) {
        if (byte_index + bpc > pos) {
            result = fat_write_sectors(
                inode->sb->bdev, data_offset + (cluster - 2) * pspc, pspc, newbuf + byte_index);
            if (!kerrno_ok(result)) {
                break;
            }
        }
        byte_index += bpc;

        if (byte_index >= pos + size) {
            break;
        }

        uint32_t entry;
        result = table->ops->table_at(table, cluster, &entry);
        if (!kerrno_ok(result)) {
            break;
        }
        uint32_t next_cluster = entry & 0x0fffffff;

        if (is_cluster_eof(layout, next_cluster)) {
            if (!append) {
                result = OPAL_ERANGE;
                break;
            }
            result = fat_table_append(table, cluster, &next_cluster);
            if (!kerrno_ok(result)) {
                result = OPAL_ENOSPC;
                break;
            }
        }
        cluster = next_cluster;
    }

    if (byte_index < pos + size) {
        if (byte_index <= pos) {
            goto err;
        }

        size = byte_index - pos;
        if (append) {
            fsz = inode->filesize + (uint32_t)size;
        }
    }

    const struct fat_dentry *dentry = get_dentry(inode);
    if (dentry) {
        struct fat_dentry new_dentry = *dentry;
        bool updated = false;
        if (first_cluster != (inode->first_cluster & 0x0fffffff)) {
            dentry_set_first_cluster(&new_dentry, layout->bits, first_cluster);
            updated = true;
        }
        uint8_t attr = dentry->attr;
        if (fsz != inode->filesize && !(attr & (FAT_ATTR_VLABEL | FAT_ATTR_DIRECTORY))) {
            new_dentry.file_size = fsz;
            updated = true;
        }

        result = OPAL_OK;
        if (updated) {
            struct fat_inode_base *parent = inode->parent;
            result = parent->ops->write_dentry(parent, &new_dentry, inode->dentry_idx);
        }
    }

    kfree(inode->buffer, inode->buflen);
    inode->first_cluster = first_cluster;
    inode->filesize = fsz;
    inode->buffer = newbuf;
    inode->buflen = newlen;
    return kerrno_ok(result) ? (fs_ssize_t)size : result;

err:
    kfree(newbuf, newlen);
    return result;
}

static fs_ssize_t fat_file_write(
    struct file *base, fs_size_t *pos, const void *buffer, fs_size_t size) {
    struct fat_file *file = container_of(base, typeof(*file), file);
    struct fat_inode *inode = container_of(file->inode, typeof(*inode), base);
    const struct fat_dentry *dentry = get_dentry(inode);
    if (!dentry || (dentry->attr & FAT_ATTR_DIRECTORY)) {
        return OPAL_EISDIR;
    }

    fs_ssize_t n = fat_inode_write(&inode->inode, *pos, buffer, size, base->mode & FILE_APPEND);
    if (kerrno_ok(n)) {
        *pos += n;
    }
    return n;
}

static kerrno_t fat_file_truncate(struct file *base, fs_size_t size) {
    struct fat_file *file = container_of(base, typeof(*file), file);
    struct fat_inode *inode = container_of(file->inode, typeof(*inode), base);
    const struct fat_dentry *dentry = get_dentry(inode);
    if (!dentry || (dentry->attr & FAT_ATTR_DIRECTORY)) {
        return OPAL_EISDIR;
    }

    if (size != 0) {
        return OPAL_EINVAL;
    }

    struct fat_layout *layout = &inode->sb->layout;

    struct fat_dentry new_dentry = *dentry;
    dentry_set_first_cluster(&new_dentry, layout->bits, 0);
    new_dentry.file_size = 0;

    kerrno_t result = OPAL_OK;

    struct fat_inode_base *parent = inode->parent;
    result = parent->ops->write_dentry(parent, &new_dentry, inode->dentry_idx);
    if (!kerrno_ok(result)) {
        return result;
    }

    struct fat_table *table = &inode->sb->table;
    uint32_t cluster = inode->first_cluster & 0x0fffffff;
    while (!is_cluster_eof(layout, cluster)) {
        uint32_t entry;
        result = table->ops->table_at(table, cluster, &entry);
        if (!kerrno_ok(result)) {
            break;
        }

        uint32_t next_cluster = entry & 0x0fffffff;
        result = table->ops->table_set(table, cluster, 0);
        if (!kerrno_ok(result)) {
            break;
        }
        cluster = next_cluster;
    }

    if (inode->buffer) {
        kfree(inode->buffer, inode->buflen);
        inode->buffer = NULL;
        inode->buflen = 0;
    }
    inode->filesize = 0;
    inode->first_cluster = 0;
    return result;
}

static kerrno_t fat_inode_write_dentry(
    struct fat_inode_base *base, const struct fat_dentry *dentry, uint32_t index) {
    struct fat_inode *inode = container_of(base, struct fat_inode, base);
    const struct fat_dentry *my_dentry = get_dentry(inode);

    if (my_dentry && !(my_dentry->attr & FAT_ATTR_DIRECTORY)) {
        return OPAL_ENOTDIR;
    }

    kerrno_t result = fat_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    uint32_t entries = inode->filesize / sizeof(*dentry);
    bool append = false;
    if (index > entries) {
        return OPAL_ERANGE;
    } else if (index == entries) {
        append = true;
    }

    result =
        fat_inode_write(&inode->inode, index * sizeof(*dentry), dentry, sizeof(*dentry), append);
    if (result < 0) {
        return result;
    } else if (result != sizeof(*dentry)) {
        return OPAL_ERANGE;
    }
    return OPAL_OK;
}

static fs_ssize_t fat_inode_alloc_dentry(struct fat_inode_base *base) {
    struct fat_inode *inode = container_of(base, struct fat_inode, base);
    const struct fat_dentry *my_dentry = get_dentry(inode);

    if (my_dentry && !(my_dentry->attr & FAT_ATTR_DIRECTORY)) {
        return OPAL_ENOTDIR;
    }

    kerrno_t result = fat_inode_readall(inode);
    if (!kerrno_ok(result)) {
        return result;
    }

    struct fat_dentry *dentries = (struct fat_dentry *)inode->buffer;
    uint32_t count = inode->filesize / sizeof(struct fat_dentry);

    for (uint32_t i = 0; i < count; i++) {
        unsigned char flag = dentries[i].name[0];
        if (flag == 0 || flag == 0xe5) {
            return i;
        }
    }

    result = fat_inode_write_dentry(base, &(struct fat_dentry){}, count);
    if (!kerrno_ok(result)) {
        return result;
    }

    return count;
}

static struct fat_inode_ops g_inode_ops = {
    .ops = {
        .close = fat_inode_close,
        .open = fat_inode_open,
        .lookup = fat_inode_lookup,
        .create = fat_inode_create,
    },
    .write_dentry = fat_inode_write_dentry,
    .alloc_dentry = fat_inode_alloc_dentry,
};

static struct file_ops g_inode_fops = {
    .close = fat_file_close,
    .seek = fat_file_seek,
    .read = fat_file_read,
    .write = fat_file_write,
    .truncate = fat_file_truncate,
};
