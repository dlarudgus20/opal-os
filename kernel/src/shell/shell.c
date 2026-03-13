#include <stdatomic.h>

#include <kc/ctype.h>
#include <kc/string.h>
#include <kc/assert.h>

#include <opal/test.h>
#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/kargs.h>
#include <opal/shell/shell.h>
#include <opal/shell/shell_cmd.h>
#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/fb/fb.h>
#include <opal/task/task.h>
#include <opal/platform/mm/pagetable.h>
#include <opal/platform/boot/bootinfo.h>
#include <opal/platform/shell/shell_cmd.h>

#define UNAME_MSG "opal-os ("OPAL_PLATFORM" "OPAL_CONFIG")"

static tid_t g_task_id = TID_INVALID;

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
        tty0_puts("  bootmodules - show boot module list\n");
        tty0_puts("  fbinfo    - show framebuffer info\n");
        tty0_puts("  allocinfo - show buddy allocator info\n");
        tty0_puts("  priotest  - run priority scheduler smoke test\n");
        tty0_puts("  fputest   - run floating-point quick test\n");
        tty0_puts("  floattest - run floating-point consistency test\n");
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
        kargs_print_log();
        tty0_puts("\n");
        return 1;
    }

    if (strcmp(cmd, "bootmodules") == 0) {
        const struct bootinfo_module_list *modules = bootinfo_get_modules();
        tty0_printf("boot modules: %u\n", modules->len);
        for (uint32_t i = 0; i < modules->len; i++) {
            const struct bootinfo_module *module = &modules->modules[i];
            tty0_printf("  [%u] [%#018"PRIphys", %#018"PRIphys") %s\n",
                i, module->begin, module->end, module->name);
        }
        return 1;
    }

    if (strcmp(cmd, "fbinfo") == 0) {
        const struct bootinfo_fb *fbinfo = bootinfo_get_fb();
        if (fbinfo) {
            tty0_printf("addr=%#010"PRIphys", %ux%u, pitch=%u, bpp=%u\n",
                fbinfo->addr, fbinfo->width, fbinfo->height, fbinfo->pitch, fbinfo->bpp);
        } else {
            tty0_printf("there is no framebuffer.\n");
        }
        return 1;
    }

    if (strcmp(cmd, "allocinfo") == 0) {
        mm_log_buddy();
        return 1;
    }

    if (strcmp(cmd, "priotest") == 0) {
        return shell_cmd_priotest();
    }

    if (strcmp(cmd, "fputest") == 0) {
        return shell_cmd_fputest();
    }

    if (strcmp(cmd, "floattest") == 0) {
        return shell_cmd_floattest();
    }

    if (cmd[0] != '\0') {
        tty0_puts("unknown command: ");
        tty0_puts(cmd);
        tty0_puts("\n");
    }

    return 1;
}

static void run_shell(uintptr_t) {
    char line[128];

    while (1) {
        print_banner();

        while (1) {
            tty0_puts("root@opal:~$ ");
            tty0_getline(line, sizeof(line));

            if (!handle_command(line)) {
                return;
            }
        }
    }
}

void shell_start(void) {
    taskptr_t task = task_create(run_shell, 0, TASK_PRIORITY_NORMAL);
    if (!task.ptr) {
        return;
    }

    g_task_id = task_release(task);
}
