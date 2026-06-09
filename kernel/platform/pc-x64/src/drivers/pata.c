#include <kc/kassert.h>
#include <kc/string.h>
#include <kc/stdlib.h>

#include <opal/klog.h>
#include <opal/timer.h>
#include <opal/fs/disk.h>
#include <opal/locks/irqlock.h>
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

#define ATA_TIMEOUT_MS          3000
#define ATA_POLL_SPIN           100000

#define REQ_SLOTS 64

enum {
    PATA_PORT_PRIMARY = 0,
    PATA_PORT_SECONDARY = 1,
    PATA_PORT_COUNT = 2,
};

enum pata_device_index : uint8_t {
    PATA_HDA,
    PATA_HDB,
    PATA_HDC,
    PATA_HDD,
    PATA_DEVICE_COUNT,
};

enum pata_port_phase : uint8_t {
    PATA_PHASE_IDLE,
    PATA_PHASE_READ_DATA,
    PATA_PHASE_WRITE_DATA,
    PATA_PHASE_WRITE_DONE,
    PATA_PHASE_COMPLETE,
};

union pata_identify {
    uint16_t data[256];
    struct {
        uint16_t config;
        uint16_t _res_1_9[9];
        uint16_t serial[10];
        uint16_t _res_20_22[3];
        uint16_t firmware_version[4];
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
        uint16_t udma_modes;
        uint16_t _res_89_99[11];
        uint16_t total_lba48[4];
    };
};

static_assert(sizeof(union pata_identify) == 512);

struct pata_disk {
    struct disk disk;

    bool present;
    bool is_atapi;
    uint8_t port;
    uint8_t drive;
    char serial[21];
    char model[41];
};

struct pata_port {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t irq_line;

    bool active;
    struct disk_req_queue req_queue;
    struct disk_request req_buffer[REQ_SLOTS];

    struct pata_disk *active_disk;
    struct disk_request *active_req;
    enum pata_port_phase phase;
    uint16_t next_sector;
    uint16_t cmd_sectors;
    uint32_t done_sectors;
    uint64_t deadline_tick;
};

static struct pata_port g_ports[PATA_PORT_COUNT] = {
    [PATA_PORT_PRIMARY] = {
        .io_base = 0x1f0,
        .ctrl_base = 0x3f6,
        .irq_line = PIC_IRQ_HDD1,
    },
    [PATA_PORT_SECONDARY] = {
        .io_base = 0x170,
        .ctrl_base = 0x376,
        .irq_line = PIC_IRQ_HDD2,
    },
};

static struct pata_disk g_devices[PATA_DEVICE_COUNT];

[[nodiscard]] static uint8_t io_read8(const struct pata_port *port, uint8_t reg) {
    return in8(port->io_base + reg);
}

static void io_write8(const struct pata_port *port, uint8_t reg, uint8_t value) {
    out8(port->io_base + reg, value);
}

[[nodiscard]] static uint8_t ctrl_read_status(const struct pata_port *port) {
    return in8(port->ctrl_base);
}

static void ctrl_write(const struct pata_port *port, uint8_t value) {
    out8(port->ctrl_base, value);
}

static void io_delay_400ns(const struct pata_port *port) {
    for (int i = 0; i < 15; i++) {
        (void)ctrl_read_status(port);
    }
}

static void pio_read_words(const struct pata_port *port, void *buf, size_t words) {
    insw(port->io_base + ATA_REG_DATA, buf, words);
}

static void pio_write_words(const struct pata_port *port, const void *buf, size_t words) {
    outsw(port->io_base + ATA_REG_DATA, buf, words);
}

static void ata_select_drive(const struct pata_port *port, uint8_t drive, uint8_t head) {
    io_write8(
        port, ATA_REG_DRIVE_HEAD, (uint8_t)(ATA_DRIVE_LBA | (drive ? 0x10 : 0x00) | (head & 0x0f)));
    io_delay_400ns(port);
}

[[nodiscard]] static bool wait_for_flags(
    const struct pata_port *port, uint8_t flags, uint8_t *status_out) {
    for (uint32_t i = 0; i < ATA_POLL_SPIN; i++) {
        uint8_t status = io_read8(port, ATA_REG_STATUS);
        if (status & ATA_STATUS_BSY) {
            continue;
        }
        if (status & flags) {
            if (status_out) {
                *status_out = status;
            }
            return true;
        }
    }
    return false;
}

