#include <kc/assert.h>
#include <kc/string.h>

#include <collections/ringbuffer.h>

#include <opal/irq.h>
#include <opal/klog.h>
#include <opal/platform/asm.h>

#define IRQ_QUEUE_SIZE 4096

static irqmsg_handler_t g_irqmsg_handlers[IRQMSG_COUNT];

static struct irqmsg g_msg_buffer[IRQ_QUEUE_SIZE];
static struct ringbuffer g_msg_queue;

static uint32_t g_msg_drops = 0;
static bool g_msg_drop_event = false;

static void isrmsg_drop(struct irqmsg) {
    kwarn("irqmsg: queue full, dropped #%u", g_msg_drops);
}

void irq_init(void) {
    irq_device_init();

    memset(g_irqmsg_handlers, 0, sizeof(g_irqmsg_handlers));
    ringbuffer_init(&g_msg_queue, g_msg_buffer, IRQ_QUEUE_SIZE);

    irqmsg_register(IRQMSG_DROP, isrmsg_drop);
}

void irqmsg_register(irqmsg_type_t msg, irqmsg_handler_t handler) {
    assert(msg < IRQMSG_COUNT);
    g_irqmsg_handlers[msg] = handler;
}

bool irqmsg_push(struct irqmsg msg) {
    if (ringbuffer_is_full(&g_msg_queue)) {
        g_msg_drops++;
        g_msg_drop_event = true;
        return false;
    }

    ringbuffer_push(&g_msg_queue, struct irqmsg, msg);
    return true;
}

static bool irqmsg_pop(struct irqmsg* msg_out) {
    if (g_msg_drop_event) {
        g_msg_drop_event = false;
        *msg_out = (struct irqmsg){ .type = IRQMSG_DROP, .data = 0 };
        return true;
    }

    if (ringbuffer_is_empty(&g_msg_queue)) {
        return false;
    }

    *msg_out = ringbuffer_pop(&g_msg_queue, struct irqmsg);
    return true;
}

void irqmsg_drain(void) {
    struct irqmsg msg;

    while (1) {
        interrupts_disable();
        if (!irqmsg_pop(&msg)) {
            break;
        }

        interrupts_enable();

        assert(msg.type < IRQMSG_COUNT);
        irqmsg_handler_t handler = g_irqmsg_handlers[msg.type];
        if (handler) {
            handler(msg);
        }
    }
}
