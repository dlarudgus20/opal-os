#include <kc/assert.h>

#include <opal/fs/block_device.h>
#include <opal/locks/irqlock.h>

#define MAX_DEVICES 64

static struct block_device *g_device_buffer[MAX_DEVICES];
static size_t g_device_count = 0;

[[nodiscard]] static struct block_request *submit_request(struct block_device *dev, const struct block_request_info *reqinfo);

void block_device_init(struct block_device *dev, const struct block_device_ops *ops) {
    dev->ops = ops;
}

bool block_device_register(struct block_device *dev) {
    irqlock_t irqlock = irqlock_acquire();

    if (g_device_count >= MAX_DEVICES) {
        irqlock_release(&irqlock);
        return false;
    }

    g_device_buffer[g_device_count++] = dev;
    irqlock_release(&irqlock);
    return true;
}

struct block_request *block_device_read(struct block_device *dev, fs_size_t lba, fs_size_t sectors, void *buffer) {
    struct block_request_info info = {
        .lba = lba,
        .sectors = sectors,
        .buffer = buffer,
        .type = BLOCK_REQUEST_READ,
    };
    return submit_request(dev, &info);
}

struct block_request *block_device_write(struct block_device *dev, fs_size_t lba, fs_size_t sectors, const void *buffer) {
    struct block_request_info info = {
        .lba = lba,
        .sectors = sectors,
        .buffer = (void *)buffer,
        .type = BLOCK_REQUEST_WRITE,
    };
    return submit_request(dev, &info);
}

size_t block_device_count(void) {
    irqlock_t irqlock = irqlock_acquire();
    size_t count = g_device_count;
    irqlock_release(&irqlock);
    return count;
}

struct block_device *block_device_get(size_t index) {
    irqlock_t irqlock = irqlock_acquire();

    if (index >= g_device_count) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct block_device *dev = g_device_buffer[index];
    irqlock_release(&irqlock);
    return dev;
}

void block_req_queue_init(struct block_req_queue *queue, struct block_request *buffer, size_t capacity) {
    queue->buffer = buffer;
    queue->capacity = capacity;
    queue->count_doing = 0;
    queue->count_done = 0;
    queue->wpos = 0;
    queue->fpos = 0;
    queue->rpos = 0;
}

static struct block_request *submit_request(struct block_device *dev, const struct block_request_info *reqinfo) {
    irqlock_t irqlock = irqlock_acquire();

    struct block_req_queue *queue = dev->req_queue;

    if (queue->count_doing + queue->count_done >= queue->capacity) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct block_request *new = &queue->buffer[queue->wpos];
    new->device = dev;
    new->info = *reqinfo;
    new->state = BLOCK_REQSTATE_QUEUED;
    new->result = FS_OK;
    completion_init(&new->completion);

    queue->wpos = (queue->wpos + 1) % queue->capacity;
    queue->count_doing++;

    irqlock_release(&irqlock);

    dev->ops->on_request(dev, new);
    return new;
}

struct block_request *block_req_queue_fetch(struct block_req_queue *queue) {
    irqlock_t irqlock = irqlock_acquire();

    if (queue->count_doing == 0) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct block_request *req = &queue->buffer[queue->fpos];
    assert(req->state == BLOCK_REQSTATE_QUEUED);
    req->state = BLOCK_REQSTATE_INFLIGHT;

    irqlock_release(&irqlock);
    return req;
}

void block_req_queue_pop_fetched(struct block_req_queue *queue) {
    irqlock_t irqlock = irqlock_acquire();

    assert(queue->count_doing > 0);

    struct block_request *req = &queue->buffer[queue->fpos];
    assert(req->state == BLOCK_REQSTATE_INFLIGHT);
    req->state = BLOCK_REQSTATE_DONE;
    completion_signal(&req->completion);

    queue->fpos = (queue->fpos + 1) % queue->capacity;
    queue->count_doing--;
    queue->count_done++;

    irqlock_release(&irqlock);
}

fs_status_t block_device_release_request(struct block_device *dev, struct block_request *req) {
    irqlock_t irqlock = irqlock_acquire();

    struct block_req_queue *queue = dev->req_queue;
    fs_status_t result = req->result;

    assert(req->device == dev);
    assert(queue->count_done > 0);
    assert(req->state == BLOCK_REQSTATE_DONE);

    req->state = BLOCK_REQSTATE_RELEASED;

    bool found = false;
    while (queue->count_done > 0) {
        struct block_request *head = &queue->buffer[queue->rpos];
        if (head->state != BLOCK_REQSTATE_RELEASED) {
            break;
        }

        if (head == req) {
            found = true;
        }

        queue->rpos = (queue->rpos + 1) % queue->capacity;
        queue->count_done--;
    }

    size_t pos = queue->rpos;
    size_t remaining = queue->count_done;
    while (!found && remaining > 0) {
        if (&queue->buffer[pos] == req) {
            found = true;
            break;
        }

        pos = (pos + 1) % queue->capacity;
        remaining--;
    }

    assert(found, "invalid request to release");

    irqlock_release(&irqlock);
    return result;
}

bool block_device_wait_request(struct block_device *dev, struct block_request *req, uint64_t timeout, fs_status_t *result) {
    completion_wait(&req->completion, timeout);

    irqlock_t irqlock = irqlock_acquire();
    assert(req->device == dev);
    bool done = req->state == BLOCK_REQSTATE_DONE;
    irqlock_release(&irqlock);

    if (!done) {
        return false;
    }

    if (result) {
        *result = block_device_release_request(dev, req);
    } else {
        (void)block_device_release_request(dev, req);
    }

    return true;
}
