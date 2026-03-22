#include <kc/inttypes.h>
#include <kc/string.h>

#include <opal/tty.h>
#include <opal/task/task.h>
#include <opal/platform/shell/shell_cmd.h>

#if !__has_attribute(target)
#error "Compiler does not support target attribute"
#endif

#define FLOATTEST_TASK_COUNT 16
#define FLOATTEST_REPEAT_COUNT 10000

struct floattest_result {
    bool ok;
    uint64_t bits;
};

struct floattest_arg {
    struct floattest_result *result;
};

[[gnu::target("sse,sse2")]]
static uint64_t f64_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void testtask_cleanup(taskptr_t *tasks, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!tasks[i].ptr) {
            continue;
        }
        task_join(tasks[i].ptr, TIMEOUT_INFINITY);
        task_release(tasks[i]);
    }
}

[[gnu::target("sse,sse2")]]
static double floattest_compute_value(void) {
    double pi = 3.14159265358979323846;
    double e = 2.71828182845904523536;

    volatile double pi_pow = 1.0;
    volatile double e_pow = 1.0;
    for (size_t i = 0; i < 10; i++) {
        pi_pow *= pi;
        e_pow *= e;
    }

    double ratio = pi_pow / e_pow;
    volatile double value = 1.0;
    for (size_t i = 0; i < 20; i++) {
        value *= ratio;
    }

    return value / (10000.0 * e);
}

[[gnu::target("sse,sse2")]]
static void floattest_task(uintptr_t argp) {
    struct floattest_arg *arg = (struct floattest_arg *)argp;

    uint64_t value = f64_bits(floattest_compute_value());
    for (int i = 1; i < FLOATTEST_REPEAT_COUNT; i++) {
        uint64_t redo = f64_bits(floattest_compute_value());
        if (value != redo) {
            arg->result->bits = redo;
            arg->result->ok = false;
            task_exit();
        }
    }

    arg->result->bits = value;
    arg->result->ok = true;
    task_exit();
}

int shell_cmd_floattest(int, char **) {
    struct floattest_result results[FLOATTEST_TASK_COUNT] = { 0 };
    struct floattest_arg args[FLOATTEST_TASK_COUNT] = { 0 };
    taskptr_t tasks[FLOATTEST_TASK_COUNT] = { 0 };

    for (size_t i = 0; i < FLOATTEST_TASK_COUNT; i++) {
        args[i].result = &results[i];
        tasks[i] = task_create(floattest_task, (uintptr_t)&args[i], TASK_PRIORITY_NORMAL);
        if (!tasks[i].ptr) {
            tty0_puts("floattest: task_create failed\n");
            testtask_cleanup(tasks, FLOATTEST_TASK_COUNT);
            return 1;
        }
    }

    testtask_cleanup(tasks, FLOATTEST_TASK_COUNT);

    uint64_t expected = results[0].bits;
    bool ok = true;
    for (size_t i = 0; i < FLOATTEST_TASK_COUNT; i++) {
        if (!results[i].ok || results[i].bits != expected) {
            ok = false;
            break;
        }
    }

    if (ok) {
        tty0_printf("floattest: PASS bits=%#018"PRIx64" tasks=%u\n",
            expected, FLOATTEST_TASK_COUNT);
    } else {
        tty0_printf("floattest: FAIL expected=%#018"PRIx64" tasks=%u\n",
            expected, FLOATTEST_TASK_COUNT);
        for (size_t i = 0; i < FLOATTEST_TASK_COUNT; i++) {
            tty0_printf("  task[%zu]=%#018"PRIx64"\n",
                i, results[i].bits);
        }
    }

    return ok ? 0 : 1;
}
