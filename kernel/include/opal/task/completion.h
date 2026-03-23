#ifndef OPAL_TASK_COMPLETION_H
#define OPAL_TASK_COMPLETION_H

#include <opal/task/wait_list.h>

struct completion {
    struct wait_list wait_list;
    bool signaled;
};

void completion_init(struct completion *c);
void completion_signal(struct completion *c);
bool completion_wait(struct completion *c, uint64_t timeout);

#endif
