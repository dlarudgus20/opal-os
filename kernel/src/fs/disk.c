#include <kc/assert.h>

#include <opal/klog.h>
#include <opal/fs/disk.h>
#include <opal/fs/block_device.h>
#include <opal/locks/irqlock.h>

#define MAX_DISKS 64

static struct disk *g_disks[MAX_DISKS];
static size_t g_disks_count = 0;

[[nodiscard]] static struct disk_request *submit_request(struct disk *disk, const struct disk_request_info *reqinfo);

void disk_init(
    struct disk *disk, const struct disk_device_ops *ops,
    const char *name, struct disk_req_queue *queue, fs_size_t sectors
) {
    disk->ops = ops;
    disk->bdev = NULL;
    disk->part_table_sectors = SPAN_NULL;
    disk->name = name;
    disk->req_queue = queue;
    disk->sectors = sectors;
    dynarray_init(&disk->partitions);
}

bool disk_register(struct disk *disk) {
    irqlock_t irqlock = irqlock_acquire();

    if (g_disks_count >= MAX_DISKS) {
        irqlock_release(&irqlock);
        return false;
    }
    g_disks[g_disks_count++] = disk;

    irqlock_release(&irqlock);
    return true;
}

void disk_register_bdev(struct disk *disk) {
    irqlock_t irqlock = irqlock_acquire();

    assert(!disk->bdev);

    disk->bdev = block_device_create(disk, disk->name, 0, disk->sectors);
    if (!disk->bdev) {
        kerror("disk_register_bdev: cannot register block device %s", disk->name);
        irqlock_release(&irqlock);
        return;
    }

    irqlock_release(&irqlock);

    disk_rescan_partition(disk, NULL);
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

size_t disk_list_count(void) {
    irqlock_t irqlock = irqlock_acquire();
    size_t count = g_disks_count;
    irqlock_release(&irqlock);
    return count;
}

struct disk *disk_list_get(size_t index) {
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

bool disk_req_queue_is_full_unlocked(struct disk_req_queue *queue) {
    return queue->count_doing + queue->count_done >= queue->capacity;
}

static struct disk_request *submit_request(struct disk *disk, const struct disk_request_info *reqinfo) {
    irqlock_t irqlock = irqlock_acquire();

    struct disk_req_queue *queue = disk->req_queue;

    if (disk_req_queue_is_full_unlocked(queue)) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct disk_request *new = &queue->buffer[queue->wpos];
    new->disk = disk;
    new->info = *reqinfo;
    new->state = DISK_REQSTATE_QUEUED;
    fs_completion_init(&new->completion);

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

void disk_req_queue_pop_fetched(struct disk_req_queue *queue, fs_status_t result) {
    irqlock_t irqlock = irqlock_acquire();

    assert(queue->count_doing > 0);

    struct disk_request *req = &queue->buffer[queue->fpos];
    assert(req->state == DISK_REQSTATE_INFLIGHT);
    req->state = DISK_REQSTATE_DONE;
    fs_completion_signal(&req->completion, result);

    queue->fpos = (queue->fpos + 1) % queue->capacity;
    queue->count_doing--;
    queue->count_done++;

    irqlock_release(&irqlock);
}

fs_status_t disk_request_release(struct disk_request *req) {
    irqlock_t irqlock = irqlock_acquire();

    struct disk_req_queue *queue = req->disk->req_queue;

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

    fs_status_t result = req->completion.result;
    irqlock_release(&irqlock);
    return result;
}

bool disk_request_wait(struct disk_request *req, uint64_t timeout, fs_status_t *result) {
    if (!fs_completion_wait(&req->completion, timeout)) {
        return false;
    }

    fs_status_t r = disk_request_release(req);
    if (result) {
        *result = r;
    }

    return true;
}
