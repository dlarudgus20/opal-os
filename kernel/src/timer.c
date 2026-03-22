#include <stdatomic.h>

#include <opal/timer.h>
#include <opal/task/task.h>
#include <opal/platform/drivers/pata.h>
#include <opal/platform/drivers/pic.h>
#include <opal/platform/drivers/pit.h>

static _Atomic uint64_t g_tick = 0;

static void isr_timer(void) {
    g_tick++;
    irq_send_eoi(PIC_IRQ_TIMER);

    pata_on_timer(g_tick);
    sched_on_timer();
}

void timer_init(void) {
    pit_init(TIMER_HZ, isr_timer);
}

uint64_t timer_get_tick(void) {
    return g_tick;
}
