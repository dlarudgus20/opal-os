#include <opal/timer.h>
#include <opal/task/task.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/drivers/pic.h>
#include <opal/platform/drivers/pit.h>

static uint64_t g_tick = 0;

static void isr_timer(void) {
    g_tick++;
    irq_send_eoi(PIC_IRQ_TIMER);

    schedule();
}

void timer_init(void) {
    pit_init(TIMER_HZ, isr_timer);
}

uint64_t timer_get_tick(void) {
    irqlock_t irqlock = irqlock_acquire();
    uint64_t tick = g_tick;
    irqlock_release(&irqlock);
    return tick;
}
