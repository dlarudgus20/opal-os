#include <kc/assert.h>
#include <kc/string.h>
#include <kc/stdio.h>

#include <opal/klog.h>
#include <opal/fs/block_device.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/coroutine.h>
#include <opal/locks/irqlock.h>

#define PARTITION_TABLE_OFFSET 0x1be

struct mbr_part_entry {
    uint8_t attr;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba;
    uint32_t sectors;
};

static_assert(sizeof(struct mbr_part_entry) == 16);

static void complete_not_null(struct fs_completion *comp, fs_status_t result) {
    if (comp) {
        fs_completion_signal(comp, result);
    }
}

[[nodiscard]] static bool lock_partitions(struct disk *disk) {
    if (disk->partitions.size == 0) {
        return true;
    }

    struct partition_entry *begin = disk->partitions.data;
    struct partition_entry *end = begin + dynarray_len(&disk->partitions, typeof(*end));
    struct partition_entry *entry = begin;
    for (; entry < end; entry++) {
        if (!block_device_retain(entry->bdev)) {
            goto undo;
        }
    }
    return true;

undo:
    for (struct partition_entry *p = begin; p < entry; p++) {
        block_device_release(p->bdev);
    }
    return false;
}


static void unlock_partitions(struct disk *disk) {
    dynarray_foreach(struct partition_entry *, entry, &disk->partitions) {
        block_device_release(entry->bdev);
    }
}

static void destroy_partition(struct block_device *bdev) {
    kfree((void *)bdev->name, strlen(bdev->name) + 1);
    bdev->name = NULL;
    block_device_destroy(bdev);
}

static void unregister_partitions(struct disk *disk) {
    dynarray_foreach(struct partition_entry *, entry, &disk->partitions) {
        destroy_partition(entry->bdev);
    }

    dynarray_resize(&disk->partitions, 0);
}

static struct span alloc_part_name(struct disk *disk, size_t index) {
    int len = snprintf_s(NULL, 0, "%s:%zu", disk->name, index);
    char *partname = kzalloc(len + 1);
    if (!partname) {
        return SPAN_NULL;
    }
    snprintf_s(partname, len + 1, "%s:%zu", disk->name, index);
    return SPAN(partname, len + 1);
}

static void build_part_table(struct disk *disk) {
    unsigned char *mbr = disk->part_table_sectors.ptr;
    if (!mbr) {
        return;
    }

    mbr[510] = 0x55;
    mbr[511] = 0xaa;

    struct mbr_part_entry table[4];
    memcpy(table, &mbr[PARTITION_TABLE_OFFSET], sizeof(table));

    dynarray_resize(&disk->partitions, 0);

    for (size_t i = 0; i < 4; i++) {
        if (table[i].type == 0) {
            continue;
        }

        uint32_t end = table[i].lba + table[i].sectors;
        if (table[i].lba == 0 || end <= table[i].lba || end > disk->sectors) {
            knotice("disk_rescan_partition: broken partition %s:%zu", disk->name, i);
            continue;
        }

        struct span partname = alloc_part_name(disk, i);
        if (!partname.ptr) {
            kerror("disk_rescan_partition: out of memory");
            break;
        }

        struct partition_entry *entry = dynarray_push_back(&disk->partitions, sizeof(*entry));
        if (!entry) {
            kerror("disk_rescan_partition: out of memory");
            kfree_span(partname);
            break;
        }

        struct block_device *dev = block_device_create(disk, partname.ptr, table[i].lba, table[i].sectors);
        if (!dev) {
            kerror("disk_rescan_partition: out of block_device memory");
            dynarray_pop_back(&disk->partitions, sizeof(*entry));
            kfree_span(partname);
            break;
        }

        entry->index = i;
        entry->bdev = dev;
    }
}

[[nodiscard]] static bool is_range_overlap(fs_size_t a_start, fs_size_t a_end, fs_size_t b_start, fs_size_t b_end) {
    return a_start < b_end && b_start < a_end;
}

[[nodiscard]] static fs_status_t apply_create_part(
    struct disk *disk, struct mbr_part_entry table[4],
    size_t index, fs_size_t lba, fs_size_t sectors, uint8_t type
) {
    fs_size_t end = lba + sectors;

    if (lba == 0) {
        return FS_ERR_RANGE;
    }
    if (end > disk->sectors) {
        return FS_ERR_RANGE;
    }

    if (table[index].type != 0) {
        return FS_ERR_EXIST;
    }

    for (size_t i = 0; i < 4; i++) {
        if (table[i].type == 0) {
            continue;
        }

        uint64_t e_start = table[i].lba;
        uint64_t e_end = e_start + table[i].sectors;
        if (is_range_overlap(lba, end, e_start, e_end)) {
            return FS_ERR_RANGE;
        }
    }

    table[index].attr = 0;
    table[index].chs_start[0] = table[index].chs_end[0] = 0xfe;
    table[index].chs_start[1] = table[index].chs_end[1] = 0xff;
    table[index].chs_start[2] = table[index].chs_end[2] = 0xff;
    table[index].type = type;
    table[index].lba = lba;
    table[index].sectors = sectors;
    return FS_OK;
}

