#include <kc/string.h>
#include <kc/inttypes.h>
#include <kc/stdlib.h>

#include <opal/tty.h>
#include <opal/mm/mm.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/wait_list.h>
#include <opal/shell/utils.h>
#include <opal/platform/drivers/pata.h>

enum {
    TESTRWSEC_ORDER = 8,
    TESTRWSEC_SECTORS = 2000,
    TESTRWSEC_PATTERN_A = 0x5a,
    TESTRWSEC_PATTERN_B = 0xa5,
};

static_assert((TESTRWSEC_SECTORS * PATA_SECTOR_SIZE) <= (PAGE_SIZE << TESTRWSEC_ORDER));

int shell_cmd_rwsec(int argc, char **argv) {
    bool is_write = false;
    unsigned long drive_ul = 0;
    unsigned long lba_ul = 0;
    unsigned long count_ul = 0;
    unsigned long fill_ul = 0;
    const size_t max_count = KMALLOC_MAX_SIZE / PATA_SECTOR_SIZE;

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
        || kstrtoul_exact(argv[1], 10, ULONG_MAX, &drive_ul) != E_OK
        || kstrtoul_exact(argv[2], 10, ULONG_MAX, &lba_ul) != E_OK
        || kstrtoul_exact(argv[3], 10, ULONG_MAX, &count_ul) != E_OK
        || (is_write && kstrtoul_exact(argv[4], 0, ULONG_MAX, &fill_ul) != E_OK)
    ) {
        tty0_puts("usage: readsec [drive] [index] [count]\n");
        tty0_puts("       writesec [drive] [index] [count] [value]\n");
        return 1;
    }

    if (drive_ul >= PATA_DEVICE_COUNT) {
        tty0_printf("%s: invalid drive %lu (expected 0..3)\n", is_write ? "writesec" : "readsec", drive_ul);
        return 1;
    }

    if (lba_ul >= 1 << 28) {
        tty0_printf("%s: invalid LBA %lu\n", is_write ? "writesec" : "readsec", lba_ul);
        return 1;
    }

    if (count_ul == 0 || count_ul > max_count) {
        tty0_printf("%s: invalid count %lu (expected 1..%zu)\n",
            is_write ? "writesec" : "readsec", count_ul, max_count);
        return 1;
    }

    if (is_write && fill_ul > UINT8_MAX) {
        tty0_printf("writesec: invalid value %lu (expected 0..255)\n", fill_ul);
        return 1;
    }

    enum pata_device_index drive = (enum pata_device_index)drive_ul;
    uint32_t lba = (uint32_t)lba_ul;
    uint32_t count = (uint32_t)count_ul;
    uint8_t fill = (uint8_t)fill_ul;
    size_t bytes = (size_t)count * PATA_SECTOR_SIZE;
    void *buf = kmalloc(bytes);
    if (!buf) {
        tty0_printf("%s: allocation failed (%zu bytes)\n", is_write ? "writesec" : "readsec", bytes);
        return 1;
    }

    pata_token_t token = PATA_INVALID_TOKEN;
    enum pata_wait_result wait_result = PATA_WAIT_INVALID;
    if (is_write) {
        memset(buf, fill, bytes);
        token = pata_write_sectors(drive, lba, buf, count);
    } else {
        token = pata_read_sectors(drive, lba, buf, count);
    }

    if (token == PATA_INVALID_TOKEN) {
        tty0_printf("%s: submit failed (drive=%u lba=%u count=%u)\n",
            is_write ? "writesec" : "readsec", drive, lba, count);
        kfree(buf, bytes);
        return 1;
    }

    wait_result = pata_wait(token, TIMEOUT_INFINITY);
    if (wait_result == PATA_WAIT_TIMEOUT) {
        tty0_printf("%s: timeout\n", is_write ? "writesec" : "readsec");
        kfree(buf, bytes);
        return 1;
    }
    if (wait_result == PATA_WAIT_INVALID) {
        tty0_printf("%s: invalid token\n", is_write ? "writesec" : "readsec");
        kfree(buf, bytes);
        return 1;
    }
    if (wait_result == PATA_WAIT_IO_FAIL) {
        tty0_printf("%s: io failed (drive=%u lba=%u count=%u)\n",
            is_write ? "writesec" : "readsec", drive, lba, count);
        kfree(buf, bytes);
        return 1;
    }

    if (is_write) {
        tty0_printf("writesec: wrote %u sector(s) to drive=%u lba=%u with %#02x\n",
            count, drive, lba, fill);
    } else {
        tty0_printf("readsec: drive=%u lba=%u count=%u (%zu bytes)\n", drive, lba, count, bytes);
        shell_hexdump((const unsigned char *)buf, bytes);
    }

    kfree(buf, bytes);
    return 0;
}

