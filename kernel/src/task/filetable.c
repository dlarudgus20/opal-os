#include <kc/string.h>

#include <opal/task/filetable.h>
#include <opal/mm/kmalloc.h>
#include <opal/fs/vfs.h>

#define BITMAP_UNIT FDTBL_BITMAP_UNIT

static uint32_t calc_bitmap_size(uint32_t count) {
    return (count + BITMAP_UNIT - 1) / BITMAP_UNIT;
}

static void free_table(struct filetable *table) {
    if (table->files != table->inline_files) {
        kfree(table->files, sizeof(*table->files) * table->capacity);
        kfree(table->bitmap, sizeof(*table->bitmap) * calc_bitmap_size(table->capacity));
    }
}

static bool realloc(struct filetable *table, uint32_t capacity) {
    const uint32_t max_capacity = FD_MAX + 1u;
    if (capacity > max_capacity) {
        return false;
    }
    if (capacity < table->end_fd) {
        return false;
    }
    if (capacity <= FDTBL_INLINE_SIZE) {
        return false;
    }

    struct span files = kzalloc_span(sizeof(*table->files) * capacity);
    if (!files.ptr) {
        return false;
    }

    uint32_t new_capacity = files.size / sizeof(*table->files);
    if (new_capacity > max_capacity) {
        new_capacity = max_capacity;
    }

    uint32_t bitmap_capacity = calc_bitmap_size(new_capacity);
    unsigned long *bitmap = kzalloc(sizeof(*bitmap) * bitmap_capacity);
    if (!bitmap) {
        kfree_span(files);
        return false;
    }

    uint32_t bitmap_size = calc_bitmap_size(table->end_fd);
    uint32_t old_bitmap_capacity = calc_bitmap_size(table->capacity);
    memcpy(bitmap, table->bitmap, sizeof(*bitmap) * bitmap_size);
    memcpy(files.ptr, table->files, sizeof(*table->files) * table->end_fd);

    if (table->files != table->inline_files) {
        kfree(table->bitmap, sizeof(*table->bitmap) * old_bitmap_capacity);
        kfree(table->files, sizeof(*table->files) * table->capacity);
    }

    table->files = files.ptr;
    table->bitmap = bitmap;
    table->capacity = new_capacity;
    return true;
}

static void bitmap_set(struct filetable *table, uint32_t idx, bool value) {
    if (value) {
        table->bitmap[idx / BITMAP_UNIT] |= (1ul << (idx % BITMAP_UNIT));
    } else {
        table->bitmap[idx / BITMAP_UNIT] &= ~(1ul << (idx % BITMAP_UNIT));
    }
}

static fd_t find_empty_bit(struct filetable *table, uint32_t begin, uint32_t end) {
    for (uint32_t idx = begin; idx < end; idx++) {
        unsigned long word = table->bitmap[idx];
        if (word == ~0ul) {
            continue;
        }

        for (uint32_t bit = 0; bit < BITMAP_UNIT; bit++) {
            if (!(word & (1ul << bit))) {
                uint32_t found = idx * BITMAP_UNIT + bit;
                return found < table->capacity ? (fd_t)found : FD_INVALID;
            }
        }
    }
    return FD_INVALID;
}

static fd_t find_empty_slot(struct filetable *table) {
    uint32_t hint = table->next_fd / BITMAP_UNIT;
    fd_t fd = find_empty_bit(table, hint, calc_bitmap_size(table->capacity));
    if (fd == FD_INVALID && hint > 0) {
        fd = find_empty_bit(table, 0, hint);
    }
    if (fd == FD_INVALID) {
        return FD_INVALID;
    }
    return (uint32_t)fd;
}

static void put_file(struct filetable *table, uint32_t idx, struct file *file) {
    file_retain(file);

    table->files[idx] = file;
    bitmap_set(table, idx, true);

    if (table->end_fd <= idx) {
        table->end_fd = idx + 1;
    }

    table->next_fd = idx + 1;
    if (table->next_fd >= table->capacity) {
        table->next_fd = 0;
    }

    table->count++;
}

static void update_end_fd(struct filetable *table) {
    if (table->end_fd == 0) {
        return;
    }

    uint32_t idx = (table->end_fd - 1) / BITMAP_UNIT;
    do {
        unsigned long word = table->bitmap[idx];
        if (word == 0ul) {
            continue;
        }

        uint32_t bit = BITMAP_UNIT;
        while (bit-- > 0) {
            if (word & (1ul << bit)) {
                table->end_fd = idx * BITMAP_UNIT + bit + 1;
                return;
            }
        }
    } while (idx-- > 0);

    table->end_fd = 0;
}

void filetable_init(struct filetable *table) {
    *table = (struct filetable){ };
    table->files = table->inline_files;
    table->bitmap = table->inline_bitmap;
    table->capacity = FDTBL_INLINE_SIZE;
}

void filetable_destroy(struct filetable *table) {
    for (uint32_t idx = 0; idx < table->end_fd; idx++) {
        struct file *file = table->files[idx];
        if (file) {
            file_release(file);
        }
    }

    free_table(table);
}

fd_t filetable_insert(struct filetable *table, struct file *file) {
    uint32_t idx;

    if (table->count == table->capacity) {
        if (table->end_fd > FD_MAX) {
            return FD_INVALID;
        }

        idx = table->end_fd;
        if (!realloc(table, table->end_fd + 1)) {
            return FD_INVALID;
        }
    } else {
        // find_empty_slot always succeeds because count < capacity
        idx = (uint32_t)find_empty_slot(table);
    }

    put_file(table, idx, file);
    return (fd_t)idx;
}

struct file *filetable_get(struct filetable *table, fd_t fd) {
    if (fd < 0 || (uint32_t)fd >= table->end_fd) {
        return NULL;
    }

    struct file *file = table->files[fd];
    if (file) {
        file_retain(file);
    }
    return file;
}

bool filetable_remove(struct filetable *table, fd_t fd) {
    if (fd < 0 || (uint32_t)fd >= table->end_fd) {
        return false;
    }

    struct file *file = table->files[fd];
    if (!file) {
        return false;
    }

    table->files[fd] = NULL;
    bitmap_set(table, fd, false);
    file_release(file);

    table->count--;
    if ((uint32_t)fd + 1 == table->end_fd) {
        update_end_fd(table);
    }

    if (table->next_fd > (uint32_t)fd) {
        table->next_fd = (uint32_t)fd;
    }
    return true;
}