[[nodiscard]] static bool wait_not_busy(const struct pata_port *port) {
    for (uint32_t i = 0; i < ATA_POLL_SPIN; i++) {
        if (!(io_read8(port, ATA_REG_STATUS) & ATA_STATUS_BSY)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] static bool wait_drq_or_error(const struct pata_port *port, uint8_t *status_out) {
    return wait_for_flags(port, ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF, status_out);
}

[[nodiscard]] static bool wait_drdy_or_drq_err(const struct pata_port *port, uint8_t *status_out) {
    return wait_for_flags(
        port, ATA_STATUS_DRDY | ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF, status_out);
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

[[nodiscard]] static bool identify_device(uint8_t portidx, uint8_t drive, struct pata_disk *out) {
    struct pata_port *port = &g_ports[portidx];

    ata_select_drive(port, drive, 0);
    io_write8(port, ATA_REG_SECTOR_COUNT, 0);
    io_write8(port, ATA_REG_LBA0, 0);
    io_write8(port, ATA_REG_LBA1, 0);
    io_write8(port, ATA_REG_LBA2, 0);
    io_write8(port, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = io_read8(port, ATA_REG_STATUS);
    if (status == 0) {
        return false;
    }

    if (!wait_drq_or_error(port, &status)) {
        kwarn("pata: ch=%u drv=%u identify timeout", portidx, drive);
        return false;
    }

    if (status & ATA_STATUS_DF) {
        kwarn("pata: ch=%u drv=%u device fault during identify", portidx, drive);
        return false;
    }

    if (status & ATA_STATUS_ERR) {
        uint8_t sig1 = io_read8(port, ATA_REG_LBA1);
        uint8_t sig2 = io_read8(port, ATA_REG_LBA2);
        if (sig1 == ATAPI_SIG1 && sig2 == ATAPI_SIG2) {
            out->present = true;
            out->is_atapi = true;
            out->port = portidx;
            out->drive = drive;
            out->serial[0] = '\0';
            out->model[0] = '\0';
            return true;
        } else {
            kwarn("pata: ch=%u drv=%u device error during identify", portidx, drive);
            return false;
        }
    }

    union pata_identify id;
    pio_read_words(port, id.data, 256);

    out->present = true;
    out->is_atapi = false;
    out->port = portidx;
    out->drive = drive;
    out->disk.sectors = (uint32_t)id.total_lba28[0] | ((uint32_t)id.total_lba28[1] << 16);
    words_to_chars(out->serial, id.serial, 10);
    words_to_chars(out->model, id.model, 20);
    return true;
}

[[nodiscard]] static bool range_valid(
    const struct pata_disk *disk, fs_size_t lba, fs_size_t sectors) {
    if (sectors == 0 || disk->disk.sectors == 0) {
        return false;
    }

    fs_size_t end = (fs_size_t)lba + (fs_size_t)sectors;
    return end <= disk->disk.sectors;
}

[[nodiscard]] static bool issue_rw_command(
    const struct pata_port *port, uint8_t drive, bool is_write, uint32_t lba, uint16_t sectors) {
    uint8_t status;

    if (!wait_not_busy(port)) {
        return false;
    }

    ata_select_drive(port, drive, (uint8_t)(lba >> 24));

    if (!wait_drdy_or_drq_err(port, &status)) {
        return false;
    }
    if (status & (ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF)) {
        return false;
    }

    io_write8(port, ATA_REG_FEATURES, 0);
    io_write8(port, ATA_REG_SECTOR_COUNT, sectors == 256 ? 0 : (uint8_t)sectors);
    io_write8(port, ATA_REG_LBA0, (uint8_t)lba);
    io_write8(port, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    io_write8(port, ATA_REG_LBA2, (uint8_t)(lba >> 16));
    io_write8(port, ATA_REG_COMMAND, is_write ? ATA_CMD_WRITE_SECTORS : ATA_CMD_READ_SECTORS);
    return true;
}

static void complete_request(struct pata_port *port, bool success) {
    kassert(port->active_req);
    kassert(port->active_disk);

    disk_req_queue_pop_fetched(&port->req_queue, success ? OPAL_OK : OPAL_EIO);

    port->active_req = NULL;
    port->active_disk = NULL;
    port->phase = PATA_PHASE_IDLE;
    port->next_sector = 0;
    port->cmd_sectors = 0;
    port->done_sectors = 0;
    port->deadline_tick = 0;
}

static bool write_first_sector(struct pata_port *port, struct disk_request *req) {
    uint8_t status = 0;

    if (!wait_drq_or_error(port, &status)) {
        complete_request(port, false);
        return false;
    }
    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
        complete_request(port, false);
        return false;
    }

    const unsigned char *bytes = req->info.buffer;
    size_t sector_index = (size_t)port->done_sectors + (size_t)port->next_sector;
    const void *sector_ptr = bytes + sector_index * DISK_SECTOR_SIZE;
    pio_write_words(port, sector_ptr, DISK_SECTOR_SIZE / 2);

    port->next_sector = 1;
    port->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);
    port->phase =
        port->next_sector >= port->cmd_sectors ? PATA_PHASE_WRITE_DONE : PATA_PHASE_WRITE_DATA;
    return true;
}

[[nodiscard]] static bool start_command(
    struct pata_port *port, struct disk_request *req, const struct pata_disk *disk) {
    if (req->info.lba > UINT32_MAX || req->info.sectors > UINT32_MAX) {
        return false;
    }

    uint32_t req_lba = (uint32_t)req->info.lba;
    uint32_t req_sectors = (uint32_t)req->info.sectors;
    if (!range_valid(disk, req_lba, req_sectors)) {
        return false;
    }

    kassert(port->done_sectors < req_sectors);

    uint32_t remaining = req_sectors - port->done_sectors;
    uint16_t cmd_sectors = remaining > 256 ? 256 : (uint16_t)remaining;
    uint32_t cmd_lba = req_lba + port->done_sectors;

    bool is_write = req->info.type == DISK_REQUEST_WRITE;
    if (!is_write && req->info.type != DISK_REQUEST_READ) {
        return false;
    }

    if (!issue_rw_command(port, disk->drive, is_write, cmd_lba, cmd_sectors)) {
        return false;
    }

    port->cmd_sectors = cmd_sectors;
    port->next_sector = 0;
    port->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);

    if (req->info.type == DISK_REQUEST_READ) {
        port->phase = PATA_PHASE_READ_DATA;
        return true;
    }

    return write_first_sector(port, req);
}

static void start_next_request(struct pata_port *port) {
    while (port->active_req == NULL) {
        struct disk_request *req = disk_req_queue_fetch(&port->req_queue);
        if (!req) {
            return;
        }

        struct pata_disk *disk = container_of(req->disk, struct pata_disk, disk);
        port->active_disk = disk;
        port->active_req = req;
        port->next_sector = 0;
        port->cmd_sectors = 0;
        port->done_sectors = 0;

        if (!start_command(port, req, disk)) {
            complete_request(port, false);
            continue;
        }
        return;
    }
}

static void complete_and_start_next(struct pata_port *port, bool success) {
    complete_request(port, success);
    start_next_request(port);
}

static void handle_irq(struct pata_port *port, uint8_t status) {
    if (port->active_req == NULL) {
        return;
    }

    struct disk_request *req = port->active_req;
    struct pata_disk *disk = port->active_disk;

    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
        complete_and_start_next(port, false);
        return;
    }

    if (status & ATA_STATUS_BSY) {
        return;
    }

    unsigned char *bytes = (unsigned char *)req->info.buffer;
    size_t sector_index = (size_t)port->done_sectors + (size_t)port->next_sector;
    void *sector_ptr = bytes + sector_index * DISK_SECTOR_SIZE;
    fs_size_t req_sectors = req->info.sectors;

    switch (port->phase) {
        case PATA_PHASE_READ_DATA:
            if (!(status & ATA_STATUS_DRQ)) {
                complete_and_start_next(port, false);
                return;
            }

            pio_read_words(port, sector_ptr, DISK_SECTOR_SIZE / 2);
            port->next_sector++;
            port->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);

            if (port->next_sector >= port->cmd_sectors) {
                port->done_sectors += port->cmd_sectors;
                if (port->done_sectors >= req_sectors) {
                    complete_and_start_next(port, true);
                } else if (!start_command(port, req, disk)) {
                    complete_and_start_next(port, false);
                }
            }
            return;

        case PATA_PHASE_WRITE_DATA:
            if (!(status & ATA_STATUS_DRQ)) {
                complete_and_start_next(port, false);
                return;
            }

            pio_write_words(port, sector_ptr, DISK_SECTOR_SIZE / 2);
            port->next_sector++;
            port->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);

            if (port->next_sector >= port->cmd_sectors) {
                port->phase = PATA_PHASE_WRITE_DONE;
            }
            return;

        case PATA_PHASE_WRITE_DONE:
            port->done_sectors += port->cmd_sectors;
            if (port->done_sectors >= req_sectors) {
                io_write8(port, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
                port->phase = PATA_PHASE_COMPLETE;
                port->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);
                return;
            }

            if (!start_command(port, req, disk)) {
                complete_and_start_next(port, false);
            }
            return;

        case PATA_PHASE_COMPLETE:
            complete_and_start_next(port, true);
            return;

        case PATA_PHASE_IDLE:
            return;
    }
}