static int submit_and_wait(
    const char *cmd_name,
    const char *phase,
    bool is_write,
    enum pata_device_index drive,
    uint32_t lba,
    void *buf,
    uint32_t count
) {
    pata_token_t token = is_write
        ? pata_write_sectors(drive, lba, buf, count)
        : pata_read_sectors(drive, lba, buf, count);
    if (token == PATA_INVALID_TOKEN) {
        tty0_printf("%s: %s submit failed (drive=%u lba=%u count=%u)\n",
            cmd_name, phase, drive, lba, count);
        return 1;
    }

    enum pata_wait_result wait_result = pata_wait(token, TIMEOUT_INFINITY);
    if (wait_result == PATA_WAIT_TIMEOUT) {
        tty0_printf("%s: %s timeout\n", cmd_name, phase);
        return 1;
    }
    if (wait_result == PATA_WAIT_INVALID) {
        tty0_printf("%s: %s invalid token\n", cmd_name, phase);
        return 1;
    }
    if (wait_result == PATA_WAIT_IO_FAIL) {
        tty0_printf("%s: %s io failed (drive=%u lba=%u count=%u)\n",
            cmd_name, phase, drive, lba, count);
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

    if (argc != 2 && argc != 3) {
        tty0_puts("usage: testrwsec [drive] [lba]\n");
        return 1;
    }

    if (kstrtoul_exact(argv[1], 10, ULONG_MAX, &drive_ul) != E_OK || drive_ul >= PATA_DEVICE_COUNT) {
        tty0_printf("testrwsec: invalid drive %lu (expected 0..3)\n", drive_ul);
        return 1;
    }
    if (argc == 3 && kstrtoul_exact(argv[2], 10, ULONG_MAX, &lba_ul) != E_OK) {
        tty0_printf("testrwsec: invalid LBA %lu\n", lba_ul);
        return 1;
    }
    if (lba_ul >= (1UL << 28)) {
        tty0_printf("testrwsec: invalid LBA %lu\n", lba_ul);
        return 1;
    }

    enum pata_device_index drive = (enum pata_device_index)drive_ul;
    uint32_t lba = (uint32_t)lba_ul;

    const struct pata_device *dev = pata_get_device(drive);
    if (!dev) {
        tty0_printf("testrwsec: drive=%u is not present\n", drive);
        return 1;
    }
    if (dev->is_atapi) {
        tty0_printf("testrwsec: drive=%u is atapi (not supported)\n", drive);
        return 1;
    }

    const uint32_t count = TESTRWSEC_SECTORS;
    uint64_t end = (uint64_t)lba + (uint64_t)count;
    if (end > dev->lba28_sectors) {
        tty0_printf("testrwsec: range out of bounds (drive=%u lba=%u count=%u sectors=%u)\n",
            drive, lba, count, dev->lba28_sectors);
        return 1;
    }

    pfn_t pfn = mm_alloc_page(TESTRWSEC_ORDER);
    if (pfn == PFN_INVALID) {
        tty0_puts("testrwsec: buffer alloc failed\n");
        return 1;
    }

    unsigned char *buf = (unsigned char *)mm_pfn_to_ptr(pfn);
    size_t bytes = (size_t)count * PATA_SECTOR_SIZE;
    int ret = 1;

    tty0_printf("testrwsec: drive=%u lba=%u count=%u bytes=%zu\n", drive, lba, count, bytes);

    memset(buf, TESTRWSEC_PATTERN_A, bytes);
    if (submit_and_wait("testrwsec", "write pattern A", true, drive, lba, buf, count) != 0) {
        goto exit;
    }
    memset(buf, 0, bytes);
    if (submit_and_wait("testrwsec", "read pattern A", false, drive, lba, buf, count) != 0) {
        goto exit;
    }
    size_t bad_idx = 0;
    uint8_t bad_val = 0;
    if (!verify_pattern(buf, bytes, TESTRWSEC_PATTERN_A, &bad_idx, &bad_val)) {
        tty0_printf("testrwsec: verify pattern A failed at offset=%zu (got=%#02x expected=%#02x)\n",
            bad_idx, bad_val, TESTRWSEC_PATTERN_A);
        goto exit;
    }

    memset(buf, TESTRWSEC_PATTERN_B, bytes);
    if (submit_and_wait("testrwsec", "write pattern B", true, drive, lba, buf, count) != 0) {
        goto exit;
    }
    memset(buf, 0, bytes);
    if (submit_and_wait("testrwsec", "read pattern B", false, drive, lba, buf, count) != 0) {
        goto exit;
    }
    if (!verify_pattern(buf, bytes, TESTRWSEC_PATTERN_B, &bad_idx, &bad_val)) {
        tty0_printf("testrwsec: verify pattern B failed at offset=%zu (got=%#02x expected=%#02x)\n",
            bad_idx, bad_val, TESTRWSEC_PATTERN_B);
        goto exit;
    }

    tty0_printf("testrwsec: PASS (2 patterns, %d sectors)\n", TESTRWSEC_SECTORS);
    ret = 0;

exit:
    mm_free_page(pfn, TESTRWSEC_ORDER);
    return ret;
}
