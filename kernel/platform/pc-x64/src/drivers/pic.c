#include <opal/irq.h>
#include <opal/platform/asm.h>
#include <opal/platform/drivers/pic.h>

#define PIC_MASTER_CMD      0x20
#define PIC_MASTER_DATA     0x21
#define PIC_SLAVE_CMD       0xa0
#define PIC_SLAVE_DATA      0xa1

#define PIC_EOI             0x20

#define ICW1_ICW4           0x01
#define ICW1_INIT           0x10
#define ICW4_8086_MODE      0x01

#define PIC_MASTER_OFFSET   PIC_INT_VECTOR
#define PIC_SLAVE_OFFSET    (PIC_INT_VECTOR + 8)

static uint16_t g_mask;
static bool g_intr_on;

static void io_wait(void) {
    out8(0x80, 0);
}

void pic_init(void) {
    out8(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out8(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    out8(PIC_MASTER_DATA, PIC_MASTER_OFFSET);
    io_wait();
    out8(PIC_SLAVE_DATA, PIC_SLAVE_OFFSET);
    io_wait();

    out8(PIC_MASTER_DATA, 1 << 2);
    io_wait();
    out8(PIC_SLAVE_DATA, 2);
    io_wait();

    out8(PIC_MASTER_DATA, ICW4_8086_MODE);
    io_wait();
    out8(PIC_SLAVE_DATA, ICW4_8086_MODE);
    io_wait();

    g_mask = 0xffff;
    g_intr_on = false;
    out8(PIC_MASTER_DATA, (uint8_t)g_mask);
    out8(PIC_SLAVE_DATA, (uint8_t)(g_mask >> 8));
}

void pic_disable_irq(uint8_t irq) {
    g_mask |= 1u << irq;

    if (g_intr_on) {
        if (irq < 8) {
            out8(PIC_MASTER_DATA, (uint8_t)g_mask);
        } else {
            out8(PIC_SLAVE_DATA, (uint8_t)(g_mask >> 8));
        }
    }
}

void pic_enable_irq(uint8_t irq) {
    g_mask &= ~(1u << irq);

    if (g_intr_on) {
        if (irq < 8) {
            out8(PIC_MASTER_DATA, (uint8_t)g_mask);
        } else {
            out8(PIC_SLAVE_DATA, (uint8_t)(g_mask >> 8));
        }
    }
}

void pic_enable_intr(void) {
    g_intr_on = true;
    out8(PIC_MASTER_DATA, (uint8_t)g_mask);
    out8(PIC_SLAVE_DATA, (uint8_t)(g_mask >> 8));
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        out8(PIC_SLAVE_CMD, PIC_EOI);
    }

    out8(PIC_MASTER_CMD, PIC_EOI);
}

#define DEFINE_IRQ_IMPL(n) \
    void isr_impl_irq##n(struct isr_stackframe *) { \
        irq_dispatch(n); \
    }

DEFINE_IRQ_IMPL(0)
DEFINE_IRQ_IMPL(1)
DEFINE_IRQ_IMPL(2)
DEFINE_IRQ_IMPL(3)
DEFINE_IRQ_IMPL(4)
DEFINE_IRQ_IMPL(5)
DEFINE_IRQ_IMPL(6)
DEFINE_IRQ_IMPL(7)
DEFINE_IRQ_IMPL(8)
DEFINE_IRQ_IMPL(9)
DEFINE_IRQ_IMPL(10)
DEFINE_IRQ_IMPL(11)
DEFINE_IRQ_IMPL(12)
DEFINE_IRQ_IMPL(13)
DEFINE_IRQ_IMPL(14)
DEFINE_IRQ_IMPL(15)

#ifdef OPAL_TEST
void isr_irq0() {}
void isr_irq1() {}
void isr_irq2() {}
void isr_irq3() {}
void isr_irq4() {}
void isr_irq5() {}
void isr_irq6() {}
void isr_irq7() {}
void isr_irq8() {}
void isr_irq9() {}
void isr_irq10() {}
void isr_irq11() {}
void isr_irq12() {}
void isr_irq13() {}
void isr_irq14() {}
void isr_irq15() {}
#endif
