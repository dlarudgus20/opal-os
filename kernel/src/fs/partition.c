#include <kc/assert.h>
#include <kc/string.h>
#include <kc/stdio.h>

#include <opal/klog.h>
#include <opal/fs/block_device.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/coroutine.h>
#include <opal/locks/irqlock.h>

#define PARTITION_TABLE_OFFSET 0x1be

struct partition_entry {
    uint8_t attr;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba;
    uint32_t sectors;
};

static_assert(sizeof(struct partition_entry) == 16);

void disk_register_bdev(struct disk *disk) {
    if (!block_device_register(disk, disk->name, 0, disk->sectors)) {
        kerror("cannot register block device for %s", disk->name);
        return;
    }

    disk_rescan_partition(disk, NULL);
}

static void complete_not_null(struct fs_completion *comp, fs_status_t result) {
    if (comp) {
        fs_completion_signal(comp, result);
    }
}

struct co_disk {
    struct coroutine co;
    unsigned char *mbr;
    struct disk_request *req;
    struct fs_completion *completion;
};

static co_state_t co_rescan_handler(struct coroutine *co);

void disk_rescan_partition(struct disk *disk, struct fs_completion *completion) {
    fs_status_t result = FS_ERR_NOMEM;

    unsigned char *mbr = kzalloc(DISK_SECTOR_SIZE);
    if (!mbr) {
        kerror("disk_rescan_partition: out of memory");
        goto err_mbr;
    }

    struct co_disk *ctx = kzalloc(sizeof(*ctx));
    if (!ctx) {
        kerror("disk_rescan_partition: out of memory");
        goto err_ctx;
    }

    ctx->mbr = mbr;
    ctx->completion = completion;
    ctx->req = disk_read(disk, 0, 1, ctx->mbr);
    if (!ctx->req) {
        result = FS_ERR_BUSY;
        kerror("disk_rescan_partition: out of disk request queue");
        goto err_req;
    }

    coroutine_init(&ctx->co, co_rescan_handler);
    coroutine_set_ready(&ctx->co);
    return;

err_req:
    kfree(ctx, sizeof(*ctx));
err_ctx:
    kfree(mbr, DISK_SECTOR_SIZE);
err_mbr:
    complete_not_null(completion, result);
}

static co_state_t co_rescan_handler(struct coroutine *co) {
    struct co_disk *ctx = container_of(co, struct co_disk, co);
    fs_status_t result;

    if (co->state == CO_DONE) {
        kfree(ctx, sizeof(*ctx));
        return CO_DONE;
    }

    struct disk *disk = ctx->req->disk;
    disk_request_wait(ctx->req, TIMEOUT_INFINITY, &result);
    if (result != FS_OK) {
        complete_not_null(ctx->completion, result);
        kfree(ctx->mbr, DISK_SECTOR_SIZE);
        kerror("disk_rescan_partition: fs error %d", result);
        return CO_DONE;
    }

    block_device_unregister_partitions(disk);

    struct partition_entry table[4];
    memcpy(table, &ctx->mbr[PARTITION_TABLE_OFFSET], sizeof(table));

    struct span old = disk->partition_table;
    disk->partition_table = SPAN(ctx->mbr, DISK_SECTOR_SIZE);
    if (old.ptr) {
        kfree(old.ptr, old.size);
    }

    for (int i = 0; i < 4; i++) {
        if (table[i].type == 0) {
            continue;
        }

        uint32_t end = table[i].lba + table[i].sectors;
        if (end <= table[i].lba || end > disk->sectors) {
            knotice("disk_rescan_partition: broken partition %s:%d", disk->name, i);
            continue;
        }

        int len = snprintf_s(NULL, 0, "%s:%d", disk->name, i);
        char *partname = kzalloc(len + 1);
        if (!partname) {
            kerror("disk_rescan_partition: out of memory");
            break;
        }

        snprintf_s(partname, len + 1, "%s:%d", disk->name, i);
        block_device_register(disk, partname, table[i].lba, table[i].sectors);
    }

    complete_not_null(ctx->completion, FS_OK);
    return CO_DONE;
}

static co_state_t co_reset_handler(struct coroutine *co);

void disk_reset_partition(struct disk *disk, struct fs_completion *completion) {
    fs_status_t result = FS_ERR_NOMEM;

    unsigned char *mbr = kzalloc(DISK_SECTOR_SIZE);
    if (!mbr) {
        kerror("disk_reset_partition: out of memory");
        goto err_mbr;
    }

    struct co_disk *ctx = kzalloc(sizeof(*ctx));
    if (!ctx) {
        kerror("disk_reset_partition: out of memory");
        goto err_ctx;
    }

    ctx->mbr = mbr;
    ctx->completion = completion;
    ctx->req = disk_write(disk, 0, 1, mbr);
    if (!ctx->req) {
        result = FS_ERR_BUSY;
        kerror("disk_reset_partition: out of disk request queue");
        goto err_req;
    }

    coroutine_init(&ctx->co, co_reset_handler);
    coroutine_set_ready(&ctx->co);
    return;

err_req:
    kfree(ctx, sizeof(*ctx));
err_ctx:
    kfree(mbr, DISK_SECTOR_SIZE);
err_mbr:
    complete_not_null(completion, result);
}

static co_state_t co_reset_handler(struct coroutine *co) {
    struct co_disk *ctx = container_of(co, struct co_disk, co);
    fs_status_t result;

    if (co->state == CO_DONE) {
        kfree(ctx, sizeof(*ctx));
        return CO_DONE;
    }

    struct disk *disk = ctx->req->disk;
    disk_request_wait(ctx->req, TIMEOUT_INFINITY, &result);
    if (result != FS_OK) {
        complete_not_null(ctx->completion, result);
        kfree(ctx->mbr, DISK_SECTOR_SIZE);
        kerror("disk_reset_partition: fs error %d", result);
        return CO_DONE;
    }

    block_device_unregister_partitions(disk);

    struct span old = disk->partition_table;
    disk->partition_table = SPAN(ctx->mbr, DISK_SECTOR_SIZE);
    if (old.ptr) {
        kfree(old.ptr, old.size);
    }

    complete_not_null(ctx->completion, FS_OK);
    return CO_DONE;
}
