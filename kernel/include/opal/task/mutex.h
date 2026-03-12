#ifndef OPAL_TASK_MUTEX_H
#define OPAL_TASK_MUTEX_H

#include <opal/task/wait_list.h>

struct task;

struct mutex {
    struct wait_list wait_list;
    struct task *owner;
};

void mutex_init(struct mutex *mutex);
void mutex_lock(struct mutex *mutex);
void mutex_unlock(struct mutex *mutex);

#endif
