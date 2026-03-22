#include <kc/string.h>
#include <kc/inttypes.h>

#include <opal/tty.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/wait_list.h>
#include <opal/shell/utils.h>
#include <opal/platform/drivers/pata.h>

static const char *skip_spaces(const char *p) {
    p += strspn(p, " ");
    return p;
}

static bool parse_u32_arg(const char **p_inout, uint32_t *out) {
    const char *p = skip_spaces(*p_inout);
    if (*p < '0' || *p > '9') {
        return false;
    }

    uint32_t value = 0;
    while (*p >= '0' && *p <= '9') {
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > UINT32_MAX / 10 || (value == UINT32_MAX / 10 && digit > UINT32_MAX % 10)) {
            return false;
        }
        value = value * 10 + digit;
        p++;
    }

    *out = value;
    *p_inout = p;
    return true;
}

static bool parse_u32_arg_auto_base(const char **p_inout, uint32_t *out) {
    const char *p = skip_spaces(*p_inout);
    uint32_t base = 10;

    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }

    uint32_t value = 0;
    bool has_digit = false;
    while (1) {
        uint32_t digit = UINT32_MAX;
        if (*p >= '0' && *p <= '9') {
            digit = (uint32_t)(*p - '0');
        } else if (base == 16 && *p >= 'a' && *p <= 'f') {
            digit = (uint32_t)(*p - 'a' + 10);
        } else if (base == 16 && *p >= 'A' && *p <= 'F') {
            digit = (uint32_t)(*p - 'A' + 10);
        }

        if (digit >= base) {
            break;
        }

        has_digit = true;
        if (value > (UINT32_MAX - digit) / base) {
            return false;
        }
        value = value * base + digit;
        p++;
    }

    if (!has_digit) {
        return false;
    }

    *out = value;
    *p_inout = p;
    return true;
}

static bool parse_cmd(const char *cmd, bool *is_write, uint32_t *drive, uint32_t *lba, uint32_t *count, uint32_t *fill) {
    const char *p = cmd;
    if (strncmp(p, "readsec", 7) == 0 && (p[7] == '\0' || p[7] == ' ')) {
        *is_write = false;
        p += 7;
    } else if (strncmp(p, "writesec", 8) == 0 && (p[8] == '\0' || p[8] == ' ')) {
        *is_write = true;
        p += 8;
    } else {
        return false;
    }

    if (!parse_u32_arg(&p, drive) || !parse_u32_arg(&p, lba) || !parse_u32_arg(&p, count)) {
        return false;
    }

    if (*is_write) {
        if (!parse_u32_arg_auto_base(&p, fill)) {
            return false;
        }
    }

    p = skip_spaces(p);
    return *p == '\0';
}

int shell_cmd_pata(const char *cmd) {
    bool is_write = false;
    uint32_t drive_u32 = 0;
    uint32_t lba = 0;
    uint32_t count_u32 = 0;
    uint32_t fill_u32 = 0;
    if (!parse_cmd(cmd, &is_write, &drive_u32, &lba, &count_u32, &fill_u32)) {
        tty0_puts("usage: readsec [drive] [index] [count]\n");
        tty0_puts("       writesec [drive] [index] [count] [value]\n");
        return 1;
    }

    if (drive_u32 >= PATA_DEVICE_COUNT) {
        tty0_printf("%s: invalid drive %u (expected 0..3)\n", is_write ? "writesec" : "readsec", drive_u32);
        return 1;
    }

    if (count_u32 == 0 || count_u32 > UINT8_MAX) {
        tty0_printf("%s: invalid count %u (expected 1..255)\n", is_write ? "writesec" : "readsec", count_u32);
        return 1;
    }

    if (is_write && fill_u32 > UINT8_MAX) {
        tty0_printf("writesec: invalid value %u (expected 0..255)\n", fill_u32);
        return 1;
    }

    uint8_t count = (uint8_t)count_u32;
    uint8_t fill = (uint8_t)fill_u32;
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
        token = pata_write_sectors((enum pata_device_index)drive_u32, lba, buf, count);
    } else {
        token = pata_read_sectors((enum pata_device_index)drive_u32, lba, buf, count);
    }

    if (token == PATA_INVALID_TOKEN) {
        tty0_printf("%s: submit failed (drive=%u lba=%u count=%u)\n",
            is_write ? "writesec" : "readsec", drive_u32, lba, count_u32);
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
            is_write ? "writesec" : "readsec", drive_u32, lba, count_u32);
        kfree(buf, bytes);
        return 1;
    }

    if (is_write) {
        tty0_printf("writesec: wrote %u sector(s) to drive=%u lba=%u with %#02x\n",
            count_u32, drive_u32, lba, fill);
    } else {
        tty0_printf("readsec: drive=%u lba=%u count=%u (%zu bytes)\n", drive_u32, lba, count_u32, bytes);
        shell_hexdump((const unsigned char *)buf, bytes);
    }

    kfree(buf, bytes);
    return 1;
}
