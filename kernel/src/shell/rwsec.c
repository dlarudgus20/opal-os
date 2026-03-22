#include <kc/string.h>
#include <kc/inttypes.h>
#include <kc/stdlib.h>

#include <opal/tty.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/wait_list.h>
#include <opal/shell/utils.h>
#include <opal/platform/drivers/pata.h>

int shell_cmd_rwsec(int argc, char **argv) {
    bool is_write = false;
    unsigned long drive_ul = 0;
    unsigned long lba_ul = 0;
    unsigned long count_ul = 0;
    unsigned long fill_ul = 0;

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

    if (count_ul == 0 || count_ul > UINT8_MAX) {
        tty0_printf("%s: invalid count %lu (expected 1..255)\n", is_write ? "writesec" : "readsec", count_ul);
        return 1;
    }

    if (is_write && fill_ul > UINT8_MAX) {
        tty0_printf("writesec: invalid value %lu (expected 0..255)\n", fill_ul);
        return 1;
    }

    enum pata_device_index drive = (enum pata_device_index)drive_ul;
    uint32_t lba = (uint32_t)lba_ul;
    uint8_t count = (uint8_t)count_ul;
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
        token = pata_write_sectors(drive, lba_ul, buf, count);
    } else {
        token = pata_read_sectors(drive, lba_ul, buf, count);
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
