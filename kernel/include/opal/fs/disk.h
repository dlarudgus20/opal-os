#ifndef OPAL_FS_DISK_H
#define OPAL_FS_DISK_H

#include <kc/span.h>

#include <collections/linkedlist.h>

#include <opal/dynarray.h>
#include <opal/fs/types.h>

#define DISK_SECTOR_SIZE 512

enum disk_request_type {
    DISK_REQUEST_READ,
    DISK_REQUEST_WRITE,
};

enum disk_request_state {
    DISK_REQSTATE_QUEUED,
    DISK_REQSTATE_INFLIGHT,
    DISK_REQSTATE_DONE,
    DISK_REQSTATE_RELEASED,
};

struct disk_request;
struct disk_req_queue;
struct disk;

struct disk_device_ops {
    void (*on_request)(struct disk *disk, struct disk_request *req);
};

struct partition_entry {
    size_t index;
    struct block_device *bdev;
};

struct disk {
    const struct disk_device_ops *ops;
    struct block_device *bdev;

    struct span part_table_sectors;
    struct dynarray partitions;
    const char *name;

    struct disk_req_queue *req_queue;
    fs_size_t sectors;
};

struct disk_request_info {
    fs_size_t lba;
    fs_size_t sectors;
    void *buffer;
    enum disk_request_type type;
};

struct disk_request {
    struct disk_request_info info;
    struct disk *disk;
    enum disk_request_state state;
    struct fs_completion completion;
};

struct disk_req_queue {
    struct disk_request *buffer;
    size_t capacity;
    size_t count_doing;
    size_t count_done;
    size_t wpos;
    size_t fpos;
    size_t rpos;
};

void disk_init(
    struct disk *disk, const struct disk_device_ops *ops,
    const char *name, struct disk_req_queue *queue, fs_size_t sectors
);
bool disk_register(struct disk *disk);

struct disk_request *disk_read(struct disk *disk, fs_size_t lba, fs_size_t sectors, void *buffer);
struct disk_request *disk_write(struct disk *disk, fs_size_t lba, fs_size_t sectors, const void *buffer);

[[nodiscard]] size_t disk_list_count(void);
[[nodiscard]] struct disk *disk_list_get(size_t index);

void disk_req_queue_init(struct disk_req_queue *queue, struct disk_request *buffer, size_t capacity);
bool disk_req_queue_is_full_unlocked(struct disk_req_queue *queue);
[[nodiscard]] struct disk_request *disk_req_queue_fetch(struct disk_req_queue *queue);
void disk_req_queue_pop_fetched(struct disk_req_queue *queue, fs_status_t result);

[[nodiscard]] fs_status_t disk_request_release(struct disk_request *req);
bool disk_request_wait(struct disk_request *req, uint64_t timeout, fs_status_t *result);

// partition.c
void disk_register_bdev(struct disk *disk);
void disk_rescan_partition(struct disk *disk, struct fs_completion *completion);
void disk_reset_partition(struct disk *disk, struct fs_completion *completion);
void disk_create_partition(
    struct disk *disk, size_t index, fs_size_t lba, fs_size_t sectors, uint8_t type,
    struct fs_completion *completion
);
void disk_remove_partition(struct disk *disk, size_t index, struct fs_completion *completion);

#endif
