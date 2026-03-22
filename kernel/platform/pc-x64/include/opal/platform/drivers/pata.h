#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_PATA_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_PATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum pata_device_index : uint8_t {
    PATA_HDA,
    PATA_HDB,
    PATA_HDC,
    PATA_HDD,
    PATA_DEVICE_COUNT,
};

#define PATA_SECTOR_SIZE 512

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
[[nodiscard]] const struct pata_device *pata_get_device(enum pata_device_index index);

[[nodiscard]] bool pata_read28(enum pata_device_index index, uint32_t lba, void *buf, uint8_t sector_count);
[[nodiscard]] bool pata_write28(enum pata_device_index index, uint32_t lba, const void *buf, uint8_t sector_count);

#endif