static void isr_pata(uint8_t portidx) {
    struct pata_port *port = &g_ports[portidx];
    uint8_t status = io_read8(port, ATA_REG_STATUS);

    handle_irq(port, status);

    irq_send_eoi(port->irq_line);
}

static void isr_pata_primary(void) {
    isr_pata(PATA_PORT_PRIMARY);
}

static void isr_pata_secondary(void) {
    isr_pata(PATA_PORT_SECONDARY);
}

static void on_request(struct disk *disk, struct disk_request *) {
    struct pata_disk *pdisk = container_of(disk, struct pata_disk, disk);
    struct pata_port *port = &g_ports[pdisk->port];
    irqlock_t irqlock = irqlock_acquire();
    if (port->active_req == NULL) {
        start_next_request(port);
    }
    irqlock_release(&irqlock);
}

static const struct disk_device_ops g_device_ops = {
    .on_request = on_request,
};

void pata_init(void) {
    memset(g_devices, 0, sizeof(g_devices));

    irq_register(PIC_IRQ_HDD1, isr_pata_primary);
    irq_register(PIC_IRQ_HDD2, isr_pata_secondary);
    irq_enable(PIC_IRQ_HDD1);
    irq_enable(PIC_IRQ_HDD2);

    const char *dev_names[] = { "hda", "hdb", "hdc", "hdd" };

    for (uint8_t portidx = 0; portidx < PATA_PORT_COUNT; portidx++) {
        struct pata_port *port = &g_ports[portidx];
        port->active = false;
        disk_req_queue_init(&port->req_queue, port->req_buffer, REQ_SLOTS);
        port->active_disk = NULL;
        port->active_req = NULL;
        port->phase = PATA_PHASE_IDLE;
        port->next_sector = 0;
        port->cmd_sectors = 0;
        port->done_sectors = 0;
        port->deadline_tick = 0;

        // check floating bus
        if (io_read8(port, ATA_REG_STATUS) == 0xff) {
            continue;
        }

        ctrl_write(port, ATA_CTRL_NIEN);
        io_delay_400ns(port);

        for (uint8_t drive = 0; drive < 2; drive++) {
            uint8_t devidx = portidx * 2 + drive;
            struct pata_disk *disk = &g_devices[devidx];
            if (!identify_device(portidx, drive, disk)) {
                continue;
            }

            ctrl_write(port, 0);
            io_delay_400ns(port);

            port->active |= disk->present;

            if (disk->is_atapi) {
                kinfo("pata: ch=%u drv=%u atapi detected", portidx, drive);
                continue;
            }

            disk_init(&disk->disk, &g_device_ops, dev_names[devidx], &port->req_queue,
                disk->disk.sectors);

            kinfo("pata: ch=%u drv=%u model='%s' serial='%s' sectors=%zu", portidx, drive,
                disk->model, disk->serial, disk->disk.sectors);

            if (!disk_register(&disk->disk)) {
                kwarn("pata: failed to register disk ch=%u drv=%u", portidx, drive);
            }
        }
    }
}

void pata_on_timer(uint64_t now_tick) {
    for (uint8_t portidx = 0; portidx < PATA_PORT_COUNT; portidx++) {
        struct pata_port *port = &g_ports[portidx];
        if (port->active_req == NULL) {
            continue;
        }

        if (now_tick < port->deadline_tick) {
            continue;
        }

        complete_and_start_next(port, false);
    }
}