struct co_part_reset {
    struct coroutine co;
    unsigned char *mbr;
    struct disk_request *req;
    struct fs_completion *completion;
    bool rescan;
};

static co_state_t co_reset_handler(struct coroutine *co);

static void reset_partition(struct disk *disk, struct fs_completion *completion, bool rescan) {
    fs_status_t result = FS_ERR_NOMEM;

    unsigned char *mbr = kzalloc(DISK_SECTOR_SIZE);
    if (!mbr) {
        goto err_mbr;
    }

    if (!rescan) {
        mbr[510] = 0x55;
        mbr[511] = 0xaa;
    }

    struct co_part_reset *ctx = kzalloc(sizeof(*ctx));
    if (!ctx) {
        goto err_ctx;
    }

    irqlock_t irqlock = irqlock_acquire();

    if (disk_req_queue_is_full_unlocked(disk->req_queue)) {
        result = FS_ERR_BUSY;
        goto err_req;
    }
    if (!lock_partitions(disk)) {
        result = FS_ERR_BUSY;
        goto err_req;
    }

    ctx->mbr = mbr;
    ctx->completion = completion;
    ctx->rescan = rescan;
    ctx->req = rescan ? disk_read(disk, 0, 1, mbr) : disk_write(disk, 0, 1, mbr);
    assert(ctx->req);

    irqlock_release(&irqlock);

    coroutine_init(&ctx->co, co_reset_handler);
    coroutine_set_ready(&ctx->co);
    return;

err_req:
    irqlock_release(&irqlock);
    kfree(ctx, sizeof(*ctx));
err_ctx:
    kfree(mbr, DISK_SECTOR_SIZE);
err_mbr:
    complete_not_null(completion, result);
}

void disk_rescan_partition(struct disk *disk, struct fs_completion *completion) {
    reset_partition(disk, completion, true);
}

void disk_reset_partition(struct disk *disk, struct fs_completion *completion) {
    reset_partition(disk, completion, false);
}

static co_state_t co_reset_handler(struct coroutine *co) {
    struct co_part_reset *ctx = container_of(co, struct co_part_reset, co);
    fs_status_t result;

    if (co->state == CO_DONE) {
        kfree(ctx, sizeof(*ctx));
        return CO_DONE;
    }

    struct disk *disk = ctx->req->disk;
    disk_request_wait(ctx->req, TIMEOUT_INFINITY, &result);
    if (result != FS_OK) {
        kerror("reset_partition: fs error %d", result);
        goto err;
    }

    irqlock_t irqlock = irqlock_acquire();

    unregister_partitions(disk);

    struct span old = disk->part_table_sectors;
    disk->part_table_sectors = SPAN(ctx->mbr, DISK_SECTOR_SIZE);
    if (old.ptr) {
        kfree(old.ptr, old.size);
    }

    if (ctx->rescan) {
        build_part_table(disk);
    }

    result = FS_OK;
    goto done;

err:
    irqlock = irqlock_acquire();
    unlock_partitions(disk);
    kfree(ctx->mbr, DISK_SECTOR_SIZE);

done:
    irqlock_release(&irqlock);
    complete_not_null(ctx->completion, result);
    return CO_DONE;
}

struct co_part_modify {
    struct coroutine co;
    struct disk_request *req;
    struct fs_completion *completion;
    struct partition_entry *entry;
    struct mbr_part_entry old_mbr_entry;
    struct span partname;
    size_t index;
    fs_size_t lba;
    fs_size_t sectors;
};

static co_state_t co_modify_handler(struct coroutine *co);

