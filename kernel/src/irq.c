#include <kc/assert.h>
#include <kc/string.h>

#include <collections/ringbuffer.h>

#include <opal/irq.h>
#include <opal/klog.h>
#include <opal/task/task.h>
#include <opal/task/waitable.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/asm.h>

#define IRQ_QUEUE_SIZE 4096

static irqmsg_handler_t g_irqmsg_handlers[IRQMSG_COUNT];

static struct irqmsg g_msg_buffer[IRQ_QUEUE_SIZE];
static struct ringbuffer g_msg_queue;

static uint32_t g_msg_drops = 0;
static bool g_msg_drop_event = false;

static struct waitable g_waitable;

static void isrmsg_drop(struct irqmsg) {
    kwarn("irqmsg: queue full, dropped #%u", g_msg_drops);
}

void irq_init(void) {
    irq_device_init();

    memset(g_irqmsg_handlers, 0, sizeof(g_irqmsg_handlers));
    ringbuffer_init(&g_msg_queue, g_msg_buffer, IRQ_QUEUE_SIZE);

    irqmsg_register(IRQMSG_DROP, isrmsg_drop);

    waitable_init(&g_waitable, true);
}

void irqmsg_register(irqmsg_type_t msg, irqmsg_handler_t handler) {
    assert(msg < IRQMSG_COUNT);
    irqlock_t irqlock = irqlock_acquire();
    g_irqmsg_handlers[msg] = handler;
    irqlock_release(&irqlock);
}

bool irqmsg_push(struct irqmsg msg) {
    irqlock_t irqlock = irqlock_acquire();

    if (ringbuffer_is_full(&g_msg_queue)) {
        g_msg_drops++;
        g_msg_drop_event = true;
        waitable_trigger(&g_waitable);

        irqlock_release(&irqlock);
        return false;
    }

    ringbuffer_push(&g_msg_queue, struct irqmsg, msg);
    waitable_trigger(&g_waitable);

    irqlock_release(&irqlock);
    return true;
}

static bool irqmsg_pop(struct irqmsg* msg_out) {
    irqlock_t irqlock = irqlock_acquire();
    bool ret = true;

    if (g_msg_drop_event) {
        g_msg_drop_event = false;
        *msg_out = (struct irqmsg){ .type = IRQMSG_DROP, .data = 0 };
        goto exit;
    }

    if (ringbuffer_is_empty(&g_msg_queue)) {
        ret = false;
        goto exit;
    }

    *msg_out = ringbuffer_pop(&g_msg_queue, struct irqmsg);

exit:
    irqlock_release(&irqlock);
    return ret;
}

[[noreturn]] void irqmsg_drain_loop(void) {
    struct irqmsg msg;

    interrupts_disable();
    while (1) {
        task_wait_for(&g_waitable, TIMEOUT_INFINITY);

        while (irqmsg_pop(&msg)) {
            interrupts_enable();

            assert(msg.type < IRQMSG_COUNT);
            irqmsg_handler_t handler = g_irqmsg_handlers[msg.type];
            if (handler) {
                handler(msg);
            }

            interrupts_disable();
        }
    }
}
