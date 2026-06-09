#include <stddef.h>

#include <kc/kassert.h>

#include <opal/irq.h>
#include <opal/klog.h>
#include <opal/platform/asm.h>
#include <opal/platform/drivers/ps2.h>
#include <opal/platform/drivers/pic.h>

#define PS2_PORT_DATA           0x60
#define PS2_PORT_STATUS         0x64
#define PS2_PORT_CMD            0x64

#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_AUX_DATA     0x20

#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_PORT1   0xad
#define PS2_CMD_ENABLE_PORT1    0xae
#define PS2_CMD_DISABLE_PORT2   0xa7
#define PS2_CMD_ENABLE_PORT2    0xa8
#define PS2_CMD_TEST_PORT1      0xab
#define PS2_CMD_TEST_PORT2      0xa9
#define PS2_CMD_SELF_TEST       0xaa
#define PS2_CMD_WRITE_PORT2     0xd4

#define PS2_DEV_CMD_RESET       0xff
#define PS2_DEV_CMD_ENABLE_SCAN 0xf4
#define PS2_DEV_ACK             0xfa
#define PS2_DEV_SELF_TEST_OK    0xaa
#define PS2_CTRL_SELF_TEST_OK   0x55

#define PS2_CFG_IRQ1            0x01
#define PS2_CFG_IRQ12           0x02
#define PS2_CFG_CLOCK_1         0x10
#define PS2_CFG_CLOCK_2         0x20
#define PS2_CFG_TRANSLATE       0x40

#define PS2_WAIT_SPIN   100000
#define PS2_INTR_SPIN   32

static bool g_enabled;
static bool g_kbd_enabled;
static bool g_mouse_enabled;

static bool is_input_clear(void) {
    return (in8(PS2_PORT_STATUS) & PS2_STATUS_INPUT_FULL) == 0;
}

static bool is_output_full(void) {
    return (in8(PS2_PORT_STATUS) & PS2_STATUS_OUTPUT_FULL) != 0;
}

static bool wait_input_clear(void) {
    for (uint32_t i = 0; i < PS2_WAIT_SPIN; i++) {
        if (is_input_clear()) {
            return true;
        }
    }
    return false;
}

static bool wait_output_full(void) {
    for (uint32_t i = 0; i < PS2_WAIT_SPIN; i++) {
        if (is_output_full()) {
            return true;
        }
    }
    return false;
}

static bool write_cmd(uint8_t cmd) {
    if (!wait_input_clear()) {
        return false;
    }
    out8(PS2_PORT_CMD, cmd);
    return true;
}

static bool write_data(uint8_t data) {
    if (!wait_input_clear()) {
        return false;
    }
    out8(PS2_PORT_DATA, data);
    return true;
}

static bool read_data(uint8_t *out) {
    kassert(out);
    if (!wait_output_full()) {
        return false;
    }
    *out = in8(PS2_PORT_DATA);
    return true;
}

static void flush_output(void) {
    for (uint32_t i = 0; i < PS2_WAIT_SPIN; i++) {
        if ((in8(PS2_PORT_STATUS) & PS2_STATUS_OUTPUT_FULL) == 0) {
            break;
        }
        (void)in8(PS2_PORT_DATA);
    }
}

static bool read_config(uint8_t *config) {
    if (!write_cmd(PS2_CMD_READ_CONFIG)) {
        return false;
    }
    return read_data(config);
}

static bool write_config(uint8_t config) {
    if (!write_cmd(PS2_CMD_WRITE_CONFIG)) {
        return false;
    }
    return write_data(config);
}

static bool send_dev1(uint8_t cmd, uint8_t *resp) {
    uint8_t ack = 0;

    if (!write_data(cmd) || !read_data(&ack) || ack != PS2_DEV_ACK) {
        return false;
    }

    if (resp && !read_data(resp)) {
        return false;
    }

    return true;
}

