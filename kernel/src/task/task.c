#include <opal/tty.h>
#include <opal/task/task.h>

static struct context from;
static struct context to;
alignas(4096) static char stack[4096];

static void testtask(void *arg) {
    tty0_printf("testtask(%p)\n", arg);
    context_switch(&to, &from);
}

void sched_init(void) {
    context_init(&to, (uintptr_t)testtask, stack, sizeof(stack), 0x1234);
    context_switch(&from, &to);
}
