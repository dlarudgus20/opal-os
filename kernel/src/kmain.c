#include <opal/test.h>
#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/hid/hid.h>
#include <opal/mm/mm.h>
#include <opal/irq.h>
#include <opal/timer.h>
#include <opal/fb/fb.h>
#include <opal/task/task.h>
#include <opal/platform/boot.h>
#include <opal/platform/descriptors.h>
#include <opal/platform/asm.h>
#include <opal/platform/drivers/uart.h>
#include <opal/platform/drivers/ps2.h>

void kmain(void) {
    tty0_init();
    uart_early_init();
    klog_init();

    boot_info_init();
    descriptors_init();

    mm_init();
    fb_init();
    hid_init();

    irq_init();
    timer_init();
    sched_init();

    ps2_init();
    uart_init();

    irq_enable_intr();
    interrupts_enable();

#ifdef OPAL_UNIT_TEST
    unit_test_run();
#endif

    kinfo("boot args=%s", boot_get_cmdline());

    irqmsg_drain_loop();
}
