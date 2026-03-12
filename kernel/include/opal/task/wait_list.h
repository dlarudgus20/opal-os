#ifndef OPAL_TASK_WAIT_LIST_H
#define OPAL_TASK_WAIT_LIST_H

#include <collections/linkedlist.h>

#include <opal/timer.h>

struct wait_list {
    struct linkedlist queue;
};

static inline void wait_list_init(struct wait_list *wl) {
    linkedlist_init(&wl->queue);
}

bool wait_list_wake_one(struct wait_list *wl);
void wait_list_wake_all(struct wait_list *wl);

#endif