static void modify_partition(
    struct disk *disk, size_t index, fs_size_t lba, fs_size_t sectors, uint8_t type,
    bool removal, struct fs_completion *completion
) {
    fs_status_t result = FS_ERR_NOMEM;

    if (!removal && (type == 0 || lba + sectors <= lba)) {
        result = FS_ERR_INVAL;
        goto err_ctx;
    }

    if (index >= 4) {
        result = FS_ERR_NOENT;
        goto err_ctx;
    }

    struct co_part_modify *ctx = kzalloc(sizeof(*ctx));
    if (!ctx) {
        goto err_ctx;
    }

    irqlock_t irqlock = irqlock_acquire();

    if (!disk->part_table_sectors.ptr) {
        result = FS_ERR_INVAL;
        goto err_req;
    }

    ctx->entry = NULL;
    dynarray_foreach(struct partition_entry *, entry, &disk->partitions) {
        if (entry->index == index) {
            ctx->entry = entry;
            break;
        }
    }

    if (removal && !ctx->entry) {
        result = FS_ERR_NOENT;
        goto err_req;
    }
    if (!removal && ctx->entry) {
        result = FS_ERR_EXIST;
        goto err_req;
    }

    struct span partname = SPAN_NULL;
    if (!ctx->entry) {
        partname = alloc_part_name(disk, index);
        if (!partname.ptr) {
            goto err_name;
        }
    }

    if (disk_req_queue_is_full_unlocked(disk->req_queue)) {
        result = FS_ERR_BUSY;
        goto err_name;
    }
    if (ctx->entry && !block_device_retain(ctx->entry->bdev)) {
        result = FS_ERR_BUSY;
        goto err_name;
    }

    struct mbr_part_entry table[4];
    unsigned char *mbr = disk->part_table_sectors.ptr;
    unsigned char *mbr_entry = &mbr[PARTITION_TABLE_OFFSET + index * sizeof(*table)];
    memcpy(table, &mbr[PARTITION_TABLE_OFFSET], sizeof(table));
    ctx->old_mbr_entry = table[index];

    if (removal) {
        memset(mbr_entry, 0, sizeof(*table));
    } else {
        fs_status_t r = apply_create_part(disk, table, index, lba, sectors, type);
        if (r != FS_OK) {
            result = r;
            goto err_name;
        }
        memcpy(mbr_entry, &table[index], sizeof(*table));
    }

    ctx->completion = completion;
    ctx->partname = partname;
    ctx->index = index;
    ctx->lba = lba;
    ctx->sectors = sectors;
    ctx->req = disk_write(disk, 0, 1, mbr);
    assert(ctx->req);

    irqlock_release(&irqlock);

    coroutine_init(&ctx->co, co_modify_handler);
    coroutine_set_ready(&ctx->co);
    return;

err_name:
    if (partname.ptr) {
        kfree_span(partname);
    }
err_req:
    irqlock_release(&irqlock);
    kfree(ctx, sizeof(*ctx));
err_ctx:
    complete_not_null(completion, result);
}

void disk_create_partition(
    struct disk *disk, size_t index, fs_size_t lba, fs_size_t sectors, uint8_t type,
    struct fs_completion *completion
) {
    modify_partition(disk, index, lba, sectors, type, false, completion);
}

void disk_remove_partition(struct disk *disk, size_t index, struct fs_completion *completion) {
    modify_partition(disk, index, 0, 0, 0, true, completion);
}

static co_state_t co_modify_handler(struct coroutine *co) {
    struct co_part_modify *ctx = container_of(co, struct co_part_modify, co);
    fs_status_t result;

    if (co->state == CO_DONE) {
        kfree(ctx, sizeof(*ctx));
        return CO_DONE;
    }

    struct disk *disk = ctx->req->disk;
    disk_request_wait(ctx->req, TIMEOUT_INFINITY, &result);

    irqlock_t irqlock = irqlock_acquire();

    if (result != FS_OK) {
        kerror("modify_partition: fs error %d", result);
        goto err_io;
    }

    if (ctx->entry) {
        destroy_partition(ctx->entry->bdev);

        size_t partidx = ctx->entry - (struct partition_entry *)disk->partitions.data;
        dynarray_remove_at(&disk->partitions, struct partition_entry, partidx);
    } else {
        struct partition_entry *new_entry = dynarray_push_back(&disk->partitions, sizeof(*new_entry));
        if (!new_entry) {
            kerror("modify_partition: cannot register block device %s", (char *)ctx->partname.ptr);
            result = FS_ERR_NOMEM;
            goto err;
        }

        struct block_device *bdev = block_device_create(disk, ctx->partname.ptr, ctx->lba, ctx->sectors);
        if (!bdev) {
            kerror("modify_partition: cannot register block device %s", (char *)ctx->partname.ptr);
            dynarray_pop_back(&disk->partitions, sizeof(*new_entry));
            result = FS_ERR_NOMEM;
            goto err;
        }

        new_entry->index = ctx->index;
        new_entry->bdev = bdev;
    }

    result = FS_OK;
    goto done;

err_io:
    size_t entry_size = sizeof(struct mbr_part_entry);
    unsigned char *mbr = disk->part_table_sectors.ptr;
    unsigned char *mbr_entry = &mbr[PARTITION_TABLE_OFFSET + ctx->index * entry_size];
    memcpy(mbr_entry, &ctx->old_mbr_entry, entry_size);

err:
    if (ctx->entry) {
        block_device_release(ctx->entry->bdev);
    }
    if (ctx->partname.ptr) {
        kfree_span(ctx->partname);
    }

done:
    irqlock_release(&irqlock);
    complete_not_null(ctx->completion, result);
    return CO_DONE;
}
