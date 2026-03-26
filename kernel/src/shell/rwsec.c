#include <kc/string.h>
#include <kc/inttypes.h>
#include <kc/stdlib.h>

#include <opal/tty.h>
#include <opal/fs/block_device.h>
#include <opal/fs/disk.h>
#include <opal/mm/mm.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/wait_list.h>
#include <opal/shell/utils.h>

enum {
    TESTRWSEC_ORDER = 8,
    TESTRWSEC_SECTORS = 2000,
    TESTRWSEC_PATTERN_A = 0x5a,
    TESTRWSEC_PATTERN_B = 0xa5,
};

static_assert((TESTRWSEC_SECTORS * DISK_SECTOR_SIZE) <= (PAGE_SIZE << TESTRWSEC_ORDER));

int shell_cmd_rwsec(int argc, char **argv) {
    bool is_write = false;
    unsigned long drive_ul = 0;
    unsigned long lba_ul = 0;
    unsigned long count_ul = 0;
    unsigned long fill_ul = 0;
    int ret = 1;

    if (argc > 0 && argv[0]) {
        if (argv[0][0] == 'r') {
            is_write = false;
        } else if (argv[0][0] == 'w') {
            is_write = true;
        } else {
            return 1;
        }
    }

    int expected_argc = is_write ? 5 : 4;
    if (argc != expected_argc
        || kstrtoul_exact(argv[1], 10, ULONG_MAX, &drive_ul) != KE_OK
        || kstrtoul_exact(argv[2], 10, ULONG_MAX, &lba_ul) != KE_OK
        || kstrtoul_exact(argv[3], 10, ULONG_MAX, &count_ul) != KE_OK
        || (is_write && kstrtoul_exact(argv[4], 0, ULONG_MAX, &fill_ul) != KE_OK)
    ) {
        tty0_puts("usage: readsec [drive] [index] [count]\n");
        tty0_puts("       writesec [drive] [index] [count] [value]\n");
        return 1;
    }

    struct block_device *dev;
    if (bdev_list_get((size_t)drive_ul, &dev) != FS_OK) {
        size_t count = bdev_list_count();
        tty0_printf("%s: invalid device %lu (expected 0..%zu)\n",
            is_write ? "writesec" : "readsec", drive_ul, count ? count - 1 : 0);
        return 1;
    }

    if (dev->sectors == 0) {
        tty0_printf("%s: invalid sector size for dev=%lu\n", is_write ? "writesec" : "readsec", drive_ul);
        goto err_dev;
    }
    const size_t max_count = KMALLOC_MAX_SIZE / DISK_SECTOR_SIZE;

    if (lba_ul > UINT32_MAX) {
        tty0_printf("%s: invalid LBA %lu\n", is_write ? "writesec" : "readsec", lba_ul);
        goto err_dev;
    }

    if (count_ul == 0 || count_ul > max_count) {
        tty0_printf("%s: invalid count %lu (expected 1..%zu)\n",
            is_write ? "writesec" : "readsec", count_ul, max_count);
        goto err_dev;
    }

    if (is_write && fill_ul > UINT8_MAX) {
        tty0_printf("writesec: invalid value %lu (expected 0..255)\n", fill_ul);
        goto err_dev;
    }

    uint32_t lba = (uint32_t)lba_ul;
    uint32_t count = (uint32_t)count_ul;
    uint8_t fill = (uint8_t)fill_ul;
    if ((uint64_t)lba + (uint64_t)count > dev->sectors) {
        tty0_printf("%s: range out of bounds (dev=%lu lba=%u count=%u sectors=%zu)\n",
            is_write ? "writesec" : "readsec", drive_ul, lba, count, dev->sectors);
        goto err_dev;
    }

    size_t bytes = (size_t)count * DISK_SECTOR_SIZE;
    void *buf = kzalloc(bytes);
    if (!buf) {
        tty0_printf("%s: allocation failed (%zu bytes)\n", is_write ? "writesec" : "readsec", bytes);
        goto err_dev;
    }

    struct disk_request *req;
    fs_status_t io_result = FS_OK;
    if (is_write) {
        memset(buf, fill, bytes);
        req = block_device_write(dev, lba, count, buf);
    } else {
        req = block_device_read(dev, lba, count, buf);
    }

    if (!req) {
        tty0_printf("%s: submit failed (dev=%lu lba=%u count=%u)\n",
            is_write ? "writesec" : "readsec", drive_ul, lba, count);
        goto err_buf;
    }

    if (!disk_request_wait(req, TIMEOUT_INFINITY, &io_result)) {
        tty0_printf("%s: timeout\n", is_write ? "writesec" : "readsec");
        goto err_buf;
    }
    if (io_result != FS_OK) {
        tty0_printf("%s: io failed (dev=%lu lba=%u count=%u status=%d)\n",
            is_write ? "writesec" : "readsec", drive_ul, lba, count, io_result);
        goto err_buf;
    }

    if (is_write) {
        tty0_printf("writesec: wrote %u sector(s) to dev=%lu lba=%u with %#02x\n",
            count, drive_ul, lba, fill);
    } else {
        tty0_printf("readsec: dev=%lu lba=%u count=%u (%zu bytes)\n", drive_ul, lba, count, bytes);
        shell_hexdump((const unsigned char *)buf, bytes);
    }

    ret = 0;

err_buf:
    kfree(buf, bytes);
err_dev:
    block_device_release(dev);
    return ret;
}