static bool send_dev2(uint8_t cmd, uint8_t *resp) {
    uint8_t ack = 0;

    if (!write_cmd(PS2_CMD_WRITE_PORT2) || !write_data(cmd) || !read_data(&ack)
        || ack != PS2_DEV_ACK) {
        return false;
    }

    if (resp && !read_data(resp)) {
        return false;
    }

    return true;
}

static void push_irqmsg_kbd(uint8_t data) {
    irqmsg_push((struct irqmsg){ .type = IRQMSG_PS2_KBD, .data = data });
}

static void push_irqmsg_mouse(uint8_t data) {
    irqmsg_push((struct irqmsg){ .type = IRQMSG_PS2_MOUSE, .data = data });
}

static void isrmsg_kbd(struct irqmsg msg) {
    ps2_keyboard_feed_raw(msg.data);
}

static void isrmsg_mouse(struct irqmsg msg) {
    ps2_mouse_feed_raw(msg.data);
}

static void ps2_drain_output(void) {
    for (int i = 0; i < PS2_INTR_SPIN; i++) {
        uint8_t status = in8(PS2_PORT_STATUS);
        if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
            return;
        }

        uint8_t data = in8(PS2_PORT_DATA);
        if ((status & PS2_STATUS_AUX_DATA) != 0) {
            if (g_mouse_enabled) {
                push_irqmsg_mouse(data);
            }
        } else {
            if (g_kbd_enabled) {
                push_irqmsg_kbd(data);
            }
        }
    }
}

static void isr_kbd(void) {
    ps2_drain_output();
    irq_send_eoi(PIC_IRQ_KEYBOARD);
}

static void isr_mouse(void) {
    ps2_drain_output();
    irq_send_eoi(PIC_IRQ_MOUSE);
}

struct device_type {
    uint8_t bytes[2];
    uint8_t n;
};

static struct device_type read_device_type(void) {
    struct device_type dt = {};

    for (int i = 0; i < PS2_WAIT_SPIN && dt.n < 2; i++) {
        if (is_output_full()) {
            dt.bytes[dt.n++] = in8(PS2_PORT_DATA);
            i = 0;
        }
    }

    return dt;
}

static bool kbd_init(void) {
    flush_output();

    uint8_t self0 = 0;
    if (!send_dev1(PS2_DEV_CMD_RESET, &self0) || self0 != PS2_DEV_SELF_TEST_OK) {
        kwarn("ps2: keyboard reset failed (%#x)", self0);
        return false;
    }

    struct device_type dt = read_device_type();
    if (dt.n != 0) {
        kwarn("ps2: invalid keyboard device id");
        return false;
    }

    if (!send_dev1(PS2_DEV_CMD_ENABLE_SCAN, NULL)) {
        kwarn("ps2: keyboard enable scanning failed");
        return false;
    }

    irq_register(PIC_IRQ_KEYBOARD, isr_kbd);
    irqmsg_register(IRQMSG_PS2_KBD, isrmsg_kbd);
    irq_enable(PIC_IRQ_KEYBOARD);
    return true;
}

static bool mouse_init(void) {
    flush_output();

    uint8_t self0 = 0;
    if (!send_dev2(PS2_DEV_CMD_RESET, &self0) || self0 != PS2_DEV_SELF_TEST_OK) {
        kwarn("ps2: mouse reset failed (%#x)", self0);
        return false;
    }

    struct device_type dt = read_device_type();
    if (!(dt.n == 1 && dt.bytes[0] == 0x00)) {
        kwarn("ps2: invalid mouse device id");
        return false;
    }

    if (!send_dev2(PS2_DEV_CMD_ENABLE_SCAN, NULL)) {
        kwarn("ps2: mouse enable data scanning failed");
        return false;
    }

    irq_register(PIC_IRQ_MOUSE, isr_mouse);
    irqmsg_register(IRQMSG_PS2_MOUSE, isrmsg_mouse);
    irq_enable(PIC_IRQ_MOUSE);
    return true;
}

