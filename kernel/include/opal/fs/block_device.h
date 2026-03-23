#ifndef OPAL_FS_BLOCK_DEVICE_H
#define OPAL_FS_BLOCK_DEVICE_H

#include <opal/fs/types.h>
#include <opal/task/completion.h>

enum block_request_type {
    BLOCK_REQUEST_READ,
    BLOCK_REQUEST_WRITE,
};

enum block_request_state {
    BLOCK_REQSTATE_QUEUED,
    BLOCK_REQSTATE_INFLIGHT,
    BLOCK_REQSTATE_DONE,
    BLOCK_REQSTATE_RELEASED,
};

struct block_request;
struct block_req_queue;
struct block_device;

struct block_device_ops {
    void (*on_request)(struct block_device *dev, struct block_request *req);
};

struct block_device {
    const struct block_device_ops *ops;
    const char *name;
    struct block_req_queue *req_queue;
    fs_size_t sector_size;
    fs_size_t sector_count;
};

struct block_request_info {
    fs_size_t lba;
    fs_size_t sectors;
    void *buffer;
    enum block_request_type type;
};

struct block_request {
    struct block_request_info info;
    struct block_device *device;
    enum block_request_state state;
    fs_status_t result;
    struct completion completion;
};

struct block_req_queue {
    struct block_request *buffer;
    size_t capacity;
    size_t count_doing;
    size_t count_done;
    size_t wpos;
    size_t fpos;
    size_t rpos;
};

void block_device_init(struct block_device *dev, const struct block_device_ops *ops);
bool block_device_register(struct block_device *dev);

struct block_request *block_device_read(struct block_device *dev, fs_size_t lba, fs_size_t sectors, void *buffer);
struct block_request *block_device_write(struct block_device *dev, fs_size_t lba, fs_size_t sectors, const void *buffer);

[[nodiscard]] size_t block_device_count(void);
[[nodiscard]] struct block_device *block_device_get(size_t index);

void block_req_queue_init(struct block_req_queue *queue, struct block_request *buffer, size_t capacity);
[[nodiscard]] struct block_request *block_req_queue_fetch(struct block_req_queue *queue);
void block_req_queue_pop_fetched(struct block_req_queue *queue);

fs_status_t block_device_release_request(struct block_device *dev, struct block_request *req);
bool block_device_wait_request(struct block_device *dev, struct block_request *req, uint64_t timeout, fs_status_t *result);

#endif
