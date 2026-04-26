#include <kc/string.h>

#include <opal/tty.h>
#include <opal/task/task.h>
#include <opal/platform/asm.h>
#include <opal/platform/shell/shell_cmd.h>

#if !__has_attribute(target)
#error "Compiler does not support target attribute"
#endif

struct fputest_result {
    bool ok;
    const char *name;
    const char *reason;
    uint32_t actual0;
    uint32_t actual1;
};

struct fputest_arg {
    struct fputest_result *result;
    size_t repeat;
};

static void fputest_spin(void) {
    for (volatile size_t spin = 0; spin < 2000000; spin++) {
    }
}

static uint32_t f32_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

[[gnu::target("sse,sse2")]]
static void x87_load(float value) {
    __asm__ volatile ("fld DWORD PTR %0" : : "m"(value) : "memory");
}

[[gnu::target("sse,sse2")]]
static void x87_add(float value) {
    __asm__ volatile ("fadd DWORD PTR %0" : : "m"(value) : "memory");
}

[[gnu::target("sse,sse2")]]
static float x87_store_pop(void) {
    float value;
    __asm__ volatile ("fstp DWORD PTR %0" : "=m"(value) : : "memory");
    return value;
}

[[gnu::target("sse,sse2")]]
static void sse_load(float value) {
    __asm__ volatile ("movss xmm0, DWORD PTR %0" : : "m"(value) : "xmm0", "memory");
}

[[gnu::target("sse,sse2")]]
static void sse_add(float value) {
    __asm__ volatile ("addss xmm0, DWORD PTR %0" : : "m"(value) : "xmm0", "memory");
}

[[gnu::target("sse,sse2")]]
static float sse_store(void) {
    float value;
    __asm__ volatile ("movss DWORD PTR %0, xmm0" : "=m"(value) : : "memory");
    return value;
}

[[gnu::target("sse,sse2")]]
static void mixed_load(float x87_value, float sse_value) {
    __asm__ volatile (
        "fld DWORD PTR %0\n\t"
        "movss xmm0, DWORD PTR %1"
        :
        : "m"(x87_value), "m"(sse_value)
        : "xmm0", "memory"
    );
}

[[gnu::target("sse,sse2")]]
static void mixed_add(float x87_value, float sse_value) {
    __asm__ volatile (
        "fadd DWORD PTR %0\n\t"
        "addss xmm0, DWORD PTR %1"
        :
        : "m"(x87_value), "m"(sse_value)
        : "xmm0", "memory"
    );
}

[[gnu::target("sse,sse2")]]
static void mixed_store(float *x87_out, float *sse_out) {
    __asm__ volatile (
        "movss DWORD PTR %1, xmm0\n\t"
        "fstp DWORD PTR %0"
        : "=m"(*x87_out), "=m"(*sse_out)
        :
        : "memory"
    );
}

[[gnu::target("sse,sse2")]]
static void fputest_x87_task(uintptr_t argp) {
    struct fputest_arg *arg = (struct fputest_arg *)argp;
    static const float seed = 1.0f;
    static const float step = 0.25f;

    arg->result->name = "x87";
    x87_load(seed);
    for (size_t i = 0; i < arg->repeat; i++) {
        fputest_spin();
        x87_add(step);
    }

    float actual = x87_store_pop();
    arg->result->actual0 = f32_bits(actual);
    arg->result->ok = arg->result->actual0 == f32_bits(seed + step * (float)arg->repeat);
    arg->result->reason = arg->result->ok ? NULL : "x87 state mismatch";
}

[[gnu::target("sse,sse2")]]
static void fputest_sse_task(uintptr_t argp) {
    struct fputest_arg *arg = (struct fputest_arg *)argp;
    static const float seed = 2.0f;
    static const float step = 0.5f;

    arg->result->name = "sse";
    sse_load(seed);
    for (size_t i = 0; i < arg->repeat; i++) {
        fputest_spin();
        sse_add(step);
    }

    float actual = sse_store();
    arg->result->actual0 = f32_bits(actual);
    arg->result->ok = arg->result->actual0 == f32_bits(seed + step * (float)arg->repeat);
    arg->result->reason = arg->result->ok ? NULL : "xmm0 state mismatch";
}

[[gnu::target("sse,sse2")]]
static void fputest_mixed_task(uintptr_t argp) {
    struct fputest_arg *arg = (struct fputest_arg *)argp;
    static const float x87_seed = 3.0f;
    static const float x87_step = 1.0f;
    static const float sse_seed = 4.0f;
    static const float sse_step = 0.125f;

    arg->result->name = "mixed";
    mixed_load(x87_seed, sse_seed);
    for (size_t i = 0; i < arg->repeat; i++) {
        fputest_spin();
        mixed_add(x87_step, sse_step);
    }

    float x87_actual;
    float sse_actual;
    mixed_store(&x87_actual, &sse_actual);
    arg->result->actual0 = f32_bits(x87_actual);
    arg->result->actual1 = f32_bits(sse_actual);
    arg->result->ok =
        arg->result->actual0 == f32_bits(x87_seed + x87_step * (float)arg->repeat) &&
        arg->result->actual1 == f32_bits(sse_seed + sse_step * (float)arg->repeat);
    arg->result->reason = arg->result->ok ? NULL : "mixed x87/xmm0 state mismatch";
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

int shell_cmd_fputest(int, char **) {
    static const size_t repeat = 32;
    struct fputest_result results[3] = { 0 };
    struct fputest_arg args[] = {
        { .result = &results[0], .repeat = repeat },
        { .result = &results[1], .repeat = repeat },
        { .result = &results[2], .repeat = repeat },
    };
    taskptr_t tasks[3] = { 0 };

    tasks[0] = ktask_start(fputest_x87_task, (uintptr_t)&args[0], TASK_PRIORITY_NORMAL);
    tasks[1] = ktask_start(fputest_sse_task, (uintptr_t)&args[1], TASK_PRIORITY_NORMAL);
    tasks[2] = ktask_start(fputest_mixed_task, (uintptr_t)&args[2], TASK_PRIORITY_NORMAL);

    if (!tasks[0].ptr || !tasks[1].ptr || !tasks[2].ptr) {
        tty0_puts("fputest: ktask_start failed\n");
        testtask_cleanup(tasks, 3);
        return 1;
    }

    testtask_cleanup(tasks, 3);

    bool ok = results[0].ok && results[1].ok && results[2].ok;
    if (ok) {
        tty0_puts("fputest: PASS\n");
    } else {
        tty0_puts("fputest: FAIL\n");
        for (size_t i = 0; i < 3; i++) {
            if (!results[i].ok) {
                tty0_printf(
                    "  %s: %s (actual0=%u actual1=%u)\n",
                    results[i].name,
                    results[i].reason,
                    results[i].actual0,
                    results[i].actual1
                );
            }
        }
    }

    return ok ? 0 : 1;
}
