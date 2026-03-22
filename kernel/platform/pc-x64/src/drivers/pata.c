#include <kc/assert.h>
#include <kc/string.h>

#include <opal/klog.h>
#include <opal/timer.h>
#include <opal/platform/asm.h>
#include <opal/platform/irq_device.h>
#include <opal/platform/drivers/pata.h>
#include <opal/platform/drivers/pic.h>

#define ATA_REG_DATA            0
#define ATA_REG_ERROR           1
#define ATA_REG_FEATURES        1
#define ATA_REG_SECTOR_COUNT    2
#define ATA_REG_LBA0            3
#define ATA_REG_LBA1            4
#define ATA_REG_LBA2            5
#define ATA_REG_DRIVE_HEAD      6
#define ATA_REG_STATUS          7
#define ATA_REG_COMMAND         7

#define ATA_STATUS_ERR          0x01
#define ATA_STATUS_DRQ          0x08
#define ATA_STATUS_DF           0x20
#define ATA_STATUS_DRDY         0x40
#define ATA_STATUS_BSY          0x80

#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_CACHE_FLUSH     0xe7
#define ATA_CMD_IDENTIFY        0xec

#define ATA_CTRL_NIEN           0x02

#define ATA_DRIVE_LBA           0xe0

#define ATAPI_SIG1              0x14
#define ATAPI_SIG2              0xeb

#define ATA_TIMEOUT_MS          1000
#define ATA_POLL_SPIN           100000

enum {
    PATA_CHANNEL_PRIMARY = 0,
    PATA_CHANNEL_SECONDARY = 1,
    PATA_CHANNEL_COUNT = 2,
};

union pata_identify {
    uint16_t data[256];
    struct {
        uint16_t config;
        uint16_t _res_1_9[9];
        uint16_t serial[10];
        uint16_t _res_20_22[3];
        uint16_t firmware[4];
        uint16_t model[20];
        uint16_t max_transfer_block;
        uint16_t _res_48;
        uint16_t capabilities;
        uint16_t _res_50_59[10];
        uint16_t total_lba28[2];
        uint16_t _res_62;
        uint16_t mdma_modes;
        uint16_t _res_64_79[16];
        uint16_t major_ver;
        uint16_t minor_ver;
        uint16_t cmds_supported[3];
        uint16_t cmds_enabled[3];
        uint16_t udma_mode;
        uint16_t _res_89_99[11];
        uint16_t total_lba48[4];
    };
};

struct pata_channel {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t irq_line;

    bool active;
    volatile bool irq_fired;
    volatile uint8_t irq_status;
};

static struct pata_channel g_channels[PATA_CHANNEL_COUNT] = {
    [PATA_CHANNEL_PRIMARY] = {
        .io_base = 0x1f0,
        .ctrl_base = 0x3f6,
        .irq_line = PIC_IRQ_HDD1,
    },
    [PATA_CHANNEL_SECONDARY] = {
        .io_base = 0x170,
        .ctrl_base = 0x376,
        .irq_line = PIC_IRQ_HDD2,
    },
};

static struct pata_device g_devices[PATA_DEVICE_COUNT];

static uint8_t io_read8(const struct pata_channel *ch, uint8_t reg) {
    return in8(ch->io_base + reg);
}

static void io_write8(const struct pata_channel *ch, uint8_t reg, uint8_t value) {
    out8(ch->io_base + reg, value);
}

static uint8_t ctrl_read_status(const struct pata_channel *ch) {
    return in8(ch->ctrl_base);
}

static void ctrl_write(const struct pata_channel *ch, uint8_t value) {
    out8(ch->ctrl_base, value);
}

static void io_delay_400ns(const struct pata_channel *ch) {
    (void)ctrl_read_status(ch);
    (void)ctrl_read_status(ch);
    (void)ctrl_read_status(ch);
    (void)ctrl_read_status(ch);
}

static void pio_read_words(const struct pata_channel *ch, uint16_t *out, size_t words) {
    for (size_t i = 0; i < words; i++) {
        out[i] = in16(ch->io_base + ATA_REG_DATA);
    }
}

static void pio_write_words(const struct pata_channel *ch, const uint16_t *in, size_t words) {
    for (size_t i = 0; i < words; i++) {
        out16(ch->io_base + ATA_REG_DATA, in[i]);
    }
}

static void ata_select_drive(const struct pata_channel *ch, uint8_t drive, uint8_t head) {
    io_write8(ch, ATA_REG_DRIVE_HEAD, (uint8_t)(ATA_DRIVE_LBA | (drive ? 0x10 : 0x00) | (head & 0x0f)));
    io_delay_400ns(ch);
}

static bool wait_not_busy_spin(const struct pata_channel *ch) {
    for (uint32_t i = 0; i < ATA_POLL_SPIN; i++) {
        if ((io_read8(ch, ATA_REG_STATUS) & ATA_STATUS_BSY) == 0) {
            return true;
        }
    }
    return false;
}

