#include <kc/assert.h>

#include <opal/fs/disk.h>
#include <opal/locks/irqlock.h>

#define MAX_DEVICES 64

static struct disk *g_disks[MAX_DEVICES];
static size_t g_disks_count = 0;

[[nodiscard]] static struct disk_request *submit_request(struct disk *disk, const struct disk_request_info *reqinfo);

void disk_init(struct disk *disk, const struct disk_device_ops *ops) {
    disk->ops = ops;
}

bool disk_register(struct disk *disk) {
    irqlock_t irqlock = irqlock_acquire();

    if (g_disks_count >= MAX_DEVICES) {
        irqlock_release(&irqlock);
        return false;
    }

    g_disks[g_disks_count++] = disk;
    irqlock_release(&irqlock);
    return true;
}

struct disk_request *disk_read(struct disk *disk, fs_size_t lba, fs_size_t sectors, void *buffer) {
    struct disk_request_info info = {
        .lba = lba,
        .sectors = sectors,
        .buffer = buffer,
        .type = DISK_REQUEST_READ,
    };
    return submit_request(disk, &info);
}

struct disk_request *disk_write(struct disk *disk, fs_size_t lba, fs_size_t sectors, const void *buffer) {
    struct disk_request_info info = {
        .lba = lba,
        .sectors = sectors,
        .buffer = (void *)buffer,
        .type = DISK_REQUEST_WRITE,
    };
    return submit_request(disk, &info);
}

size_t disk_count(void) {
    irqlock_t irqlock = irqlock_acquire();
    size_t count = g_disks_count;
    irqlock_release(&irqlock);
    return count;
}

struct disk *disk_get(size_t index) {
    irqlock_t irqlock = irqlock_acquire();

    if (index >= g_disks_count) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct disk *disk = g_disks[index];
    irqlock_release(&irqlock);
    return disk;
}

void disk_req_queue_init(struct disk_req_queue *queue, struct disk_request *buffer, size_t capacity) {
    queue->buffer = buffer;
    queue->capacity = capacity;
    queue->count_doing = 0;
    queue->count_done = 0;
    queue->wpos = 0;
    queue->fpos = 0;
    queue->rpos = 0;
}

static struct disk_request *submit_request(struct disk *disk, const struct disk_request_info *reqinfo) {
    irqlock_t irqlock = irqlock_acquire();

    struct disk_req_queue *queue = disk->req_queue;

    if (queue->count_doing + queue->count_done >= queue->capacity) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct disk_request *new = &queue->buffer[queue->wpos];
    new->disk = disk;
    new->info = *reqinfo;
    new->state = DISK_REQSTATE_QUEUED;
    new->result = FS_OK;
    completion_init(&new->completion);

    queue->wpos = (queue->wpos + 1) % queue->capacity;
    queue->count_doing++;

    irqlock_release(&irqlock);

    disk->ops->on_request(disk, new);
    return new;
}

struct disk_request *disk_req_queue_fetch(struct disk_req_queue *queue) {
    irqlock_t irqlock = irqlock_acquire();

    if (queue->count_doing == 0) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct disk_request *req = &queue->buffer[queue->fpos];
    assert(req->state == DISK_REQSTATE_QUEUED);
    req->state = DISK_REQSTATE_INFLIGHT;

    irqlock_release(&irqlock);
    return req;
}

void disk_req_queue_pop_fetched(struct disk_req_queue *queue) {
    irqlock_t irqlock = irqlock_acquire();

    assert(queue->count_doing > 0);

    struct disk_request *req = &queue->buffer[queue->fpos];
    assert(req->state == DISK_REQSTATE_INFLIGHT);
    req->state = DISK_REQSTATE_DONE;
    completion_signal(&req->completion);

    queue->fpos = (queue->fpos + 1) % queue->capacity;
    queue->count_doing--;
    queue->count_done++;

    irqlock_release(&irqlock);
}

fs_status_t disk_release_request(struct disk *disk, struct disk_request *req) {
    irqlock_t irqlock = irqlock_acquire();

    struct disk_req_queue *queue = disk->req_queue;
    fs_status_t result = req->result;

    assert(req->disk == disk);
    assert(queue->count_done > 0);
    assert(req->state == DISK_REQSTATE_DONE);

    req->state = DISK_REQSTATE_RELEASED;

    bool found = false;
    while (queue->count_done > 0) {
        struct disk_request *head = &queue->buffer[queue->rpos];
        if (head->state != DISK_REQSTATE_RELEASED) {
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

bool disk_wait_request(struct disk *disk, struct disk_request *req, uint64_t timeout, fs_status_t *result) {
    completion_wait(&req->completion, timeout);

    irqlock_t irqlock = irqlock_acquire();
    assert(req->disk == disk);
    bool done = req->state == DISK_REQSTATE_DONE;
    irqlock_release(&irqlock);

    if (!done) {
        return false;
    }

    if (result) {
        *result = disk_release_request(disk, req);
    } else {
        (void)disk_release_request(disk, req);
    }

    return true;
}
