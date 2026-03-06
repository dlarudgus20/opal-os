#include <stddef.h>
#include <stdint.h>

#include <kc/ctype.h>
#include <kc/string.h>
#include <kc/assert.h>

#include <opal/test.h>
#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/irq.h>
#include <opal/drivers/uart.h>
#include <opal/drivers/fb/fb.h>
#include <opal/platform/boot.h>
#include <opal/platform/descriptors.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>
#include <opal/platform/drivers/ps2.h>

#define UNAME_MSG "opal-os ("OPAL_PLATFORM" "OPAL_CONFIG")"

static void print_banner(void) {
    tty0_puts("\n");
    tty0_puts("========================================\n");
    tty0_puts("  "UNAME_MSG"\n");
    tty0_puts("========================================\n");
}

static int handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        tty0_puts("commands:\n");
#ifdef OPAL_UNIT_TEST
        tty0_puts("  unit      - run unit tests\n");
        tty0_puts("  uheavy    - run heavy unit tests\n");
#endif
        tty0_puts("  help      - show this message\n");
        tty0_puts("  uname     - show kernel name\n");
        tty0_puts("  echo TEXT - print TEXT\n");
        tty0_puts("  exit      - logout\n");
        tty0_puts("  halt      - halt system\n");
        tty0_puts("  klog TEXT - log TEXT\n");
        tty0_puts("  kmsg      - read logs\n");
        tty0_puts("  mmap      - log memory map\n");
        tty0_puts("  ptable    - show pagetable\n");
        tty0_puts("  pfns      - show pfn list\n");
        tty0_puts("  kargs     - show kernel boot argument\n");
        tty0_puts("  fbinfo    - show framebuffer info\n");
        return 1;
    }

#ifdef OPAL_UNIT_TEST
    if (strcmp(cmd, "unit") == 0) {
        unit_test_run();
        return 1;
    }

    if (strcmp(cmd, "uheavy") == 0) {
        unit_test_run_heavy();
        return 1;
    }
#endif

    if (strcmp(cmd, "uname") == 0) {
        tty0_puts(UNAME_MSG"\n");
        return 1;
    }

    if (strncmp(cmd, "echo", 4) == 0) {
        const char *text = cmd + 4 + strspn(cmd + 4, " ");
        tty0_puts(text);
        tty0_puts("\n");
        return 1;
    }

    if (strcmp(cmd, "exit") == 0) {
        tty0_puts("logout\n");
        return 0;
    }

    if (strcmp(cmd, "halt") == 0) {
        panic("system halt is not implemented");
    }

    if (strncmp(cmd, "klog", 4) == 0) {
        const char *text = cmd + 4 + strspn(cmd + 4, " ");
        uint16_t level = KLOG_INFO;
        if (isdigit(*text)) {
            level = *text - '0';
            text += strspn(text + 1, " ") + 1;
        }
        klogf(level, "%s", text);
        return 1;
    }

    if (strcmp(cmd, "kmsg") == 0) {
        klog_print_all_tty0(true);
        return 1;
    }

    if (strcmp(cmd, "mmap") == 0) {
        mm_log_map();
        return 1;
    }

    if (strcmp(cmd, "ptable") == 0) {
        mm_pagetable_print();
        return 1;
    }

    if (strcmp(cmd, "pfns") == 0) {
        mm_pfn_print_all();
        return 1;
    }

    if (strcmp(cmd, "kargs") == 0) {
        tty0_puts(boot_get_cmdline());
        tty0_puts("\n");
        return 1;
    }

    if (strcmp(cmd, "fbinfo") == 0) {
        const struct boot_fbinfo *fbinfo = boot_get_fbinfo();
        if (fbinfo) {
            tty0_printf("addr=%#010"PRIphys", %ux%u, pitch=%u, bpp=%u\n",
                fbinfo->addr, fbinfo->width, fbinfo->height, fbinfo->pitch, fbinfo->bpp);
        } else {
            tty0_printf("there is no framebuffer.\n");
        }
        return 1;
    }

    if (cmd[0] != '\0') {
        tty0_puts("unknown command: ");
        tty0_puts(cmd);
        tty0_puts("\n");
    }

    return 1;
}

static void run_shell(void) {
    char line[128];

    while (1) {
        tty0_puts("root@opal:~$ ");
        uart_read_line(line, sizeof(line));

        if (!handle_command(line)) {
            return;
        }
    }
}

void kmain(void) {
    boot_info_init();
    descriptors_init();

    tty0_init();
    uart_init();
    klog_init();

    mm_init();
    fb_init();

    irq_init();
    ps2_init();

    irq_enable_intr();

#ifdef OPAL_UNIT_TEST
    unit_test_run();
#endif
    print_banner();

    kinfo("boot args=%s", boot_get_cmdline());

    while (1) {
        interrupts_enable_and_wait();
        irqmsg_drain();
    }

    (void)run_shell;
}

DEFINE_UNIT_TEST(simple_test) {
    TEST_EXPECT_EQ(1, 1);
    TEST_EXPECT_TRUE(2 > 1);
}
