#ifndef OPAL_TASK_TYPES_H
#define OPAL_TASK_TYPES_H

struct task;
struct process;

typedef struct taskptr {
    struct task *ptr;
} taskptr_t;

typedef struct procptr {
    struct process *ptr;
} procptr_t;

#endif
