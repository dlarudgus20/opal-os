#include <kc/kassert.h>

#include <opal/platform/asm.h>
#include <opal/platform/drivers/pic.h>
#include <opal/platform/drivers/pit.h>

#define PIT_INPUT_HZ            1193182u

#define PIT_CHANNEL0_DATA       0x40
#define PIT_MODE_COMMAND        0x43

#define PIT_CMD_CHANNEL0        0x00
#define PIT_CMD_ACCESS_LOHI     0x30
#define PIT_CMD_MODE_RATEGEN    0x04
#define PIT_CMD_BINARY          0x00

static void pit_set_rate(uint32_t hz) {
    kassert(hz > 0);

    uint32_t divisor = (PIT_INPUT_HZ + (hz / 2u)) / hz;
    if (divisor == 0) {
        divisor = 1;
    } else if (divisor > 0xffffu) {
        divisor = 0xffffu;
    }

    uint8_t command =
        PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LOHI | PIT_CMD_MODE_RATEGEN | PIT_CMD_BINARY;
    out8(PIT_MODE_COMMAND, command);
    out8(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xffu));
    out8(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xffu));
}

void pit_init(uint32_t hz, irq_handler_t isr) {
    pit_set_rate(hz);
    irq_register(PIC_IRQ_TIMER, isr);
    irq_enable(PIC_IRQ_TIMER);
}
