#include <stdatomic.h>

#include <kc/assert.h>

#include <opal/tty.h>
#include <opal/shell/shell_cmd.h>
#include <opal/task/task.h>

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

        for (volatile size_t spin = 0; spin < 1000000; spin++) {}
    }
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

int shell_cmd_priotest(int, char **) {
    struct priotest_state state = { 0 };
    struct priotest_arg args[] = {
        { .state = &state, .tag = 'a', .repeat = 4 },
        { .state = &state, .tag = 'H', .repeat = 4 },
        { .state = &state, .tag = 'b', .repeat = 4 },
        { .state = &state, .tag = 'L', .repeat = 4 },
    };
    taskptr_t tasks[4] = { 0 };

    tasks[0] = ktask_start(priotest_task, (uintptr_t)&args[0], TASK_PRIORITY_LOW);
    tasks[1] = ktask_start(priotest_task, (uintptr_t)&args[1], TASK_PRIORITY_HIGH);
    tasks[2] = ktask_start(priotest_task, (uintptr_t)&args[2], TASK_PRIORITY_LOW);
    tasks[3] = ktask_start(priotest_task, (uintptr_t)&args[3], TASK_PRIORITY_LOWEST);

    if (!tasks[0].ptr || !tasks[1].ptr || !tasks[2].ptr || !tasks[3].ptr) {
        tty0_puts("priotest: ktask_start failed\n");
        testtask_cleanup(tasks, 4);
        return 1;
    }

    testtask_cleanup(tasks, 4);

    size_t len = atomic_load(&state.len);
    state.log[len] = '\0';
    bool ok = priotest_match(&state);
    tty0_printf("priotest: log=%s expected=H{4}[ab]{8}L{4}\n", state.log);
    tty0_puts(ok ? "priotest: PASS\n" : "priotest: FAIL\n");
    return ok ? 0 : 1;
}
