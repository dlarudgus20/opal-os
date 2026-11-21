#include <opal/test.h>

#ifndef OPAL_TEST

void test_run(void) {}
void test_run_heavy(void) {}
void test_abort(void) {}

#else

#include <opal/platform/asm.h>
#include <opal/platform/debug_exit.h>
#include <opal/platform/drivers/uart.h>
#include <kc/kassert.h>

extern const struct test_info *const test_start__[];
extern const struct test_info *const test_end__[];

static struct {
    bool running;
    uint32_t run_count;
    uint32_t pass_count;
    uint32_t fail_count;
    uint32_t current_failures;
    const char *current_name;
} g_test_state;

static void test_begin(const char *name) {
    g_test_state.current_name = name;
    g_test_state.current_failures = 0;
}

static void test_end(void) {
    g_test_state.run_count++;
    if (g_test_state.current_failures == 0) {
        g_test_state.pass_count++;
        platform_uart_printf("[       OK ] %s\n", g_test_state.current_name);
    } else {
        g_test_state.fail_count++;
        platform_uart_printf("[  FAILED  ] %s (%u failures)\n", g_test_state.current_name,
            g_test_state.current_failures);
    }
    g_test_state.current_name = 0;
}

static void test_print_summary(void) {
    platform_uart_printf("==== test end ==== run=%u pass=%u fail=%u\n", g_test_state.run_count,
        g_test_state.pass_count, g_test_state.fail_count);
}

void test_condition_failed(const char *check, const char *expr, const char *file, unsigned line) {
    g_test_state.current_failures++;
    platform_uart_printf("  Failure %s:%u: %s(%s)\n", file, line, check, expr);
}

void test_expect_eq_u64_failed(const char *expected_expr, const char *actual_expr,
    uint64_t expected, uint64_t actual, const char *file, unsigned line) {
    g_test_state.current_failures++;
    platform_uart_printf("  Failure %s:%u: EXPECT_EQ(%s, %s), expected=0x%llx actual=0x%llx\n",
        file, line, expected_expr, actual_expr, (unsigned long long)expected,
        (unsigned long long)actual);
}

void test_expect_streq_failed(const char *expected_expr, const char *actual_expr,
    const char *expected, const char *actual, const char *file, unsigned line) {
    g_test_state.current_failures++;
    platform_uart_printf("  Failure %s:%u: EXPECT_STREQ(%s, %s), expected=\"%s\" actual=\"%s\"\n",
        file, line, expected_expr, actual_expr, expected ? expected : "(null)",
        actual ? actual : "(null)");
}

static void debug_exit(bool ok) {
#ifdef PLATFORM_HAS_DEBUG_EXIT
    platform_debug_exit(ok);
    while (1) {
        wait_for_interrupt();
    }
#endif
}

static void run_test_group(bool heavy) {
    static bool uart_initialized;

    if (!uart_initialized) {
        platform_uart_init();
        uart_initialized = true;
    }

    g_test_state.run_count = 0;
    g_test_state.pass_count = 0;
    g_test_state.fail_count = 0;
    g_test_state.current_failures = 0;
    g_test_state.current_name = 0;
    g_test_state.running = true;
    platform_uart_printf("\n==== test run ====\n");
    for (const struct test_info *const *pp = test_start__; pp < test_end__; pp++) {
        if (!*pp) {
            continue;
        }
        const struct test_info *p = *pp;
        bool is_heavy = strncmp(p->item, "heavy__", 7) == 0;
        if (is_heavy == heavy) {
            platform_uart_printf("[ RUN      ] %s\n", p->item);
            test_begin(p->item);
            p->fn();
            test_end();
        }
    }
    test_print_summary();
    if (g_test_state.fail_count != 0) {
        panicf("test failed: %u", g_test_state.fail_count);
    }
    g_test_state.running = false;

    debug_exit(true);
}

void test_run(void) {
    run_test_group(false);
}

void test_run_heavy(void) {
    run_test_group(true);
}

void test_abort(void) {
    if (!g_test_state.running) {
        return;
    }
    g_test_state.running = false;

    if (g_test_state.current_name) {
        platform_uart_printf("[  FAILED  ] %s (aborted)\n", g_test_state.current_name);
        g_test_state.current_name = 0;
        g_test_state.run_count++;
        g_test_state.fail_count++;
        test_print_summary();
    }

    debug_exit(false);
}

#endif // OPAL_TEST
