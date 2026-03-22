#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_PATA_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_PATA_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define PATA_SECTOR_SIZE 512

#define PATA_INVALID_TOKEN UINT32_MAX

enum pata_device_index : uint8_t {
    PATA_HDA,
    PATA_HDB,
    PATA_HDC,
    PATA_HDD,
    PATA_DEVICE_COUNT,
};

enum pata_wait_result : uint8_t {
    PATA_WAIT_INVALID,
    PATA_WAIT_TIMEOUT,
    PATA_WAIT_IO_OK,
    PATA_WAIT_IO_FAIL,
};

typedef uint32_t pata_token_t;

struct pata_device {
    bool present;
    bool is_atapi;
    uint8_t channel;
    uint8_t drive;
    uint32_t lba28_sectors;
    char serial[21];
    char model[41];
};

void pata_init(void);
void pata_on_timer(uint64_t now_tick);
[[nodiscard]] const struct pata_device *pata_get_device(enum pata_device_index index);

[[nodiscard]] pata_token_t pata_read_sectors(enum pata_device_index index, uint32_t lba, void *buf, uint8_t sectors);
[[nodiscard]] pata_token_t pata_write_sectors(enum pata_device_index index, uint32_t lba, const void *buf, uint8_t sectors);

[[nodiscard]] enum pata_wait_result pata_wait(pata_token_t token, uint64_t timeout);

#endif
