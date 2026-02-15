#include <stdarg.h>

#include <kc/fmt.h>

#include <opal/test.h>
#include <opal/tty.h>

#ifdef UNIT_TEST

extern const struct unit_test_info *unit_test_start__[];
extern const struct unit_test_info *unit_test_end__[];

static struct {
    uint32_t run_count;
    uint32_t pass_count;
    uint32_t fail_count;
    uint32_t current_failures;
    const char *current_name;
} g_test_state;

static void unit_test_begin(const char *name) {
    g_test_state.current_name = name;
    g_test_state.current_failures = 0;
}

static void unit_test_end(void) {
    g_test_state.run_count++;
    if (g_test_state.current_failures == 0) {
        g_test_state.pass_count++;
        tty0_printf("\x1b[1;92m[       OK ]\x1b[0m %s\n", g_test_state.current_name);
    } else {
        g_test_state.fail_count++;
        tty0_printf("\x1b[1;91m[  FAILED  ]\x1b[0m %s (%u failures)\n",
            g_test_state.current_name, g_test_state.current_failures);
    }
}

void unit_test_expect_true_failed(const char *expr, const char *file, unsigned line) {
    g_test_state.current_failures++;
    tty0_printf("\x1b[1;91m  Failure\x1b[0m %s:%u: EXPECT_TRUE(%s)\n", file, line, expr);
}

void unit_test_expect_eq_u64_failed(
    const char *expected_expr,
    const char *actual_expr,
    uint64_t expected,
    uint64_t actual,
    const char *file,
    unsigned line
) {
    g_test_state.current_failures++;
    tty0_printf(
        "\x1b[1;91m  Failure\x1b[0m %s:%u: EXPECT_EQ(%s, %s), expected=0x%llx actual=0x%llx\n",
        file,
        line,
        expected_expr,
        actual_expr,
        (unsigned long long)expected,
        (unsigned long long)actual
    );
}

void unit_test_expect_streq_failed(
    const char *expected_expr,
    const char *actual_expr,
    const char *expected,
    const char *actual,
    const char *file,
    unsigned line
) {
    g_test_state.current_failures++;
    tty0_printf(
        "\x1b[1;91m  Failure\x1b[0m %s:%u: EXPECT_STREQ(%s, %s), expected=\"%s\" actual=\"%s\"\n",
        file,
        line,
        expected_expr,
        actual_expr,
        expected ? expected : "(null)",
        actual ? actual : "(null)"
    );
}

void unit_test_run(void) {
    g_test_state.run_count = 0;
    g_test_state.pass_count = 0;
    g_test_state.fail_count = 0;
    g_test_state.current_failures = 0;
    g_test_state.current_name = 0;
    tty0_printf("\n==== unit test run ====\n");
    for (const struct unit_test_info **pp = unit_test_start__; pp < unit_test_end__; pp++) {
        if (!*pp) {
            continue;
        }
        const struct unit_test_info *p = *pp;
        tty0_printf("\x1b[1;92m[ RUN      ]\x1b[0m %s\n", p->item);
        unit_test_begin(p->item);
        p->fn();
        unit_test_end();
    }
    tty0_printf(
        "==== unit test end ==== run=%u pass=%u fail=%u\n",
        g_test_state.run_count,
        g_test_state.pass_count,
        g_test_state.fail_count
    );
    if (g_test_state.fail_count != 0) {
        panicf("unit test failed: %u", g_test_state.fail_count);
    }
}

#endif
