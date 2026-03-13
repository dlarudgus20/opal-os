#include <stdatomic.h>

#include <kc/ctype.h>
#include <kc/string.h>
#include <kc/assert.h>

#include <opal/test.h>
#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/fb/fb.h>
#include <opal/task/task.h>
#include <opal/platform/boot.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>

#define UNAME_MSG "opal-os ("OPAL_PLATFORM" "OPAL_CONFIG")"

static tid_t g_task_id = TID_INVALID;

static int run_priotest(void);

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
        tty0_puts("  priotest  - run priority scheduler smoke test\n");
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

    if (strcmp(cmd, "priotest") == 0) {
        return run_priotest();
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

struct priotest_state {
    _Atomic size_t len;
    char log[64];
};

struct priotest_arg {
    struct priotest_state *state;
    char tag;
    size_t repeat;
};

static bool priotest_match(const struct priotest_state *state) {
    static const size_t repeat = 4;
    size_t len = atomic_load(&state->len);
    if (len != repeat * 4) {
        return false;
    }

    for (size_t i = 0; i < repeat; i++) {
        if (state->log[i] != 'H') {
            return false;
        }
    }

    for (size_t i = repeat; i < repeat * 3; i++) {
        if (state->log[i] != 'a' && state->log[i] != 'b') {
            return false;
        }
    }

    for (size_t i = repeat * 3; i < repeat * 4; i++) {
        if (state->log[i] != 'L') {
            return false;
        }
    }

    return true;
}

static void priotest_task(uintptr_t argp) {
    struct priotest_arg *arg = (struct priotest_arg *)argp;
    for (size_t i = 0; i < arg->repeat; i++) {
        size_t idx = atomic_fetch_add(&arg->state->len, 1);
        assert(idx < sizeof(arg->state->log));
        arg->state->log[idx] = arg->tag;

        for (volatile size_t spin = 0; spin < 1000000; spin++) {
        }
    }
    task_exit();
}

static void priotest_cleanup(taskptr_t *tasks, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!tasks[i].ptr) {
            continue;
        }
        (void)task_join(tasks[i].ptr, TIMEOUT_INFINITY);
        task_release(tasks[i]);
    }
}

static int run_priotest(void) {
    struct priotest_state state = { 0 };
    struct priotest_arg args[] = {
        { .state = &state, .tag = 'a', .repeat = 4 },
        { .state = &state, .tag = 'H', .repeat = 4 },
        { .state = &state, .tag = 'b', .repeat = 4 },
        { .state = &state, .tag = 'L', .repeat = 4 },
    };
    taskptr_t tasks[4] = { 0 };

    tasks[0] = task_create(priotest_task, (uintptr_t)&args[0], TASK_PRIORITY_LOW);
    tasks[1] = task_create(priotest_task, (uintptr_t)&args[1], TASK_PRIORITY_HIGH);
    tasks[2] = task_create(priotest_task, (uintptr_t)&args[2], TASK_PRIORITY_LOW);
    tasks[3] = task_create(priotest_task, (uintptr_t)&args[3], TASK_PRIORITY_LOWEST);

    if (!tasks[0].ptr || !tasks[1].ptr || !tasks[2].ptr || !tasks[3].ptr) {
        tty0_puts("priotest: task_create failed\n");
        priotest_cleanup(tasks, 4);
        return 1;
    }

    priotest_cleanup(tasks, 4);

    size_t len = atomic_load(&state.len);
    state.log[len] = '\0';
    tty0_printf("priotest: log=%s expected=H{4}[ab]{8}L{4}\n", state.log);
    tty0_puts(priotest_match(&state) ? "priotest: PASS\n" : "priotest: FAIL\n");
    return 1;
}
