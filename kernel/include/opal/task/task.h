#ifndef OPAL_TASK_TASK_H
#define OPAL_TASK_TASK_H

#include <opal/platform/task/context.h>

struct task {
    struct context ctx;
};

void sched_init(void);

#endif