static int submit_and_wait(
    const char *cmd_name,
    const char *phase,
    bool is_write,
    struct block_device *dev,
    unsigned long dev_index,
    uint32_t lba,
    void *buf,
    uint32_t count
) {
    struct disk_request *req = is_write
        ? block_device_write(dev, lba, count, buf)
        : block_device_read(dev, lba, count, buf);
    if (!req) {
        tty0_printf("%s: %s submit failed (dev=%lu lba=%u count=%u)\n",
            cmd_name, phase, dev_index, lba, count);
        return 1;
    }

    fs_status_t io_result = FS_OK;
    if (!disk_request_wait(req, TIMEOUT_INFINITY, &io_result)) {
        tty0_printf("%s: %s timeout\n", cmd_name, phase);
        return 1;
    }
    if (io_result != FS_OK) {
        tty0_printf("%s: %s io failed (dev=%lu lba=%u count=%u status=%d)\n",
            cmd_name, phase, dev_index, lba, count, io_result);
        return 1;
    }

    return 0;
}

[[nodiscard]] static bool verify_pattern(const unsigned char *buf, size_t len, uint8_t pattern, size_t *bad_idx, uint8_t *bad_val) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != pattern) {
            if (bad_idx) {
                *bad_idx = i;
            }
            if (bad_val) {
                *bad_val = buf[i];
            }
            return false;
        }
    }
    return true;
}

int shell_cmd_testrwsec(int argc, char **argv) {
    unsigned long drive_ul = 0;
    unsigned long lba_ul = 0;
    int ret = 1;

    if (argc != 2 && argc != 3) {
        tty0_puts("usage: testrwsec [drive] (lba)\n");
        return 1;
    }

    if (kstrtoul_exact(argv[1], 10, ULONG_MAX, &drive_ul) != KE_OK) {
        tty0_printf("testrwsec: invalid drive\n");
        return 1;
    }
    if (argc == 3 && kstrtoul_exact(argv[2], 10, ULONG_MAX, &lba_ul) != KE_OK) {
        tty0_printf("testrwsec: invalid LBA\n");
        return 1;
    }

    struct block_device *dev;
    if (bdev_list_get((size_t)drive_ul, &dev) != FS_OK) {
        size_t count = bdev_list_count();
        tty0_printf("testrwsec: invalid device %lu (expected 0..%zu)\n",
            drive_ul, count ? count - 1 : 0);
        return 1;
    }
    if (lba_ul >= dev->sectors) {
        tty0_printf("testrwsec: invalid LBA %lu\n", lba_ul);
        goto err_dev;
    }

    uint32_t lba = (uint32_t)lba_ul;

    const uint32_t count = TESTRWSEC_SECTORS;
    uint64_t end = (uint64_t)lba + (uint64_t)count;
    if (end > dev->sectors) {
        tty0_printf("testrwsec: range out of bounds (dev=%lu lba=%u count=%u sectors=%zu)\n",
            drive_ul, lba, count, dev->sectors);
        goto err_dev;
    }

    unsigned char *buf = mm_alloc_page_ptr(TESTRWSEC_ORDER);
    if (!buf) {
        tty0_puts("testrwsec: buffer alloc failed\n");
        goto err_dev;
    }

    size_t bytes = (size_t)count * DISK_SECTOR_SIZE;

    tty0_printf("testrwsec: dev=%lu lba=%u count=%u bytes=%zu\n", drive_ul, lba, count, bytes);

    memset(buf, TESTRWSEC_PATTERN_A, bytes);
    if (submit_and_wait("testrwsec", "write pattern A", true, dev, drive_ul, lba, buf, count) != 0) {
        goto err_buf;
    }
    memset(buf, 0, bytes);
    if (submit_and_wait("testrwsec", "read pattern A", false, dev, drive_ul, lba, buf, count) != 0) {
        goto err_buf;
    }
    size_t bad_idx = 0;
    uint8_t bad_val = 0;
    if (!verify_pattern(buf, bytes, TESTRWSEC_PATTERN_A, &bad_idx, &bad_val)) {
        tty0_printf("testrwsec: verify pattern A failed at offset=%zu (got=%#02x expected=%#02x)\n",
            bad_idx, bad_val, TESTRWSEC_PATTERN_A);
        goto err_buf;
    }

    memset(buf, TESTRWSEC_PATTERN_B, bytes);
    if (submit_and_wait("testrwsec", "write pattern B", true, dev, drive_ul, lba, buf, count) != 0) {
        goto err_buf;
    }
    memset(buf, 0, bytes);
    if (submit_and_wait("testrwsec", "read pattern B", false, dev, drive_ul, lba, buf, count) != 0) {
        goto err_buf;
    }
    if (!verify_pattern(buf, bytes, TESTRWSEC_PATTERN_B, &bad_idx, &bad_val)) {
        tty0_printf("testrwsec: verify pattern B failed at offset=%zu (got=%#02x expected=%#02x)\n",
            bad_idx, bad_val, TESTRWSEC_PATTERN_B);
        goto err_buf;
    }

    tty0_printf("testrwsec: PASS (2 patterns, %u sectors)\n", TESTRWSEC_SECTORS);

    ret = 0;

err_buf:
    mm_free_page_ptr(buf, TESTRWSEC_ORDER);
err_dev:
    block_device_release(dev);
    return ret;
}
