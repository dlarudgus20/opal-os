#ifndef OPAL_TASK_EVENT_H
#define OPAL_TASK_EVENT_H

#include <opal/task/wait_list.h>

struct event {
    struct wait_list wait_list;
    bool signaled;
    bool auto_reset;
};

void event_init(struct event *ev, bool auto_reset);
void event_signal(struct event *ev);
void event_reset(struct event *ev);
bool event_wait(struct event *ev, uint64_t timeout);

#endif