void ps2_init(void) {
    g_enabled = false;
    g_kbd_enabled = false;
    g_mouse_enabled = false;

    // disable & flush output buffer
    (void)write_cmd(PS2_CMD_DISABLE_PORT1);
    (void)write_cmd(PS2_CMD_DISABLE_PORT2);
    flush_output();

    // turn off irqs, disable both clocks
    uint8_t config = 0;
    if (!read_config(&config)) {
        kwarn("ps2: failed to read controller config");
        return;
    }

    config &= ~(PS2_CFG_IRQ1 | PS2_CFG_IRQ12);
    config |= PS2_CFG_CLOCK_1 | PS2_CFG_CLOCK_2 | PS2_CFG_TRANSLATE;
    if (!write_config(config)) {
        kwarn("ps2: failed to write controller config");
        return;
    }

    // test controller
    uint8_t ctrl_test = 0;
    if (!write_cmd(PS2_CMD_SELF_TEST) || !read_data(&ctrl_test)
        || ctrl_test != PS2_CTRL_SELF_TEST_OK) {
        kwarn("ps2: controller self-test failed (%#x)", ctrl_test);
        return;
    }

    // rewrite config
    if (!write_config(config)) {
        kwarn("ps2: failed to write controller config");
        return;
    }

    // detect channel 2
    if (!write_cmd(PS2_CMD_ENABLE_PORT2) || !read_config(&config)) {
        kwarn("ps2: channel 2 test failed");
    } else {
        g_mouse_enabled = !(config & PS2_CFG_CLOCK_2);
    }

    // test ports
    uint8_t test = 0xff;
    if (!write_cmd(PS2_CMD_TEST_PORT1) || !read_data(&test) || test != 0x00) {
        kwarn("ps2: keyboard port test failed (%#x)", test);
    } else {
        (void)write_cmd(PS2_CMD_ENABLE_PORT1);
        g_kbd_enabled = true;
    }

    if (g_mouse_enabled) {
        test = 0xff;
        if (!write_cmd(PS2_CMD_TEST_PORT2) || !read_data(&test) || test != 0x00) {
            g_mouse_enabled = false;
            (void)write_cmd(PS2_CMD_DISABLE_PORT2);
            kwarn("ps2: mouse port test failed (%#x)", test);
        }
    }

    if (!g_kbd_enabled && !g_mouse_enabled) {
        kwarn("ps2: no available devices");
        return;
    }

    if (!read_config(&config)) {
        kwarn("ps2: failed to re-read controller config");
        return;
    }

    // enable clocks
    config &= ~(PS2_CFG_CLOCK_1 | PS2_CFG_CLOCK_2 | PS2_CFG_IRQ1 | PS2_CFG_IRQ12);
    config |= PS2_CFG_TRANSLATE;
    if (!write_config(config)) {
        kwarn("ps2: failed to enable clock config");
        return;
    }

    // initialize devices
    if (g_kbd_enabled) {
        if (!kbd_init()) {
            g_kbd_enabled = false;
        }
    }

    if (g_mouse_enabled) {
        if (!mouse_init()) {
            g_mouse_enabled = false;
        }
    }

    g_enabled = g_kbd_enabled || g_mouse_enabled;
    if (g_enabled) {
        kinfo("ps2: enabled (kbd=%u mouse=%u)", g_kbd_enabled, g_mouse_enabled);
    } else {
        kwarn("ps2: initialized but all devices disabled");
    }

    // turn on irqs
    if (g_kbd_enabled) {
        config |= PS2_CFG_IRQ1;
    }
    if (g_mouse_enabled) {
        config |= PS2_CFG_IRQ12;
    }
    if (!write_config(config)) {
        kwarn("ps2: failed to enable irq config");
        // ignore write_config error at this point
    }
}

bool ps2_is_enabled(void) {
    return g_enabled;
}
