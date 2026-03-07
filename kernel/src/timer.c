#include <opal/timer.h>
#include <opal/klog.h>
#include <opal/platform/drivers/pit.h>

static void timer_isr(void) {
    static unsigned counter = 0;
    if (++counter % TIMER_HZ == 0) {
        kdebug("timeout");
    }
}

void timer_init(void) {
    pit_init(TIMER_HZ, timer_isr);
}
