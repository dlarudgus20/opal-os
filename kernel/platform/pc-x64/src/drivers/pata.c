#include <kc/assert.h>
#include <kc/string.h>

#include <collections/ringbuffer.h>

#include <opal/klog.h>
#include <opal/locks/irqlock.h>
#include <opal/task/event.h>
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

#define ATA_TIMEOUT_MS          3000
#define ATA_POLL_SPIN           100000

#define REQ_SLOTS 64

enum {
    PATA_CHANNEL_PRIMARY = 0,
    PATA_CHANNEL_SECONDARY = 1,
    PATA_CHANNEL_COUNT = 2,
};

enum pata_channel_phase : uint8_t {
    PATA_PHASE_IDLE,
    PATA_PHASE_READ_DATA,
    PATA_PHASE_WRITE_DATA,
    PATA_PHASE_WRITE_FLUSH,
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

struct pata_channel {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t irq_line;

    bool active;

    uint8_t req_buffer[REQ_SLOTS];
    struct ringbuffer req_queue;

    uint8_t active_req;
    enum pata_channel_phase phase;
    uint8_t next_sector;
    uint64_t deadline_tick;
};

enum pata_request_state : uint8_t {
    PATA_REQUEST_FREE,
    PATA_REQUEST_QUEUED,
    PATA_REQUEST_INFLIGHT,
    PATA_REQUEST_DONE,
};

struct pata_request {
    uint16_t token_dummy;

    enum pata_device_index device;
    uint8_t sectors;
    uint32_t lba;
    void *buf;
    bool is_write;