static bool wait_drq_or_error_spin(const struct pata_channel *ch, uint8_t *status_out) {
    for (uint32_t i = 0; i < ATA_POLL_SPIN; i++) {
        uint8_t status = io_read8(ch, ATA_REG_STATUS);
        if ((status & ATA_STATUS_BSY) != 0) {
            continue;
        }
        if ((status & (ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) {
            if (status_out) {
                *status_out = status;
            }
            return true;
        }
    }
    return false;
}

static bool wait_irq_or_status(struct pata_channel *ch, uint8_t *status_out) {
    uint64_t start = timer_get_tick();
    const uint64_t timeout = ticks_from_ms(ATA_TIMEOUT_MS);

    while (timer_get_tick() - start <= timeout) {
        if (ch->irq_fired) {
            ch->irq_fired = false;
            if (status_out) {
                *status_out = ch->irq_status;
            }
            return true;
        }

        uint8_t status = io_read8(ch, ATA_REG_STATUS);
        if ((status & ATA_STATUS_BSY) == 0) {
            if (status_out) {
                *status_out = status;
            }
            if ((status & (ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) {
                return true;
            }
        }

        if (interrupt_is_enabled()) {
            wait_for_interrupt();
        }
    }

    return false;
}

static void words_to_chars(char *out, const uint16_t *id_words, int len) {
    for (int i = 0; i < len; i++) {
        uint16_t w = id_words[i];
        out[i * 2 + 0] = (char)(w >> 8);
        out[i * 2 + 1] = (char)(w & 0xff);
    }
    out[len * 2] = '\0';

    for (int i = len * 2 - 1; i >= 0; i--) {
        if (out[i] == ' ' || out[i] == '\0') {
            out[i] = '\0';
            continue;
        }
        break;
    }
}

static bool identify_device(uint8_t channel_idx, uint8_t drive, struct pata_device *out) {
    struct pata_channel *ch = &g_channels[channel_idx];

    ata_select_drive(ch, drive, 0);
    io_write8(ch, ATA_REG_SECTOR_COUNT, 0);
    io_write8(ch, ATA_REG_LBA0, 0);
    io_write8(ch, ATA_REG_LBA1, 0);
    io_write8(ch, ATA_REG_LBA2, 0);
    io_write8(ch, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = io_read8(ch, ATA_REG_STATUS);
    if (status == 0) {
        return false;
    }

    if (!wait_drq_or_error_spin(ch, &status)) {
        kwarn("pata: ch=%u drv=%u identify fail", channel_idx, drive);
        return false;
    }

    if (status & ATA_STATUS_DF) {
        kwarn("pata: ch=%u drv=%u device fault during identify", channel_idx, drive);
        return false;
    }

    if (status & ATA_STATUS_ERR) {
        uint8_t sig1 = io_read8(ch, ATA_REG_LBA1);
        uint8_t sig2 = io_read8(ch, ATA_REG_LBA2);
        if (sig1 == ATAPI_SIG1 && sig2 == ATAPI_SIG2) {
            memset(out, 0, sizeof(*out));
            out->present = true;
            out->is_atapi = true;
            out->channel = channel_idx;
            out->drive = drive;
            return true;
        } else {
            kwarn("pata: ch=%u drv=%u device error during identify", channel_idx, drive);
            return false;
        }
    }

    union pata_identify id;
    pio_read_words(ch, id.data, 256);

    memset(out, 0, sizeof(*out));
    out->present = true;
    out->is_atapi = false;
    out->channel = channel_idx;
    out->drive = drive;
    out->lba28_sectors = (uint32_t)id.total_lba28[0] | ((uint32_t)id.total_lba28[1] << 16);
    words_to_chars(out->serial, id.serial, 10);
    words_to_chars(out->model, id.model, 20);
    return true;
}

static bool range_valid(const struct pata_device *dev, uint32_t lba, uint8_t sector_count) {
    if (sector_count == 0 || dev->lba28_sectors == 0) {
        return false;
    }

    uint64_t end = (uint64_t)lba + (uint64_t)sector_count;
    return end <= dev->lba28_sectors;
}

static bool read28_impl(struct pata_device *dev, uint32_t lba, void *buf, uint8_t sector_count) {
    struct pata_channel *ch = &g_channels[dev->channel];
    uint8_t *bytes = buf;

    if (!wait_not_busy_spin(ch)) {
        return false;
    }

    ata_select_drive(ch, dev->drive, (uint8_t)(lba >> 24));
    io_write8(ch, ATA_REG_FEATURES, 0);
    io_write8(ch, ATA_REG_SECTOR_COUNT, sector_count);
    io_write8(ch, ATA_REG_LBA0, (uint8_t)lba);
    io_write8(ch, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    io_write8(ch, ATA_REG_LBA2, (uint8_t)(lba >> 16));

    ch->irq_fired = false;
    io_write8(ch, ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    for (uint8_t i = 0; i < sector_count; i++) {
        uint8_t status = 0;
        if (!wait_irq_or_status(ch, &status)) {
            return false;
        }
        if ((status & (ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) {
            return false;
        }
        if ((status & ATA_STATUS_DRQ) == 0) {
            if (!wait_drq_or_error_spin(ch, &status) || (status & ATA_STATUS_DRQ) == 0) {
                return false;
            }
        }

        pio_read_words(ch, (uint16_t *)(bytes + (size_t)i * PATA_SECTOR_SIZE), PATA_SECTOR_SIZE / 2);
    }

    return true;
}

static bool write28_impl(struct pata_device *dev, uint32_t lba, const void *buf, uint8_t sector_count) {
    struct pata_channel *ch = &g_channels[dev->channel];
    const uint8_t *bytes = buf;

    if (!wait_not_busy_spin(ch)) {
        return false;
    }

    ata_select_drive(ch, dev->drive, (uint8_t)(lba >> 24));
    io_write8(ch, ATA_REG_FEATURES, 0);
    io_write8(ch, ATA_REG_SECTOR_COUNT, sector_count);
    io_write8(ch, ATA_REG_LBA0, (uint8_t)lba);
    io_write8(ch, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    io_write8(ch, ATA_REG_LBA2, (uint8_t)(lba >> 16));

    ch->irq_fired = false;
    io_write8(ch, ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    for (uint8_t i = 0; i < sector_count; i++) {
        uint8_t status = 0;
        if (!wait_drq_or_error_spin(ch, &status)) {
            return false;
        }
        if ((status & (ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) {
            return false;
        }

        pio_write_words(ch, (const uint16_t *)(bytes + (size_t)i * PATA_SECTOR_SIZE), PATA_SECTOR_SIZE / 2);

        if (!wait_irq_or_status(ch, &status)) {
            return false;
        }
        if ((status & (ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) {
            return false;
        }
    }

    ch->irq_fired = false;
    io_write8(ch, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);

    uint8_t status = 0;
    if (!wait_irq_or_status(ch, &status)) {
        return false;
    }

    return (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) == 0;
}

static void isr_pata_primary(void) {
    struct pata_channel *ch = &g_channels[PATA_CHANNEL_PRIMARY];
    ch->irq_status = io_read8(ch, ATA_REG_STATUS);
    ch->irq_fired = true;
    irq_send_eoi(ch->irq_line);
}

static void isr_pata_secondary(void) {
    struct pata_channel *ch = &g_channels[PATA_CHANNEL_SECONDARY];
    ch->irq_status = io_read8(ch, ATA_REG_STATUS);
    ch->irq_fired = true;
    irq_send_eoi(ch->irq_line);
}

void pata_init(void) {
    memset(g_devices, 0, sizeof(g_devices));

    irq_register(PIC_IRQ_HDD1, isr_pata_primary);
    irq_register(PIC_IRQ_HDD2, isr_pata_secondary);
    irq_enable(PIC_IRQ_HDD1);
    irq_enable(PIC_IRQ_HDD2);

    for (uint8_t channel = 0; channel < PATA_CHANNEL_COUNT; channel++) {
        struct pata_channel *ch = &g_channels[channel];
        ch->irq_fired = false;
        ch->irq_status = 0;

        // check floating bus
        if (io_read8(ch, ATA_REG_STATUS) == 0xff) {
            continue;
        }

        // disable interrupt
        ctrl_write(ch, ATA_CTRL_NIEN);

        for (uint8_t drive = 0; drive < 2; drive++) {
            struct pata_device *dev = &g_devices[channel * 2 + drive];
            if (!identify_device(channel, drive, dev)) {
                continue;
            }

            ch->active |= dev->present;

            if (dev->is_atapi) {
                kinfo("pata: ch=%u drv=%u atapi detected", channel, drive);
            } else {
                kinfo("pata: ch=%u drv=%u model='%s' serial='%s' sectors=%u",
                    channel, drive, dev->model, dev->serial, dev->lba28_sectors);
            }
        }
    }

    for (uint8_t channel = 0; channel < PATA_CHANNEL_COUNT; channel++) {
        struct pata_channel *ch = &g_channels[channel];
        if (!ch->active) {
            continue;
        }

        // enable interrupt
        ctrl_write(ch, 0);
    }
}

const struct pata_device *pata_get_device(enum pata_device_index index) {
    assert(index < PATA_DEVICE_COUNT);

    const struct pata_device *dev = &g_devices[index];
    if (!dev->present) {
        return NULL;
    }

    return dev;
}

bool pata_read28(enum pata_device_index index, uint32_t lba, void *buf, uint8_t sector_count) {
    assert(buf);
    assert(index < PATA_DEVICE_COUNT);

    struct pata_device *dev = &g_devices[index];
    if (!dev->present || dev->is_atapi) {
        return false;
    }

    if (!range_valid(dev, lba, sector_count)) {
        return false;
    }

    return read28_impl(dev, lba, buf, sector_count);
}

bool pata_write28(enum pata_device_index index, uint32_t lba, const void *buf, uint8_t sector_count) {
    assert(buf);
    assert(index < PATA_DEVICE_COUNT);

    struct pata_device *dev = &g_devices[index];
    if (!dev->present || dev->is_atapi) {
        return false;
    }

    if (!range_valid(dev, lba, sector_count)) {
        return false;
    }

    return write28_impl(dev, lba, buf, sector_count);
}
