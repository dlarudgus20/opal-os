#include <opal/test.h>
#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/irq.h>
#include <opal/timer.h>
#include <opal/kargs.h>
#include <opal/mm/mm.h>
#include <opal/fb/fb.h>
#include <opal/hid/hid.h>
#include <opal/task/task.h>
#include <opal/fs/disk.h>
#include <opal/fs/vfs.h>
#include <opal/shell/shell.h>
#include <opal/platform/asm.h>
#include <opal/platform/boot/boot.h>
#include <opal/platform/drivers/uart.h>
#include <opal/platform/drivers/ps2.h>
#include <opal/platform/drivers/pata.h>

static void drivers_init(void) {
    ps2_init();
    uart_init();
    pata_init();
}

static void run_user(void) {
#ifdef OPAL_UNIT_TEST
    unit_test_run();
#endif
    shell_start();
}

static void all_disks_register_bdev(void) {
    size_t count = disk_list_count();
    for (size_t i = 0; i < count; i++) {
        disk_register_bdev(disk_list_get(i));
    }
}

void kmain(void) {
    tty0_init();
    uart_early_init();
    klog_init();

    boot_init();
    kargs_init();

    mm_init();
    fb_init();
    hid_init();

    irq_init();
    timer_init();
    sched_init();

    vfs_init();
    drivers_init();
    all_disks_register_bdev();

    irq_enable_intr();
    interrupts_enable();

    run_user();

    irqmsg_drain_loop();
}