    enum pata_request_state state;
    bool success;
    struct event done_event;
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
static struct pata_request g_requests[REQ_SLOTS];

[[nodiscard]] static uint8_t io_read8(const struct pata_channel *ch, uint8_t reg) {
    return in8(ch->io_base + reg);
}

static void io_write8(const struct pata_channel *ch, uint8_t reg, uint8_t value) {
    out8(ch->io_base + reg, value);
}

[[nodiscard]] static uint8_t ctrl_read_status(const struct pata_channel *ch) {
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

static void pio_read_words(const struct pata_channel *ch, void *buf, size_t words) {
    insw(ch->io_base + ATA_REG_DATA, buf, words);
}

static void pio_write_words(const struct pata_channel *ch, const void *buf, size_t words) {
    outsw(ch->io_base + ATA_REG_DATA, buf, words);
}

static void ata_select_drive(const struct pata_channel *ch, uint8_t drive, uint8_t head) {
    io_write8(ch, ATA_REG_DRIVE_HEAD, (uint8_t)(ATA_DRIVE_LBA | (drive ? 0x10 : 0x00) | (head & 0x0f)));
    io_delay_400ns(ch);
}

[[nodiscard]] static bool wait_for_flags(const struct pata_channel *ch, uint8_t flags, uint8_t *status_out) {
    for (uint32_t i = 0; i < ATA_POLL_SPIN; i++) {
        uint8_t status = io_read8(ch, ATA_REG_STATUS);
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

[[nodiscard]] static bool wait_not_busy(const struct pata_channel *ch) {
    for (uint32_t i = 0; i < ATA_POLL_SPIN; i++) {
        if (!(io_read8(ch, ATA_REG_STATUS) & ATA_STATUS_BSY)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] static bool wait_drq_or_error(const struct pata_channel *ch, uint8_t *status_out) {
    return wait_for_flags(ch, ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF, status_out);
}

[[nodiscard]] static bool wait_drdy_or_drq_err(const struct pata_channel *ch, uint8_t *status_out) {
    return wait_for_flags(ch, ATA_STATUS_DRDY | ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF, status_out);
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

[[nodiscard]] static bool identify_device(uint8_t channel_idx, uint8_t drive, struct pata_device *out) {
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

    if (!wait_drq_or_error(ch, &status)) {
        kwarn("pata: ch=%u drv=%u identify timeout", channel_idx, drive);
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

[[nodiscard]] static bool range_valid(const struct pata_device *dev, uint32_t lba, uint8_t sector_count) {
    if (sector_count == 0 || dev->lba28_sectors == 0) {
        return false;
    }

    uint64_t end = (uint64_t)lba + (uint64_t)sector_count;
    return end <= dev->lba28_sectors;
}

[[nodiscard]] static bool issue_rw_command(const struct pata_channel *ch, const struct pata_request *req, const struct pata_device *dev) {
    uint8_t status;

    if (!wait_not_busy(ch)) {
        return false;
    }

    ata_select_drive(ch, dev->drive, (uint8_t)(req->lba >> 24));

    if (!wait_drdy_or_drq_err(ch, &status)) {
        return false;
    }
    if (status & (ATA_STATUS_DRQ | ATA_STATUS_ERR | ATA_STATUS_DF)) {
        return false;
    }

    io_write8(ch, ATA_REG_FEATURES, 0);
    io_write8(ch, ATA_REG_SECTOR_COUNT, req->sectors);
    io_write8(ch, ATA_REG_LBA0, (uint8_t)req->lba);
    io_write8(ch, ATA_REG_LBA1, (uint8_t)(req->lba >> 8));
    io_write8(ch, ATA_REG_LBA2, (uint8_t)(req->lba >> 16));
    io_write8(ch, ATA_REG_COMMAND, req->is_write ? ATA_CMD_WRITE_SECTORS : ATA_CMD_READ_SECTORS);
    return true;
}

[[nodiscard]] static pata_token_t encode_token(uint16_t idx, uint16_t dummy) {
    return ((uint32_t)dummy << 16) | idx;
}

[[nodiscard]] static uint16_t token_index(pata_token_t token) {
    return (uint16_t)(token & 0xffff);
}

[[nodiscard]] static uint16_t token_dummy(pata_token_t token) {
    return (uint16_t)(token >> 16);
}

static void complete_active_request(struct pata_channel *ch, bool success) {
    assert(ch->active_req < REQ_SLOTS);

    struct pata_request *req = &g_requests[ch->active_req];
    req->success = success;
    req->state = PATA_REQUEST_DONE;
    event_signal(&req->done_event);

    ch->active_req = UINT8_MAX;
    ch->phase = PATA_PHASE_IDLE;
    ch->next_sector = 0;
    ch->deadline_tick = 0;
}

static bool write_first_sector(struct pata_channel *ch, struct pata_request *req) {
    uint8_t status = 0;

    if (!wait_drq_or_error(ch, &status)) {
        complete_active_request(ch, false);
        return false;
    }
    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
        complete_active_request(ch, false);
        return false;
    }

    pio_write_words(ch, req->buf, PATA_SECTOR_SIZE / 2);

    ch->next_sector = 1;
    ch->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);
    ch->phase = req->sectors <= 1 ? PATA_PHASE_WRITE_FLUSH : PATA_PHASE_WRITE_DATA;
    return true;
}

static void start_next_request(struct pata_channel *ch) {
    while (ch->active_req == UINT8_MAX) {
        if (ringbuffer_is_empty(&ch->req_queue)) {
            return;
        }

        uint8_t req_idx = ringbuffer_pop(&ch->req_queue, uint8_t);
        struct pata_request *req = &g_requests[req_idx];
        struct pata_device *dev = &g_devices[req->device];

        req->state = PATA_REQUEST_INFLIGHT;
        ch->next_sector = 0;

        if (!issue_rw_command(ch, req, dev)) {
            req->success = false;
            req->state = PATA_REQUEST_DONE;
            event_signal(&req->done_event);
            continue;
        }

        ch->active_req = req_idx;
        ch->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);

        if (!req->is_write) {
            ch->phase = PATA_PHASE_READ_DATA;
        } else {
            if (!write_first_sector(ch, req)) {
                continue;
            }
        }
        return;
    }
}

static void handle_channel_irq(struct pata_channel *ch, uint8_t status) {
    if (ch->active_req == UINT8_MAX) {
        return;
    }

    struct pata_request *req = &g_requests[ch->active_req];

    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
        complete_active_request(ch, false);
        start_next_request(ch);
        return;
    }

    if (status & ATA_STATUS_BSY) {
        return;
    }

    unsigned char *bytes = (unsigned char *)req->buf;
    void *sector_ptr = bytes + (size_t)ch->next_sector * PATA_SECTOR_SIZE;

    switch (ch->phase) {
    case PATA_PHASE_READ_DATA:
        if (!(status & ATA_STATUS_DRQ)) {
            complete_active_request(ch, false);
            start_next_request(ch);
            return;
        }

        pio_read_words(ch, sector_ptr, PATA_SECTOR_SIZE / 2);
        ch->next_sector++;
        ch->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);

        if (ch->next_sector >= req->sectors) {
            complete_active_request(ch, true);
            start_next_request(ch);
        }
        return;

    case PATA_PHASE_WRITE_DATA:
        if (!(status & ATA_STATUS_DRQ)) {
            complete_active_request(ch, false);
            start_next_request(ch);
            return;
        }

        pio_write_words(ch, sector_ptr, PATA_SECTOR_SIZE / 2);
        ch->next_sector++;
        ch->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);

        if (ch->next_sector >= req->sectors) {
            ch->phase = PATA_PHASE_WRITE_FLUSH;
        }
        return;

    case PATA_PHASE_WRITE_FLUSH:
        io_write8(ch, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
        ch->phase = PATA_PHASE_COMPLETE;
        ch->deadline_tick = timer_get_tick() + ticks_from_ms(ATA_TIMEOUT_MS);
        return;

    case PATA_PHASE_COMPLETE:
        complete_active_request(ch, true);
        start_next_request(ch);
        return;

    case PATA_PHASE_IDLE:
    default:
        return;
    }
}

static void isr_pata_channel(uint8_t channel_idx) {
    struct pata_channel *ch = &g_channels[channel_idx];
    uint8_t status = io_read8(ch, ATA_REG_STATUS);

    handle_channel_irq(ch, status);

    irq_send_eoi(ch->irq_line);
}

static void isr_pata_primary(void) {
    isr_pata_channel(PATA_CHANNEL_PRIMARY);
}

static void isr_pata_secondary(void) {
    isr_pata_channel(PATA_CHANNEL_SECONDARY);
}

void pata_init(void) {
    memset(g_devices, 0, sizeof(g_devices));
    memset(g_requests, 0, sizeof(g_requests));
    for (uint8_t i = 0; i < REQ_SLOTS; i++) {
        event_init(&g_requests[i].done_event, true);
    }

    irq_register(PIC_IRQ_HDD1, isr_pata_primary);
    irq_register(PIC_IRQ_HDD2, isr_pata_secondary);
    irq_enable(PIC_IRQ_HDD1);
    irq_enable(PIC_IRQ_HDD2);

    for (uint8_t channel = 0; channel < PATA_CHANNEL_COUNT; channel++) {
        struct pata_channel *ch = &g_channels[channel];
        ch->active = false;
        ch->active_req = UINT8_MAX;
        ch->phase = PATA_PHASE_IDLE;
        ch->next_sector = 0;
        ch->deadline_tick = 0;
        ringbuffer_init(&ch->req_queue, ch->req_buffer, REQ_SLOTS);

        // check floating bus
        if (io_read8(ch, ATA_REG_STATUS) == 0xff) {
            continue;
        }

        ctrl_write(ch, ATA_CTRL_NIEN);
        io_delay_400ns(ch);

        for (uint8_t drive = 0; drive < 2; drive++) {
            struct pata_device *dev = &g_devices[channel * 2 + drive];
            if (!identify_device(channel, drive, dev)) {
                continue;
            }

            ctrl_write(ch, 0);
            io_delay_400ns(ch);

            ch->active |= dev->present;

            if (dev->is_atapi) {
                kinfo("pata: ch=%u drv=%u atapi detected", channel, drive);
            } else {
                kinfo("pata: ch=%u drv=%u model='%s' serial='%s' sectors=%u",
                    channel, drive, dev->model, dev->serial, dev->lba28_sectors);
            }
        }
    }
}

void pata_on_timer(uint64_t now_tick) {
    for (uint8_t channel = 0; channel < PATA_CHANNEL_COUNT; channel++) {
        struct pata_channel *ch = &g_channels[channel];
        if (ch->active_req == UINT8_MAX) {
            continue;
        }

        if (now_tick < ch->deadline_tick) {
            continue;
        }

        complete_active_request(ch, false);
        start_next_request(ch);
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

static uint8_t alloc_request(void) {
    for (uint8_t i = 0; i < REQ_SLOTS; i++) {
        if (g_requests[i].state == PATA_REQUEST_FREE) {
            return i;
        }
    }
    return UINT8_MAX;
}

[[nodiscard]] static pata_token_t submit_request(enum pata_device_index index, uint32_t lba, void *buf, uint8_t sectors, bool is_write) {
    assert(buf);
    assert(index < PATA_DEVICE_COUNT);

    struct pata_device *dev = &g_devices[index];
    if (!dev->present || dev->is_atapi) {
        return PATA_INVALID_TOKEN;
    }

    if (!range_valid(dev, lba, sectors)) {
        return PATA_INVALID_TOKEN;
    }

    struct pata_channel *ch = &g_channels[dev->channel];
    if (!ch->active) {
        return PATA_INVALID_TOKEN;
    }

    irqlock_t lock = irqlock_acquire();

    if (ringbuffer_is_full(&ch->req_queue)) {
        irqlock_release(&lock);
        return PATA_INVALID_TOKEN;
    }

    uint8_t req_idx = alloc_request();
    if (req_idx == UINT8_MAX) {
        irqlock_release(&lock);
        return PATA_INVALID_TOKEN;
    }

    ringbuffer_push(&ch->req_queue, uint8_t, req_idx);

    struct pata_request *req = &g_requests[req_idx];
    req->token_dummy++;
    req->device = index;
    req->sectors = sectors;
    req->lba = lba;
    req->buf = buf;
    req->is_write = is_write;
    req->state = PATA_REQUEST_QUEUED;
    req->success = false;
    event_reset(&req->done_event);

    pata_token_t token = encode_token(req_idx, req->token_dummy);

    if (ch->active_req == UINT8_MAX) {
        start_next_request(ch);
    }

    irqlock_release(&lock);
    return token;
}

pata_token_t pata_read_sectors(enum pata_device_index index, uint32_t lba, void *buf, uint8_t sectors) {
    return submit_request(index, lba, buf, sectors, false);
}

pata_token_t pata_write_sectors(enum pata_device_index index, uint32_t lba, const void *buf, uint8_t sectors) {
    return submit_request(index, lba, (void *)buf, sectors, true);
}

enum pata_wait_result pata_wait(pata_token_t token, uint64_t timeout) {
    uint16_t idx = token_index(token);
    uint16_t dummy = token_dummy(token);

    if (idx >= REQ_SLOTS) {
        return PATA_WAIT_INVALID;
    }

    while (1) {
        irqlock_t lock = irqlock_acquire();
        struct pata_request *req = &g_requests[idx];
        if (req->token_dummy != dummy || req->state == PATA_REQUEST_FREE) {
            irqlock_release(&lock);
            return PATA_WAIT_INVALID;
        }
        if (req->state == PATA_REQUEST_DONE) {
            bool ok = req->success;
            req->state = PATA_REQUEST_FREE;
            irqlock_release(&lock);
            return ok ? PATA_WAIT_IO_OK : PATA_WAIT_IO_FAIL;
        }
        irqlock_release(&lock);

        if (!event_wait(&req->done_event, timeout)) {
            return PATA_WAIT_TIMEOUT;
        }
    }
}
