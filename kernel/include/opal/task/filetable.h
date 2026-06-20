#ifndef TASK_FILETABLE_H
#define TASK_FILETABLE_H

#include <stdint.h>

#include <kc/kerrno.h>

#define FD_INVALID -1
#define FD_MAX 8192

#define FDTBL_BITMAP_UNIT (8 * sizeof(unsigned long))
#define FDTBL_INLINE_SIZE FDTBL_BITMAP_UNIT

typedef int32_t fd_t;

struct file;

struct filetable {
    struct file **files;
    unsigned long *bitmap;
    uint32_t capacity;
    uint32_t count;
    uint32_t end_fd;
    uint32_t next_fd;

    struct file *inline_files[FDTBL_INLINE_SIZE];
    unsigned long inline_bitmap[1];
};

void filetable_init(struct filetable *table);
void filetable_destroy(struct filetable *table);
[[nodiscard]] kerrno_t filetable_clone(struct filetable *dst, struct filetable *src);
fd_t filetable_insert(struct filetable *table, struct file *file);
kerrno_t filetable_insert_at(struct filetable *table, fd_t fd, struct file *file);
struct file *filetable_get(struct filetable *table, fd_t fd);
bool filetable_remove(struct filetable *table, fd_t fd);

#endif
